"""Transport actual Helping Hand API snapshots; no generated expectations."""
from pathlib import Path
import json,sys
rows=Path(sys.argv[1]).read_text().splitlines();h=rows[0].split();assert h[:2]==['HELPING_HAND','1'];n=int(h[2]);assert len(rows)==n+2 and rows[-1]==f'HELPING_HAND_COMPLETE {n}'
ops=['add','other','custom','end','pre','faint','remove','source_remove','transfer'];out=[f'HELPINGHAND 1 {n}']
for i,line in enumerate(rows[1:-1]):
 r=json.loads(line);assert r['i']==i;out.append('C '+' '.join(map(str,[i,r['target'],r['source'],r['other'],r['turn'],r['move'],len(r['snapshots'])])));out.append('W '+r['waveSeed'])
 for p in r['input']:out.append('MON '+' '.join(map(str,p)))
 for state,key in zip(r['before'],['R','B']):out.append(key+' '+state.removeprefix('!rnd,').replace(',',' '))
 for snap in r['snapshots']:
  out.append(f'OP {ops.index(snap["op"])} {-1 if snap["result"] is None else int(snap["result"])} {len(snap["tags"])}')
  for t in snap['tags']:
   assert t['type']=='HELPING_HAND';out.append('T '+' '.join(map(str,[t['count'],t['move'],t['source'],t['baton'],len(t['lapse']),*t['lapse']])))
  out.append('LEVELS '+str(len(snap['queue'])))
  for l,q in enumerate(snap['queue']):
   out.append(f'L {l} {len(q)}')
   for p in q:
    assert p['name']=='MessagePhase';out.append('MSG '+(p['text'].encode().hex() if p['text'] else '-'))
  out.append('DYNAMIC '+str(snap['dynamic']))
 for state,key in zip(r['after'],['RO','BO']):out.append(key+' '+state.removeprefix('!rnd,').replace(',',' '))
 out.append('E')
out.append(f'HELPINGHAND_COMPLETE {n}');Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
