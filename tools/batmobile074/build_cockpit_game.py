# HERO VR Batmobile cockpit — original geometry, Made by Agent Mulder. Frame: front -Y, up +Z, driver (LHD) at +X, origin = cockpit floor centre, metres.
import bpy, bmesh, math, os, sys
from mathutils import Matrix, Vector as V, Euler
D='/workspace/hero-0.0.72/batmobile/cockpit'; T=D+'/textures/'; OUT='/workspace/hero-0.0.74/batconv/out'
bpy.ops.wm.read_factory_settings(use_empty=True)
root=bpy.data.objects.new('HERO_BatCockpit',None); bpy.context.scene.collection.objects.link(root)
# ---------- materials (all image textures)
def img(n,noncolor=False):
    im=bpy.data.images.load(T+n,check_existing=True)
    if noncolor: im.colorspace_settings.name='Non-Color'
    return im
MATS={}
def mat(name,pre,emis=0.0,orm=True,normal=True,rough=0.5):
    m=bpy.data.materials.new(name); m.use_nodes=True; nt=m.node_tree; b=nt.nodes['Principled BSDF']
    t=nt.nodes.new('ShaderNodeTexImage'); t.image=img(pre+('.png' if pre.startswith('../screens/') else '_emissive.png' if emis else '_basecolor.png'))
    nt.links.new(t.outputs[0],b.inputs['Base Color'])
    if emis:
        nt.links.new(t.outputs[0],b.inputs['Emission Color']); b.inputs['Emission Strength'].default_value=emis; b.inputs['Roughness'].default_value=0.15
    else: b.inputs['Roughness'].default_value=rough
    if orm:
        o=nt.nodes.new('ShaderNodeTexImage'); o.image=img(pre+'_orm.png',True); s=nt.nodes.new('ShaderNodeSeparateColor')
        nt.links.new(o.outputs[0],s.inputs[0]); nt.links.new(s.outputs[1],b.inputs['Roughness']); nt.links.new(s.outputs[2],b.inputs['Metallic'])
    if normal:
        n=nt.nodes.new('ShaderNodeTexImage'); n.image=img(pre+'_normal.png',True); nm=nt.nodes.new('ShaderNodeNormalMap')
        nt.links.new(n.outputs[0],nm.inputs['Color']); nt.links.new(nm.outputs[0],b.inputs['Normal'])
    MATS[name]=m; return m
mat('BC_Leather','leather'); mat('BC_Metal','metal'); mat('BC_Carbon','carbon'); mat('BC_Rubber','rubber')
for _n,_pg in (('BC_ScreenCentre','bat_map'),('BC_ScreenPassenger','bat_cases'),('BC_ScreenConsole','bat_vehicle_status')):
    mat(_n,'../screens/'+_pg,emis=1.6,orm=False,normal=False)   # Batcomputer pages (screens/*.png, 1024x600)
mat('BC_Gauges','gauges',emis=1.6,orm=False,normal=False)
mat('BC_RedCap','redcap',orm=False,normal=False,rough=0.35); mat('BC_Paint','paint',orm=False,normal=False,rough=0.45)
# ---------- mesh helpers
def finish(bm,name,m,M,uvmode='box',k=2.0,smooth=True,uvfn=None):
    uv=bm.loops.layers.uv.verify()
    if uvfn:   # local-space explicit UVs
        for f in bm.faces:
            for l in f.loops: l[uv].uv=uvfn(l.vert.co)
    bmesh.ops.transform(bm,matrix=M,verts=bm.verts)
    bm.normal_update()
    if uvmode=='box' and not uvfn:
        for f in bm.faces:
            a=max(range(3),key=lambda i:abs(f.normal[i])); i,j=[x for x in range(3) if x!=a]
            for l in f.loops: l[uv].uv=(l.vert.co[i]*k,l.vert.co[j]*k)
    me=bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    me.materials.append(MATS[m])
    for p in me.polygons: p.use_smooth=smooth
    if smooth: me.set_sharp_from_angle(angle=math.radians(35))
    o=bpy.data.objects.new(name,me); bpy.context.scene.collection.objects.link(o); o.parent=root; return o
def Mx(loc=(0,0,0),rot=(0,0,0),parent=None):
    M=Matrix.Translation(V(loc))@Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()
    return parent@M if parent else M
def box(name,size,m,M,bev=0.0,seg=3,k=2.0):
    bm=bmesh.new(); bmesh.ops.create_cube(bm,size=1.0)
    for v in bm.verts: v.co=V((v.co.x*size[0],v.co.y*size[1],v.co.z*size[2]))
    if bev>0: bmesh.ops.bevel(bm,geom=list(bm.edges),offset=bev,segments=seg,profile=0.5,affect='EDGES')
    return finish(bm,name,m,M,k=k)
def cyl(name,r,depth,m,M,seg=16,r2=None,k=4.0):   # axis local Z
    bm=bmesh.new(); bmesh.ops.create_cone(bm,cap_ends=True,cap_tris=False,segments=seg,radius1=r,radius2=r if r2 is None else r2,depth=depth)
    return finish(bm,name,m,M,k=k)
def between(name,p1,p2,r,m,seg=12):
    p1,p2=V(p1),V(p2); d=p2-p1; q=d.to_track_quat('Z','Y')
    return cyl(name,r,d.length,m,Matrix.Translation((p1+p2)/2)@q.to_matrix().to_4x4(),seg=seg)
def sphere(name,r,m,M,seg=16):
    bm=bmesh.new(); bmesh.ops.create_uvsphere(bm,u_segments=seg,v_segments=seg//2,radius=r); return finish(bm,name,m,M,k=8)
def torus(name,R,r,m,M,nu=64,nv=16,k=2.0):
    bm=bmesh.new(); vs=[]
    for i in range(nu):
        u=2*math.pi*i/nu; row=[]
        for j in range(nv):
            w=2*math.pi*j/nv; row.append(bm.verts.new(((R+r*math.cos(w))*math.cos(u),(R+r*math.cos(w))*math.sin(u),r*math.sin(w))))
        vs.append(row)
    uv=bm.loops.layers.uv.verify()
    for i in range(nu):
        for j in range(nv):
            f=bm.faces.new((vs[i][j],vs[(i+1)%nu][j],vs[(i+1)%nu][(j+1)%nv],vs[i][(j+1)%nv]))
            for l,(a,b) in zip(f.loops,[(i,j),(i+1,j),(i+1,j+1),(i,j+1)]): l[uv].uv=(a/nu*2*math.pi*R*k*4,b/nv*2*math.pi*r*k*4)
    bmesh.ops.transform(bm,matrix=M,verts=bm.verts); bm.normal_update()
    me=bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); me.materials.append(MATS[m])
    for p in me.polygons: p.use_smooth=True
    o=bpy.data.objects.new(name,me); bpy.context.scene.collection.objects.link(o); o.parent=root; return o
FACE=Euler((math.radians(-90),0,0)).to_matrix().to_4x4()   # local XY plane -> faces +Y (towards driver), texture upright for a viewer facing -Y
def disc(name,r,cell,M,seg=48):
    cu,cv=(cell%2)*0.5+0.25,1-((cell//2)*0.5+0.25); s=0.25*240/256
    bm=bmesh.new(); bmesh.ops.create_circle(bm,cap_ends=True,segments=seg,radius=r)
    return finish(bm,name,'BC_Gauges',M@FACE,uvfn=lambda c:(cu-s*c.x/r,cv-s*c.y/r))
def screen(name,w,h,M,m='BC_ScreenCentre'):
    bm=bmesh.new(); bmesh.ops.create_grid(bm,x_segments=1,y_segments=1,size=0.5)
    for v in bm.verts: v.co=V((v.co.x*w,v.co.y*h,0))
    return finish(bm,name,m,M@FACE,uvfn=lambda c:(0.5-c.x/w,0.5-c.y/h),smooth=False)
def toggle(name,P,lx,lz):   # P = panel frame whose +Y is the panel face normal
    cyl(name+'_base',0.007,0.008,'BC_Metal',P@Mx((lx,0.004,lz),(-90,0,0)),seg=10)
    between(name+'_lever',(P@V((lx,0.006,lz))),(P@V((lx,0.026,lz+0.016))),0.0035,'BC_Metal',seg=8)
    sphere(name+'_cap',0.0055,'BC_RedCap',Matrix.Translation(P@V((lx,0.028,lz+0.018))),seg=10)
# ---------- shell: floor, firewall, rear bulkhead, doors
box('Floor',(1.40,1.95,0.03),'BC_Rubber',Mx((0,-0.35,-0.015)),k=1.0)
box('Firewall',(1.40,0.06,0.62),'BC_Carbon',Mx((0,-1.30,0.31)),bev=0.01,seg=1)
box('FootwellRamp',(1.36,0.30,0.02),'BC_Rubber',Mx((0,-1.17,0.09),(-30,0,0)),k=1.0)
box('RearBulkhead',(1.40,0.05,0.95),'BC_Carbon',Mx((0,0.68,0.475)),bev=0.01,seg=1)
for sd,sx in (('L',1),('R',-1)):
    box('DoorPanel_'+sd,(0.05,1.55,0.72),'BC_Leather',Mx((sx*0.70,-0.25,0.40)),bev=0.015,seg=2)
    box('DoorTrim_'+sd,(0.09,1.55,0.04),'BC_Carbon',Mx((sx*0.69,-0.25,0.78)),bev=0.01,seg=2,k=4)
    box('Armrest_'+sd,(0.09,0.55,0.07),'BC_Leather',Mx((sx*0.635,0.05,0.48)),bev=0.025,seg=4)
    box('DoorPull_'+sd,(0.02,0.14,0.03),'BC_Metal',Mx((sx*0.665,-0.22,0.60)),bev=0.006,seg=2,k=8)
# ---------- seats
for sd,sx in (('L',0.37),('R',-0.37)):
    box('SeatBase_'+sd,(0.36,0.40,0.14),'BC_Metal',Mx((sx,0.08,0.07)),bev=0.01,seg=1)
    box('SeatCushion_'+sd,(0.44,0.50,0.12),'BC_Leather',Mx((sx,0.06,0.22),(-6,0,0)),bev=0.045,seg=8)
    box('SeatBack_'+sd,(0.44,0.11,0.72),'BC_Leather',Mx((sx,0.36,0.64),(-14,0,0)),bev=0.045,seg=8)
    for b,bx in (('o',1),('i',-1)):
        box('SeatBolsterCush_%s%s'%(sd,b),(0.07,0.48,0.10),'BC_Leather',Mx((sx+bx*0.215,0.06,0.30),(-6,0,0)),bev=0.03,seg=5)
        box('SeatBolsterBack_%s%s'%(sd,b),(0.08,0.16,0.62),'BC_Leather',Mx((sx+bx*0.22,0.33,0.62),(-14,0,0)),bev=0.035,seg=5)
    box('Headrest_'+sd,(0.26,0.10,0.18),'BC_Leather',Mx((sx,0.474,1.096),(-14,0,0)),bev=0.04,seg=6)
# ---------- dashboard
box('Dash',(1.36,0.44,0.32),'BC_Paint',Mx((0,-0.84,0.66)),bev=0.05,seg=5)
box('DashTop',(1.30,0.40,0.02),'BC_Carbon',Mx((0,-0.86,0.82)),bev=0.008,seg=1,k=4)
B=Mx((0.36,-0.66,0.72),(15,0,0))
box('Binnacle',(0.54,0.10,0.26),'BC_Paint',B,bev=0.02,seg=3)
box('BinnacleHood',(0.58,0.18,0.03),'BC_Leather',Mx((0.36,-0.63,0.865),(8,0,0)),bev=0.012,seg=3)
for i,(nm,lx,lz,r) in enumerate([('Speedo',0.09,0.02,0.06),('Tacho',-0.09,0.02,0.06),('Turbine',0.205,-0.03,0.04),('Fuel',-0.205,-0.03,0.04)]):
    disc('Gauge_'+nm,r,i,B@Mx((lx,0.051,lz)))
    torus('GaugeBezel_'+nm,r+0.004,0.005,'BC_Metal',B@Mx((lx,0.052,lz),(-90,0,0)),nu=40,nv=8)
# 0.0.74 test 1: the centre and passenger screens have no tilt (flush, square to the driver)
S=Mx((0,-0.60,0.69),(0,0,0)); box('ScreenBezel_C',(0.32,0.02,0.20),'BC_Carbon',S,bev=0.006,seg=2,k=4); screen('Screen_Centre',0.29,0.17,S@Mx((0,0.0105,0)))
S2=Mx((-0.36,-0.60,0.70),(0,0,0)); box('ScreenBezel_P',(0.38,0.02,0.235),'BC_Carbon',S2,bev=0.006,seg=2,k=4); screen('Screen_Passenger',0.35,0.205,S2@Mx((0,0.0105,0)),'BC_ScreenPassenger')
P=Mx((0,-0.60,0.55),(15,0,0)); box('TogglePlate_Dash',(0.32,0.02,0.07),'BC_Metal',P,bev=0.004,seg=2,k=6)
for i in range(8): toggle('ToggleD%d'%i,P,-0.13+i*0.26/7,-0.008)
# ---------- steering column + round wheel (wheel plane normal = (0,0.94,0.34))
C=Mx((0.36,-0.33,0.74),(-70,0,0)); ax=(C.to_3x3()@V((0,0,1))).normalized(); c=C.translation
torus('SteeringRim',0.185,0.017,'BC_Leather',C,nu=80,nv=20)
cyl('SteeringHub',0.05,0.045,'BC_Metal',C,seg=24)
cyl('SteeringHubCap',0.032,0.05,'BC_Carbon',C@Mx((0,0,0.004)),seg=24)
for nm,d in (('L',V((1,0,0))),('R',V((-1,0,0))),('B',V((0,1,0)))):
    w=C.to_3x3()@d; between('SteeringSpoke_'+nm,c+w*0.045,c+w*0.172,0.011,'BC_Metal',seg=10)
between('SteeringColumn',c-ax*0.02,c-ax*0.38,0.032,'BC_Paint',seg=16)
# ---------- centre console + gear lever
box('ConsoleStack',(0.26,0.32,0.56),'BC_Paint',Mx((0,-0.78,0.28)),bev=0.02,seg=3)
box('Console',(0.24,0.92,0.40),'BC_Carbon',Mx((0,-0.16,0.20)),bev=0.025,seg=3,k=4)
box('ConsoleTop',(0.22,0.88,0.02),'BC_Leather',Mx((0,-0.16,0.405)),bev=0.008,seg=2)
CS=Mx((0,-0.45,0.43),(60,0,0)); box('ScreenBezel_Console',(0.18,0.015,0.11),'BC_Carbon',CS,bev=0.005,seg=2,k=4); screen('Screen_Console',0.16,0.094,CS@Mx((0,0.008,0)),'BC_ScreenConsole')
CP=Mx((0,-0.24,0.42),(90,0,0)); box('TogglePlate_Console',(0.18,0.012,0.08),'BC_Metal',CP,bev=0.003,seg=2,k=6)
for i in range(4): toggle('ToggleC%d'%i,CP,-0.06+i*0.04,0.0)
cyl('GearGaiter',0.045,0.06,'BC_Leather',Mx((0,0.02,0.44)),seg=16,r2=0.012)
between('GearLever',(0,0.02,0.41),(0,0.05,0.60),0.008,'BC_Metal',seg=10)
sphere('GearKnob',0.028,'BC_Metal',Mx((0,0.052,0.615)),seg=20)
# ---------- pedals (driver side)
for nm,px,w in (('Brake',0.30,0.08),('Throttle',0.45,0.05)):
    between('PedalArm_'+nm,(px,-1.24,0.50),(px,-1.12,0.16),0.01,'BC_Metal',seg=8)
    box('PedalPad_'+nm,(w,0.015,0.10),'BC_Metal',Mx((px,-1.11,0.14),(30,0,0)),bev=0.004,seg=2,k=8)
box('FootRest',(0.07,0.015,0.16),'BC_Metal',Mx((0.58,-1.10,0.13),(30,0,0)),bev=0.004,seg=2,k=8)
# ---------- report, save, export
tri=0; per={}
for o in root.children:
    o.data.calc_loop_triangles(); n=len(o.data.loop_triangles); tri+=n; m=o.data.materials[0].name; per[m]=per.get(m,0)+n
print('TRIS',tri,per,'OBJECTS',len(root.children))
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/cockpit_game.blend')
print('SAVED')
