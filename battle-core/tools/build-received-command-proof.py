#!/usr/bin/env python3
from pathlib import Path
import json,math,sys
j=json.loads(Path(sys.argv[1]).read_text());assert set(j)=={'order','before','after','records'}
b=j['before'];a=j['after'];assert j['order']==[11,22,11,44,11,33,11] and b['speedStages']==[6,6,-6,-5]
assert b['levels']==[100,100,1000,1000] and len(b['actors'])==4 and len(j['records'])==40
assert b['pp']==0 and a['pp']==4 and b['hp']>a['hp']>0
lines=['/* Actual naturally ordered four-command pinned turn. */','static const struct {uint32_t id;int species,level,stats[6];} received_command_actors[4]={']
for k,p in enumerate(b['actors']):
 assert p['id']==(k+1)*11 and p['species']==(925 if k==0 else 765) and p['level']==b['levels'][k]
 assert p['nature']==0 and p['ivs']==[15]*6 and len(p['stats'])==6 and all(type(x)==int and 0<x<2147483648 for x in p['stats'])
 lines.append('{'+','.join(map(str,[p['id'],p['species'],p['level']]))+',{'+','.join(map(str,p['stats']))+'}},')
lines.append('};')
def rng(s):
 f=s.split(',');assert len(f)==5 and f[0]=='!rnd'
 v=[float(x)for x in f[1:]];assert all(math.isfinite(x)for x in v) and v[0]>=0 and all(0<=x<1 for x in v[1:]);return '{'+','.join(f[1:])+',0}'
for name,stage in [('before',b),('after',a)]:
 lines.append('static const pr_rng received_command_'+name+'[2]={'+rng(stage['global'])+','+rng(stage['battle'])+'};')
lines+=['static const int received_command_hp='+str(a['hp'])+';','static const pr_attack_record received_command_records[40]={']
for r in j['records']:
 assert set(r)=={'move','result','damage','critical','sourceId','sourceBattlerIndex'}
 assert r['move']==860 and r['damage']>0 and r['sourceId']==11 and r['sourceBattlerIndex']==0 and type(r['critical'])==bool
 lines.append('{'+','.join(map(str,[r['move'],r['result'],int(r['critical']),r['sourceBattlerIndex'],r['sourceId'],r['damage']]))+'},')
lines.append('};');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
