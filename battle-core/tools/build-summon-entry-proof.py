"""Encode actual source SummonPhase observations without calculating outcomes."""
from pathlib import Path
import json,sys
rows=Path(sys.argv[1]).read_text().splitlines();h=rows[0].split();assert h[:2]==['SUMMON_ENTRY','3'];n=int(h[2]);assert len(rows)==n+2 and rows[-1]==f'SUMMON_ENTRY_COMPLETE {n}'
out=[f'SUMMONENTRY 3 {n}']
def actor(p):return [p[k] for k in ['id','species','form','level','nature','hp','ability','onField','switching','turnCount','waveTurnCount','summoned','shiny','hitCount']]+p['ivs']+p['stats']
for i,line in enumerate(rows[1:-1]):
 r=json.loads(line);assert r['i']==i;out.append(f'C {i} {int(r["double"])} {r["side"]} {r["field"]}')
 for name in r['trainerNames']:out.append('NAME '+(name.encode().hex() if name else '-'))
 for p in r['actors']:out.append('MON '+' '.join(map(str,actor(p))))
 for state,key in zip([r['before']['global'],r['before']['battle']],['R','B']):out.append(key+' '+state.removeprefix('!rnd,').replace(',',' '))
 for p in r['output']:out.append('OUT '+' '.join(map(str,actor(p))))
 for side,ids in enumerate(r['party']):out.append('PARTY '+str(side)+' '+' '.join(map(str,ids)))
 out.append('FIELD '+' '.join('-1' if p is None else str(p) for p in r['fieldIds']))
 out.append('TEXTS '+str(len(r['texts'])))
 for text in r['texts']:out.append('TEXT '+(text.encode().hex() if text else '-'))
 out.append('LEVELS '+str(len(r['queue'])))
 for l,q in enumerate(r['queue']):
  out.append(f'L {l} {len(q)}')
  for p in q:out.append('Q '+' '.join(map(str,p)))
 out.append('DYNTYPES '+str(len(r['dynamic'])))
 for name,q in r['dynamic']:
  out.append(f'D {name} {len(q)}')
  for p in q:out.append('Q '+' '.join(map(str,p)))
 for state,key in zip([r['after']['global'],r['after']['battle']],['RO','BO']):out.append(key+' '+state.removeprefix('!rnd,').replace(',',' '))
 out.append('E')
out.append(f'SUMMONENTRY_COMPLETE {n}');Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
