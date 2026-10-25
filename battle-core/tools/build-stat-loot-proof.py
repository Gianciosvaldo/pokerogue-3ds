"""Transport actual pinned observations without computing expected behavior."""
import json,sys
from pathlib import Path
capture=json.loads(Path(sys.argv[1]).read_text())
capture_version=capture['version'];assert capture_version in (1,2,3)
version=2;count=704 if capture_version==1 else 960
assert len(capture['rows'])==count
out=[f'STATLOOT {capture_version} {count}']
assert len(capture['tiers'])==8
for t in capture['tiers']:
 assert len(t['values'])==2
 out.append('T '+t['key']+' '+' '.join(str(v) if v is not None else '-1' for v in t['values']))
def item(m):
 assert m['ref']>=0 and m['max']<=99
 return ' '.join(map(str,[m['ref'],m['key'],m['arg'],m['owner'],m['stack'],m['virtual'],m['transferable'],m['max'],m['cls']]))
def rng(key,value):return key+' '+value.removeprefix('!rnd,').replace(',',' ')
for i,r in enumerate(capture['rows']):
 assert r['i']==i
 if i<640:assert r['scenario']==(i//4)%20 and r['mode']==i%4
 elif i<704:assert r['scenario']==1 and r['mode']==3 and r['shape']==(i-640)%8
 else:
  keys=['EVIOLITE','LIGHT_BALL','THICK_CLUB','METAL_POWDER','QUICK_POWDER','DEEP_SEA_SCALE','DEEP_SEA_TOOTH','GRIP_CLAW']
  assert r['scenario']==5 and r['mode']==(i-704)%2 and r['shape']==-1
  assert r['variant']['key']==keys[(i-704)//32] and r['cross']=={'key':keys[((i-704)//4)%8],'clone':bool(((i-704)//2)%2)}
 out.append('C '+' '.join(map(str,[i,r['scenario'],r['mode'],int(r['victory']),int(r['simulated']),-1 if r['result'] is None else int(r['result'])])))
 out+=['V '+r['variant']['key']+' '+str(r['variant']['arg']),'W '+r['waveSeed'],'TURN '+str(r['turn'])]
 b=r['before'];a=r['after']
 if version==2:out.append('X '+str(r['shape'])+' '+str(r['wave']))
 for k,p in enumerate(b['actors']):
  assert len(p['ivs'])==6 and len(p['stats'])==6
  out.append('MON '+' '.join(map(str,[p['id'],p['species'],p['form'],p['level'],p['nature'],p['hp'],p['ability'],int(k<2),*p['ivs'],*p['stats']])))
  if version==2:out.append('F '+' '.join(map(str,[p['passive'],p['suppressed'],p['onField']])))
  out.append('H '+' '.join(map(str,[len(p['waveAbilities']),*p['waveAbilities'],len(p['summonAbilities']),*p['summonAbilities']])))
  if capture_version>=3:
   assert len(p['effective'])==5
   out.append('S '+' '.join(map(str,[k,*p['effective']])))
 for state,prefix in [(b,'I'),(a,'O')]:
  if version==2:out.append(prefix+'M '+str(state['money']))
  for side,mods in enumerate(state['inventory']):
   out.append(f'{prefix} {side} {len(mods)}')
   for m in mods:
    out.append('M '+item(m))
    p=m['parameters'];out.append('D '+' '.join(map(str,[p['mask'],p['mult'],len(p['species']),*p['species']])))
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
 if capture_version>=3:
  for k,p in enumerate(a['actors']):
   assert len(p['effective'])==5
   out.append('S '+' '.join(map(str,[k,*p['effective']])))
 out.append('E')
out.append(f'STATLOOT_COMPLETE {count}')
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
if len(sys.argv)>3:
 reloads=[r for r in capture['reloads'] if 'clone' in r]
 assert len(reloads)==16
 saves=['STATLOOT_SAVE 1 16']
 keys=['EVIOLITE','LIGHT_BALL','THICK_CLUB','METAL_POWDER','QUICK_POWDER','DEEP_SEA_SCALE','DEEP_SEA_TOOTH','GRIP_CLAW']
 for i,r in enumerate(reloads):
  assert r['key']==keys[i//2] and int(r['clone'])==i%2
  saves.append(f'C {i} {r["key"]} {int(r["clone"])}')
  for prefix,state in [('I',r['before']),('O',r['after'])]:
   assert state and state['stack']==1 and state['virtual']==0
   saves.append(prefix+' '+' '.join(map(str,[state['cls'],state['mask'],state['mult'],len(state['species']),*state['species']])))
  saves.append('E')
 saves.append('STATLOOT_SAVE_COMPLETE 16')
 Path(sys.argv[3]).write_text('\n'.join(saves)+'\n')
