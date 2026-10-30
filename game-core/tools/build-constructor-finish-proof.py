#!/usr/bin/env python3
"""Strict transport of native joined constructor observations, without an expected-state model."""
from pathlib import Path
import gzip,json,math,sys
ROOT=Path(__file__).resolve().parents[2]
assert len(sys.argv)==3,'native.json output.inc'
def unique(pairs):
 d={}
 for k,v in pairs:assert k not in d,'duplicate key';d[k]=v
 return d
def integer(v,lo,hi):assert type(v)==int and lo<=v<=hi;return v
def boolean(v):assert type(v)==bool

def a(v):return '{'+','.join(map(str,v))+'}'
def rng(v):
 assert type(v)==str;p=v.split(',');assert len(p)==5 and p[0]=='!rnd';n=list(map(float,p[1:]));assert all(math.isfinite(t)for t in n)and n[0]>=0 and all(0<=t<1 for t in n[1:]);return a(p[1:]+['0'])
capture=Path(sys.argv[1]);raw=gzip.decompress(capture.read_bytes()).decode()if capture.suffix=='.gz'else capture.read_text()
x=json.loads(raw,object_pairs_hook=unique)
assert set(x)=={'version','scope','rows'}and type(x['version'])==int and x['version']==1
assert x['scope']=='native boss base, moveset, finish, stat-update and threshold APIs in both Endless modes'
species=[1,81,133,151,163,164,175,176,209,210,468,582,583,702,974,975,1016]
expected=[]
for mode in [2,1]:
 expected += [(mode,s,l,d,k,0)for s in species for l in [100,65536,70000]for d in [False,True]for k in [0,1]]
 expected += [(mode,species[k%len(species)],100 if k%2 else 70000,bool(k%3),k,4)for k in range(600)]
expected += [(mode,s,65535,d,k,0)for mode in [2,1]for s in species for d in [False,True]for k in [0,1]]
assert len(x['rows'])==len(expected)==1744
old=json.loads(gzip.decompress((ROOT/'game-core/tests/fixtures/fusion-power.constructors.json.gz').read_bytes()))
for r,o in zip(x['rows'][:804],old['rows']):assert {k:v for k,v in r.items()if k not in ['final','refreshed','poolAfter']}==o,'original 804 constructor observations changed'
frame_keys={'id','species','form','level','gender','nature','abilityIndex','ability','fusion','fusionForm','fusionGender','fusionAbilityIndex','tera','ivs','stats','hp','boss','bossSegments','passive','passiveAbility','trainer','activeTypeChanger','passiveTypeChanger','types'}
def frame(m,final=False):
 assert set(m)==frame_keys|({'friendship','exp','name','shiny','variant','luck'}if final else set())
 for k in ['boss','passive','trainer','activeTypeChanger','passiveTypeChanger']:boolean(m[k])
 assert m['boss']and m['passive']and not m['trainer']
 integer(m['id'],0,4294967295)
 for k in ['species','ability','passiveAbility']:integer(m[k],1,65535)
 integer(m['fusion'],0,65535)
 for k in ['form','fusionForm']:integer(m[k],0,255)
 for k in ['gender','fusionGender']:integer(m[k],-1,1)
 for k in ['abilityIndex','fusionAbilityIndex']:integer(m[k],0,2)
 integer(m['level'],1,70000);integer(m['nature'],0,24);integer(m['tera'],0,18);integer(m['bossSegments'],2,255)
 assert len(m['ivs'])==len(m['stats'])==6
 for v in m['ivs']:integer(v,0,31)
 for v in m['stats']:integer(v,1,2147483647)
 integer(m['hp'],0,2147483647)
 assert 1<=len(m['types'])<=2
 for v in m['types']:integer(v,0,18)
 if final:
  integer(m['friendship'],0,255);assert type(m['exp'])in[int,float]and math.isfinite(m['exp'])and 0<=m['exp']<=9007199254740991
  assert type(m['name'])==str and 0<len(m['name'])<200;boolean(m['shiny']);assert not m['shiny'];integer(m['variant'],0,0);integer(m['luck'],0,0)
def gen(m):
 d={k:m[j]for k,j in [('id','id'),('form','form'),('gender','gender'),('nature','nature'),('ability_index','abilityIndex'),('fusion_species','fusion'),('fusion_form','fusionForm'),('fusion_gender','fusionGender'),('fusion_ability_index','fusionAbilityIndex'),('tera_type','tera'),('boss_segments','bossSegments')]}
 d['fusion_species']=d['fusion_species']or -1
 return '{'+','.join('.'+k+'='+str(v)for k,v in d.items())+',.ivs='+a(m['ivs'])+',.friendship='+str(m.get('friendship',-1))+'}'
lines=['/* Native base -> moveset -> finish -> explicit empty-item/stat owner observations. */','typedef struct{int mode,species,level,dbl,charms,ability,passive,stats[6],final_stats[6];double hp,exp;int n,moves[4][4],ntypes,types[2];pr_rng before,gen_before,gen_after,after,pool_after;pr_enemy_gen pre,final;}native_finish_case;','static const native_finish_case native_finish_cases[]={']
for r,e in zip(x['rows'],expected):
 assert set(r)=={'mode','species','level','sample','charms','double','weather','terrain','before','generation','after','final','refreshed','poolAfter'}
 for k in ['mode','species','level','sample','charms']:integer(r[k],0,70000)
 boolean(r['double']);assert tuple(r[k]for k in ['mode','species','level','double','sample','charms'])==e
 integer(r['weather'],0,0);integer(r['terrain'],0,0)
 g=r['generation'];assert set(g)=={'before','after','input','pool','implemented','powers','moves'}
 m=g['input'];f=r['final'];ref=r['refreshed'];frame(m);frame(f,True);frame(ref)
 for v in [m,f,ref]:assert v['species']==r['species']and v['level']==r['level']and bool(v['fusion'])==(r['mode']==2)
 assert 0<len(g['pool'])<=64 and len(g['implemented'])==len(g['pool'])
 for t,b in zip(g['pool'],g['implemented']):
  assert len(t)==3;integer(t[0],-1,100000);integer(t[1],1,1999);integer(t[2],0,7);boolean(b)
 required=list(dict.fromkeys(t[1]for t,b in zip(g['pool'],g['implemented'])if b))
 assert [t[0]for t in g['powers']]==required
 for t in g['powers']:assert len(t)==2 and type(t[1])in[int,float]and math.isfinite(t[1])and t[1]>=0
 assert 0<len(g['moves'])<=4
 for t in g['moves']:
  assert set(t)=={'move','used','up','max'};integer(t['move'],1,1999);integer(t['used'],0,0);integer(t['up'],-4,3);integer(t['max'],-1,-1)
 moves='{'+','.join(a([v[k]for k in ['move','used','up','max']])for v in g['moves'])+'}'
 lines.append('{'+','.join(map(str,[r['mode'],r['species'],r['level'],int(r['double']),r['charms'],m['ability'],m['passiveAbility']]))+','+a(m['stats'])+','+a(ref['stats'])+','+str(ref['hp'])+','+str(f['exp'])+','+str(len(g['moves']))+','+moves+','+str(len(ref['types']))+','+a(ref['types'])+','+','.join(rng(v)for v in [r['before'],g['before'],g['after'],r['after'],r['poolAfter']])+','+gen(m)+','+gen(f)+'},')
lines.append('};');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
