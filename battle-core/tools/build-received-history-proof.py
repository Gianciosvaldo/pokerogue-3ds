#!/usr/bin/env python3
"""Transcribe actual pinned observations; validate complete fixture coverage."""
from pathlib import Path
import itertools,json,math,sys

def obj(pairs):
 d={}
 for k,v in pairs:
  assert k not in d,('duplicate key',k)
  d[k]=v
 return d
j=json.loads(Path(sys.argv[1]).read_text(),object_pairs_hook=obj)
assert set(j)=={'version','ids','templates','rows'} and j['version']==2
assert j['ids']==[0,4294967295,2147483648,7,11,21]
rows=j['rows'];assert len(rows)==384
assert [(r['subject'],r['context'],r['length'],r['pattern']) for r in rows]==list(itertools.product([0,2],range(3),[0,1,31,32,33,64,65535,65536],range(8)))
assert len(j['templates'])==32
assert [(t['defender'],t['attacker'],t['move']) for t in j['templates']]==list(itertools.product([0,2],range(4),[64,52,33,93]))
def record(r):
 assert set(r)=={'move','result','damage','critical','sourceId','sourceBattlerIndex'}
 assert type(r['move'])==int and 0<=r['move']<=920
 assert type(r['result'])==int and 0<=r['result']<=7
 assert type(r['damage']) in (int,float) and math.isfinite(r['damage'])
 assert type(r['critical'])==bool and type(r['sourceId'])==int and 0<=r['sourceId']<=4294967295
 assert type(r['sourceBattlerIndex'])==int and -1<=r['sourceBattlerIndex']<4
 return '{'+','.join([str(r['move']),str(r['result']),str(int(r['critical'])),str(r['sourceBattlerIndex']),str(r['sourceId']),repr(r['damage'])])+'}'
for t in j['templates']:
 assert set(t)=={'defender','attacker','move','effectiveness','record'}
 record(t['record']);assert type(t['effectiveness'])==int and t['effectiveness']==(2 if t['move']in[64,52]else 1);assert t['record']['damage']>0 and t['record']['move']==t['move']
def rng(s):
 f=s.split(',');assert len(f)==5 and f[0]=='!rnd'
 v=[float(x)for x in f[1:]];assert all(math.isfinite(x)for x in v) and v[0]>=0 and all(0<=x<1 for x in v[1:])
 return '{'+','.join(f[1:])+',0}'
def array(a):return '{'+','.join(str(int(v))for v in a)+'}'
lines=['/* Actual pinned received-hit caller observations. */',
'static const uint32_t received_ids[6]={0,4294967295u,2147483648u,7,11,21};',
'static const struct {int subject,context,length,pattern;pr_attack_record first,filler,incoming,fresh;struct {double damage[4];int target[4],condition[4],power[6],enigma,focus,cancelled,rage_count,rage_battler,rage_total,rage_markers,rage_levels;pr_rng rng[2];}stage[5];} received_cases[384]={']
for r in rows:
 assert set(r)=={'subject','context','length','pattern','first','filler','incoming','fresh','stages'} and len(r['stages'])==5
 stages=[]
 for s in r['stages']:
  assert set(s)=={'counters','powers','enigma','interrupted','cancelled','rageResult','added','rageQueue','rng'}
  assert len(s['counters'])==4 and [c['move']for c in s['counters']]==[68,243,368,894]
  for c in s['counters']:
   assert set(c)=={'move','result','damage','redirected','target','condition'}
   assert c['result'] is True and type(c['redirected'])==bool and type(c['condition'])==bool
   assert type(c['damage']) in (int,float) and math.isfinite(c['damage']) and c['damage']>=1 and int(c['damage'])==c['damage']
   assert type(c['target'])==int and -2<=c['target']<4
  assert len(s['powers'])==6 and all(set(v)=={'result','value'} and type(v['result'])==bool and type(v['value']) in (int,float) and v['value'] in [1,2] for v in s['powers'])
  assert all(type(s[k])==bool for k in ['enigma','interrupted','cancelled','rageResult']) and s['rageResult']
  assert len(s['added'])<=1 and len(s['rng'])==2
  for added in s['added']:
   assert set(added)=={'name','battlerIndex','source','changes'} and added['name']=='StatStageChangePhase' and added['source'] is True
   assert added['changes']==[{'stat':1,'stages':1}]
   assert type(added['battlerIndex'])==int and -1<=added['battlerIndex']<4
  queue=s['rageQueue'];assert set(queue)=={'levels','dynamic'}
  assert all(type(level)==list and all(name=='Marker:StatStageChangePhase' for name in level) for level in queue['levels'])
  assert len(queue['dynamic'])<=1
  for group in queue['dynamic']:assert set(group)=={'name','phases'} and group['name']=='StatStageChangePhase' and group['phases']==['StatStageChangePhase']
  markers=sum(len(level)for level in queue['levels']);dynamic=sum(len(group['phases'])for group in queue['dynamic'])
  assert markers==dynamic==len(s['added'])
  stages.append('{'+','.join([array([c['damage']for c in s['counters']]),array([c['target']for c in s['counters']]),array([c['condition']for c in s['counters']]),array([v['value']for v in s['powers']]),str(int(s['enigma'])),str(int(s['interrupted'])),str(int(s['cancelled'])),str(len(s['added'])),str(s['added'][0]['battlerIndex'] if s['added'] else -2),str(markers+dynamic),str(markers),str(len(queue['levels'])),'{'+','.join(rng(x)for x in s['rng'])+'}'])+'}')
 lines.append('{'+','.join([str(r[k])for k in ['subject','context','length','pattern']]+[record(r[k])for k in ['first','filler','incoming','fresh']]+['{'+','.join(stages)+'}'])+'},')
lines.append('};');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
