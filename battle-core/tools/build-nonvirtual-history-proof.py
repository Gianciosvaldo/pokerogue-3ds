#!/usr/bin/env python3
"""Lossless transport of the ported TurnMove fields observed from native owners."""
from pathlib import Path
import json,sys
j=json.loads(Path(sys.argv[1]).read_text());assert j['version']==1 and len(j['rows'])==972
lines=['NVHISTORY 1 972']
def move(m):
 assert type(m['move'])is int and 0<=m['move']<921
 assert m['useMode']in range(1,7) and m['result']in [0,1,2,3,4,255]
 assert len(m['targets'])<=4 and all(type(x)is int and x in range(4) for x in m['targets'])
 return ' '.join(map(str,[m['move'],m['useMode'],m['result'],len(m['targets']),*m['targets']]))
for i,c in enumerate(j['rows']):
 assert c['length']==[0,1,7,8,9,64,65535,65536,65537][i//108]
 assert c['mode']==1+i//18%6 and c['shape']==i%6
 assert sum(r['count']for r in c['runs'])==c['length']
 lines.append(f"CASE {i} {c['length']} {c['mode']} {c['first']} {c['shape']} {len(c['runs'])}")
 for r in c['runs']:
  assert type(r['count'])is int and r['count']>0
  lines.append(f"RUN {r['count']} "+move(r['entry']))
 lines+=['ADD '+move(c['added']),'RESETADD '+move(c['resetAdded'])]
 assert len(c['result'])==6 and all(len(s)==4 for s in c['result'])
 for stage,results in enumerate(c['result']):
  lines.append(f'STAGE {stage}')
  for m in results:lines.append('RESULT '+('0'if m is None else '1 '+move(m)))
  assert len(c['owners'][stage])==5 and all(type(x)is bool for x in c['owners'][stage])
  lines.append('OWNERS '+' '.join(str(int(x))for x in c['owners'][stage]))
 lines.append('END')
assert len(j['aliases'])==12
lines.append('ALIASES 12')
for i,c in enumerate(j['aliases']):
 assert c['prefix']==[0,9,65536][i//4] and len(c['result'])==7
 lines.append(f"ALIAS {i} {c['prefix']} {c['mode']} {c['move']}")
 for stage,results in enumerate(c['result']):
  assert len(results)==4
  lines.append(f'ASTAGE {stage}')
  for m in results:lines.append('RESULT '+('0'if m is None else '1 '+move(m)))
 lines.append('AEND')
lines.append('COMPLETE 972');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
