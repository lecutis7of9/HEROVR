import bpy, sys, json, math, mathutils
V=mathutils.Vector
argv=sys.argv[sys.argv.index('--')+1:]
src,spec=argv[0],json.loads(argv[1])
bpy.ops.wm.read_factory_settings(use_empty=True)
if src.endswith('.gltf'): bpy.ops.import_scene.gltf(filepath=src)
else: bpy.ops.wm.obj_import(filepath=src)
for o in list(bpy.context.scene.objects):
    if any(h==o.name or (o.type=='MESH' and h in [m.name for m in o.data.materials]) for h in spec.get('hide',[])):
        bpy.data.objects.remove(o)
xf=spec.get('xform',{'s':1})
root=bpy.data.objects.new('root',None); bpy.context.scene.collection.objects.link(root)
for o in bpy.context.scene.objects:
    if o.parent is None and o is not root: o.parent=root
root.scale=(xf['s'],)*3; root.rotation_euler=[math.radians(a) for a in xf.get('r',(0,0,0))]
bpy.context.view_layer.update()
mn=V((1e9,)*3); mx=-mn; tris=0; mats={}
dg=bpy.context.evaluated_depsgraph_get()
for o in bpy.context.scene.objects:
    if o.type=='MESH':
        for c in o.bound_box:
            w=o.matrix_world@V(c); mn=V(map(min,mn,w)); mx=V(map(max,mx,w))
        me=o.data; me.calc_loop_triangles()
        for t in me.loop_triangles:
            tris+=1; mn_=me.materials[t.material_index].name if me.materials else '-'; mats[mn_]=mats.get(mn_,0)+1
print('SIZE',[round(x,3) for x in (mx-mn)],'TRIS',tris)
for m,n in sorted(mats.items(),key=lambda x:-x[1])[:40]:
    mm=bpy.data.materials.get(m); a=None
    if mm and mm.use_nodes:
        b=[n_ for n_ in mm.node_tree.nodes if n_.type=='BSDF_PRINCIPLED']
        if b: a=(tuple(round(x,2) for x in b[0].inputs['Base Color'].default_value), round(b[0].inputs['Alpha'].default_value,2), mm.blend_method)
    print('MAT',m,n,a)
