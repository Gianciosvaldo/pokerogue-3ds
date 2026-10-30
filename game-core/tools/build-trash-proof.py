#!/usr/bin/env python3
"""Strict native Trash to Treasure transport; expected outcomes are never simulated."""
from pathlib import Path
import gzip,json,math,sys
assert len(sys.argv)==3,'native.json[.gz] output.inc'
def unique(pairs):
 d={}
 for k,v in pairs:assert k not in d;d[k]=v
 return d
def integer(v,lo,hi):assert type(v)==int and lo<=v<=hi

def a(v):return '{'+','.join(map(str,v))+'}'
def rng(v):
 assert type(v)==str;p=v.split(',');assert len(p)==5 and p[0]=='!rnd';n=list(map(float,p[1:]));assert all(math.isfinite(t)for t in n)and n[0]>=0 and all(0<=t<1 for t in n[1:]);return a(p[1:]+['0'])
p=Path(sys.argv[1]);raw=gzip.decompress(p.read_bytes()).decode()if p.suffix=='.gz'else p.read_text();x=json.loads(raw,object_pairs_hook=unique)
assert set(x)=={'version','battleLocale','rows'}and type(x['version'])==int and x['version']==1
assert type(x['battleLocale'])==str;json.loads(x['battleLocale'])
expected=[(w,o,s,k)for w in [100,123,180]for o in [0,1]for s in range(4)for k in range(8)]
assert len(x['rows'])==len(expected)==192
keys={'LEFTOVERS','SHELL_BELL','EXP_SHARE','MYSTERY_ENCOUNTER_BLACK_SLUDGE','BERRY','BASE_STAT_BOOSTER','TOXIC_ORB','SOOTHE_BELL','LUCKY_EGG','GOLDEN_EGG'}
def items(v,config=False):
 assert type(v)==list and len(v)<16
 for i in v:
  assert set(i)==({'key','arg','stack'}if config else {'key','arg','owner','stack'})and i['key']in keys
  integer(i['arg'],-1,10);integer(i['stack'],0 if config else 1,4)
  if not config:integer(i['owner'],0,4294967295)
 return '{'+','.join('{'+json.dumps(i['key'])+','+str(i['arg'])+','+str(i.get('owner',0))+','+str(i['stack'])+'}'for i in v)+'}'
lines=['/* Actual pinned Trash to Treasure config/option observations. */','static const char battle_locale[]='+json.dumps(x['battleLocale'])+';','typedef struct{const char*key;int arg;uint32_t owner;int stack;}item_row;','typedef struct{int species,form,level;uint32_t id;int ivs[6],nature,ability,gender,friendship,hp,stats[6],boss,variant,luck;int moves[4][4];}enemy_row;','typedef struct{int wave,option,stock,sample,order[3];pr_rng before,after;int ni,no,ne;item_row input[16],output[16],enemy_items[16],config[10];enemy_row enemy;const char*text[3];const char*cost;}trash_case;','static const trash_case cases[]={']
zero=cap=0
for index,(r,meta)in enumerate(zip(x['rows'],expected)):
 assert set(r)=={'index','wave','option','stock','sample','order','before','afterInit','after','input','output','config','cost','requirements','leave','texts','queued','reward','effects','enemy','enemyItems'}
 for k in ['index','wave','option','stock','sample']:integer(r[k],0,192)
 assert r['index']==index and tuple(r[k]for k in ['wave','option','stock','sample'])==meta
 assert r['order']==([2,0,1]if r['sample']%2 else [0,1,2])
 assert r['before']==r['afterInit'];rng(r['afterInit'])
 assert type(r['requirements'])==list and len(r['requirements'])==2 and all(type(t)==bool and t for t in r['requirements'])
 assert r['cost']=='2.5'and type(r['texts'])==list and len(r['texts'])==(3 if r['option']else 1)and all(type(t)==str and t for t in r['texts'])
 cfg=r['config'];assert set(cfg)=={'form','add','segments','moves','items'}and cfg['form']==1 and cfg['add']==0.5 and cfg['segments']==1 and cfg['moves']==[441,707,359,371]and len(cfg['items'])==10
 assert [t['key']for t in cfg['items']]==['BERRY']*4+['BASE_STAT_BOOSTER']*2+['TOXIC_ORB','SOOTHE_BELL','LUCKY_EGG','GOLDEN_EGG']
 zero+=not r['input'];enemy='{0}'
 if r['option']==0:
  assert r['leave']==[]and r['queued']==[['MysteryEncounterBattlePhase',True]]and type(r['queued'][0][1])==bool
  assert r['reward']=={'funcs':['LEFTOVERS'],'tiers':[3,2,1],'fill':True}and type(r['reward']['fill'])==bool
  assert r['effects']==[[2,[0],92,2],[2,[2],254,2]]
  e=r['enemy'];assert set(e)=={'species','form','level','id','ivs','nature','abilityIndex','gender','friendship','hp','stats','boss','shiny','variant','luck','passive','moves'}
  assert e['species']==569 and e['form']==1 and type(e['shiny'])==type(e['passive'])==bool and not e['shiny']and not e['passive']
  integer(e['level'],1,1000);integer(e['id'],0,4294967295);integer(e['nature'],0,24);integer(e['abilityIndex'],0,2);integer(e['gender'],-1,1);integer(e['friendship'],0,255);integer(e['boss'],2,255);integer(e['variant'],0,2);integer(e['luck'],0,3)
  assert len(e['ivs'])==len(e['stats'])==6
  for v in e['ivs']:integer(v,0,31)
  for v in e['stats']:integer(v,1,2147483647)
  integer(e['hp'],1,2147483647);assert len(e['moves'])==4
  for m in e['moves']:assert len(m)==4;integer(m[0],1,1999);integer(m[1],0,0);integer(m[2],0,0);integer(m[3],-1,-1)
  enemy='{'+','.join(str(e[k])for k in ['species','form','level','id'])+','+a(e['ivs'])+','+','.join(str(e[k])for k in ['nature','abilityIndex','gender','friendship','hp'])+','+a(e['stats'])+','+','.join(str(e[k])for k in ['boss','variant','luck'])+',{'+','.join(a(m)for m in e['moves'])+'}}'
 else:assert r['enemy']is None and r['enemyItems']==[]and r['effects']==[]and r['queued']==[]and r['reward']is None and r['leave']==[True]and type(r['leave'][0])==bool
 lines.append('{'+','.join(str(r[k])for k in ['wave','option','stock','sample'])+','+a(r['order'])+','+rng(r['before'])+','+rng(r['after'])+','+','.join(map(str,[len(r['input']),len(r['output']),len(r['enemyItems'])]))+','+items(r['input'])+','+items(r['output'])+','+items(r['enemyItems'])+','+items(cfg['items'],True)+','+enemy+',{'+','.join(json.dumps(t)for t in r['texts'])+'},'+json.dumps(r['cost'])+'},')
assert zero==24
lines.append('};');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
