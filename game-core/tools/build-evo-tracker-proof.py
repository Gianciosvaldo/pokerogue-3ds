#!/usr/bin/env python3
"""Lossless native snapshot transport; no expected behavior is calculated."""
import json,sys
r=json.load(open(sys.argv[1]));assert r['version']==1 and len(r['rows'])==576 and len(r['cleanup'])==12
out=['EVOTRACKER 1 576 12'];seen=set()
def stock(label,items):
 out.append(f'{label} {len(items)}')
 for m in items:
  assert m['key'] in ('EVOLUTION_TRACKER_GIMMIGHOUL','GOLDEN_PUNCH','AMULET_COIN','GOLDEN_POKEBALL')
  assert -1<=m['owner']<3 and m['virtual']==0 and 1<=m['stack']<=999
  out.append(f"I {m['key']} {m['owner']} {m['stack']} {m['virtual']} {int(m['locked'])}")
for i,x in enumerate(r['rows']):
 k=(x['family'],x['arrangement'],x['count'],x['treasure']);assert k not in seen;seen.add(k)
 assert 0<=k[0]<3 and 0<=k[1]<6 and k[2] in (0,1,7,8,9,997,998,999) and 0<=k[3]<4
 assert len(x['mons'])==len(x['evolutions'])==3
 out.append('C '+ ' '.join(map(str,[i,*k,x['wave'],x['before'],x['money'],x['balls']])))
 for m,e in zip(x['mons'],x['evolutions']):out.append('M '+' '.join(map(str,[m['species'],m['fusion'],m['form'],m['treasure'],m['beforeEvo'],e])))
 stock('B',x['input']);stock('A',x['after'])
for i,x in enumerate(r['cleanup']):
 out.append('D '+' '.join(map(str,[i,x['form'],x['copies'],x['fused'],x['species'],x['fusion']])))
 stock('B',x['before']);stock('A',x['after'])
out.append('END 576 12');open(sys.argv[2],'w').write('\n'.join(out)+'\n')
