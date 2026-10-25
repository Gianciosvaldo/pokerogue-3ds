"""Transport actual pinned observations without computing expected behavior."""
import json,sys
from pathlib import Path
capture=json.loads(Path(sys.argv[1]).read_text())
version=capture['version'];assert version in (1,2)
count=4000 if version==1 else 4400
assert len(capture['rows'])==count
out=[f'PICKUP {version} {count}']
if version==2:
 assert len(capture['tiers'])==21
 for t in capture['tiers']:
  assert len(t['values'])==2 and all(isinstance(v,int) for v in t['values'])
  out.append('T '+t['key']+' '+' '.join(map(str,t['values'])))
def item(m):
 assert m['ref']>=0 and m['max']<=99
 return ' '.join(map(str,[m['ref'],m['key'],m['arg'],m['owner'],m['stack'],m['virtual'],m['transferable'],m['max'],m['cls']]))
def rng(key,value):return key+' '+value.removeprefix('!rnd,').replace(',',' ')
for i,r in enumerate(capture['rows']):
 assert r['i']==i
 if i<4000:assert r['scenario']==(i//4)%20 and r['mode']==i%4
 else:assert r['scenario']==1 and r['mode']==3 and r['shape']==(i-4000)%8
 out.append('C '+' '.join(map(str,[i,r['scenario'],r['mode'],int(r['victory']),int(r['simulated']),-1 if r['result'] is None else int(r['result'])])))
 out+=['V '+r['variant']['key']+' '+str(r['variant']['arg']),'W '+r['waveSeed'],'TURN '+str(r['turn'])]
 b=r['before'];a=r['after']
 if version==2:out.append('X '+str(r['shape'])+' '+str(r['wave']))
 for k,p in enumerate(b['actors']):
  assert len(p['ivs'])==6 and len(p['stats'])==6
  out.append('MON '+' '.join(map(str,[p['id'],p['species'],p['form'],p['level'],p['nature'],p['hp'],p['ability'],int(k<2),*p['ivs'],*p['stats']])))
  if version==2:out.append('F '+' '.join(map(str,[p['passive'],p['suppressed'],p['onField']])))
  out.append('H '+' '.join(map(str,[len(p['waveAbilities']),*p['waveAbilities'],len(p['summonAbilities']),*p['summonAbilities']])))
 for state,prefix in [(b,'I'),(a,'O')]:
  if version==2:out.append(prefix+'M '+str(state['money']))
  for side,mods in enumerate(state['inventory']):
   out.append(f'{prefix} {side} {len(mods)}')
   for m in mods:out.append('M '+item(m))
  out.append(prefix+'L '+' '.join(map(str,[len(state['loot']),*state['loot']])))
  out.append(prefix+'C '+str(state['cache']))
  out.append(rng(prefix+'R',state['global']));out.append(rng(prefix+'B',state['battle']))
 out.append('LEVELS '+str(len(r['queue'])))
 for level,q in enumerate(r['queue']):
  out.append(f'L {level} {len(q)}')
  for p in q:
   assert p['name'] in ['MessagePhase','ShowAbilityPhase','HideAbilityPhase']
   text=p['text'].encode().hex() if p['text'] else '-'
   out.append('P '+' '.join(map(str,[p['name'],p['battler'],-1 if p['passive'] is None else int(p['passive']),-1 if p['prompt'] is None else int(p['prompt']),text])))
 for k,p in enumerate(a['actors']):
  out.append('A '+' '.join(map(str,[k,p['hp'],*p['stats'],len(p['waveAbilities']),*p['waveAbilities'],len(p['summonAbilities']),*p['summonAbilities']])))
 out.append('E')
out.append(f'PICKUP_COMPLETE {count}')
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
