#!/usr/bin/env python3
from pathlib import Path
from collections import Counter
import itertools,json,math,sys
def unique(pairs):
 d={}
 for k,v in pairs:
  assert k not in d, 'duplicate JSON key'
  d[k]=v
 return d
j=json.loads(Path(sys.argv[1]).read_text(),object_pairs_hook=unique);assert set(j)=={'version','ids','globalRng','battleRng','rows'} and j['version']==1
assert j['ids']==[0,4294967295,2147483648,7,11,21,31,41,51,61,71,81]
rows=j['rows'];assert len(rows)==216
assert [(r['length'],r['pattern'],r['selected'])for r in rows]==list(itertools.product([0,1,7,8,9,19,255,256,65536],range(6),[0,1,8,11]))
def arr(a):return '{'+','.join(map(str,a))+'}'
def rng(s):
 f=s.split(',');assert len(f)==5 and f[0]=='!rnd';v=[float(x)for x in f[1:]];assert all(math.isfinite(x)for x in v) and v[0]>=0 and all(0<=x<1 for x in v[1:]);return '{'+','.join(f[1:])+',0}'
def record(r):
 assert set(r)=={'move','result','useMode','targets'} and type(r['move'])==int and 0<=r['move']<921 and type(r['result'])==int and 0<=r['result']<=4 and r['useMode']==1 and r['targets']==[2]
 return '{.move='+str(r['move'])+',.result='+str(r['result'])+',.use_mode=1,.n_targets=1,.targets={2}}'
lines=['/* Native input arrays are compressed losslessly; outputs are observed. */','static const uint32_t order_ids[12]='+arr([str(x)+'u'for x in j['ids']])+';', 'static const pr_rng order_rng[2]={'+rng(j['globalRng'])+','+rng(j['battleRng'])+'};','static const struct{int length,pattern,selected,period_n,period[12],patch_n;struct{int at,value;}patch[2];pr_turn_move initial[12],update;int power[5][2];}order_cases[216]={']
for r in rows:
 assert set(r)=={'length','pattern','selected','order','initial','update','stages'} and len(r['order'])==r['length'] and all(type(x)==int and 0<=x<12 for x in r['order']) and len(r['initial'])==12 and len(r['stages'])==5
 order=r['order']
 for period_n in range(1,13):
  period=[Counter(order[k::period_n]).most_common(1)[0][0]if order[k::period_n]else 0 for k in range(period_n)]
  patch=[(i,v)for i,v in enumerate(order)if v!=period[i%period_n]]
  if len(patch)<=2:break
 else:raise AssertionError('input cannot be represented by the retained lossless recipe')
 rebuilt=[period[i%period_n]for i in range(len(order))]
 for i,v in patch:rebuilt[i]=v
 assert rebuilt==order
 stages=[]
 for s in r['stages']:
  assert set(s)=={'values','rng'} and s['rng']==[j['globalRng'],j['battleRng']] and len(s['values'])==2
  assert [v['move']for v in s['values']]==[558,559]
  for v in s['values']:assert set(v)=={'move','applied','power'} and type(v['applied'])==bool and type(v['power'])in(int,float) and v['power']in[1,2]
  stages.append(arr([v['power']for v in s['values']]))
 lines.append('{'+','.join(map(str,[r['length'],r['pattern'],r['selected'],period_n]))+','+arr(period)+','+str(len(patch))+',{'+','.join(arr(p)for p in patch)+'},{'+','.join(record(m)for m in r['initial'])+'},'+record(r['update'])+',{'+','.join(stages)+'}},')
lines.append('};');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
