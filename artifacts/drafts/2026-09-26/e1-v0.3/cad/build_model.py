"""ORBITAL POD / E1, mechanical prototype v0.3. Units mm, CadQuery 2.7.
Glass2 is represented from its published mechanical drawing, not bare glass.
Supplier size-table discrepancy remains an explicit first-article fit gate.
"""
from pathlib import Path
import cadquery as cq
import math,json,csv,itertools
from mesh_cleanup import clean_stl
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'cad'; PRINT=ROOT/'print_candidate'
for p in [OUT,PRINT,ROOT/'drawings']:p.mkdir(exist_ok=True)
P=dict(width=104,depth=87,body_top=38,rear_plane_y=35,wall_nominal=2.4,
       pcb_width=74,pcb_depth=58,pcb_bottom=6,pcb_thickness=1.6,
       screen_angle=65,screen_origin_y=12,screen_origin_z=17,
       glass2_drawing_width=42,glass2_top_to_mount_centres=44,
       glass2_ear_extension=3,glass2_drawing_depth=6.35,
       supplier_table_size=[53,42,6],size_discrepancy_unresolved=True,
       bezel_width=47.2,bezel_height=32,bezel_aperture=[37.4,19.4],
       insert_pilot_M2=3.1,insert_pilot_M2_5=3.5)
(OUT/'parameters.json').write_text(json.dumps(P,indent=2))
def box(w,d,h,x=0,y=0,z=0):return cq.Workplane('XY').box(w,d,h,centered=(True,True,False)).translate((x,y,z))
def cyl(r,h,x=0,y=0,z=0):return cq.Workplane('XY').circle(r).extrude(h).translate((x,y,z))
def rounded(w,d,h,r,x=0,y=0,z=0):return box(w,d,h,x,y,z).edges('|Z').fillet(r)
def screen(s):return s.rotate((0,0,0),(1,0,0),65).translate((0,12,17))
def ellipsoid(rad,zrad,zc):
 s=cq.Workplane('XY').sphere(rad).val().transformGeometry(cq.Matrix([[1,0,0,0],[0,1,0,0],[0,0,zrad/rad,zc],[0,0,0,1]]))
 return cq.Workplane(obj=s)
def keep_rear(y,z=-10,h=100):return box(200,200,h,0,y-100,z)
case_screws=[(-41,-18),(41,-18),(-40,22),(40,22)]
board_screws=[(-34.5,-23),(-34.5,24),(34,24),(11,-24)]
feet_xy=[(-27,-27),(27,-27),(-27,22),(27,22)]
# Oblate spherical pod. A short flat rear provides usable connector access.
outer=ellipsoid(52,28,10).intersect(box(150,150,80,z=2.4)).intersect(keep_rear(35))
inner=ellipsoid(49.6,25.6,10).intersect(keep_rear(32.8))
# Low local saddles give the side connector reserve sufficient cover thickness.
for x in [-27,27]:outer=outer.union(ellipsoid(8,5,29).translate((x,9,0)))
shell=outer.cut(inner)
bottom=cyl(49.32,2).intersect(keep_rear(35))
lip=cyl(46.0,2,z=2).intersect(keep_rear(31.8)).cut(cyl(44.5,3,z=1.9).intersect(keep_rear(30.3)))
for x,y in case_screws:lip=lip.cut(cyl(3.5,5,x,y,1.5))
bottom=bottom.union(lip)
for x,y in board_screws:bottom=bottom.union(cyl(2.65,4,x,y,2)).cut(cyl(1.55,3.2,x,y,2.9))
for x,y in case_screws:
 bottom=bottom.cut(cyl(1.25,8,x,y,-1)).cut(cyl(2.1,1.1,x,y,-.1))
 # Radial web connects each screw boss to the curved side wall.
 r=math.hypot(x,y);a=math.degrees(math.atan2(y,x))
 web=box(10,2.4,15,r+2,0,2.4).rotate((0,0,0),(0,0,1),a)
 shell=shell.union(cyl(3,15,x,y,2.4).union(web).intersect(outer))
 shell=shell.cut(cyl(1.55,3.2,x,y,2.3)).cut(cyl(1.05,7,x,y,2.3))
for x,y in feet_xy:bottom=bottom.cut(cyl(5.2,.55,x,y,-.05))
bottom=bottom.union(box(9.8,1.9,6.3,0,32.95,2)).union(box(14.4,1.9,4.5,24,32.95,2))
shell=shell.cut(box(10.6,14,12,0,35,2.3)).cut(box(15.2,14,13,24,35,2.3))
for x,w,z in [(0,14.8,6.6),(24,20.8,5.6)]:shell=shell.cut(box(w,16,12,x,41.9,z))
# E1 slim cosmetic bezel, integrated only at its lower saddle into the pod.
bezel=rounded(47.2,32,6.6,2.2,y=30,z=-2.4).edges('>Z or <Z').fillet(.65)
aperture=rounded(37.4,19.4,40,.45,y=32,z=-20)
bezel=bezel.cut(aperture)
saddle=rounded(50.4,25.8,12,2.4,y=9.2,z=-3).edges('>Z or <Z').fillet(1.0)
shell=shell.union(screen(bezel)).union(screen(saddle))
# Rear loading pocket accommodates the real PCB frame and glass sheet.
shell=shell.cut(screen(rounded(43.4,49.2,10,.6,y=20.2,z=-8)))
shell=shell.cut(screen(box(43.4,25,15.4,0,8,-8)))
shell=shell.cut(screen(aperture))
# A removable rear carrier conceals the rear face of the original module frame.
carrier=rounded(46.4,31.2,1.9,1.9,y=30,z=-4.6).cut(aperture)
carrier=carrier.union(rounded(43,29.2,1.5,1.1,y=30,z=-2.7).cut(aperture))
for x in [-16,16]:
 carrier=carrier.union(box(7.2,21,2.9,x,6.5,-5.6))
 carrier=carrier.union(cyl(3.6,4.6,x,0,-5.6)).cut(cyl(1.75,3.4,x,0,-4.3))
carrier=screen(carrier)
for x in [-22,22]:carrier=carrier.union(box(16,16,3,x,20,17))
for x in [-26,26]:carrier=carrier.cut(cyl(1.25,6,x,24,16))
carrier=carrier.intersect(box(200,200,100,z=12.2))
carrier=carrier.cut(screen(box(43.4,49.2,12,0,20.2,-1.2)))
carrier=carrier.cut(box(7.6,8.6,14,33,10,8.5))
for x in [-26,26]:carrier=carrier.cut(cyl(3.6,40,x,24,20))
# Clearance round the rear carrier rim; seam is hidden at the back of the display.
shell=shell.cut(screen(rounded(47.0,31.8,3.0,2.1,y=30,z=-4.9)))
# A curved rear skirt closes the root pocket while preserving the viewing aperture.
root_cap=outer.intersect(screen(box(43.0,11.1,4,0,16.55,-8.3)))
carrier=carrier.union(root_cap)
shell=shell.cut(screen(box(43.4,11.7,4.6,0,16.55,-8.6)))
# Capture from the bottom with two vertical M2 screws into shell posts.
for x in [-26,26]:
 shell=shell.union(cyl(3.3,20,x,24,20).intersect(outer)).cut(cyl(1.55,3.2,x,24,19.9))
# Side Grove plug remains inside the pod, with a clear lead-out toward its right.
shell=shell.cut(screen(box(10.6,7.6,6.95,26,8,-.3)))
# Controls: closed well floors hide PCB, shafts run in separate guide bores.
shell=shell.union(cyl(14.4,20,24,-20,19.4).intersect(outer))
shell=shell.cut(cyl(12.4,35,24,-20,24.9)).cut(cyl(5.3,30,24,-20,14))
shell=shell.union(rounded(21,16,16,4,0,-25,23).intersect(outer))
shell=shell.cut(rounded(16,12,12.9,3,0,-25,12))
shell=shell.cut(rounded(17,10,30,4.8,0,-25,29.2)).cut(rounded(11,7,25,3,0,-25,23.5))
guide=cyl(5.4,24,-35.5,17,10.2).intersect(outer)
shell=shell.union(guide).cut(cyl(4.4,15.1,-35.5,17,10.1))
shell=shell.cut(cyl(2.5,35,-35.5,17,10)).cut(cyl(3.75,22,-35.5,17,25.2))
# Rear carrier feet enter from below; relief leaves the screw bearing planes at Z=20.
for x in [-22,22]:shell=shell.cut(box(16.6,16.6,3.3,x,20,16.7))
# Mechanical PCB carrier, same main envelope and reserved TP/RF openings.
pcb=rounded(74,58,1.6,3,y=1,z=6)
for x,y in case_screws:pcb=pcb.cut(cyl(3.8,3,x,y,5.5))
for x,y in board_screws:pcb=pcb.cut(cyl(1.2,3,x,y,5.5))
pcb=pcb.cut(rounded(13,10,3,1,x=-21.5,y=-18,z=5.5))
pcb=pcb.cut(rounded(11.2,7,3,.7,x=0,y=12.5,z=5.5))
knob=cyl(11,6,24,-20,27.2).edges('>Z or <Z').fillet(.8).union(cyl(4.5,10.4,24,-20,17.8))
knob=knob.cut(cyl(3.15,13,24,-20,17.7))
for i in range(24):
 a=2*math.pi*i/24;knob=knob.cut(cyl(.55,4,24+11.15*math.cos(a),-20+11.15*math.sin(a),28.2))
knob=knob.cut(cq.Workplane('YZ').circle(.85).extrude(12).translate((24,-20,28.3)))
backbutton=rounded(10,6,10.5,2.8,0,-25,23.7).union(rounded(14,10,1.2,2,0,-25,23.5)).union(cyl(1.8,9.6,0,-25,14.1))
reset=cyl(2,17,-35.5,17,9).union(cyl(4,1.2,-35.5,17,23.8))
print_parts={'01_upper_shell':shell,'02_bottom':bottom,'03_display_carrier':carrier,'04_knob':knob,'05_back_button':backbutton,'06_reset_button':reset}
electronics={}
electronics['PCB_carrier']=pcb
electronics['Pico_board']=box(21,51,1.0,-21.5,1.5,7.8)
electronics['Pico_components']=box(16,33,2.7,-21.5,5,8.8)
electronics['Pico_microUSB']=box(8,6,3.1,-21.5,-21.5,8.8)
electronics['XIAO_board']=box(17.8,21,1.0,0,20.5,7.8)
electronics['XIAO_shield']=box(12,12,2.5,0,22,8.8)
electronics['XIAO_USB_C']=box(9,7,3.2,0,30.5,9.9)
electronics['USB_A']=box(13.4,14.2,7.0,24,26.9,7.6)
electronics['EC12D_body']=box(12,12,5,24,-20,7.6)
electronics['EC12D_shaft']=cyl(3,17.5,24,-20,12.6)
electronics['D2F_01']=box(12.8,5.8,6.5,0,-25,7.6)
electronics['SW4_RUN_external']=box(3,2.5,1.2,-35.5,17,7.6)
electronics['SW3_RUN_internal']=box(3,2.5,1.2,12,-12,7.6)
electronics['J3_Grove']=box(8,6,5,33,5.5,7.6)
# Glass2 coordinates: local h=0 at M3 mounting centres, +h toward window top.
frame=box(42,40,1,0,24,-1).cut(box(37,19,3,0,32,-2))
for x in [-16,16]:frame=frame.union(box(6,5,1,x,2,-1)).union(cyl(3,1,x,0,-1)).cut(cyl(1.5,3,x,0,-2))
electronics['OLED_PCB_frame']=screen(frame)
electronics['OLED_lower_housing']=screen(box(42,19,6.35,0,10.5,0))
# The visible glass envelope is restricted to the open window to avoid double
# rendering the PCB overlap; full sheet dimensions are recorded in parameters.
electronics['OLED_glass']=screen(box(37,19,1.25,0,32,0))
electronics['OLED_Grove_plug']=screen(box(10,7,6.35,26,8,0))
P['glass_sheet_size']=[42.04,27.22,1.25]
small=[('D2',3.8,2,1.4,13,1),('U5',3,3,1.5,21,11),('R_ILIM',1,0.5,.5,17,11),('C_VBUSA_100u',7.3,4.3,4.5,24,4),('C_VBUSA_10u',1.6,.8,.9,19,5),('U6',3,3,1.5,15,16),('R_I2C_1',1,.5,.5,29,0),('R_I2C_2',1,.5,.5,29,-2),('C_DEC_1',1,.5,.5,18,13),('C_DEC_2',1,.5,.5,13,13),('C_DEC_3',1,.5,.5,18,-9),('C_BULK_1',1.6,.8,.9,12,-5),('C_BULK_2',1.6,.8,.9,15,-5),('R_ENC_1',1,.5,.5,17,-5),('R_ENC_2',1,.5,.5,20,-5),('C_ENC_1',1,.5,.5,23,-5),('C_ENC_2',1,.5,.5,26,-5),('LED1_optional',1.6,.8,.8,7,-9),('R_LED',1,.5,.5,9,-9)]
for name,w,d,h,x,y in small:electronics[name]=box(w,d,h,x,y,7.6)
reserves={'USB_C_plug':box(14,27,9,0,48,7),'USB_A_plug':box(20,30,11,24,49,6),
          'RF_no_copper':box(21,16,10,0,10,6),'Grove_wire_corridor':box(1,1,1,33,10,10),
          'USB_TP_access':box(13,10,6,-21.5,-18,0),
          'OLED_full_glass_sheet':screen(box(42.04,27.22,1.25,0,30.39,0))}
# Swept wiring reserve, from PCB Grove to the display plug strain relief.
wire_points=[(33,5.5,12.6),(33,10,23),(31.5,12.5,25.6)]
wire=None
for aa,bb in zip(wire_points,wire_points[1:]):
    dv=cq.Vector(*bb).sub(cq.Vector(*aa))
    rod=cq.Workplane(obj=cq.Solid.makeCylinder(1.8,dv.Length,cq.Vector(*aa),dv.normalized()))
    wire=rod if wire is None else wire.union(rod)
for pp in wire_points:wire=wire.union(cq.Workplane('XY').sphere(1.8).translate(pp))
reserves['Grove_wire_corridor']=wire
colors={'01_upper_shell':(.15,.17,.19),'02_bottom':(.08,.10,.12),'03_display_carrier':(.15,.17,.19),'04_knob':(.23,.25,.27),'05_back_button':(.22,.24,.26),'06_reset_button':(.14,.16,.18)}
assembly=cq.Assembly(name='ORBITAL_POD_E1_v03');render_objects=[]
def add(name,s,color,kind,alpha=1):
 assembly.add(s,name=name,color=cq.Color(*color,alpha));vertices,triangles=s.val().tessellate(.06,.08)
 render_objects.append(dict(name=name,kind=kind,color=color,alpha=alpha,v=[[round(p.x,3),round(p.y,3),round(p.z,3)] for p in vertices],f=triangles))
cleanup_log={}
for n,s in print_parts.items():
 assert s.val().isValid(),n
 if len(s.solids().vals())!=1:
  for q in s.solids().vals():
   b=q.BoundingBox();print(n,q.Volume(),[round(x,2) for x in [b.xmin,b.xmax,b.ymin,b.ymax,b.zmin,b.zmax]],flush=True)
  raise ValueError(n+' disconnected')
 cq.exporters.export(s,str(OUT/(n+'.step')));cq.exporters.export(s,str(PRINT/(n+'.stl')),tolerance=.05,angularTolerance=.1)
 cleanup_log[n+'.stl']=clean_stl(PRINT/(n+'.stl'))
 add(n,s,colors[n],'print')
for n,s in electronics.items():
 color=(.11,.38,.32)
 if any(v in n for v in ['shield','USB','shaft']):color=(.66,.7,.72)
 elif n=='OLED_glass':color=(.30,.73,.79)
 elif n not in ['PCB_carrier','Pico_board','XIAO_board']:color=(.20,.22,.24)
 add(n,s,color,'electronics',.28 if n=='OLED_glass' else 1)
for i,(x,y) in enumerate(feet_xy):add('foot_'+str(i),cyl(5,1.5,x,y,-1),(.06,.07,.08),'hardware')
assembly.export(str(OUT/'assembly_review.step'));cq.exporters.export(pcb,str(OUT/'PCB_mechanical_envelope.step'));cq.exporters.export(pcb.faces('<Z'),str(OUT/'PCB_outline_NPTH.dxf'))
(ROOT/'drawings/mesh.json').write_text(json.dumps(render_objects,separators=(',',':')))
checks={'parameters':P,'parts':{},'collisions':[],'notes':['Nominal CAD-envelope checks, not electrical routing, RF, tolerance-stack, strength or physical assembly validation.','Glass2 uses the published mechanical drawing: width42, top-to-mount44, ears3, depth6.35. Size-table53x42x6 remains unresolved; verify the physical unit before manufacture.','Glass2 full glass sheet is checked separately; rendered glass only fills the transparent aperture.','Insert pilot diameters, push travel and actual connector and shaft positions require first-article adjustment.']}
for n,s in print_parts.items():
 b=s.val().BoundingBox();checks['parts'][n]=dict(valid=s.val().isValid(),solids=len(s.solids().vals()),volume_mm3=round(s.val().Volume(),2),bbox_conservative_mm=[round(b.xlen,2),round(b.ylen,2),round(b.zlen,2)])
def check(a,b,an,bn):
 v=a.val().intersect(b.val()).Volume()
 if v>.02:checks['collisions'].append([an,bn,round(v,4)])
for n,s in electronics.items():
 for pn in ['01_upper_shell','02_bottom','03_display_carrier']:check(s,print_parts[pn],n,pn)
for an,bn in itertools.combinations(print_parts,2):check(print_parts[an],print_parts[bn],an,bn)
for n in ['USB_C_plug','USB_A_plug','OLED_full_glass_sheet','Grove_wire_corridor']:
 for pn in ['01_upper_shell','02_bottom','03_display_carrier']:check(reserves[n],print_parts[pn],n,pn)
checks['control_motion']={}
for pn,travel in [('04_knob',.5),('05_back_button',.6),('06_reset_button',.4)]:
    moved=print_parts[pn].translate((0,0,-travel))
    vv=moved.val().intersect(shell.val()).Volume()
    checks['control_motion'][pn]={'downward_travel_mm':travel,'shell_overlap_mm3':round(vv,5),'status':'clear' if vv<.02 else 'collision'}
checks['plug_to_plug_overlap_mm3']=reserves['USB_C_plug'].val().intersect(reserves['USB_A_plug'].val()).Volume()
checks['nominal_pa12_volume_cm3']=round(sum(s.val().Volume() for s in print_parts.values())/1000,2)
checks['assembly_bbox_mm']=[round(max(o['v'][i][j] for o in render_objects for i in range(len(o['v'])))-min(o['v'][i][j] for o in render_objects for i in range(len(o['v']))),2) for j in range(3)]
(ROOT/'stl_cleanup.json').write_text(json.dumps(cleanup_log,indent=2))
(ROOT/'fit_checks.json').write_text(json.dumps(checks,indent=2));(OUT/'parameters.json').write_text(json.dumps(P,indent=2))
with (ROOT/'component_positions.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['reference','xmin_mm','xmax_mm','ymin_mm','ymax_mm','zmin_mm','zmax_mm','geometry_status'])
 for n,s in electronics.items():
  b=s.val().BoundingBox();w.writerow([n,*[round(v,3) for v in [b.xmin,b.xmax,b.ymin,b.ymax,b.zmin,b.zmax]],'mechanical_envelope_not_footprint'])
print(json.dumps(checks,indent=2),flush=True)
