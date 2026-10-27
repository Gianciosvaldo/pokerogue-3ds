#!/usr/bin/env python3
"""Transport complete native TM learning/history/memory observations."""
from pathlib import Path
import json,math,sys
assert len(sys.argv)==3,'native.json output.inc'
def unique(pairs):
 d={}
 for k,v in pairs:assert k not in d,'duplicate key';d[k]=v
 return d
def integer(v,lo,hi):assert type(v)==int and lo<=v<=hi;return v
def a(v):return '{'+','.join(map(str,v))+'}'
def rng(s):
 assert type(s)==str;v=s.split(',');assert len(v)==5 and v[0]=='!rnd';n=list(map(float,v[1:]));assert all(math.isfinite(x)for x in n)and n[0]>=0 and all(0<=x<1 for x in n[1:]);return a(v[1:]+['0'])
x=json.loads(Path(sys.argv[1]).read_text(),object_pairs_hook=unique)
assert set(x)=={'version','scope','available','implemented','catalog','level','species','initial','rows'}and type(x['version'])==int and x['version']==1
assert x['scope']=='native LearnMovePhase.learnMove TM callback before text acknowledgement; animation/audio/UI presentation bypassed'
assert type(x['species'])==type(x['level'])==int and x['species']==151 and x['level']==5
assert len(x['available'])==len(x['implemented'])==320 and len(x['catalog'])==322
for k in ['available','catalog']:
 assert len(x[k])==len(set(x[k]))
 for v in x[k]:integer(v,1,920)
assert x['catalog']==sorted(x['catalog'])and set(x['available'])<=set(x['catalog'])
assert all(type(v)==bool for v in x['implemented'])and sum(x['implemented'])==312
selected=[m for m,ok in zip(x['available'],x['implemented'])if ok];assert [r['move']for r in x['rows']]==selected+[selected[0]]and len(x['rows'])==313
def frame(f):
 assert set(f)=={'used','moves','memory','global','battle'}and len(f['used'])<=322 and len(f['used'])==len(set(f['used']))and set(f['used'])<=set(x['catalog'])
 for v in f['used']:integer(v,1,920)
 assert len(f['moves'])==4 and len(f['memory'])<=486
 for m in f['moves']:
  assert set(m)=={'move','used','up','max'};integer(m['move'],1,920);integer(m['used'],0,255);integer(m['up'],0,3);integer(m['max'],-1,-1)
 for r in f['memory']:
  assert len(r)==3 and(r[0]is None or type(r[0])==int and -1<=r[0]<=70000);integer(r[1],1,920);integer(r[2],0,13)
 return '{'+str(len(f['used']))+','+a(f['used'])+',{'+','.join(a([m[k]for k in ['move','used','up','max']])for m in f['moves'])+'},'+str(len(f['memory']))+',{'+','.join(a([-1 if v is None else v for v in r])for r in f['memory'])+'},{'+rng(f['global'])+','+rng(f['battle'])+'}}'
lines=['/* Native actual TM learning callback, ordered history and complete memory rows. */','static const uint16_t native_tm_catalog[]='+a(x['catalog'])+';','typedef struct{int n;uint16_t used[322];int moves[4][4],nmemory,memory[486][3];pr_rng rng[2];}native_tm_frame;','static const native_tm_frame native_tm_initial='+frame(x['initial'])+';','static const struct{int move;native_tm_frame after;}native_tm_cases[]={']
prev=x['initial'];assert prev['used']==[]
for r in x['rows']:
 assert set(r)=={'move','before','after'}and r['before']==prev;integer(r['move'],1,920);frame(r['before']);lines.append('{'+str(r['move'])+','+frame(r['after'])+'},');prev=r['after']
assert len(prev['used'])==312
lines+=['};'];Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
