"""Encode actual native switch-branch observations without modeling outcomes."""
from pathlib import Path
import json,sys
x=json.loads(Path(sys.argv[1]).read_text());assert x['version'] in (1,2) and len(x['rows'])==256
out=[f'BATONSWITCH {x["version"]} 256']
for i,r in enumerate(x['rows']):
 assert r['i']==i
 out += [f'C {i} {r["side"]} {r["field"]} {r["kind"]}'+(' '+str(-1 if r['can'] is None else int(r['can'])) if x['version']==2 else ''),'W baton-switch','TURN 1']
 for p in r['beforeActors']:
  assert not p['tags']
  out.append('MON '+' '.join(map(str,[p['id'],p['species'],p['form'],p['level'],p['nature'],p['hp'],p['ability'],0,*p['ivs'],*p['stats']])))
 for key,prefix in [('global','R'),('battle','B')]:out.append(prefix+' '+r['before'][key].removeprefix('!rnd,').replace(',',' '))
 for key,prefix in [('input','I'),('output','O')]:
  for side,mods in enumerate(r[key]):
   out.append(f'{prefix} {side} {len(mods)}')
   for m in mods:out.append('M '+' '.join(map(str,[m['key'],m['arg'],m['owner'],m['stack'],m['virtual'],m['transferable'],m['max'],m['tier']])))
 for p in r['actors']:out.append('A '+' '.join(map(str,[p['hp'],*p['stats'],len(p['tags']),*p['tags']])))
 for key,prefix in [('global','RO'),('battle','BO')]:out.append(prefix+' '+r['after'][key].removeprefix('!rnd,').replace(',',' '))
 out.append('E')
if x['version']==2:out.append('BATON_TIERS '+' '.join(map(str,x['tiers'])))
out.append('BATONSWITCH_COMPLETE 256');Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
