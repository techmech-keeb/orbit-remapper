from pathlib import Path
import json,math
import numpy as np
from PIL import Image,ImageDraw,ImageFont
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle,Circle,FancyBboxPatch
ROOT=Path(__file__).resolve().parents[1]
M=json.loads((ROOT/'drawings/mesh.json').read_text())
font='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
def render(path,az=-65,el=22,mode='outside',W=2000,H=1400,dim=False):
    bg=(238,241,243)
    im=Image.new('RGB',(W,H),bg); draw=ImageDraw.Draw(im)
    if mode!='exploded':draw.ellipse((W*.21,H*.71,W*.8,H*.88),fill=(214,220,224))
    a,e=math.radians(az),math.radians(el)
    # Camera looks toward origin from azimuth and elevation.
    right=np.array([-math.sin(a),math.cos(a),0])
    up=np.array([-math.cos(a)*math.sin(e),-math.sin(a)*math.sin(e),math.cos(e)])
    depth=np.array([math.cos(a)*math.cos(e),math.sin(a)*math.cos(e),math.sin(e)])
    R=np.array([right,up,depth])
    scene=[]; allverts=[]
    for ob in M:
        if mode=='display' and not (ob['name'].startswith('OLED_') or ob['name']=='03_display_carrier'):continue
        if mode=='inside' and ob['name'] in ['01_upper_shell','03_display_carrier','04_knob','05_back_button','06_reset_button','OLED_glass','OLED_lower_housing','OLED_PCB_frame','OLED_Grove_plug']:continue
        v=np.array(ob['v'],float)
        if mode=='exploded':
            shift={'01_upper_shell':(0,0,37),'03_display_carrier':(0,18,24),'02_bottom':(0,0,-12),'04_knob':(0,0,60),'05_back_button':(0,0,56),'06_reset_button':(0,0,56)}
            if ob['name'] in shift:v+=shift[ob['name']]
            if ob['name'].startswith('OLED_'):v+=np.array([0,0,38])
        p=v@R.T
        scene.append((ob,v,p));allverts.append(p)
    av=np.concatenate(allverts); mn=av[:,:2].min(0);mx=av[:,:2].max(0)
    scale=min(W*.85/(mx[0]-mn[0]),H*.79/(mx[1]-mn[1]));center=(mn+mx)/2
    faces=[]
    light=np.array([-.3,-.45,.84]);light/=np.linalg.norm(light)
    for ob,v,p in scene:
        fi=np.array(ob['f']); fn=np.cross(v[fi[:,1]]-v[fi[:,0]],v[fi[:,2]]-v[fi[:,0]])
        vn=np.zeros_like(v)
        for j in range(3):np.add.at(vn,fi[:,j],fn)
        vn/=np.maximum(np.linalg.norm(vn,axis=1,keepdims=True),1e-12)
        for f in ob['f']:
            inds=np.array(f);t=p[inds];n=np.cross(v[f[1]]-v[f[0]],v[f[2]]-v[f[0]])
            norm=np.linalg.norm(n)
            if norm<1e-9:continue
            n/=norm
            if np.dot(n,depth)<-.0001:continue
            c=np.array(ob['color']);shade=(.56+.44*np.maximum(0,vn[inds]@light))[:,None]
            # Lift black material to retain readable facet highlights.
            c=np.clip((c*.85+.075)*shade*255,0,255)
            if ob['name']=='OLED_glass':c=np.tile([153,204,209],(3,1))
            pts=np.column_stack(((t[:,0]-center[0])*scale+W*.5,H*.48-(t[:,1]-center[1])*scale))
            faces.append((t[:,2],pts,c,ob['name']))
    # Rasterize using a per-pixel depth buffer; triangle sorting cannot resolve
    # large coplanar CAD faces crossing smaller component triangles.
    rgb=np.array(im,dtype=float); zbuf=np.full((H,W),-np.inf)
    faces.sort(key=lambda f:f[3]=='OLED_glass')
    for depths,p,c,name in faces:
        x0=max(0,int(np.floor(p[:,0].min())));x1=min(W-1,int(np.ceil(p[:,0].max())))
        y0=max(0,int(np.floor(p[:,1].min())));y1=min(H-1,int(np.ceil(p[:,1].max())))
        if x1<x0 or y1<y0:continue
        xx,yy=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5)
        a,b,d=p
        den=(b[1]-d[1])*(a[0]-d[0])+(d[0]-b[0])*(a[1]-d[1])
        if abs(den)<1e-9:continue
        w0=((b[1]-d[1])*(xx-d[0])+(d[0]-b[0])*(yy-d[1]))/den
        w1=((d[1]-a[1])*(xx-d[0])+(a[0]-d[0])*(yy-d[1]))/den
        w2=1-w0-w1;z=w0*depths[0]+w1*depths[1]+w2*depths[2]
        zb=zbuf[y0:y1+1,x0:x1+1];patch=rgb[y0:y1+1,x0:x1+1]
        mask=(w0>=-1e-7)&(w1>=-1e-7)&(w2>=-1e-7)&(z>zb+1e-6)
        if name=='OLED_glass':patch[mask]=patch[mask]*.72+np.array(c[0])*.28
        else:patch[mask]=w0[mask,None]*c[0]+w1[mask,None]*c[1]+w2[mask,None]*c[2]
        zb[mask]=z[mask]
    im=Image.fromarray(np.uint8(np.clip(rgb,0,255)));draw=ImageDraw.Draw(im)
    title={'outside':'ORBITAL POD / E1','inside':'PCB + COMPONENT ENVELOPES','exploded':'ASSEMBLY STUDY','display':'GLASS2 + REAR CARRIER'}[mode]
    draw.text((W*.055,H*.04),title,font=ImageFont.truetype(font,int(W*.018)),fill=(37,49,59))
    draw.text((W*.055,H*.94),'MECHANICAL PROTOTYPE v0.3  /  mm',font=ImageFont.truetype(font,int(W*.01)),fill=(94,106,114))
    im.save(ROOT/'drawings'/path)

render('perspective.png')
render('rear.png',az=64,el=24)
render('inside.png',az=-62,el=62,mode='inside')
render('exploded.png',az=-58,el=25,mode='exploded',H=1800)
render('side.png',az=0,el=0,W=1800,H=1400)
render('front.png',az=-90,el=0,W=1800,H=1400)
render('display_mount.png',az=64,el=25,mode='display',W=1800,H=1400)

# Exact nominal top layout. Reference list/CSV supplies every small component.
fig,ax=plt.subplots(figsize=(10,8));fig.patch.set_facecolor('#f5f7f8');ax.set_facecolor('#f5f7f8')
from matplotlib.patches import Polygon
angs=np.linspace(math.pi-math.asin(35/52),2*math.pi+math.asin(35/52),220)
outline=np.c_[52*np.cos(angs),52*np.sin(angs)]
ax.add_patch(Polygon(outline,closed=True,fc='#e2e7eb',ec='#5c6c79',lw=1.2))
ax.add_patch(FancyBboxPatch((-37,-28),74,58,boxstyle='round,pad=0,rounding_size=3',fc='#edf5f1',ec='#2c6957',lw=1.2))
def rect(x,y,w,h,label,color='#cae3db'):
    ax.add_patch(Rectangle((x-w/2,y-h/2),w,h,fc=color,ec='#385d57',lw=1))
    ax.text(x,y,label,ha='center',va='center',fontsize=8,color='#263d38')
rect(-21.5,1.5,21,51,'U2\nPico\n21 x 51')
rect(0,20.5,17.8,21,'U1\nXIAO Plus')
rect(24,26.9,13.4,14.2,'J2\nUSB-A','#d6e1ec')
rect(0,30.5,9,7,'USB-C','#d6e1ec')
rect(24,-20,12,12,'SW1\nEC12D')
ax.add_patch(Circle((24,-20),11,fill=False,ec='#33444f',ls='--'))
rect(0,-25,12.8,5.8,'SW2')
rect(-35.5,17,3,2.5,'','#d0d0d0');ax.annotate('SW4 reset',(-35.5,17),(-49,18),fontsize=8,arrowprops=dict(arrowstyle='-',color='#546a78'),ha='right')
rect(12,-12,3,2.5,'','#d0d0d0');ax.text(12,-10,'SW3',fontsize=7,ha='center')
rect(33,5.5,8,6,'J3')
rect(22,6,15,13,'POWER\nU5 / C / R','#eee4c8')
rect(15,16,3,3,'','#eee4c8');ax.text(15,19,'U6',fontsize=8,ha='center')
rect(19,-5,18,3,'RC / C','#eee4c8')
ax.add_patch(Rectangle((-10.5,2),21,16,fc='#f4e3d2',ec='#b76c34',hatch='///',alpha=.65))
ax.text(0,5.2,'RF KEEP-OUT',ha='center',fontsize=7,color='#995222')
ax.add_patch(Rectangle((-5.6,9),11.2,7,fc='white',ec='#b76c34',lw=1))
ax.text(0,12.5,'CUT',ha='center',va='center',fontsize=7)
ax.add_patch(Rectangle((-28,-23),13,10,fc='white',ec='#b76c34',lw=1))
ax.text(-21.5,-18,'TP2/3\nACCESS',ha='center',va='center',fontsize=7)
for x,y in [(-34.5,-23),(-34.5,24),(34,24),(11,-24)]:ax.add_patch(Circle((x,y),1.2,fc='white',ec='#3f6357'))
route=np.array([[-21.5,-18],[-21.5,-9],[10,-9],[13,-6],[13,12],[15,16],[24,20]])
ax.plot(route[:,0],route[:,1],color='#347aa6',lw=2,ls='--',label='USB D+/D- route reserve (bottom side)')
ax.annotate('104',(-52,-57),(0,-57),ha='center',va='center',arrowprops=dict(arrowstyle='-'))
ax.annotate('',(-52,-57),(52,-57),arrowprops=dict(arrowstyle='<->',color='#465864'))
ax.text(0,38,'REAR / PC + KEYBOARD',ha='center',fontsize=10,color='#304b59')
ax.text(0,-64,'FRONT / USER',ha='center',fontsize=10,color='#304b59')
ax.set_xlim(-65,58);ax.set_ylim(-69,43);ax.set_aspect('equal');ax.axis('off')
ax.legend(loc='lower left',fontsize=8,frameon=False)
fig.savefig(ROOT/'drawings/pcb_layout.png',dpi=220,bbox_inches='tight');plt.close(fig)
print('rendered 8 views')
