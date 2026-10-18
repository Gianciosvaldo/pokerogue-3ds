"""Transport real source EncounterPhase.end observations, without expectations."""
import json,sys
from pathlib import Path
rows=Path(sys.argv[1]).read_text().splitlines();h=rows[0].split();assert h[:2]==['ENCOUNTER_END','2'];n=int(h[2]);assert len(rows)==n+2 and rows[-1]==f'ENCOUNTER_END_COMPLETE {n}'
out=[f'ENCOUNTEREND 2 {n}']
for i,line in enumerate(rows[1:-1]):
 r=json.loads(line);assert r['i']==i;a=r['input'];out.append('C '+' '.join(map(str,[i,int(a['double']),int(a['loaded']),a['count'],a['enemyCount'],a['mode'],a['battleType'],a['wave'],int(a.get('common',False))])))
 for p in a['players']:out.append('P '+' '.join(map(str,p)))
 for p in a['enemies']:out.append('N '+' '.join(map(str,[p[0],p[1] if len(p)>1 else 1])))
 for states,keys in [(r['before'],['R','B'])]:
  for state,key in zip(states,keys):out.append(key+' '+state.removeprefix('!rnd,').replace(',',' '))
 out.append('LEVELS '+str(len(r['queue'])))
 for k,q in enumerate(r['queue']):
  out.append(f'L {k} {len(q)}')
  for p in q:out.append('Q '+' '.join(map(str,p)))
 out.append('DYNTYPES '+str(len(r['dynamic'])))
 for name,q in r['dynamic']:
  out.append(f'D {name} {len(q)}')
  for p in q:out.append('Q '+' '.join(map(str,p)))
 for state,key in zip(r['after'],['RO','BO']):out.append(key+' '+state.removeprefix('!rnd,').replace(',',' '))
 out.append('E')
out.append(f'ENCOUNTEREND_COMPLETE {n}');Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
