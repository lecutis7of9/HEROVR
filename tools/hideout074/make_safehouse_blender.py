"""0.0.74 HIDEOUTS: the van's PLACEHOLDER safehouse (a plain concrete room, made here, no outside asset).

    blender -b -P make_safehouse_blender.py -- <out folder>

Writes safehouse-room.glb (the room), safehouse-collision.glb (COL_floor, COL_walls) and safehouse-room.json (the
batcave-room.json schema); make_hideout.py then makes the shipped folder. Blender Z-up metres, the floor at z 0.
Made by Agent Mulder. A placeholder until a real safehouse model is picked.
"""
import json
import math
import os
import sys

import bpy

out = sys.argv[sys.argv.index('--') + 1]
os.makedirs(out, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
mats = {}


def mat(name, rgb):
    if name not in mats:
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (*rgb, 1)
        m.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = .9
        mats[name] = m
    return mats[name]


def box(name, centre, size, material, yaw=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=centre)
    o = bpy.context.object
    o.name = name
    o.scale = size
    o.rotation_euler[2] = math.radians(yaw)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    o.data.materials.append(mat(*material))
    return o


CONCRETE = ('concrete', (.32, .31, .30))
WALL = ('wall', (.36, .30, .26))
DARK = ('dark_metal', (.10, .10, .11))
STEEL = ('steel', (.45, .46, .48))
WOOD = ('wood', (.33, .22, .13))
CORK = ('cork', (.55, .40, .24))
PAPER = ('paper', (.85, .83, .76))
RED = ('red_string', (.70, .05, .05))
SKULL = ('skull_white', (.90, .90, .88))
CLOTH = ('cot_cloth', (.20, .24, .17))
EXIT = ('exit_light', (.10, .90, .30))
LAMP = ('lamp', (1.0, .85, .55))

W, D, H, T = 10.0, 7.0, 3.0, .2  # inside width (x), depth (y), height, wall thickness
props = []


def prop(name, centre, size, material, yaw=0.0):
    props.append({'name': name, 'center': list(centre), 'size': list(size), 'yaw_deg': yaw})
    return box(name, centre, size, material, yaw)


floor = box('floor', (0, 0, -.05), (W, D, .1), CONCRETE)
box('ceiling', (0, 0, H + .05), (W, D, .1), ('ceiling', (.22, .22, .22)))
walls = [box('wall_n', (0, D / 2 + T / 2, H / 2), (W + 2 * T, T, H), WALL),
         box('wall_s', (0, -D / 2 - T / 2, H / 2), (W + 2 * T, T, H), WALL),
         box('wall_e', (W / 2 + T / 2, 0, H / 2), (T, D, H), WALL),
         box('wall_w', (-W / 2 - T / 2, 0, H / 2), (T, D, H), WALL)]
# the door (south wall, west end) back to the van, its EXIT light
box('door', (-3.5, -D / 2 + .03, 1.05), (1.0, .06, 2.1), DARK)
box('door_exit_sign', (-3.5, -D / 2 + .05, 2.35), (.5, .04, .18), EXIT)
# the case board (north wall): cork, papers, red string, the skull
box('case_board', (0, D / 2 - .04, 1.6), (3.0, .05, 1.4), CORK)
for k, (x, z) in enumerate([(-1.1, 2.0), (-.4, 1.8), (.5, 2.05), (1.1, 1.5), (-.9, 1.3), (.2, 1.25), (.9, 1.1)]):
    box(f'case_paper_{k}', (x, D / 2 - .07, z), (.32, .01, .24), PAPER)
for k, (x, z, length, angle) in enumerate([(-.75, 1.9, .8, -15), (.05, 1.92, .9, 12), (.8, 1.6, .7, 40), (-.35, 1.28, 1.1, 2)]):
    box(f'case_string_{k}', (x, D / 2 - .08, z), (length, .005, .012), RED, 0)
    bpy.context.object.rotation_euler[1] = math.radians(angle)
box('skull_mark', (0, D / 2 - .03, 2.55), (.6, .02, .5), SKULL)
prop('case_table', (0, D / 2 - 1.1, .38), (2.0, .9, .76), WOOD)
# the weapons wall (east)
box('weapons_wall', (W / 2 - .05, .5, 1.5), (.08, 3.6, 1.8), DARK)
for k in range(5):
    box(f'rifle_{k}', (W / 2 - .12, -.9 + k * .7, 1.5), (.05, .12, 1.0), STEEL)
prop('ammo_crates', (W / 2 - .5, -2.5, .3), (.8, 1.4, .6), ('crate', (.22, .26, .16)))
# the workbench (west), the cot (north-east), a lamp
prop('workbench', (-W / 2 + .45, 1.2, .45), (.8, 2.4, .9), WOOD)
box('vice', (-W / 2 + .45, .4, .97), (.2, .3, .15), STEEL)
prop('cot', (3.3, D / 2 - .55, .25), (2.0, .8, .5), CLOTH)
box('ceiling_lamp', (0, 0, H - .15), (.5, .5, .1), LAMP)
box('ceiling_lamp_2', (3.0, 1.0, H - .15), (.4, .4, .1), LAMP)

points = [
    {'id': 'spawn', 'type': 'spawn', 'yaw_deg': 90.0, 'pos': [-3.5, -2.4, 0.0], 'note': "Arrival from the van's dash 'Go to safehouse', inside the door."},
    {'id': 'exit_to_van', 'type': 'interact', 'radius': 1.4, 'prompt': 'Get in van', 'action': 'return_to_city_in_vehicle', 'pos': [-3.5, -3.1, 0.0]},
    {'id': 'case_board', 'type': 'interact', 'radius': 1.6, 'prompt': 'Case board (coming soon)', 'action': 'open_case_board', 'pos': [0.0, 2.2, 0.0]},
    {'id': 'weapons_wall', 'type': 'interact', 'radius': 1.6, 'prompt': 'Weapons locker (coming soon)', 'action': 'weapons_reserved', 'pos': [4.0, .5, 0.0]},
]
for p in points:
    x, y, z = p['pos']
    p['pos_gltf'] = [x, z, -y]

for o in bpy.data.objects:
    o.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(out, 'safehouse-room.glb'), export_format='GLB', use_selection=True,
                          export_apply=True, export_lights=False, export_cameras=False)
# the collision: the floor, the walls (the props are prop_blockers)
bpy.ops.object.select_all(action='DESELECT')
floor.name = 'COL_floor'
for w in walls:
    w.name = 'COL_' + w.name
floor.select_set(True)
for w in walls:
    w.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(out, 'safehouse-collision.glb'), export_format='GLB', use_selection=True,
                          export_apply=True, export_lights=False, export_cameras=False, export_materials='NONE')
json.dump({'asset': 'HERO VR 0.0.74 safehouse PLACEHOLDER (made by Agent Mulder in Blender, tools/hideout074)', 'name': 'SAFEHOUSE',
           'units': 'meters', 'floor_height_m': 0.0, 'render_hint': 'flat colours, the mod shades by face',
           'points': points,
           'bounds': {'playable_min': [-W / 2, -D / 2, 0], 'playable_max': [W / 2, D / 2, H], 'kill_z': -3.0,
                      'room_enclosure_min': [-W / 2 - T, -D / 2 - T, -.1], 'room_enclosure_max': [W / 2 + T, D / 2 + T, H + .1]},
           'collision': {'file': 'safehouse-collision.glb', 'prop_blockers': props}},
          open(os.path.join(out, 'safehouse-room.json'), 'w'), indent=1)
print('safehouse written to', out)
