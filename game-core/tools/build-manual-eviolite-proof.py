#!/usr/bin/env python3
"""Transport pinned manual clone/stack-full observations; never calculate expected results."""
import json,sys,itertools,gzip
from pathlib import Path
def mfields(f):
 b={'id':f['id'],'species':f['species'],'form':f['form'],'fusion_species':f['fusion'],'fusion_form':f.get('fusionForm',0),'ability_index':f['abilityIndex'],'fusion_ability_index':f.get('fusionAbilityIndex',0),'gender':f['gender'],'fusion_gender':f.get('fusionGender',-1),'fusion_gender_known':'fusionGender'in f,'level':f['level'],'nature':f.get('effectiveNature',f['nature']),'hp':f['hp'],'friendship':f['friendship'],'passive_flag':f.get('passive',False),'shiny':f['shiny'],'variant':f['variant'],'tera_type':f['tera'],'custom_ability':f['customAbility'],'custom_passive':f['customPassive'],'fusion_custom_ability':-1,'lookup_ability_override':-1,'daily_boss_ability':-1,'ability_override':-1,'ability_lookup_known':True,'base_ability':f['ability'],'ability':f['ability'],'is_player':True,'n_moves':len(f['moves'])}
 m={'exp':f['exp'],'ability_index':f['abilityIndex'],'variant':f['variant'],'pokerus':f['pokerus'],'luck':f['luck'],'pokeball':f['ball'],'met_level':f['met'][0],'met_biome':f['met'][1],'met_species':f['met'][2],'met_wave':f['met'][3],'fusion_shiny':f.get('fusionShiny',False),'fusion_variant':f.get('fusionVariant',0),'fusion_luck':f.get('fusionLuck',0),'base_nature':f['nature'],'has_custom_nature':f['customNature']>=0,'n_used_tms':len(f['used'])}
 b.update(status=(f.get('status') or {}).get('effect',0),status_toxic_turns=(f.get('status') or {}).get('toxicTurnCount',0),status_sleep_turns=(f.get('status') or {}).get('sleepTurnsRemaining',-1),status_freeze_turns=(f.get('status') or {}).get('freezeTurnsRemaining',-1))
 return b,m

def unique(pairs):
 d={}
 for k,v in pairs:
  assert k not in d,('duplicate key',k)
  d[k]=v
 return d
raw=Path(sys.argv[1]).read_bytes();j=json.loads(gzip.decompress(raw) if raw[:2]==b'\x1f\x8b' else raw,object_pairs_hook=unique)
assert set(j)=={'version','locales','env','scope','rows'} and type(j['version'])==int and j['version']==1
assert set(j['locales'])=={'battle','modifier-type','pokeball'} and all(type(v)==str for v in j['locales'].values())
for v in j['locales'].values():json.loads(v,object_pairs_hook=unique)
assert j['env']==dict(seed='test',waveSeed='uftu',wave=1,biome=0,offset=20,weather=0,terrain=0)
required=list(itertools.product([1,3],[False,True],range(3),range(-1,5)))
assert len(j['rows'])==len(required)==72
for row,(species,clone,shape,tier)in zip(j['rows'],required):
 assert set(row)=={'species','sourceClone','targetShape','tier','can','result','before','after','messages','restored'}
 assert type(row['species'])==type(row['targetShape'])==type(row['tier'])==int
 assert (row['species'],row['sourceClone'],row['targetShape'],row['tier'])==(species,clone,shape,tier) and row['sourceClone'] is clone
 assert type(row['can'])==type(row['result'])==bool
 assert len(row['before']['party'])==2 and row['before']['party'][0]['species']==1 and row['before']['party'][1]['species']==species
 assert len(row['before']['stock'])==(2 if shape else 1) and row['before']['stock'][0]['tier']==tier
 assert len(row['messages'])<=1 and all(type(v)==str and v for v in row['messages'])
out=[]

def emit(s):out.append(s)
def lit(x):return str(int(x)) if isinstance(x,bool) else json.dumps(x)
def rng(s):
 import math
 f=s.split(',');assert len(f)==5 and f[0]=='!rnd'
 nums=list(map(float,f[1:]));assert all(math.isfinite(x) for x in nums) and nums[0]>=0 and all(0<=x<1 for x in nums[1:])
 return '{'+','.join(f[1:]+['0'])+'}'
fields=None;words=[];cache={}
def intern(v):
 t=tuple(v)
 if t not in cache:cache[t]=len(words);words.extend(t)
 return cache[t]
def mon(p):
 global fields
 allowed={'id','species','form','fusion','fusionForm','abilityIndex','fusionAbilityIndex','ability','gender','fusionGender','level','exp','nature','effectiveNature','customAbility','customPassive','customNature','ivs','hp','stats','status','friendship','passive','pokerus','ball','shiny','variant','luck','fusionShiny','fusionVariant','fusionLuck','fusionTera','tera','met','used','moves'}
 required=allowed-{'fusionForm','fusionAbilityIndex','fusionGender','status','passive','fusionShiny','fusionVariant','fusionLuck','fusionTera'}
 assert required<=set(p)<=allowed
 for key in ['id','species','form','fusion','fusionForm','abilityIndex','fusionAbilityIndex','ability','gender','fusionGender','level','exp','nature','effectiveNature','customAbility','customPassive','customNature','hp','friendship','ball','variant','luck','fusionVariant','fusionLuck','fusionTera','tera']:
  if key in p:assert type(p[key])==int and -1<=p[key]<=4294967295,(key,p[key])
 for key in ['passive','pokerus','shiny','fusionShiny']:
  if key in p:assert type(p[key])==bool
 if p.get('status') is not None:
  assert set(p['status'])<={'effect','toxicTurnCount','sleepTurnsRemaining','freezeTurnsRemaining'}
  assert all(type(v)==int and 0<=v<=32767 for v in p['status'].values())
 assert len(p['ivs'])==len(p['stats'])==6 and len(p['met'])==4
 assert all(type(x)==int and 0<=x<=31 for x in p['ivs'])
 assert all(type(x)==int and 0<x<=2147483647 for x in p['stats'])
 assert type(p['hp'])==int and 0<p['hp']<=p['stats'][0]
 assert 0<len(p['moves'])<=4 and len(p['used'])<=322 and len(set(p['used']))==len(p['used'])
 assert all(type(x)==int and 0<x<=65535 for x in p['used'])
 for move in p['moves']:
  assert set(move)=={'move','used','up','max'} and all(type(v)==int for v in move.values())
  assert 0<move['move']<=65535 and move['used']>=0 and 0<=move['up']<=3 and move['max']>=-1
 b,m=mfields(p);d={'b.'+k:v for k,v in b.items()};d.update(m)
 for key in ['ivs','stats']:
  for i,v in enumerate(p[key]):d[f'b.{key}[{i}]']=v
 for i in range(4):
  v=p['moves'][i]if i<len(p['moves'])else dict(move=0,used=0,up=0,max=-1)
  for k,n in [('moves','move'),('pp_used','used'),('pp_up','up'),('max_pp_override','max')]:d[f'b.{k}[{i}]']=v[n]
 if fields is None:fields=list(d)
 assert fields==list(d)
 # Optional undefined fusion metadata is not equal to zero in JS. Record presence.
 return intern(list(d.values())+[int('fusionForm'in p),int('fusionAbilityIndex'in p)]+p['used'])
dexmap={'seen_attr':'seenAttr','caught_attr':'caughtAttr','nature_attr':'natureAttr','seen_count':'seenCount','caught_count':'caughtCount','hatched_count':'hatchedCount','ribbon_flags':'ribbons'}
stmap={'ability_attr':'abilityAttr','passive_attr':'passiveAttr','value_reduction':'valueReduction','candy_count':'candyCount','friendship':'friendship','egg_moves':'eggMoves','classic_win_count':'classicWinCount'}
profilefields=[]
for x in j['rows'][0]['before']['profile']:
 i=x['id']
 profilefields += [(f'dex[{i}].{k}',i,'dex',v)for k,v in dexmap.items()]
 profilefields += [(f'dex[{i}].ivs[{n}]',i,'dex',n)for n in range(6)]
 profilefields += [(f'starters[{i}].{k}',i,'starter',v)for k,v in stmap.items()]
def profile(p):
 assert [x['id'] for x in p]==[1,2,151,290,291,292]
 for x in p:
  assert set(x)=={'id','dex','starter'} and set(x['dex'])=={'seenAttr','caughtAttr','natureAttr','seenCount','caughtCount','hatchedCount','ivs','ribbons'}
  assert len(x['dex']['ivs'])==6 and all(type(v)==int and 0<=v<=31 for v in x['dex']['ivs'])
  for k in ['seenAttr','caughtAttr','ribbons']:
   assert type(x['dex'][k])==str and x['dex'][k].isdigit() and 0<=int(x['dex'][k])<2**63
  if x['starter'] is not None:
   assert set(x['starter'])=={'moveset','eggMoves','candyCount','friendship','abilityAttr','passiveAttr','valueReduction','classicWinCount'} and x['starter']['moveset'] is None
 d={x['id']:x for x in p};v=[]
 for _,i,kind,key in profilefields:
  entry=d[i][kind]
  v.append(0 if entry is None else int(entry['ivs'][key]if type(key)==int else entry[key]))
 return intern(v)
frames=[]
def frame(f):
 assert set(f)=={'party','profile','stock','balls','global','battle'} and 1<=len(f['party'])<=6
 assert len(f['stock'])<=12 and set(f['balls'])==set(map(str,range(5)))
 assert all(type(v)==int and 0<=v<=65535 for v in f['balls'].values())
 for v in f['stock']:
  assert set(v)=={'key','owner','stack','virtual','locked','cls','tier'} and v['key']=='EVIOLITE'
  assert v['cls'] in ['TurnHealModifier','EvolutionStatBoosterModifier','StatBoosterModifier','PokemonIncrementingStatModifier','PokemonNatureWeightModifier']
  assert type(v['owner'])==int and 0<=v['owner']<=4294967295 and type(v['locked'])==bool
  assert type(v['stack'])==int and v['stack']==1 and v['virtual']==0
  assert type(v['tier'])==int and -1<=v['tier']<=4
 ids=[mon(p)for p in f['party']];pr=profile(f['profile'])
 stock=['{'+','.join([json.dumps(v['key']),str(v['owner'])+'u',str(v['stack']),str(v['virtual']),lit(v['locked']),lit(v['cls']=='StatBoosterModifier'),str(v['tier']+1)])+'}'for v in f['stock']]
 balls=[str(f['balls'][str(n)])for n in range(5)]
 frames.append('{'+str(len(ids))+',{'+','.join(map(str,ids))+'},'+str(pr)+','+str(len(stock))+',{'+','.join(stock)+'},{'+','.join(balls)+'},'+rng(f['global'])+','+rng(f['battle'])+'}')
 return len(frames)-1
cases=[]
for row in j['rows']:
 cases.append('{'+','.join(map(str,[frame(row['before']),frame(row['after']),int(row['result']),frame(row['restored']),len(row['messages'])]))+','+(json.dumps(row['messages'][0],ensure_ascii=False)if row['messages']else 'NULL')+'}')
emit('/* Actual pinned native observations; compact field vectors, no calculated outcomes. */')
emit('static const int64_t words[]={'+','.join(map(lambda x:str(int(x)),words))+'};')
emit('typedef struct {int n;unsigned mons[6],profile;int nstock;struct {const char *key;uint32_t owner;int stack,virtual_stack,locked,eviolite_base,tier;}stock[12];int balls[5];pr_rng global,battle;} frame;')
emit('static const frame frames[]={'+',\n'.join(frames)+'};')
emit('static const struct {int before,after,result,restored,nmessages;const char*message;} cases[]={'+',\n'.join(cases)+'};')
emit('static const struct {const char *name,*json;} native_locales[]={'+','.join('{'+json.dumps(k)+','+json.dumps(v,ensure_ascii=False)+'}'for k,v in j['locales'].items())+'};')
emit('static void mon_io(gc_mon*m,unsigned offset,bool write){const int64_t*p=words+offset;')
never={'b.base_ability','b.ability_lookup_known','b.is_player','b.daily_boss_ability','b.lookup_ability_override','b.ability_override'}
for k,name in enumerate(fields):
 cond='true'
 if name in never:cond='false'
 if name=='b.fusion_form':cond=f'(m->b.fusion_species||p[{len(fields)}])'
 if name=='b.fusion_ability_index':cond=f'(m->b.fusion_species||p[{len(fields)+1}])'
 if name=='base_nature':cond='m->has_custom_nature'
 emit(f'if(write)m->{name}=p[{k}];else if({cond})EQ(m->{name},p[{k}]);')
emit(f'for(int i=0;i<m->n_used_tms;i++){{if(write)m->used_tms[i]=p[{len(fields)+2}+i];else EQ(m->used_tms[i],p[{len(fields)+2}+i]);}}')
emit('if(!write)EQ(pr_get_ability(&m->b,false),m->b.ability);}')
emit('static void profile_io(gc_system*s,unsigned offset,bool write){const int64_t*p=words+offset;')
for k,(name,*_)in enumerate(profilefields):emit(f'if(write)s->{name}=p[{k}];else EQ(s->{name},p[{k}]);')
emit('}')
emit('static const char *seed='+json.dumps(j['env']['seed'])+',*wave_seed='+json.dumps(j['env']['waveSeed'])+';')
emit('static const int env[]={'+','.join(map(str,[j['env'][k]for k in ['wave','biome','offset','weather','terrain']]))+'};')
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
print(len(cases),'cases',len(frames),'frames',len(words),'interned words')
