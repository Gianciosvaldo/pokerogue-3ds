#!/usr/bin/env python3
from pathlib import Path
import json,sys
def unique(pairs):
 d={}
 for k,v in pairs:
  assert k not in d, 'duplicate JSON key'
  d[k]=v
 return d
j=json.loads(Path(sys.argv[1]).read_text(),object_pairs_hook=unique);assert set(j)=={'version','before','after','order','consumers'} and j['version']==1
assert j['order']==[11,22,44,33,22,11,22,44,33,44,11,22,44,33,33,11,22,44,33]
for stage in ['before','after']:
 assert set(j[stage])=={'actors','global','battle'} and len(j[stage]['actors'])==4
 for k,p in enumerate(j[stage]['actors']):
  assert set(p)=={'id','species','form','level','hp','stats','stages','moves','history'}
  assert p['id']==11*(k+1) and p['species']==242 and p['form']==0 and p['level']==1000
  assert type(p['hp'])==int and 0<p['hp']<=p['stats'][0] and len(p['stats'])==6 and all(type(x)==int and 0<x<2147483648 for x in p['stats'])
  assert len(p['stages'])==7 and all(type(x)==int and -6<=x<=6 for x in p['stages'])
  assert len(p['moves'])==2 and [m['move']for m in p['moves']]==[552,689] and all(set(m)=={'move','pp'} and type(m['pp'])==int and 0<=m['pp']<=15 for m in p['moves'])
  assert len(p['history'])<=5
  for h in p['history']:assert set(h)=={'move','result','useMode','targets'} and h['move']in[552,689] and type(h['result'])==int and 0<=h['result']<=4 and h['useMode']in[1,3] and len(h['targets'])==1 and h['targets'][0]in[0,2]
assert all(not p['history'] and p['hp']==p['stats'][0] and [m['pp']for m in p['moves']]==[0,0]for p in j['before']['actors'])
assert len(j['consumers'])==2 and [c['move']for c in j['consumers']]==[558,559]
for c in j['consumers']:assert set(c)=={'move','applied','power'} and type(c['applied'])==bool and type(c['power'])in[int,float] and c['power']in[1,2]
def arr(a):return '{'+','.join(map(str,a))+'}'
def rng(s):
 import math
 f=s.split(',');assert len(f)==5 and f[0]=='!rnd';v=[float(x)for x in f[1:]];assert all(math.isfinite(x)for x in v) and v[0]>=0 and all(0<=x<1 for x in v[1:]);return '{'+','.join(f[1:])+',0}'
lines=['static const struct{uint32_t id;int species,form,level,hp,stats[6],stages[7];}command_actors[4]={']
for p in j['before']['actors']:lines.append('{'+','.join(map(str,[p['id'],p['species'],p['form'],p['level'],p['hp']]))+','+arr(p['stats'])+','+arr(p['stages'])+'},')
lines+=['};','static const pr_rng command_before[2]={'+rng(j['before']['global'])+','+rng(j['before']['battle'])+'};','static const pr_rng command_after[2]={'+rng(j['after']['global'])+','+rng(j['after']['battle'])+'};','static const uint32_t command_order[]='+arr(j['order'])+';','static const struct{int hp,stages[7],pp[2],nh;pr_turn_move history[5];}command_expected[4]={']
for p in j['after']['actors']:
 histories=['{.move='+str(h['move'])+',.result='+str(h['result'])+',.use_mode='+str(h['useMode'])+',.n_targets='+str(len(h['targets']))+',.targets='+arr(h['targets'])+'}'for h in p['history']]
 lines.append('{'+str(p['hp'])+','+arr(p['stages'])+','+arr([m['pp']for m in p['moves']])+','+str(len(histories))+',{'+','.join(histories)+'}},')
lines+=['};','static const int command_power[2]='+arr([c['power']for c in j['consumers']])+';'];Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
