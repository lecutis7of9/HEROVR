"""ULTIMATE ALLIANCE EXPANSION (M1): the Stark Tower hub as a HERO VR walkable room - the Blender half.

Run by tools/ua/convert_ua.py (never by hand needed):
  blender -b --factory-startup --python build_stark_tower.py -- <__combined.igb> <stark1.engb.xml> <out_dir>

Reads the player's own MUA1 PS2 stark1 package (igb-blender add-on, mua1_ps2 profile) and writes into out_dir:
  scene.glb   everything drawn: the hub, its props, the city backdrop (raised + pushed out so the skyline sits
              round a standing eye, not under the floor), and a new CEILING (the original hub is open-topped for
              the game's high camera) made of the hub's own textures: dark stk_floor panels, gold `rail` beams,
              warm light panels. Metres, glTF Y-up, the engb player start at the origin.
  walk.glb    the walkable geometry only (no backdrop, no ceiling): make_hideout.py's `mesh` collision input.
  room.json   spawn, RETURN TO CITY point, playable bounds (make_hideout.py's description).
  npcs.txt    hooks for the next update (characters): every NPC spawner of the engb, room space.
  report.txt  counts, bounds, what was moved.
No game file is written; nothing here is shipped - the player builds it from their own disc.
"""
import sys, os, math, json, argparse
import xml.etree.ElementTree as ET
import bpy, bmesh, addon_utils, mathutils

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument('igb'); ap.add_argument('engb'); ap.add_argument('out')
ap.add_argument('--scale', type=float, default=0.0254)      # metres a unit: Raven's units are inches (a 72-unit hero ~ 1.83 m)
ap.add_argument('--skyline-raise', type=float, default=520)  # units the city's buildings + ground go up
ap.add_argument('--skyline-push', type=float, default=1.3)   # how much farther out (about the hub's middle)
ap.add_argument('--sky-raise', type=float, default=900)      # the sky box (its painted city band) up
ap.add_argument('--ceiling', type=float, default=960)        # units: the glass walls' top
ap.add_argument('--no-ceiling', action='store_true')
ap.add_argument('--place', default='stark1', choices=['stark1', 'stark2'])  # stark1: the hub; stark2: Tony Stark's / Iron Man's Lab
ap.add_argument('--offset', type=float, nargs=2, default=[0.0, 0.0])  # metres (x, z of the room): where this place sits in the combined room
a = ap.parse_args(argv)
os.makedirs(a.out, exist_ok=True)
S = a.scale

addon_utils.enable('igb_blender', default_set=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
addon_utils.enable('igb_blender', default_set=True)
bpy.ops.import_scene.igb(filepath=os.path.abspath(a.igb), game_preset='mua1_ps2', import_normals=True, import_uvs=True,
                         import_vertex_colors=True, import_materials=True, import_collision=False, import_lights=False)
for ob in list(bpy.data.objects):
    if ob.type != 'MESH':
        bpy.data.objects.remove(ob, do_unlink=True)

# ---- the engb: world extents, player start, NPC spawners ----
root = ET.parse(a.engb).getroot()
world = root.find("entity[@name='world']")
ext_lo = [float(v) for v in world.get('extent_min').split()] if world.get('extent_min') else None
ext_hi = [float(v) for v in world.get('extent_max').split()] if world.get('extent_max') else None
insts = {i.get('name'): i for i in root.iter('inst')}
inst_all = {}  # every instance of a name (a type placed several times: pillars, machines)
for i in root.iter('inst'):
    inst_all.setdefault(i.get('name'), []).append(i)
def vec(s): return [float(v) for v in s.split()]
start = insts.get('player_start_all01') or next(i for n, i in insts.items() if n.startswith('player_start'))
O = vec(start.get('pos'))                       # engb units, z-up: becomes the room's origin (floor 0)
start_yaw = math.degrees(vec(start.get('orient') or '0 0 0')[2])


def images_of(o):
    out = []
    for s in o.material_slots:
        if s.material and s.material.use_nodes:
            out += [n.image.name for n in s.material.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image]
    return out


def bbox(o):
    pts = [o.matrix_world @ mathutils.Vector(c) for c in o.bound_box]
    return [min(p[i] for p in pts) for i in range(3)], [max(p[i] for p in pts) for i in range(3)]



# ---- UA EXPANSION fix 4 (the missing entities): the map's props that are ENTITIES (the elevator doors, the lift, the
# waterfall with its statue and pond, the bridge rail) are stored in __combined.igb once, at the engine's origin
# (under the south end of the hub, below its lowest floor), and the engine puts a copy at each `entinst` of the
# engb. Importing only the combined file left them there: the door openings stood open on the city, the lift's
# round hatch was a hole and the waterfall's pond a crater. Each prototype (found by its import name AND its texture,
# so another disc build leaves it alone with a warning) is copied to each of its instances. ----
ENTITY_PARTS_HUB = {  # entity type -> [(import name, texture, triangles)]
    'door': [('__combined_032', 'stk_door_logo', 15), ('__combined_033', 'stk_door_logo', 13)],
    'doors_logo_stk_d_01': [('__combined_032', 'stk_door_logo', 15), ('__combined_033', 'stk_door_logo', 13)],
    'lift': [('__combined_1337', 'u_platform_lift_gold_atl_m', 76), ('__combined_1336', 'console_lift_gold_stk_s', 200),
             ('__combined_040', 'stk_floor_metal_alt_2', 26)],
    'waterfall': [('__combined_021', 'waterfall_stk_water', 48), ('__combined_022', 'waterfall_stk_water', 45),
                  ('__combined_023', 'waterfall_stk_water', 14), ('__combined_043', 'statue_women_stk', 1298)],
    'bridge_railing': [('__combined_1355', 'stk_basic_2', 199), ('__combined_1356', 'stk_basic_2', 26),
                       ('__combined_1357', 'stk_basic_2', 22)],
}
# stark2 (Tony Stark's / Iron Man's Lab): the same, found by looking at each prototype under the lab (textures and sizes).
ENTITY_PARTS_LAB = {
    'doors_stk_d01': [('__combined_590', 'doors_stk_a', 15), ('__combined_591', 'doors_stk_a', 18)],
    'ironman_suit': [('__combined_593', '1501', 1514)],
    'pillar_square_40ft_silver_stk_e': [('__combined_010', 'pillars_01_20ft_silver_stk_e', 359)],
    'column_cables_stk_e02': [('__combined_008', 'cables_stk_s', 163), ('__combined_006', 'u_platform_lift_atl_m', 710)],
    'machine_stk_a': [('__combined_%03d' % k, None, 0) for k in range(571, 584)],
    'holo_projector_stk_a01': [('__combined_011', 'base', 284), ('__combined_009', 'light_cone_02_yellow', 24)],
    'hologram': [('__combined_587', 'iron_man_hologram', 1444)],
    'blackpanther_actionfigure01': [('__combined_012', 'action_figure_blackpanther', 1559)],
}
ENTITY_PARTS = ENTITY_PARTS_HUB if a.place == 'stark1' else ENTITY_PARTS_LAB
PLACED_PROTOS = set()


def place_entities():
    placed = []
    for etype, parts in ENTITY_PARTS.items():
        where = inst_all.get(etype, [])
        if not where:
            continue
        for (name, tex, ntri) in parts:
            src = bpy.data.objects.get(name)
            if src is None or (tex is not None and tex not in images_of(src)):  # (ntri: the exported count, after the duplicate-face pass)
                placed.append(f'{etype}: prototype {name} not as expected, skipped')
                continue
            for k, inst_el in enumerate(where):
                pos = vec(inst_el.get('pos'))
                yaw = vec(inst_el.get('orient') or '0 0 0')[2]
                cp = src.copy()
                cp.data = src.data.copy()
                bpy.context.scene.collection.objects.link(cp)
                cp.name = f'ua_ent_{etype}_{name}_{k}'
                mw = src.matrix_world.copy()
                c, sn = math.cos(yaw), math.sin(yaw)
                for v in cp.data.vertices:
                    w = mw @ v.co
                    v.co = mathutils.Vector((w.x * c - w.y * sn + pos[0], w.x * sn + w.y * c + pos[1], w.z + pos[2]))
                cp.matrix_world = mathutils.Matrix.Identity(4)
            PLACED_PROTOS.add(name)
        placed.append(f'{etype} placed at ' + ', '.join(str(vec(i.get('pos'))) for i in where))
    for name in sorted(PLACED_PROTOS):  # the prototypes themselves (under the hub): gone
        ob = bpy.data.objects.get(name)
        if ob is not None:
            bpy.data.objects.remove(ob, do_unlink=True)
    return placed


entity_notes = place_entities()
if ext_lo is None:  # the lab's world has no extents: its own walls, floors and furniture (not the far city)
    _near = [bbox(o) for o in bpy.data.objects if o.type == 'MESH' and max(bbox(o)[1][0] - bbox(o)[0][0], bbox(o)[1][1] - bbox(o)[0][1]) < 1500 and bbox(o)[1][1] > 200]
    ext_lo = [min(b[0][i] for b in _near) for i in range(3)]
    ext_hi = [max(b[1][i] for b in _near) for i in range(3)]
    entity_notes.append(f'extents from the geometry: {[round(v) for v in ext_lo]} .. {[round(v) for v in ext_hi]}')

# ---- the backdrop: anything far bigger than the hub (sky box, city ground, buildings) ----
span = max(ext_hi[0] - ext_lo[0], ext_hi[1] - ext_lo[1])
cx, cy = (ext_lo[0] + ext_hi[0]) / 2, (ext_lo[1] + ext_hi[1]) / 2
backdrop, moved = [], []
for o in bpy.data.objects:
    lo, hi = bbox(o)
    if max(hi[0] - lo[0], hi[1] - lo[1]) > 1.6 * span:
        backdrop.append(o)
for o in backdrop:
    imgs = ' '.join(images_of(o))
    sky = 'skybox' in imgs
    push = a.skyline_push * (1.15 if sky else 1.0)
    up = a.sky_raise if sky else a.skyline_raise
    me = o.data
    mw = o.matrix_world.copy()
    for v in me.vertices:
        w = mw @ v.co
        w.x = cx + (w.x - cx) * push
        w.y = cy + (w.y - cy) * push
        w.z += up
        v.co = mw.inverted() @ w
    moved.append(f'{o.name} [{imgs}] raised {up:.0f} u, pushed x{push:.2f}')

# ---- the window panes: blended glass (stk_floor_glass) the HERO VR room renderer cannot draw see-through
# (it has cut-out alpha only): left out of scene.glb so the skyline shows through the gold lattice, kept in
# walk.glb so the walk grid still stops at the walls. ----
panes = []
for o in bpy.data.objects:
    if o not in backdrop and images_of(o) == ['stk_floor_glass']:
        lo, hi = bbox(o)
        if min(hi[0] - lo[0], hi[1] - lo[1]) < 12 and hi[2] - lo[2] > 200:  # thin and tall: a wall pane
            panes.append(o)

# ---- the ceiling ----
def image_named(name):
    return bpy.data.images.get(name)


def make_material(name, image=None, color=(1, 1, 1, 1), emit=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; bsdf = nt.nodes['Principled BSDF']
    bsdf.inputs['Base Color'].default_value = color
    if image is not None:
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = image
        nt.links.new(t.outputs['Color'], bsdf.inputs['Base Color'])
        if emit:
            nt.links.new(t.outputs['Color'], bsdf.inputs['Emission Color'])
    elif emit:
        bsdf.inputs['Emission Color'].default_value = color
    if emit:
        bsdf.inputs['Emission Strength'].default_value = emit
    return m


def light_image():
    im = bpy.data.images.new('ua_ceiling_light', 32, 32)
    px = []
    for j in range(32):
        for i in range(32):
            d = max(abs(i - 15.5), abs(j - 15.5)) / 16
            k = 1.0 if d < .7 else (0.55 if d < .85 else 0.25)  # a bright core, a soft rim, a gold frame
            px += [1.0 * k, .86 * k, .55 * k, 1.0]
    im.pixels = px
    im.pack()
    return im


def box(name, lo, hi, mat, uv_tile, unit_uv=False):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = mathutils.Vector([lo[i] + (v.co[i] + .5) * (hi[i] - lo[i]) for i in range(3)])
    uv = bm.loops.layers.uv.new('UVMap')
    for f in bm.faces:
        n = f.normal
        ax = (0, 1) if abs(n.z) > .5 else ((1, 2) if abs(n.x) > .5 else (0, 2))
        for l in f.loops:
            if unit_uv:  # the whole texture once across the face
                l[uv].uv = ((l.vert.co[ax[0]] - lo[ax[0]]) / max(hi[ax[0]] - lo[ax[0]], 1e-6),
                            (l.vert.co[ax[1]] - lo[ax[1]]) / max(hi[ax[1]] - lo[ax[1]], 1e-6))
            else:
                l[uv].uv = (l.vert.co[ax[0]] / uv_tile, l.vert.co[ax[1]] / uv_tile)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
    return ob


ceiling = []
if not a.no_ceiling:
    panel = make_material('ua_ceiling_panel', image_named('stk_floor'))
    gold = make_material('ua_ceiling_beam', image_named('rail') or image_named('stk_floor_metal_gold'))
    lamp = make_material('ua_ceiling_light', light_image(), emit=2.0)
    x0, x1, y0, y1 = ext_lo[0], ext_hi[0], ext_lo[1], ext_hi[1]
    z = a.ceiling
    ceiling.append(box('ua_ceiling', (x0, y0, z + 6), (x1, y1, z + 18), panel, 240))  # its underside 6 u over the walls
    # UA EXPANSION fix 4 (the gap between the north wall and the ceiling): the big north wall (the elevator / waterfall wall,
    # y 2075..2100 u) is only 720 u high - MUA's own camera looks down from far above and never sees over it - while the ceiling
    # grid and the glass sides stand to 960 u: 240 u (6 m) of open sky between the wall's top and the ceiling, the whole width.
    # A bulkhead (the ceiling's own dark panel) closes it, flush with the wall's face, from just under its top to the ceiling.
    wall_top = 0 if a.place != 'stark1' else max((bbox(o)[1][2] for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith('__combined_') and
                    images_of(o) == ['stk_basic_2'] and bbox(o)[0][1] > 2000 and bbox(o)[1][1] < 2105 and bbox(o)[1][2] <= 740), default=0)
    if wall_top > 0:
        ceiling.append(box('ua_wall_bulkhead', (x0, 2075, wall_top - 4), (x1, 2100, z + 6), panel, 240))
        ceiling.append(box('ua_wall_backing', (x0, 2092, wall_top - 60), (x1, 2100, wall_top), panel, 240))  # behind the arched caps' dips
        entity_notes.append(f'north wall bulkhead: {wall_top:.0f} u up to the ceiling ({z + 6:.0f} u)')
    elif a.place == 'stark1':
        entity_notes.append('WARNING north wall not found: its gap to the ceiling stays open')
    step = 240
    gx = x0
    while gx <= x1 + 1:                           # gold beams both ways, hanging under the panels
        ceiling.append(box(f'ua_beam_x{gx:.0f}', (gx - 7, y0, z - 16), (gx + 7, y1, z + 6), gold, 120)); gx += step
    gy = y0
    while gy <= y1 + 1:
        ceiling.append(box(f'ua_beam_y{gy:.0f}', (x0, gy - 7, z - 16), (x1, gy + 7, z + 6), gold, 120)); gy += step
    gx = x0 + step / 2                             # a light panel in every other bay
    i = 0
    while gx < x1:
        gy = y0 + step / 2; j = 0
        while gy < y1:
            if (i + j) % 2 == 0:
                ceiling.append(box(f'ua_light_{i}_{j}', (gx - 48, gy - 48, z - 4), (gx + 48, gy + 48, z + 6), lamp, 96, unit_uv=True))
            gy += step; j += 1
        gx += step; i += 1

# ---- to metres, the player start at the origin (Blender z-up; the glTF exporter makes it y-up) ----
for o in bpy.data.objects:
    me = o.data
    mw = o.matrix_world.copy()
    if me.users > 1:
        o.data = me = me.copy()
    for v in me.vertices:
        w = mw @ v.co
        v.co = mathutils.Vector(((w.x - O[0]) * S + a.offset[0], (w.y - O[1]) * S - a.offset[1], (w.z - O[2]) * S))
    o.matrix_world = mathutils.Matrix.Identity(4)


# ---- UA EXPANSION fix 2 (the black band at the windows' top): the sky box is open-topped (MUA's camera never
# looked up); from the eye, the glass just under the 12 m ceiling looks out over its top wall into nothing - the
# black backdrop. A lid closes it: a low cone from the box's top ring up to a point over the middle, every corner
# taking the top ring's own UVs (the apex their mean), so the painted sky's top colour carries on overhead. ----
import bmesh  # noqa: E402
import math as _m  # noqa: E402
for o in backdrop:
    if 'skybox' not in ' '.join(images_of(o)):
        continue
    bm = bmesh.new()
    bm.from_mesh(o.data)
    uvl = bm.loops.layers.uv.active
    top = max(v.co.z for v in bm.verts)
    ring = {}
    for v in bm.verts:
        if v.co.z > top - 0.25 and v.link_loops:
            key = (round(v.co.x, 2), round(v.co.y, 2))
            ring.setdefault(key, (v.co.copy(), tuple(v.link_loops[0][uvl].uv) if uvl else (0.5, 0.0)))
    pts = list(ring.values())
    if len(pts) >= 3:
        mx = sum(p[0].x for p in pts) / len(pts)
        my = sum(p[0].y for p in pts) / len(pts)
        pts.sort(key=lambda p: _m.atan2(p[0].y - my, p[0].x - mx))
        reach = max(_m.hypot(p[0].x - mx, p[0].y - my) for p in pts)
        apex = bm.verts.new((mx, my, top + reach * 0.35))
        auv = (sum(p[1][0] for p in pts) / len(pts), sum(p[1][1] for p in pts) / len(pts))
        made = 0
        for i in range(len(pts)):
            (pa, ua), (pb, ub) = pts[i], pts[(i + 1) % len(pts)]
            va, vb = bm.verts.new(pa), bm.verts.new(pb)
            try:
                f = bm.faces.new((va, vb, apex))
            except ValueError:
                continue
            if uvl:
                for loop, uv in zip(f.loops, (ua, ub, auv)):
                    loop[uvl].uv = uv
            made += 1
        bm.to_mesh(o.data)
        moved.append(f'{o.name}: sky lid, {made} faces over its open top ({top:.1f} m, apex +{reach * 0.35:.1f} m)')
    bm.free()


# ---- UA EXPANSION fix 2 (the furniture's flicker): exact duplicate faces dropped. The map places some pieces more
# than once in the same spot (a lamp cone twelve times, a desk's detail three times, a combined block 28 times, pillar
# and display copies): the same face drawn twice, with no culling, depth-tested equal - wasted triangles at best, and
# two faces with different UVs or lighting fight in stripes. The first of each (its corners to the millimetre) kept.
import bmesh  # noqa: E402
seen, dropped = set(), 0
for o in sorted((o for o in bpy.data.objects if o.type == 'MESH'), key=lambda o: o.name):
    bm = bmesh.new()
    bm.from_mesh(o.data)
    dup = []
    for f in bm.faces:
        key = tuple(sorted(tuple(round(c, 3) for c in v.co) for v in f.verts))
        if key in seen:
            dup.append(f)
        else:
            seen.add(key)
    if dup:
        bmesh.ops.delete(bm, geom=dup, context='FACES_ONLY')
        bm.to_mesh(o.data)
        dropped += len(dup)
    bm.free()
empty = [o for o in bpy.data.objects if o.type == 'MESH' and not o.data.polygons]
for o in empty:
    for coll in (backdrop, ceiling, panes):
        if o in coll:
            coll.remove(o)
    bpy.data.objects.remove(o)
moved.append(f'duplicate faces dropped {dropped} ({len(empty)} objects left empty, removed)')


def to_room(p):  # engb units (z-up) -> room.txt / glTF (x, y up, z = -blender y), metres
    return [round((p[0] - O[0]) * S + a.offset[0], 3), round((p[2] - O[2]) * S, 3), round(-(p[1] - O[1]) * S + a.offset[1], 3)]


def export(objs, path):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    kw = dict(filepath=path, export_format='GLB', use_selection=True, export_yup=True, export_apply=True,
              export_lights=False, export_cameras=False, export_extras=False)
    try:
        bpy.ops.export_scene.gltf(export_vertex_color='ACTIVE', **kw)
    except TypeError:
        bpy.ops.export_scene.gltf(export_colors=True, **kw)


everything = [o for o in bpy.data.objects if o.type == 'MESH']
skip = set(backdrop) | set(ceiling)
walk = [o for o in everything if o not in skip]
drawn = [o for o in everything if o not in set(panes)]
export(drawn, os.path.join(a.out, 'scene.glb'))
export(walk, os.path.join(a.out, 'walk.glb'))

# ---- room.json (make_hideout.py): Blender z-up metres for bounds; pos_gltf for points ----
quinjet = insts.get('sp_wyattwingfoot01')
qpos = to_room(vec(quinjet.get('pos'))) if quinjet is not None else [0.0, 0.0, 3.0]
OFF = a.offset  # this place's x, z offset in the combined room (metres; glTF x, z)


def shifted(pg):  # (to_room already carries the offset; the spawn is the place's origin)
    return pg


room = {
    'name': 'STARK TOWER' if a.place == 'stark1' else "IRON MAN'S LAB",
    'asset': f"HERO VR UA EXPANSION: Marvel: Ultimate Alliance (PS2) maps/act1/stark/{a.place}, built from the player's own disc",
    'units': 'meters', 'scale_from_source': S, 'origin_source_units': O, 'offset_xz': OFF,
    'bounds': {'playable_min': [(ext_lo[0] - O[0]) * S + OFF[0], (ext_lo[1] - O[1]) * S - OFF[1], 0],
               'playable_max': [(ext_hi[0] - O[0]) * S + OFF[0], (ext_hi[1] - O[1]) * S - OFF[1], 0], 'kill_z': -8.0},
    'points': [{'id': 'spawn', 'type': 'spawn', 'yaw_deg': round(start_yaw, 2), 'pos_gltf': [round(OFF[0], 3), 0.0, round(OFF[1], 3)]}],
}
if a.place == 'stark1':
    room['points'].append({'id': 'return_to_city', 'type': 'interact', 'action': 'return_to_city', 'radius': 2.0,
                           'prompt': "RETURN TO CITY - Wyatt Wingfoot's Quinjet (A or trigger)", 'pos_gltf': shifted(qpos)})
    # UA EXPANSION v5: the places in this hub that lead on in MUA1 (the engb's own link triggers) are TELEPORTS now
    # (hideout074::Action::teleport; a 'to' names the landing it puts him on, in this room or in the lab's).
    lift = insts.get('lift')
    wf = insts.get('link_stark2')
    lp = to_room(vec(lift.get('pos'))) if lift is not None else None
    wp = to_room(vec(wf.get('pos'))) if wf is not None else None
    if lp:
        # MUA1's lift is a mover the map's opening cutscene (scripts/act1/stark/move_lift.py) raises 240 units (6.1 m)
        # with the heroes on it: arrival only, no map of its own. The sensible place it takes him in the same tower is
        # the raised central platform (the player start: Nick Fury's desk, the conference table); the way back is a
        # lift marker there, which puts him on the pad again.
        room['points'] += [
            {'id': 'arrival_lift', 'type': 'interact', 'action': 'teleport', 'to': 'upper_landing', 'radius': 2.6,
             'prompt': "ARRIVAL LIFT - up to the central platform (A or trigger)", 'pos_gltf': [lp[0], 0.0, lp[2]]},
            {'id': 'lift_down', 'type': 'interact', 'action': 'teleport', 'to': 'pad_landing', 'radius': 1.3,
             'prompt': "LIFT - down to the arrival pad (A or trigger)", 'pos_gltf': [0.0, 0.0, -3.2]},
            {'id': 'upper_landing', 'type': 'landing', 'yaw_deg': -90.0, 'pos_gltf': [0.0, 0.0, 1.2]},
            {'id': 'pad_landing', 'type': 'landing', 'yaw_deg': 90.0, 'pos_gltf': [lp[0], -3.048, lp[2] - 1.0]},
        ]
    if wp:
        room['points'] += [
            {'id': 'ironman_lab', 'type': 'interact', 'action': 'teleport', 'to': 'lab_landing', 'radius': 3.0,
             'prompt': "IRON MAN'S LAB - through the waterfall (A or trigger)", 'pos_gltf': [wp[0], 0.0, wp[2]]},
            {'id': 'waterfall_landing', 'type': 'landing', 'yaw_deg': -90.0, 'pos_gltf': [wp[0], -3.048, wp[2] + 3.4]},
        ]
else:
    door = insts.get('doors_stk_d01')
    dp = to_room(vec(door.get('pos'))) if door is not None else [0.0, 0.0, 4.7]
    room['points'] += [
        {'id': 'lab_door', 'type': 'interact', 'action': 'teleport', 'to': 'waterfall_landing', 'radius': 2.2,
         'prompt': "STARK TOWER - the door back to the waterfall (A or trigger)", 'pos_gltf': [dp[0], 0.0, dp[2] - 0.6]},
        {'id': 'lab_landing', 'type': 'landing', 'yaw_deg': round(start_yaw, 2), 'pos_gltf': [OFF[0], 0.0, OFF[1]]},
    ]
# The doors and lifts BETWEEN the places are added by combine_places.py (it knows both ends).
json.dump(room, open(os.path.join(a.out, 'room.json'), 'w'), indent=1)

# ---- npcs.txt: the characters update's hooks (every spawner the engb places) ----
lines = ['HEROVR-UA-NPCS 1', '# id x y z yaw_deg  (room space: glTF metres, y up; yaw as room.txt spawn)']
for n, i in sorted(insts.items()):
    if n.startswith('sp_'):
        p = to_room(vec(i.get('pos')))
        yaw = math.degrees(vec(i.get('orient') or '0 0 0')[2])
        lines.append(f'npc {n[3:]} {p[0]:.3f} {p[1]:.3f} {p[2]:.3f} {yaw:.1f}')
open(os.path.join(a.out, 'npcs.txt'), 'w', newline='\n').write('\n'.join(lines) + '\n')

tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in everything)
rep = [f'scale {S} m/unit, origin {O} (player_start_all01), start yaw {start_yaw:.1f}',
       f'objects {len(everything)} (walk {len(walk)}, backdrop {len(backdrop)}, ceiling {len(ceiling)}, window panes left out {len(panes)}), triangles {tris}',
       *entity_notes, *moved, f'npcs {len(lines) - 2}']
open(os.path.join(a.out, 'report.txt'), 'w').write('\n'.join(rep) + '\n')
print('\n'.join(rep))
