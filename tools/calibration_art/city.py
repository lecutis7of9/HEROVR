import numpy as np
from PIL import Image
def hash2(a,b,s=0):
    a=np.asarray(a,np.int64); b=np.asarray(b,np.int64)
    h=(a*73856093) ^ (b*19349663) ^ (s*83492791)
    h=((h ^ (h>>13)) & 0xffffffff)*12741261
    h=h ^ (h>>16)
    return (h & 0xffffff)/float(0xffffff)
AV=110.0; AVW=13.0   # avenue spacing (x) and half width
ST=60.0; STW=8.0     # street spacing (z) and half width
SIDE=4.0             # sidewalk width
LOT=18.0
def height(x,z):
    # main avenue at x=0; others every AV
    ax=np.abs(((x+AV/2)%AV)-AV/2); az=np.abs(((z+ST/2)%ST)-ST/2)
    road=(ax<AVW)|(az<STW)
    lx=np.floor(x/LOT); lz=np.floor(z/LOT)
    r=hash2(lx,lz,1)
    dist=np.hypot((lx+.5)*LOT,(lz+.5)*LOT*.35)
    base=14+110*r**2.2
    base+= 60*np.exp(-dist/600)*hash2(lx,lz,7)
    tall=hash2(lx,lz,3)>.93
    base=np.where(tall,base+90,base)
    # setbacks inside lot
    fx=(x/LOT)%1; fz=(z/LOT)%1
    inner=(np.abs(fx-.5)<.30)&(np.abs(fz-.5)<.30)
    h=np.where(inner&(hash2(lx,lz,5)>.5),base*1.25,base)
    gap=(np.minimum(fx,1-fx)<.03)|(np.minimum(fz,1-fz)<.03)
    h=np.where(gap&(hash2(lx,lz,9)>.4),base*.6,h)
    return np.where(road,0.0,h),road,ax,az,lx,lz
def render(W=1024,H=512,ss=2):
    W2,H2=W*ss,H*ss
    cam=np.array([6.0,230.0,-60.0])
    pitch=np.radians(-38); fov=np.radians(58)
    f=(W2/2)/np.tan(fov/2)
    u,v=np.meshgrid(np.arange(W2)+.5-W2/2,-(np.arange(H2)+.5-H2/2))
    d=np.stack([u,v,np.full(u.shape,f)],-1)
    cp,sp=np.cos(pitch),np.sin(pitch)
    d=np.stack([d[...,0],d[...,1]*cp+d[...,2]*sp,-d[...,1]*sp+d[...,2]*cp],-1)
    d/=np.linalg.norm(d,axis=-1,keepdims=True)
    D=d.reshape(-1,3); N=D.shape[0]
    t=np.full(N,1.0); hit=np.zeros(N,bool); tmax=1800.0
    prev=np.zeros(N)
    for i in range(700):
        live=~hit&(t<tmax)
        if not live.any(): break
        idx=np.nonzero(live)[0]
        p=cam+D[idx]*t[idx,None]
        h,_,_,_,_,_=height(p[:,0],p[:,2])
        inside=p[:,1]<=h
        hit[idx[inside]]=True
        step=np.maximum(.6,(p[:,1]-h)*.35)
        step=np.minimum(step,np.maximum(.6,t[idx]*.012))
        prev[idx[~inside]]=t[idx[~inside]]
        t[idx[~inside]]+=step[~inside]
    # refine
    lo=prev.copy(); hi=t.copy()
    for i in range(12):
        m=(lo+hi)/2; p=cam+D*m[:,None]
        h,_,_,_,_,_=height(p[:,0],p[:,2]); ins=p[:,1]<=h
        hi=np.where(ins,m,hi); lo=np.where(ins,lo,m)
    t=np.where(hit,hi,tmax)
    p=cam+D*t[:,None]; pl=cam+D*lo[:,None]
    h,road,ax,az,lx,lz=height(p[:,0],p[:,2])
    # surface classification: roof if pl above h (vertical drop) else wall
    hl,_,_,_,_,_=height(pl[:,0],pl[:,2])
    roof=(pl[:,1]>=h-1e-3)&(h>0)&(np.abs(p[:,1]-h)<.8)
    ground=(h<=0)|(p[:,1]<.05)
    wall=hit&~roof&~ground
    # wall normal: which axis changed lot/height
    hx,_,_,_,_,_=height(p[:,0]-D[:,0]*.4,pl[:,2]*0+p[:,2])
    nx_wall=np.abs(hx-h)>1e-3
    col=np.zeros((N,3))
    # sky (barely visible)
    sky=~hit
    col[sky]=np.array([.03,.022,.02])
    # ground: asphalt with lane marks, sidewalks
    g=ground&hit
    gx,gz=p[:,0],p[:,2]
    side=g&(((ax>AVW-SIDE)&(ax<AVW))|((az>STW-SIDE)&(az<STW)))
    asph=np.array([.01,.01,.012]); walk=np.array([.035,.033,.03])
    col[g]=asph; col[side]=walk
    lane=g&~side&(ax<AVW-SIDE)&(np.abs(np.abs(((gx+AV/2)%AV)-AV/2)-0)<.12)&(((gz/6)%1)<.55)
    col[lane]=np.array([.12,.10,.05])
    # streetlight pools along avenues + streets
    light=np.zeros(N)
    for sx in (-1,1):
        for k in range(-2,60):
            lzp=k*30.0; lxp=sx*(AVW-SIDE+.8)
            for off in (0.0,):
                dd=(gx-lxp)**2+(gz-lzp)**2
                light+=np.exp(-dd/ (2*5.5**2))
    sodium=np.array([1.0,.55,.18])
    col[g]+= (sodium*.34)*light[g,None]
    # cross-street lamps
    cl=np.zeros(N)
    for k in range(-3,40):
        for sz in (-1,1):
            czp=k*ST+sz*(STW-SIDE+.8)
            for cxk in range(-6,7):
                dd=(gx-cxk*22.0-11)**2+(gz-czp)**2
                cl+=np.exp(-dd/(2*4.0**2))
    col[g]+=(sodium*.35)*cl[g,None]
    # walls: windows
    wx=np.where(nx_wall,p[:,2],p[:,0])
    wy=p[:,1]
    cu=np.floor(wx/3.2); cv=np.floor(wy/3.6)
    fu=(wx/3.2)%1; fv=(wy/3.6)%1
    win=(fu>.18)&(fu<.82)&(fv>.25)&(fv<.8)
    lit=hash2(cu+lx*131,cv+lz*71,11)
    tint=hash2(lx,lz,13)
    bcol=np.array([.022,.022,.026])*(.6+.8*tint[:,None])
    shade=np.where(nx_wall,.55,.9)
    col[wall]=(bcol*shade[:,None])[wall]
    wcol=np.where((lit>.88)[:,None],np.array([1.0,.72,.38])*(.5+.6*hash2(cu,cv,17))[:,None],np.array([.02,.025,.035]))
    ww=wall&win
    col[ww]=wcol[ww]
    # walls near ground lit by streetlights
    glow=np.exp(-np.maximum(wy,0)/14)
    col[wall]+=(sodium*.06)[None,:]*glow[wall,None]
    # roofs
    col[roof]=np.array([.03,.03,.034])*(.7+.6*hash2(lx,lz,19))[roof,None]
    bx=(p[:,0]/LOT)%1-.5; bz=(p[:,2]/LOT)%1-.5
    rv=roof&(hash2(lx,lz,23)>.85)&(bx**2+bz**2<.0016)
    col[rv]=np.array([1.0,.12,.05])  # rooftop beacons
    # cars: head/tail light streaks on avenue
    lanepos=np.abs(gx)
    cars=np.zeros(N)
    for k in range(80):
        cz=hash2(k,3,29)*1400-50; cx=(hash2(k,5,31)*2-1)*(AVW-SIDE-1.5)
        dd=((gx-cx)/0.7)**2+((gz-cz)/2.4)**2
        cars+=np.exp(-dd)
    carcol=np.where((gx<0)[:,None],np.array([1.0,.08,.04]),np.array([1.0,.9,.7]))
    col[g]+= (carcol*1.2*cars[:,None])[g]
    # fog
    fogc=np.array([.035,.026,.022])
    fogk=1-np.exp(-np.maximum(t-200,0)/900)
    col=col*(1-fogk[:,None])+fogc*fogk[:,None]
    img=col.reshape(H2,W2,3)
    # downsample
    img=img.reshape(H,ss,W,ss,3).mean((1,3))
    return img
def boxblur(a,r,axis):
    if r<1: return a
    a=np.moveaxis(a,axis,0)
    pad=np.concatenate([np.repeat(a[:1],r+1,0),a,np.repeat(a[-1:],r,0)],0)
    c=np.cumsum(pad,0)
    out=(c[2*r+1:]-c[:-2*r-1])/(2*r+1)
    return np.moveaxis(out,0,axis)
def gaussian_filter(a,sig):
    s=sig[0]; r=max(1,int(round(s*0.9)))
    for _ in range(3):
        a=boxblur(a,r,0); a=boxblur(a,r,1)
    return a
def post(img):
    H,W,_=img.shape
    bright=np.clip(img-.18,0,None)
    bloom=sum(gaussian_filter(bright,(s,s,0))*w for s,w in ((2,.6),(8,.5),(24,.4)))
    img=img+bloom
    # desaturate 50%
    lum=img@np.array([.2126,.7152,.0722])
    img=img*.5+lum[...,None]*.5
    # warm grade + darken
    img=img*np.array([1.05,.95,.85])*.62
    yy,xx=np.mgrid[0:H,0:W]
    vig=1-.75*(((xx-W/2)/(W/2))**2*.6+((yy-H/2)/(H/2))**2*.8)
    img*=np.clip(vig,.15,1)[...,None]
    return img
if __name__=='__main__':
    import time,sys;t=time.time()
    ss=int(sys.argv[1]) if len(sys.argv)>1 else 1
    img=render(1024,512,ss); np.save('city_raw.npy',img)
    out=post(img); np.save('city_lin.npy',out)
    s=np.clip(out,0,1); s=np.where(s<=.0031308,s*12.92,1.055*s**(1/2.4)-.055)
    Image.fromarray((s*255).round().astype(np.uint8)).save(sys.argv[2] if len(sys.argv)>2 else 'city.png'); print(time.time()-t)
