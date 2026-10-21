"""Strict transport of observations from the pinned native scene helpers."""
import json,sys
from pathlib import Path
capture=json.loads(Path(sys.argv[1]).read_text())
assert capture['version'] in (1,2)
rows=capture['rows'];assert len(rows)==(3520 if capture['version']==1 else 3608)
out=[f'SCENETRANSFER {capture["version"]} {len(rows)}']
def item(m):
 return ' '.join(map(str,[m['key'],m['arg'],m['owner'],m['stack'],m['virtual'],m['transferable'],m['max'],-1]))
for i,r in enumerate(rows):
 assert r['i']==i
 out.append('C '+' '.join(map(str,[i,r['holder'],r['fromSide'],r['stockIndex'],r['mode'],r['quantity'],int(r['ignoreUpdate']),int(r['itemLost']),int(r['orphan']),int(r['result'])])))
 out += ['W '+r['waveSeed'],'TURN '+str(r['turn'])]
 for p in r['beforeActors']:
  out.append('MON '+' '.join(map(str,[p['id'],p['species'],p['form'],p['level'],p['nature'],p['hp'],p['ability'],0,*p['ivs'],*p['stats']])));assert not p['tags']
 for key,prefix in [('global','R'),('battle','B')]:out.append(prefix+' '+r['before'][key].removeprefix('!rnd,').replace(',',' '))
 out.append('S '+item(r['stockBefore']))
 for key,prefix in [('input','I'),('output','O')]:
  for side,mods in enumerate(r[key]):
   out.append(f'{prefix} {side} {len(mods)}')
   for m in mods:out.append('M '+item(m))
 out.append('LEVELS '+str(len(r['queue'])))
 for level,q in enumerate(r['queue']):
  out.append(f'L {level} {len(q)}')
  for p in q:out.append(f'P {p["name"]} {p["battler"]} '+(p['text'].encode().hex() if p['text'] else '-'))
 for p in r['actors']:out.append('A '+' '.join(map(str,[p['hp'],*p['stats'],len(p['tags']),*p['tags']])))
 for key,prefix in [('global','RO'),('battle','BO')]:out.append(prefix+' '+r['after'][key].removeprefix('!rnd,').replace(',',' '))
 out.append('E')
out.append(f'SCENETRANSFER_COMPLETE {len(rows)}')
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
