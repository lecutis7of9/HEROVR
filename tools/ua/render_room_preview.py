"""Preview renders of a built UA room (scene.glb, metres, glTF y-up) - no game needed.
  blender -b --factory-startup --python render_room_preview.py -- <scene.glb> <out_prefix> <shot> [<shot> ...]
A shot: name:x,y,z:yaw_deg,pitch_deg[:lens_mm]  in room space (glTF: x right, y up, z = -north); yaw like room.txt
(degrees from +x counter-clockwise seen from above), pitch up positive.
A floor plan: plan:x0,z0,x1,z1:cut_y[:px_per_metre]  an orthographic view straight down over that room-space
rectangle (the top of the image is z0, as make_hideout.py's walk.png), nothing above cut_y drawn."""
import sys, math, bpy
argv = sys.argv[sys.argv.index('--') + 1:]
glb, prefix, shots = argv[0], argv[1], argv[2:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.samples = 32; sc.cycles.device = 'CPU'; sc.cycles.use_denoising = False
sc.view_settings.view_transform = 'Standard'
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True
bg = w.node_tree.nodes['Background']; bg.inputs[0].default_value = (1, 1, 1, 1); bg.inputs[1].default_value = 1.0
for shot in shots:
    parts = shot.split(':')
    if parts[0].startswith('plan'):
        x0, z0, x1, z1 = [float(v) for v in parts[1].split(',')]
        cut = float(parts[2])
        ppm = float(parts[3]) if len(parts) > 3 else 16
        cam = bpy.data.cameras.new(parts[0]); cam.type = 'ORTHO'
        cam.clip_start = .01; cam.clip_end = cut + 200
        w_m, h_m = x1 - x0, z1 - z0
        cam.ortho_scale = max(w_m, h_m)
        ob = bpy.data.objects.new(parts[0], cam); sc.collection.objects.link(ob)
        ob.location = ((x0 + x1) / 2, -(z0 + z1) / 2, cut)   # looking down -Z (Blender), image up = Blender +y = room -z
        ob.rotation_euler = (0, 0, 0)
        sc.render.resolution_x, sc.render.resolution_y = int(round(w_m * ppm)), int(round(h_m * ppm))
        sc.camera = ob
        sc.render.filepath = f'{prefix}{parts[0]}.png'
        bpy.ops.render.render(write_still=True)
        sc.render.resolution_x, sc.render.resolution_y = 1280, 720
        print('WROTE', sc.render.filepath)
        continue
    name, pos, ang = parts[0], [float(v) for v in parts[1].split(',')], [float(v) for v in parts[2].split(',')]
    lens = float(parts[3]) if len(parts) > 3 else 16
    cam = bpy.data.cameras.new(name); cam.lens = lens; cam.clip_start = .05; cam.clip_end = 2000
    ob = bpy.data.objects.new(name, cam); sc.collection.objects.link(ob)
    x, y, z = pos                                   # glTF -> Blender: (x, -z, y)
    ob.location = (x, -z, y)
    yaw, pitch = math.radians(ang[0]), math.radians(ang[1])
    ob.rotation_euler = (math.pi / 2 + pitch, 0, yaw - math.pi / 2)
    sc.camera = ob
    sc.render.filepath = f'{prefix}{name}.png'
    bpy.ops.render.render(write_still=True)
    print('WROTE', sc.render.filepath)
