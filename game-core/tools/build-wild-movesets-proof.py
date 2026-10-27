#!/usr/bin/env python3
"""Transport native pools/moves/RNG. Retain all original adapter input seeds."""
from pathlib import Path
import json,math,itertools,sys,hashlib
def unique(pairs):
 d={}
 for k,v in pairs:assert k not in d,'duplicate key';d[k]=v
 return d
assert len(sys.argv)==5,'native.json retained-native.json legacy-inputs.json output.inc'
def read(n):return json.loads(Path(sys.argv[n]).read_text(),object_pairs_hook=unique)
def integer(v,lo,hi):assert type(v)==int and lo<=v<=hi

def a(v):return '{'+','.join(map(str,v))+'}'
def rng(s):
 v=s.split(',');assert len(v)==5 and v[0]=='!rnd';n=list(map(float,v[1:]));assert all(math.isfinite(x)for x in n)and n[0]>=0 and all(0<=x<1 for x in n[1:]);return a(v[1:]+['0'])
def row(r,kind,index):
 g=r['generation']if kind==2 else r;m=g['input'];assert not m['boss']and not m['passive']and not m['trainer']if kind==2 else not m['boss']and not m['passive']
 assert len(g['pool'])<=64 and len(g['moves'])<=4 and len(m['ivs'])==6 and len(m['stats'])==6
 assert all(type(x)==int and 0<=x<=31 for x in m['ivs'])and all(type(x)==int and x>0 for x in m['stats'])
 assert m['species']==r.get('species',m['species']);assert m['fusion']>=0 and m['fusionGender']in[-1,0,1]
 for k in ['id','species','form','level','nature','gender','abilityIndex','ability','fusion','fusionForm','fusionGender','fusionAbilityIndex','tera']:
  integer(m[k],-1 if k in ['gender','fusionGender']else 0,4294967295 if k=='id'else 2147483647)
 assert m['gender']in[-1,0,1]and m['fusionGender']in[-1,0,1]and 0<=m['nature']<25
 assert m['abilityIndex']in[0,1,2]and m['fusionAbilityIndex']in[0,1,2]
 assert 0<m['species']<65536 and 0<=m['fusion']<65536 and 0<=m['form']<256 and 0<=m['fusionForm']<256 and 0<=m['tera']<=18
 fields=dict(id=m['id'],form=m['form'],gender=m['gender'],nature=m['nature'],ability_index=m['abilityIndex'],fusion_species=m['fusion']or-1,fusion_form=m['fusionForm'],fusion_gender=m['fusionGender'],fusion_ability_index=m['fusionAbilityIndex'],tera_type=m['tera'],friendship=-1)
 for k,v in fields.items():assert type(v)==int and -(2**31)<=v<2**32
 fields=','.join('.'+k+'='+str(v)for k,v in fields.items())+',.ivs='+a(m['ivs'])
 for t in g['pool']:assert len(t)==3 and all(type(x)==int for x in t)and -1<=t[0]<=100000 and 0<t[1]<2000 and 0<=t[2]<=3
 for v in g['moves']:assert set(v)=={'move','used','up','max'}and all(type(x)==int for x in v.values())and 0<v['move']<2000 and v['used']==0 and -4<=v['up']<=3 and v['max']==-1
 return '{'+','.join(map(str,[kind,index,r.get('seed',0),r.get('mode',0),m['species'],m['level'],m['ability'],len(g['moves']),len(g['pool'])]))+',{'+fields+'},'+a(m['stats'])+','+rng(g['before'])+','+rng(g['after'])+',{'+','.join(a(t)for t in g['pool'])+'},{'+','.join(a([v[k]for k in ['move','used','up','max']])for v in g['moves'])+'}}'
x=read(1);y=read(2);legacy=read(3)
assert set(x)=={'version','scope','weather','terrain','double','pokemon','rows'}
assert set(y)=={'version','scope','weather','terrain','double','rows'}
assert type(x['version'])==type(y['version'])==int and x['version']==y['version']==1
assert x['scope']=='actual native constructor moveset generation; wild non-boss shiny-locked inputs'
assert y['scope']=='native generator from retained adapter/constructor snapshots; not a native full encounter capture'
assert hashlib.sha256(Path(sys.argv[3]).read_bytes()).hexdigest()=="1fbbbc409268b031c4c2fdbc878aeb86f5d05be227c8cd39578f1c6b566f1256",'original retained inputs changed'
required=[(*v,0)for v in itertools.product([1,2],[1,132,151,81],[1,100,70000],[0,1,2])]
required += [(2,[1,133,81,151][i%4],100 if i%2 else 70000,i,4)for i in range(500)]
required += [(mode,species,65536,sample,0)for mode in [1,2]for species in [1,133,81,151]for sample in [0,1]]
assert [tuple(r[k]for k in ['mode','species','level','sample','charms'])for r in x['rows']]==required
for r in x['rows']:
 assert set(r)=={'mode','species','level','sample','charms','before','generation','after','mon'}
 for k in ['mode','species','level','sample','charms']:integer(r[k],0,2147483647)
 assert set(r['generation'])=={'before','after','input','pool','moves'}
 assert r['generation']['after']==r['after']
 m=r['generation']['input'];assert set(m)=={'id','species','form','level','gender','nature','abilityIndex','ability','fusion','fusionForm','fusionGender','fusionAbilityIndex','tera','ivs','stats','hp','boss','passive','trainer','types'}
 assert all(type(m[k])==bool and not m[k]for k in ['boss','passive','trainer'])
 assert m['hp']==m['stats'][0]and bool(m['fusion'])==(r['mode']==2 and r['level']>1)
 assert m['level']==r['level']and m['stats']==r['mon']['stats']
 rng(r['before'])
for r in y['rows']:
 assert set(r)=={'kind','index','seed','legacy','before','input','pool','after','moves'}
 for k in ['kind','index','seed','legacy']:integer(r[k],0,2147483647)

assert len(x['rows'])==588 and len(y['rows'])==1025 and len(legacy)==2900
assert [r['index']for r in legacy if r['kind']==0]==list(range(2100))
assert [r['index']for r in legacy if r['kind']==1]==list(range(800))
assert [(r['kind'],r['index'])for r in y['rows']]==[(r['kind'],r['index'])for r in legacy if r['kind']==1 or(not r['legacy']and not r['input']['boss'])]
for r in y['rows']:
 b=legacy[r['index']+(2100 if r['kind']else 0)];assert all(r[k]==v for k,v in b.items())
assert all(x[k]==v and y[k]==v for k,v in dict(weather=0,terrain=0,double=False).items())
lines=['/* Native ordered pools, PokemonMove metadata and global RNG states; never C-derived outputs. */','typedef struct{int kind,index,seed,mode,species,level,ability,n,np;pr_enemy_gen g;int stats[6];pr_rng before,after;int pool[64][3],moves[4][4];} native_move_case;', 'static const native_move_case native_fused_moves[]={']
lines += [row(r,2,i)+','for i,r in enumerate(x['rows'])];lines+=['};','static const native_move_case native_retained_moves[]={']
lines += [row(r,r['kind'],r['index'])+','for r in y['rows']];lines+=['};','static const unsigned native_adapter_seeds[2100]='+a([r['seed']for r in legacy[:2100]])+';']
Path(sys.argv[4]).write_text('\n'.join(lines)+'\n')
