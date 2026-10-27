#!/usr/bin/env python3
"""Transport native constructor frames; do not derive fusion outputs."""
from pathlib import Path
import itertools,json,math,sys
def unique(pairs):
 out={}
 for k,v in pairs:assert k not in out,'duplicate key';out[k]=v
 return out
j=json.loads(Path(sys.argv[1]).read_text(),object_pairs_hook=unique)
assert set(j)=={'version','scope','pokemon','rows'}and type(j['version'])==int and j['version']==1
assert j['scope']=='native constructor with fixed no-RNG moveset; no enemy items/boss/complete encounter claim'
assert type(j['pokemon'])==dict and len(j['rows'])==572
required=[(*v,0)for v in itertools.product([1,2],[1,132,151,81],[1,100,70000],[0,1,2])]
required += [(2,[1,133,81,151][i%4],100 if i%2 else 70000,i,4)for i in range(500)]
def integer(v,lo,hi):assert type(v)==int and lo<=v<=hi;return str(v)
def array(v):return '{'+','.join(map(str,v))+'}'
def rng(v):
 assert type(v)==str;parts=v.split(',');assert len(parts)==5 and parts[0]=='!rnd'
 nums=list(map(float,parts[1:]));assert all(math.isfinite(x)for x in nums)and nums[0]>=0 and all(0<=x<1 for x in nums[1:])
 return array(parts[1:]+['0'])
keys=['id','species','form','gender','nature','abilityIndex','ability','fusion','fusionForm','fusionGender','fusionAbilityIndex','tera','hp','friendship','variant','luck']
lines=['/* Actual native frames at an explicit fixed-moveset constructor boundary. */',
 'static const char source_pokemon_json[]='+json.dumps(json.dumps(j['pokemon'],ensure_ascii=False,separators=(',',':')))+';',
 'typedef struct{uint32_t id;int '+','.join(keys[1:])+';int ivs[6],stats[6],ntypes,types[2];double exp;const char*name;}native_enemy_frame;',
 'static const struct{int mode,species,level,sample,charms;pr_rng post_rng;native_enemy_frame mon;}native_enemy_cases[572]={']
forms=set();indices=set();genders=set();fused=0
for row,req in zip(j['rows'],required):
 assert set(row)=={'mode','species','level','sample','charms','before','after','mon'}
 for k in ['mode','species','level','sample','charms']:integer(row[k],0,2147483647)
 assert tuple(row[k]for k in ['mode','species','level','sample','charms'])==req
 rng(row['before']);post=rng(row['after']);m=row['mon']
 assert set(m)==set(keys+['ivs','stats','exp','shiny','fusionShiny','fusionVariant','fusionLuck','name','types'])
 for k in keys:integer(m[k],-1 if k in ['gender','fusionGender']else 0,4294967295 if k=='id'else 2147483647)
 for k in ['species','fusion','ability']:integer(m[k],0,65535)
 for k in ['form','fusionForm','friendship']:integer(m[k],0,255)
 assert m['species']==row['species']and m['gender']in[-1,0,1]and m['fusionGender']in[-1,0,1]
 assert m['abilityIndex']in[0,1,2]and m['fusionAbilityIndex']in[0,1,2]and 0<=m['nature']<25
 assert type(m['shiny'])==type(m['fusionShiny'])==bool and not m['shiny']and not m['fusionShiny']
 assert m['variant']==m['luck']==m['fusionVariant']==m['fusionLuck']==0 # shiny-locked input scope
 assert type(m['exp'])in[int,float]and math.isfinite(m['exp'])and 0<=m['exp']<=9007199254740991
 assert type(m['name'])==str and m['name']and len(m['name'])<200
 assert len(m['ivs'])==len(m['stats'])==6
 for v in m['ivs']:integer(v,0,31)
 for v in m['stats']:integer(v,1,2147483647)
 assert m['hp']==m['stats'][0]and 1<=len(m['types'])<=2
 for v in m['types']:integer(v,0,18)
 assert bool(m['fusion'])==(row['mode']==2 and row['level']>1)
 if m['fusion']:fused+=1;forms.add(m['fusionForm']);indices.add(m['fusionAbilityIndex']);genders.add(m['fusionGender'])
 frame='{'+','.join(str(m[k])for k in keys)+','+array(m['ivs'])+','+array(m['stats'])+','+str(len(m['types']))+','+array(m['types'])+','+str(m['exp'])+','+json.dumps(m['name'])+'}'
 lines.append('{'+','.join(map(str,req))+','+post+','+frame+'},')
assert fused==524 and {0,1,2}<=indices and {-1,0,1}==genders and max(forms)>0
lines.append('};');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
