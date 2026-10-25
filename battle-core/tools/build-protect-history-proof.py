#!/usr/bin/env python3
"""Transport actual Protect conditions and exact native RNG states."""
from pathlib import Path
import json,sys
j=json.loads(Path(sys.argv[1]).read_text());assert j['version']==1
protect=[182,197,203,588,596,661,743,792,852,908];history=protect+[501,469]
lengths=[0,1,7,8,9,19,20,33,34,40,100,645,646,647,648,65535,65536]
assert j['protect']==protect and j['historyMoves']==history and j['lengths']==lengths and len(j['rows'])==1224
assert any(x['result']for r in j['rows']for x in r['stages'][0])
states=[]
def state(s):
 if s not in states:states.append(s)
 return states.index(s)
records=[]
for i,r in enumerate(j['rows']):
 assert (r['length'],r['historyMove'],r['mode'])==(lengths[i//72],history[i//6%12],1+i%6)
 records.append(f"CASE {i} {r['length']} {r['historyMove']} {r['mode']}")
 assert len(r['stages'])==5
 for stage,obs in enumerate(r['stages']):
  assert len(obs)==40;records.append(f'STAGE {stage}')
  for q,x in enumerate(obs):
   assert x['move']==protect[q//4] and x['rng']==q%4 and type(x['result'])is bool
   records.append(f"OBS {x['move']} {x['rng']} {state(x['before'])} {state(x['after'])} {int(x['result'])}")
 records.append('END')
lines=[f'PROTECTHISTORY 1 1224 {len(states)}',f"BATTLE {j['battleSeed']} {j['turn']}"]
for i,s in enumerate(states):
 if s is None:lines.append(f'STATE {i} 0')
 else:
  parts=s.split(',');assert parts[0]=='!rnd' and len(parts)==5
  lines.append(f'STATE {i} 1 '+' '.join(parts[1:]))
lines+=records;assert len(j['gates'])==24;lines.append('GATES 24')
for i,x in enumerate(j['gates']):
 assert x['move']==history[i%12] and x['queued']==(i>=12) and type(x['result'])is bool
 lines.append(f"GATE {int(x['queued'])} {x['move']} {int(x['result'])}")
lines.append('COMPLETE 1224');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
