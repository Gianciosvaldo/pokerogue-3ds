#!/usr/bin/env python3
"""Transport native history entries, active movesets and actual condition outcomes."""
from pathlib import Path
import json,sys
from itertools import groupby
j=json.loads(Path(sys.argv[1]).read_text());assert j['version']==2 and len(j['rows'])==432
out=['LRHISTORY 2 432'];shapes=[0]*8;modes=[0]*6
for i,c in enumerate(j['rows']):
 assert len(c['history'])==c['length'] and len(c['result'])==5 and all(type(r)is bool for r in c['result'])
 shapes[c['shape']]+=1;modes[c['mode']-1]+=1
 out.append(f"CASE {i} {c['shape']} {c['length']} {c['mode']} {len(c['moves'])} {int(c['active'])} {c['futureMove']}")
 out.append('SET '+' '.join(map(str,c['moves'])))
 entries=[]
 for m in c['history']:
  assert len(m['targets'])<=4
  entries.append((m['move'],m['useMode'],m.get('result',255),len(m['targets']),*m['targets']))
 for fields,group in groupby(entries):
  count=sum(1 for _ in group)
  out.append('RUN '+str(count)+' '+' '.join(map(str,fields)))
 out.append('RESULT '+' '.join(str(int(r))for r in c['result']))
 out.append('END')
assert shapes==[54]*8 and modes==[72]*6
out.append('COMPLETE 432')
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
