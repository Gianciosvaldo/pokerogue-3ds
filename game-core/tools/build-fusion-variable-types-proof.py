#!/usr/bin/env python3
"""Transport native variable-type endpoints; never calculate expected types."""
from pathlib import Path
import json,math,sys
assert len(sys.argv)==3,'native.json output.inc'
def unique(pairs):
 d={}
 for k,v in pairs:assert k not in d,'duplicate key';d[k]=v
 return d
def integer(v,lo,hi):assert type(v)==int and lo<=v<=hi;return v
def arr(v):return '{'+','.join(map(str,v))+'}'
def rng(s):
 assert type(s)==str;v=s.split(',');assert len(v)==5 and v[0]=='!rnd';n=list(map(float,v[1:]));assert all(math.isfinite(x)for x in n)and n[0]>=0 and all(0<=x<1 for x in n[1:]);return arr(v[1:]+['0'])
x=json.loads(Path(sys.argv[1]).read_text(),object_pairs_hook=unique)
assert set(x)=={'version','scope','definitions','rows'}and type(x['version'])==int and x['version']==1
assert x['scope']=='native variable attribute apply, Pokemon.getMoveType, item-spawn and moveset type owners; raw legal species/form snapshots'
defs=[{'moves':[449,718],'species':[493,773],'forms':[[493,18],[773,18]]},{'moves':[546],'species':[649],'forms':[[649,5]]},{'moves':[783],'species':[877],'forms':[[877,2]]},{'moves':[873],'species':[8128],'forms':[[8128,3]]},{'moves':[904],'species':[1017],'forms':[[1017,8]]}]
assert x['definitions']==defs
expected=[]
for d in defs:
 forms=dict(d['forms'])
 for move in d['moves']:
  expected += [(move,151,0,0,0),(move,151,0,19,0)]
  for sid in d['species']:
   for f in range(forms[sid]):expected += [(move,sid,f,0,0),(move,sid,f,19,0),(move,151,0,sid,f)]
   for other in d['species']:
    for f in range(forms[sid]):
     for ff in range(forms[other]):expected.append((move,sid,f,other,ff))
assert len(x['rows'])==len(expected)==2976
lines=['/* Native raw variable-type, actual Pokemon type, item-spawn and recorded AI type endpoints. */','typedef struct{int move,primary,form,fusion,fusionForm,fusionSpeciesForm,fusionGender,gender,ability,applied,variable,actual,nitems,items[2],movegen;pr_rng rng;}native_variable_type_case;','static const native_variable_type_case native_variable_type_cases[]={']
for r,e in zip(x['rows'],expected):
 assert set(r)=={'move','primary','form','fusion','fusionForm','fusionSpeciesForm','fusionGender','gender','ability','applied','variable','actual','items','movegen','before','after'}
 for k in ['move','primary','form','fusion','fusionForm','ability']:integer(r[k],0,65535)
 assert tuple(r[k]for k in ['move','primary','form','fusion','fusionForm'])==e
 assert type(r['applied'])==bool and r['fusionSpeciesForm']==(0 if r['fusion']else -1)
 for k in ['fusionSpeciesForm','fusionGender','gender']:integer(r[k],-1,1)
 for k in ['variable','actual','movegen']:integer(r[k],0,18)
 assert type(r['items'])==list and 1<=len(r['items'])<=2
 for t in r['items']:integer(t,0,18)
 rng(r['before']);rng(r['after']);assert r['before']==r['after']
 fields=[r[k]for k in ['move','primary','form','fusion','fusionForm','fusionSpeciesForm','fusionGender','gender','ability']]+[int(r['applied']),r['variable'],r['actual'],len(r['items'])]
 lines.append('{'+','.join(map(str,fields))+','+arr(r['items'])+','+str(r['movegen'])+','+rng(r['before'])+'},')
lines+=['};'];Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
