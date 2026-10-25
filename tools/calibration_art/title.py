import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os
ORB=os.environ.get('ORBITRON', '/usr/share/fonts/truetype/sand-box/google/Orbitron/Orbitron-VariableFont_wght.ttf')
def orbitron(size):
    f=ImageFont.truetype(ORB,size)
    try: f.set_variation_by_axes([900])
    except Exception as e: print('var',e)
    return f
def title(text,W=1024,H=128,size=86,track=.30,shear=.22):
    S=3  # supersample
    f=orbitron(size*S)
    # measure with tracking
    adv=[f.getlength(c) for c in text]; sp=size*S*track
    total=sum(adv)+sp*(len(text)-1)
    scale=1.0
    maxw=(W-60)*S
    if total>maxw: scale=maxw/total
    f=orbitron(int(size*S*scale)); adv=[f.getlength(c) for c in text]; sp=size*S*scale*track; total=sum(adv)+sp*(len(text)-1)
    mask=Image.new('L',(W*S,H*S),0); d=ImageDraw.Draw(mask)
    asc,desc=f.getmetrics()
    bbox=f.getbbox('H'); capTop,capBot=bbox[1],bbox[3]
    y=(H*S-(capBot-capTop))/2-capTop
    x=(W*S-total)/2
    for c,a in zip(text,adv):
        d.text((x,y),c,font=f,fill=255); x+=a+sp
    # italic shear about the middle
    mask=mask.transform(mask.size,Image.AFFINE,(1,shear,-shear*H*S/2,0,1,0),resample=Image.BICUBIC)
    fill=np.asarray(mask.resize((W,H),Image.LANCZOS),float)/255
    # outline: dilate
    big=mask.filter(ImageFilter.MaxFilter(1+2*int(3.2*S)))
    outline=np.asarray(big.resize((W,H),Image.LANCZOS),float)/255
    glow=np.asarray(big.filter(ImageFilter.GaussianBlur(9*S)).resize((W,H),Image.LANCZOS),float)/255
    glow=np.clip(glow*1.6,0,1)
    # gradient: gold top -> orange bottom, measured over the cap height
    top=(H-(capBot-capTop)/S)/2; bot=H-top
    t=np.clip((np.arange(H)[:,None]-top)/(bot-top),0,1)*np.ones((1,W))
    gold=np.array([255,226,92])/255; mid=np.array([255,170,30])/255; orange=np.array([235,92,10])/255
    g=np.where((t<.5)[...,None], gold+(mid-gold)*(t/.5)[...,None], mid+(orange-mid)*((t-.5)/.5)[...,None])
    # a bright sheen line just above the middle
    sheen=np.exp(-((t-.42)/.06)**2)*.35
    g=np.clip(g+sheen[...,None],0,1)
    glowc=np.array([255,120,10])/255
    # compose: glow (premult), outline black, fill gradient
    rgb=glowc*glow[...,None]*.55; a=glow*.55
    a2=outline; rgb=rgb*(1-a2[...,None]); a=a2+a*(1-a2)
    rgb=rgb*(1-fill[...,None])+g*fill[...,None]; a=fill+a*(1-fill)
    out=np.concatenate([np.where(a[...,None]>0,rgb/np.maximum(a[...,None],1e-6),0),a[...,None]],-1)
    return np.clip(out,0,1)
if __name__=='__main__':
    for n,t in (('t1','BODY CALIBRATION'),('t2','CALIBRATED')):
        o=title(t)
        bg=np.zeros(o.shape[:2]+(3,))+.05
        c=bg*(1-o[...,3:])+o[...,:3]*o[...,3:]
        Image.fromarray((c*255).astype(np.uint8)).save(n+'.png')
