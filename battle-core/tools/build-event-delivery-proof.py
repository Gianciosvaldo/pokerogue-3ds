#!/usr/bin/env python3
from pathlib import Path
import json,math,sys
def unique(pairs):
 d={}
 for k,v in pairs:
  assert k not in d,'duplicate key'
  d[k]=v
 return d
j=json.loads(Path(sys.argv[1]).read_text(),object_pairs_hook=unique)
assert set(j)=={'version','before','after','order','records','hits','texts','phases','locales'}and type(j['version'])==int and j['version']==1
assert j['order']==[0,2,0,1,0,3,0]and len(j['records'])==len(j['hits'])==40
assert len(j['texts'])==104 and all(type(x)==str and x for x in j['texts'])
assert j['phases'][-1]=='BerryPhase'and j['phases'].count('TurnStartPhase')==1 and len(j['phases'])==452
assert set(j['locales'])=={'battle','pokemon-info','move','ability','pokemon','weather','terrain','move-trigger'}
def integer(x,lo,hi):assert type(x)==int and lo<=x<=hi;return str(x)
def arr(a):return '{'+','.join(map(str,a))+'}'
def rng(s):
 f=s.split(',');assert len(f)==5 and f[0]=='!rnd';v=list(map(float,f[1:]));assert all(math.isfinite(x)for x in v)and v[0]>=0 and all(0<=x<1 for x in v[1:]);return arr(f[1:]+['0'])
for name in ['before','after']:
 d=j[name];assert set(d)=={'weather','terrain','actors','global','battle'}and len(d['actors'])==4
 for env in ['weather','terrain']:
  assert set(d[env])=={'type','turns','max'};[integer(x,0,32767)for x in d[env].values()]
 for k,p in enumerate(d['actors']):
  assert set(p)=={'id','species','form','level','hp','stats','stages','gender','types','ability','moves','history'}
  assert p['id']==11*(k+1)and p['species']==(242 if k<2 else 377)and p['form']==0 and p['level']==1000
  integer(p['hp'],1,100000);assert len(p['stats'])==6;[integer(x,1,2147483647)for x in p['stats']]
  assert len(p['stages'])==7;[integer(x,-6,6)for x in p['stages']]
  integer(p['gender'],-1,1);assert 1<=len(p['types'])<=3;[integer(x,0,18)for x in p['types']]
  integer(p['ability'],0,400);assert p['ability']==(30 if k<2 else 133)
  assert len(p['moves'])==2 and [m['move']for m in p['moves']]==[860,689]
  for m in p['moves']:assert set(m)=={'move','pp'};integer(m['pp'],0,40)
  assert len(p['history'])<=4
  for h in p['history']:
   assert set(h)=={'move','result','useMode','targets'}and h['move']in[860,689]and h['targets']in[[0],[2]]
   integer(h['result'],0,4);integer(h['useMode'],0,4);[integer(x,0,3)for x in h['targets']]
assert all(not p['history']and p['hp']==p['stats'][0]and [m['pp']for m in p['moves']]==[0,0]for p in j['before']['actors'])
for r in j['records']:
 assert set(r)=={'move','result','damage','critical','sourceId','sourceBattlerIndex'}and r['move']==860 and r['sourceId']==11 and r['sourceBattlerIndex']==0 and type(r['critical'])==bool
 integer(r['damage'],1,10000);integer(r['result'],0,20);integer(r['sourceBattlerIndex'],0,0)
for h in j['hits']:
 assert set(h)=={'amount','hp','critical','result'}and type(h['critical'])==bool
 integer(h['amount'],1,10000);integer(h['hp'],1,100000);integer(h['result'],0,20)
lines=['/* Actual pinned commands and UI observations, not computed expected gameplay. */','static const uint32_t event_order[]='+arr([j['before']['actors'][i]['id']for i in j['order']])+';']
for name in ['before','after']:
 d=j[name];lines+=['static const pr_rng event_'+name+'_rng[2]={'+rng(d['global'])+','+rng(d['battle'])+'};','static const int event_'+name+'_env[6]='+arr([d[e][k]for e in ['weather','terrain']for k in ['type','turns','max']])+';', 'static const struct{uint32_t id;int species,level,hp,gender,ability,stats[6],stages[7],types[3],nt,pp[2],nh;pr_turn_move history[4];}event_'+name+'_actors[4]={']
 for p in d['actors']:
  hs=['{.move='+str(h['move'])+',.result='+str(h['result'])+',.use_mode='+str(h['useMode'])+',.n_targets=1,.targets='+arr(h['targets'])+'}'for h in p['history']]
  lines.append('{'+','.join(map(str,[p['id'],p['species'],p['level'],p['hp'],p['gender'],p['ability']]))+','+arr(p['stats'])+','+arr(p['stages'])+','+arr(p['types'])+','+str(len(p['types']))+','+arr([m['pp']for m in p['moves']])+','+str(len(hs))+',{'+','.join(hs)+'}},')
 lines.append('};')
lines+=['static const struct{int amount,hp,result;bool critical;}event_hits[40]={'+','.join('{'+','.join(map(str,[h['amount'],h['hp'],h['result']]))+','+str(int(h['critical']))+'}'for h in j['hits'])+'};','static const pr_attack_record event_records[40]={'+','.join('{'+','.join(map(str,[r['move'],r['result'],int(r['critical']),r['sourceBattlerIndex'],r['sourceId'],r['damage']]))+'}'for r in j['records'])+'};','static const char*event_texts[104]={'+','.join(json.dumps(x)for x in j['texts'])+'};']
phases=j['phases'][j['phases'].index('TurnStartPhase'):];lines+=['static const char*event_phases[]={' +','.join(json.dumps(x)for x in phases)+'};','static const struct{const char*name,*json;}event_locales[]={']
for key,val in j['locales'].items():assert type(val)==dict;lines.append('{'+json.dumps(key)+','+json.dumps(json.dumps(val,ensure_ascii=False,separators=(',',':')))+'},')
lines.append('};');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
