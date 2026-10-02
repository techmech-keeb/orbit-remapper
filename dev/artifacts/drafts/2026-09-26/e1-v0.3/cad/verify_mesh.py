"""Check closed triangle edge topology for exported binary STL files."""
from pathlib import Path
from collections import Counter
import struct,json,numpy as np
r=Path(__file__).resolve().parents[1];checks={}
for p in sorted((r/'print_candidate').glob('*.stl')):
 raw=p.read_bytes();n=struct.unpack('<I',raw[80:84])[0]
 a=np.frombuffer(raw,offset=84,count=n,dtype=np.dtype([('normal','<f4',(3,)),('v','<f4',(3,3)),('attr','<u2')]))
 edges=Counter()
 for v in a['v']:
  pts=[tuple(np.round(q,5)) for q in v]
  for i,j in [(0,1),(1,2),(2,0)]:edges[tuple(sorted([pts[i],pts[j]]))]+=1
 checks[p.name]={'triangles':n,'nonmanifold_or_open_edges':sum(v!=2 for v in edges.values()),'closed_two_manifold':all(v==2 for v in edges.values())}
assert all(x['closed_two_manifold'] for x in checks.values()), checks
(r/'mesh_checks.json').write_text(json.dumps(checks,indent=2));print('All six STL meshes have closed two-manifold edge topology.')
