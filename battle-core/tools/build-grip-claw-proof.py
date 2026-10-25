"""Encode actual Grip Claw observations; no expected behavior calculation."""
from pathlib import Path
import json,sys
rows=Path(sys.argv[1]).read_text().splitlines();h=rows[0].split();n=int(h[2]);assert h[:2]==['GRIPCLAW','1'] and len(rows)==n+3 and rows[-1]==f'GRIPCLAW_COMPLETE {n}'
out=[f'GRIPCLAW 1 {n}']
for i,line in enumerate(rows[1:-2]):
 r=json.loads(line);assert r['i']==i;out.append(f'C {i} {r["holder"]} {r["target"]} {r["mode"]} {int(r["result"]) if r["result"] is not None else -1}')
 out.append('W '+r['waveSeed']);out.append('TURN '+str(r['turn']))
 for p in r['beforeActors']:
  out.append('MON '+' '.join(map(str,[p['id'],p['species'],p['form'],p['level'],p['nature'],p['hp'],p['ability'],int(p['hasTrainer']),*p['ivs'],*p['stats']])));assert not p['tags']
 for tag in ['before','after']:
  if tag=='after':continue
  for key,prefix in [('global','R'),('battle','B')]:out.append(prefix+' '+r[tag][key].removeprefix('!rnd,').replace(',',' '))
 for key,prefix in [('input','I'),('output','O')]:
  for side,mods in enumerate(r[key]):
   out.append(f'{prefix} {side} {len(mods)}')
   for m in mods:out.append('M '+' '.join(map(str,[m['key'],m['arg'],m['owner'],m['stack'],m['virtual'],m['transferable'],m['max'],m['tier']])))
 out.append('LEVELS '+str(len(r['queue'])))
 for level,q in enumerate(r['queue']):
  out.append(f'L {level} {len(q)}')
  for p in q:out.append(f'P {p["name"]} {p["battler"]} '+(p['text'].encode().hex() if p['text'] else '-'))
 for p in r['actors']:out.append('A '+' '.join(map(str,[p['hp'],*p['stats'],len(p['tags']),*p['tags']])))
 for key,prefix in [('global','RO'),('battle','BO')]:out.append(prefix+' '+(r['after'][key] or r['before'][key]).removeprefix('!rnd,').replace(',',' '))
 out.append('E')
assert rows[-2].startswith('GRIPCLAW_TIERS ');out.append(rows[-2])
out.append(f'GRIPCLAW_COMPLETE {n}');Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
