#!/usr/bin/env python3
"""Transport pinned native power observations; never calculate expected power."""
from pathlib import Path
import json,math,sys
assert len(sys.argv)==4,'focused.json constructors.json output.inc'
def unique(pairs):
 d={}
 for k,v in pairs:assert k not in d,'duplicate key';d[k]=v
 return d
def integer(v,lo,hi):assert type(v)==int and lo<=v<=hi;return v
def a(v):return '{'+','.join(map(str,v))+'}'
def rng(s):
 assert type(s)==str;v=s.split(',');assert len(v)==5 and v[0]=='!rnd';n=list(map(float,v[1:]));assert all(math.isfinite(x)for x in n)and n[0]>=0 and all(0<=x<1 for x in n[1:]);return a(v[1:]+['0'])
shapes=[(86,728,2),(283,728,2),(456,728,2),(690,728,2),(87,730,2),(284,730,2),(86,728,0),(209,300,1)]
lines=['/* Native full-generator input snapshots and independently observed effective powers. */','typedef struct{int mode,dbl,weather,terrain,species,level,ability,passive,stats[6],np,n;pr_enemy_gen g;pr_rng rng;int pool[64][3];uint16_t moves[64];double powers[64];}native_fusion_power_case;','static const native_fusion_power_case native_fusion_power_cases[]={']
for source,count,scope in [(1,8,'actual native focused fusion ability power pairs; finish RNG recorded separately'),(2,804,'actual native fused boss generator and power boundary; finish RNG recorded separately')]:
 x=json.loads(Path(sys.argv[source]).read_text(),object_pairs_hook=unique)
 assert set(x)=={'version','scope','rows'}and type(x['version'])==int and x['version']==1 and x['scope']==scope and len(x['rows'])==count
 for index,r in enumerate(x['rows']):
  assert set(r)=={'mode','species','level','sample','charms','double','weather','terrain','before','generation','after'}
  integer(r['sample'],0,803);assert type(r['double'])==bool
  integer(r['mode'],0,2);integer(r['species'],1,65535);integer(r['level'],1,70000);integer(r['weather'],0,9);integer(r['terrain'],0,4);integer(r['charms'],0,4);rng(r['before']);rng(r['after'])
  g=r['generation'];assert set(g)=={'before','after','input','pool','implemented','powers','moves'};rng(g['after']);rng(g['before'])
  m=g['input'];assert set(m)=={'id','species','form','level','gender','nature','abilityIndex','ability','fusion','fusionForm','fusionGender','fusionAbilityIndex','tera','ivs','stats','hp','boss','bossSegments','passive','passiveAbility','trainer','activeTypeChanger','passiveTypeChanger','types'}
  for k in ['boss','passive','trainer','activeTypeChanger','passiveTypeChanger']:assert type(m[k])==bool
  assert m['boss']and m['passive']and not m['trainer']and m['species']==r['species']and m['level']==r['level']and m['hp']==m['stats'][0]
  if source==1:assert (m['species'],m['fusion'],m['fusionAbilityIndex'])==shapes[index] and m['level']==100 and r['mode']==0 and r['sample']==index
  else:
   species=[1,81,133,151,163,164,175,176,209,210,468,582,583,702,974,975,1016]
   expected=[(s,l,d,k,0)for s in species for l in [100,65536,70000]for d in [False,True]for k in [0,1]]
   expected += [(species[k%len(species)],100 if k%2 else 70000,bool(k%3),k,4)for k in range(600)]
   assert r['mode']==2 and tuple(r[k]for k in ['species','level','double','sample','charms'])==expected[index]
  integer(m['id'],0,4294967295)
  for k in ['species','fusion']:integer(m[k],1,65535)
  for k in ['form','fusionForm']:integer(m[k],0,255)
  for k in ['gender','fusionGender']:integer(m[k],-1,1)
  for k in ['abilityIndex','fusionAbilityIndex']:integer(m[k],0,2)
  integer(m['nature'],0,24);integer(m['level'],1,70000);integer(m['tera'],0,18);integer(m['bossSegments'],2,255)
  for k in ['ability','passiveAbility']:integer(m[k],1,65535)
  assert len(m['ivs'])==len(m['stats'])==6 and 1<=len(m['types'])<=2
  for v in m['ivs']:integer(v,0,31)
  for v in m['stats']:integer(v,1,2147483647)
  for v in m['types']:integer(v,0,18)
  assert 0<len(g['pool'])<=64 and len(g['implemented'])==len(g['pool'])
  for t,implemented in zip(g['pool'],g['implemented']):
   assert len(t)==3 and type(implemented)==bool;integer(t[0],-1,100000);integer(t[1],1,1999);integer(t[2],0,7)
  required=list(dict.fromkeys(t[1]for t,implemented in zip(g['pool'],g['implemented'])if implemented))
  assert [t[0]for t in g['powers']]==required and len(required)<=64
  for t in g['powers']:
   assert len(t)==2 and type(t[1])in[int,float]and math.isfinite(t[1])and t[1]>=0;integer(t[0],1,1999)
  assert 0<len(g['moves'])<=4
  for t in g['moves']:
   assert set(t)=={'move','used','up','max'};integer(t['move'],1,1999);integer(t['used'],0,0);integer(t['up'],-4,3);integer(t['max'],-1,-1)
  f=dict(id=m['id'],form=m['form'],gender=m['gender'],nature=m['nature'],ability_index=m['abilityIndex'],fusion_species=m['fusion'],fusion_form=m['fusionForm'],fusion_gender=m['fusionGender'],fusion_ability_index=m['fusionAbilityIndex'],tera_type=m['tera'],friendship=-1,boss_segments=m['bossSegments'])
  fields=','.join('.'+k+'='+str(v)for k,v in f.items())+',.ivs='+a(m['ivs'])
  lines.append('{'+','.join(map(str,[r['mode'],int(r['double']),r['weather'],r['terrain'],m['species'],m['level'],m['ability'],m['passiveAbility']]))+','+a(m['stats'])+','+str(len(g['pool']))+','+str(len(required))+',{'+fields+'},'+rng(g['before'])+',{'+','.join(a(t)for t in g['pool'])+'},'+a(required)+','+a([repr(t[1])for t in g['powers']])+'},')
lines+=['};'];Path(sys.argv[3]).write_text('\n'.join(lines)+'\n')
