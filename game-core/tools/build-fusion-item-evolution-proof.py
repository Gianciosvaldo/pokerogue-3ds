#!/usr/bin/env python3
"""Transport native item selection, queued target and evolution mutation."""
from pathlib import Path
import itertools,json,math,sys
assert len(sys.argv)==3,'native.json output.inc'
def unique(pairs):
 d={}
 for k,v in pairs:assert k not in d,'duplicate key';d[k]=v
 return d
def integer(v,lo,hi):assert type(v)==int and lo<=v<=hi;return v
def arr(v):return '{'+','.join(map(str,v))+'}'
def rng(s):
 assert type(s)==str;f=s.split(',');assert len(f)==5 and f[0]=='!rnd';v=list(map(float,f[1:]));assert all(math.isfinite(x)for x in v)and v[0]>=0 and all(0<=x<1 for x in v[1:]);return arr(f[1:]+['0'])
x=json.loads(Path(sys.argv[1]).read_text(),object_pairs_hook=unique)
assert set(x)=={'version','scope','biome','wave','offset','weather','terrain','seed','pokemon','pairs','items','rows'}and type(x['version'])==int and x['version']==1
assert x['scope']=='native evolution-item selectFilter/apply queue and Pokemon.evolve; raw compatible fusion snapshots'
pairs=[[151,58],[58,133],[25,58],[151,361],[361,151],[151,281],[281,58],[151,133],[133,58],[151,37],[151,64],[151,123]];items=[5,7,6,10,9,1,51]
assert x['pairs']==pairs and x['items']==items and len(x['rows'])==2016 and type(x['seed'])==str and type(x['pokemon'])==dict
for pair in x['pairs']:
 for v in pair:integer(v,1,65535)
for v in x['items']:integer(v,1,65535)
for k in ['biome','wave','offset','weather','terrain']:integer(x[k],0,65535)
assert x['offset']==5 and x['weather']==x['terrain']==0
keys=['id','species','form','fusion','fusionForm','gender','fusionGender','abilityIndex','fusionAbilityIndex','ability','level','nature','hp','friendship']
def frame(d):
 assert set(d)==set(keys+['ivs','stats','pause','name','moves','stock','global','battle'])
 for k in keys:integer(d[k],-1 if k in ['gender','fusionGender']else 0,4294967295 if k=='id'else 2147483647)
 assert d['id']==101 and d['gender']in[-1,0,1]and d['fusionGender']in[-1,0,1]and type(d['pause'])==bool and type(d['name'])==str
 assert 0<d['species']<65536 and 0<d['fusion']<65536 and d['form']==d['fusionForm']==0 and 0<d['level']<=70000 and d['abilityIndex']in[0,1,2]and d['fusionAbilityIndex']in[0,1,2]and 0<=d['nature']<25 and 0<=d['friendship']<=255
 assert len(d['ivs'])==len(d['stats'])==6 and d['stock']==[]
 for v in d['ivs']:integer(v,0,31)
 for v in d['stats']:integer(v,1,2147483647)
 assert 0<=d['hp']<=d['stats'][0] and len(d['moves'])<=4
 moves=[]
 for m in d['moves']:
  assert set(m)=={'move','used','up','max'};integer(m['move'],1,1999);integer(m['used'],0,255);integer(m['up'],0,3);integer(m['max'],-1,-1);moves.append(arr([m[k]for k in ['move','used','up','max']]))
 return '{'+','.join(map(str,[d[k]for k in keys]))+','+str(int(d['pause']))+','+arr(d['ivs'])+','+arr(d['stats'])+','+json.dumps(d['name'])+','+str(len(moves))+',{'+','.join(moves)+'},{'+rng(d['global'])+','+rng(d['battle'])+'}}'
lines=['/* Native selected item evolution, queued half and actual evolve frames. */','static const int item_evo_env[]='+arr([x[k]for k in ['biome','wave','offset','weather','terrain']])+';','static const char *item_evo_seed='+json.dumps(x['seed'])+';','static const char item_evo_pokemon_json[]='+json.dumps(json.dumps(x['pokemon'],ensure_ascii=False,separators=(',',':')))+';','typedef struct{uint32_t id;int species,form,fusion,fusionForm,gender,fusionGender,abilityIndex,fusionAbilityIndex,ability,level,nature,hp,friendship,pause,ivs[6],stats[6];const char*name;int nmoves,moves[4][4];pr_rng rng[2];}native_item_evo_frame;','typedef struct{int item;native_item_evo_frame before,after;int filter,accepted,fusion,species,level,lastLevel;const char*preForm,*evoForm;}native_item_evo_case; static const native_item_evo_case native_item_evo_cases[]={']
required=itertools.product(pairs,items,[0,1],[1,50,70000],[0,2],[False,True])
for r,(pair,item,gender,level,index,pause)in zip(x['rows'],required):
 assert set(r)=={'pair','item','gender','level','abilityIndex','pause','before','filter','accepted','selected','after'}
 assert (r['pair'],r['item'],r['gender'],r['level'],r['abilityIndex'],r['pause'])==(pair,item,gender,level,index,pause)
 for k in ['item','gender','level','abilityIndex']:integer(r[k],0,70000)
 assert type(r['pause'])==type(r['accepted'])==bool and(r['filter']is None or type(r['filter'])==str)
 b=r['before'];assert [b['species'],b['fusion']]==pair and b['level']==level and b['fusionAbilityIndex']==index and b['pause']==pause and b['fusionGender']==(-1 if pair[1]==151 else gender)
 assert b['gender']==(-1 if pair[0]==151 else 1-gender)and b['abilityIndex']==0 and b['ivs']==[15]*6 and b['nature']==0 and b['friendship']==220
 selected=r['selected'];assert bool(selected)==r['accepted']
 tail='0,0,0,0,NULL,NULL'
 if selected is not None:
  assert set(selected)=={'species','level','item','preForm','evoForm','fusion','lastLevel'}and type(selected['fusion'])==bool
  for k in ['species','level','item','lastLevel']:integer(selected[k],0,70000)
  assert selected['item']==item and selected['lastLevel']==level-1
  for k in ['preForm','evoForm']:assert selected[k]is None or type(selected[k])==str
  tail=','.join(map(str,[int(selected['fusion']),selected['species'],selected['level'],selected['lastLevel']]))+','+','.join('NULL'if selected[k]is None else json.dumps(selected[k])for k in ['preForm','evoForm'])
 lines.append('{'+str(item)+','+frame(b)+','+frame(r['after'])+','+str(int(r['filter']is None))+','+str(int(r['accepted']))+','+tail+'},')
lines+=['};'];Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
