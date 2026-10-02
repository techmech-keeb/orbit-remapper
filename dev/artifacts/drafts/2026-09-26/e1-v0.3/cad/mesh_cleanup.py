"""Remove only zero-area CAD tessellation triangles (e.g. ellipsoid poles)."""
from pathlib import Path
import struct,json
def clean_stl(path):
 p=Path(path);raw=p.read_bytes();n=struct.unpack('<I',raw[80:84])[0];kept=[];removed=0
 for i in range(n):
  block=raw[84+i*50:134+i*50];v=struct.unpack('<12fH',block)[3:12]
  a=[v[j+3]-v[j] for j in range(3)];b=[v[j+6]-v[j] for j in range(3)]
  cr=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
  if sum(t*t for t in cr)<1e-18:removed+=1
  else:kept.append(block)
 if removed:p.write_bytes(raw[:80]+struct.pack('<I',len(kept))+b''.join(kept))
 return {'removed_zero_area_triangles':removed,'retained_triangles':len(kept)}
if __name__=='__main__':
 r=Path(__file__).resolve().parents[1]
 log={p.name:clean_stl(p) for p in (r/'print_candidate').glob('*.stl')}
 (r/'stl_cleanup.json').write_text(json.dumps(log,indent=2));print(log)
