#!/usr/bin/env python3
"""Transport native unfused boss generator observations without deriving outputs."""
from pathlib import Path
import json,sys,itertools,hashlib,math
assert len(sys.argv)==5,'constructors.json retained.json original-inputs.json output.inc'
def unique(pairs):
 d={}
 for k,v in pairs:assert k not in d,'duplicate key';d[k]=v
 return d
def read(n):return json.loads(Path(sys.argv[n]).read_text(),object_pairs_hook=unique)
def integer(v,lo,hi):assert type(v)==int and lo<=v<=hi;return v
def a(v):return '{'+','.join(map(str,v))+'}'
def rng(s):
 assert type(s)==str;v=s.split(',');assert len(v)==5 and v[0]=='!rnd';n=list(map(float,v[1:]));assert all(math.isfinite(x)for x in n)and n[0]>=0 and all(0<=x<1 for x in n[1:]);return a(v[1:]+['0'])
x=read(1);y=read(2);baseline=read(3)
assert set(x)=={'version','scope','rows'}and type(x['version'])==int and x['version']==1
assert x['scope']=='actual native unfused boss constructor generator boundary; finish RNG recorded separately'
assert set(y)=={'version','scope','weather','terrain','double','rows'}and type(y['version'])==int and y['version']==1
assert y['scope']=='native unfused boss generator from retained rejected snapshots; not full encounter'
assert type(y['double'])==bool and not y['double'];integer(y['weather'],0,0);integer(y['terrain'],0,0)
assert len(x['rows'])==1016 and len(y['rows'])==750 and len(baseline)==2900
# Bind the complete original corpus, including all assertion-count input seeds.
assert hashlib.sha256(Path(sys.argv[3]).read_bytes()).hexdigest()=='1fbbbc409268b031c4c2fdbc878aeb86f5d05be227c8cd39578f1c6b566f1256'
required=[(s,l,d,0,0)for s in [1,10,13,19,25,129,132,133,151,212,230,235,382,383,479,555,658,798]for l in [1,50,70,100,65536,70000]for d in [False,True]]
required += [(s,l,d,w,t)for s in [230,382,383,555]for l in [100,70000]for w in range(10)for t in range(5)for d in [False,True]]
assert [tuple(r[k]for k in ['species','level','double','weather','terrain'])for r in x['rows']]==required
selected=[r for r in baseline if r['kind']==0 and not r['legacy']and r['input']['boss']]
assert len(selected)==750
for r,b in zip(y['rows'],selected):
 assert set(r)=={'kind','index','seed','legacy','before','input','pool','after','moves'}
 assert all(r[k]==v for k,v in b.items())
def encode(row,constructor):
 if constructor:
  assert set(row)=={'mode','species','level','sample','double','weather','terrain','before','generation','after'}
  integer(row['mode'],0,0);integer(row['sample'],0,1015);assert type(row['double'])==bool
  g=row['generation'];assert set(g)=={'before','after','input','pool','moves'}
  rng(row['before']);rng(row['after']) # Whole-constructor finish state is deliberately separate.
  m=g['input'];assert set(m)=={'id','species','form','level','gender','nature','abilityIndex','ability','fusion','fusionForm','fusionGender','fusionAbilityIndex','tera','ivs','stats','hp','boss','bossSegments','passive','passiveAbility','trainer','types'}
  assert all(type(m[k])==bool for k in ['boss','passive','trainer'])and m['boss']and m['passive']and not m['trainer']
  integer(m['hp'],1,2147483647)
  assert m['species']==row['species']and m['level']==row['level']and m['hp']==m['stats'][0]
  segments=integer(m['bossSegments'],2,255);passive=integer(m['passiveAbility'],1,65535)
  assert 1<=len(m['types'])<=2
  for t in m['types']:integer(t,0,18)
  index=row['sample'];seed=0;weather=integer(row['weather'],0,9);terrain=integer(row['terrain'],0,4);double=int(row['double'])
 else:
  g=row;m=row['input']
  for k in ['kind','index','seed','legacy']:integer(row[k],0,2147483647)
  assert type(m['boss'])==type(m['passive'])==int and m['boss']==m['passive']==1 and not m['fusion'];segments=2;passive=-1
  index=row['index'];seed=row['seed'];weather=terrain=double=0
 integer(m['fusion'],0,0)
 assert len(m['ivs'])==len(m['stats'])==6
 for k in ['id','species','form','level','gender','nature','abilityIndex','ability','fusionForm','fusionGender','fusionAbilityIndex','tera']:
  integer(m[k],-1 if k in ['gender','fusionGender']else 0,4294967295 if k=='id'else 2147483647)
 assert 0<m['species']<65536 and 0<=m['form']<256 and 0<m['level']<=70000 and m['gender']in[-1,0,1]and m['fusionGender']in[-1,0,1]
 assert 0<=m['nature']<25 and m['abilityIndex']in[0,1,2]and m['fusionAbilityIndex']in[0,1,2]and 0<=m['tera']<=18
 for v in m['ivs']:integer(v,0,31)
 for v in m['stats']:integer(v,1,2147483647)
 assert len(g['pool'])<=64 and len(g['moves'])<=4
 for t in g['pool']:
  assert len(t)==3;integer(t[0],-1,100000);integer(t[1],1,1999);integer(t[2],0,7)
 for v in g['moves']:
  assert set(v)=={'move','used','up','max'};integer(v['move'],1,1999);integer(v['used'],0,0);integer(v['up'],-4,3);integer(v['max'],-1,-1)
 f=dict(id=m['id'],form=m['form'],gender=m['gender'],nature=m['nature'],ability_index=m['abilityIndex'],fusion_species=-1,fusion_form=0,fusion_gender=m['fusionGender'],fusion_ability_index=0,tera_type=m['tera'],friendship=-1,boss_segments=segments)
 fields=','.join('.'+k+'='+str(v)for k,v in f.items())+',.ivs='+a(m['ivs'])
 frame='{'+','.join(map(str,[0,index,seed,0,m['species'],m['level'],m['ability'],len(g['moves']),len(g['pool'])]))+',{'+fields+'},'+a(m['stats'])+','+rng(g['before'])+','+rng(g['after'])+',{'+','.join(a(t)for t in g['pool'])+'},{'+','.join(a([v[k]for k in ['move','used','up','max']])for v in g['moves'])+'}}'
 return '{'+frame+','+','.join(map(str,[weather,terrain,double,passive]))+'}'
assert [r['sample']for r in x['rows']]==list(range(1016))
lines=['/* Native ordered pools, moves and pre-finish RNG; native_move_case is defined by the retained ordinary-wild fixture. */','typedef struct{native_move_case mon;int weather,terrain,dbl,passive;}native_boss_move_case;','static const native_boss_move_case native_boss_constructor_moves[]={']
lines += [encode(r,True)+','for r in x['rows']];lines += ['};','static const native_boss_move_case native_boss_retained_moves[]={']
lines += [encode(r,False)+','for r in y['rows']];lines+=['};'];Path(sys.argv[4]).write_text('\n'.join(lines)+'\n')
