#!/usr/bin/env python3
"""Transport actual getEvolution/evolve observations; do not calculate outcomes."""
from pathlib import Path
import itertools,json,math,sys
def unique(pairs):
 d={}
 for k,v in pairs:assert k not in d,'duplicate key';d[k]=v
 return d
j=json.loads(Path(sys.argv[1]).read_text(),object_pairs_hook=unique)
assert set(j)=={'version','biome','wave','offsets','weather','terrain','seed','pokemon','rows'}and type(j['version'])==int and j['version']==1
assert j['offsets']==[20,5]
for k in ['biome','wave','weather','terrain']:assert type(j[k])==int and 0<=j[k]<=65535
assert len(j['rows'])==1440 and type(j['pokemon'])==dict and type(j['seed'])==str
pairs=[(151,999),(1,999),(151,415),(151,757),(151,1),(151,175),(151,133),(151,705)]
required=[(pair,level,gender,index,track,form,offset)for offset in [20,5]for form in [0,1]for pair in pairs if not form or pair[1]==999 for level,gender,index,track in itertools.product([5,16,21,33,50],[-1,0,1],[0,2],[0,10])]
required += [((151,10),level,gender,index,track,0,offset)for offset in [20,5]for level,gender,index,track in itertools.product([5,16,21,33,50],[-1,0,1],[0,2],[0,10])]
required += [((151,328),level,gender,index,track,0,offset)for offset in [20,5]for level,gender,index,track in itertools.product([5,16,21,33,50],[-1,0,1],[0,2],[0,10])]
def integer(x,lo,hi):assert type(x)==int and lo<=x<=hi;return str(x)
def arr(x):return '{'+','.join(map(str,x))+'}'
def rng(s):
 f=s.split(',');assert len(f)==5 and f[0]=='!rnd';v=list(map(float,f[1:]));assert all(math.isfinite(x)for x in v)and v[0]>=0 and all(0<=x<1 for x in v[1:]);return arr(f[1:]+['0'])
keys=['id','species','form','fusion','fusionForm','gender','fusionGender','abilityIndex','fusionAbilityIndex','ability','level','nature','hp','friendship']
def frame(d):
 assert set(d)==set(keys+['ivs','stats','pause','name','stock','global','battle'])
 for k in keys:integer(d[k],-1 if k in ['gender','fusionGender']else 0,4294967295 if k=='id'else 2147483647)
 assert d['id']==101 and d['gender']in[-1,0,1]and d['fusionGender']in[-1,0,1]and type(d['pause'])==bool and type(d['name'])==str
 assert len(d['ivs'])==len(d['stats'])==6;[integer(x,0,31)for x in d['ivs']];[integer(x,1,2147483647)for x in d['stats']]
 assert len(d['stock'])<=1
 stock=[]
 for it in d['stock']:
  assert set(it)=={'key','owner','stack','virtual'}and it['key']=='EVOLUTION_TRACKER_GIMMIGHOUL'and it['owner']==101 and it['stack']==10 and it['virtual']==0
  stock.append('{101,10,0}')
 return '{'+','.join(str(d[k])for k in keys)+','+str(int(d['pause']))+','+arr(d['ivs'])+','+arr(d['stats'])+','+json.dumps(d['name'])+','+str(len(stock))+',{'+','.join(stock)+'},{'+rng(d['global'])+','+rng(d['battle'])+'}}'
lines=['/* Source outputs are observations, not C-derived expected evolutions. */','static const int source_env[]='+arr([j['biome'],j['wave'],0,j['weather'],j['terrain']])+';','static const char *source_seed='+json.dumps(j['seed'])+';','static const char source_pokemon_json[]='+json.dumps(json.dumps(j['pokemon'],ensure_ascii=False,separators=(',',':')))+';','typedef struct{uint32_t id;int species,form,fusion,fusionForm,gender,fusionGender,abilityIndex,fusionAbilityIndex,ability,level,nature,hp,friendship,pause,ivs[6],stats[6];const char*name;int nstock;struct{uint32_t owner;int stack,virtual_stack;}stock[1];pr_rng rng[2];}native_evo_frame;','static const struct{int offset;native_evo_frame before,after;bool selected,fusion;int species,level,item;const char*pre_form,*evo_form;}native_evo_cases[1440]={']
for row,req in zip(j['rows'],required):
 assert set(row)=={'offset','before','after','selected'};b=row['before'];pair,level,gender,index,track,form,offset=req
 assert row['offset']==offset
 assert (b['species'],b['fusion'])==pair and b['level']==level and b['fusionGender']==gender and b['fusionAbilityIndex']==index and len(b['stock'])==bool(track)and b['fusionForm']==form
 e=row['selected'];tail='false,false,0,0,0,NULL,NULL'
 if e is not None:
  assert set(e)=={'species','level','item','preForm','evoForm','fusion'}and type(e['fusion'])==bool
  for k in ['species','level','item']:integer(e[k],0,65535)
  for k in ['preForm','evoForm']:assert e[k]is None or type(e[k])==str
  tail='true,'+str(e['fusion']).lower()+','+','.join(str(e[k])for k in ['species','level','item'])+','+','.join('NULL'if e[k]is None else json.dumps(e[k])for k in ['preForm','evoForm'])
 lines.append('{'+str(row['offset'])+','+frame(b)+','+frame(row['after'])+','+tail+'},')
lines.append('};');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
