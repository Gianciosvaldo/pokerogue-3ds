#!/usr/bin/env python3
"""Transport actual Trainer.genPartyMember observations; no C-derived expectations."""
import gzip,hashlib,json,math,sys
from pathlib import Path

def unique(pairs):
 d={}
 for k,v in pairs: assert k not in d,'duplicate key';d[k]=v
 return d

def integer(v,lo,hi): assert type(v)==int and lo<=v<=hi;return v

def boolean(v): assert type(v)==bool

def arr(v): return '{'+','.join(map(str,v))+'}'

def rng(v):
 assert type(v)==list and len(v)==4
 integer(v[0],0,2097152)
 assert all(type(t)in(int,float)and math.isfinite(t)and 0<=t<1 for t in v[1:])
 return arr(v+[0])

def frame(p,initial=False):
 assert set(p)=={'slot','onField','trainer','boss','passive','willTera','species','form','level','id','abilityIndex','ability','gender','nature','ivs','stats','fusion','tera','hp','friendship','shiny','variant','luck','moves'}
 for k in ['onField','trainer','boss','passive','willTera','shiny']: boolean(p[k])
 assert p['trainer'] and not any(p[k]for k in ['onField','boss','passive','willTera'])
 integer(p['slot'],1,2);integer(p['species'],1,65535);integer(p['form'],0,255);integer(p['level'],1,201)
 integer(p['id'],0,4294967295);integer(p['abilityIndex'],0,2);integer(p['ability'],0,65535)
 integer(p['gender'],-1,1);integer(p['nature'],0,24);assert p['fusion']==-1
 integer(p['tera'],0,18);integer(p['hp'],1,2147483647);integer(p['friendship'],0,255)
 integer(p['variant'],0,2);integer(p['luck'],0,3)
 assert len(p['ivs'])==len(p['stats'])==6
 for v in p['ivs']: integer(v,0,31)
 for v in p['stats']: integer(v,1,2147483647)
 if initial: assert p['moves'] is None
 else:
  assert type(p['moves'])==list and len(p['moves'])<=4
  for m in p['moves']:
   assert set(m)in[{'move','ppUsed','ppUp'},{'move','ppUsed','ppUp','maxPp'}]
   integer(m['move'],1,920);assert m['ppUsed']==0;integer(m['ppUp'],-4,3)
   if 'maxPp'in m: assert type(m['maxPp'])==int and m['maxPp']==-1

def gen(p):
 fields={'id':str(p['id'])+'u','form':p['form'],'gender':p['gender'],'nature':p['nature'],'ability_index':p['abilityIndex'],'tera_type':p['tera'],'fusion_species':p['fusion'],'ivs':arr(p['ivs']),'friendship':p['friendship'],'shiny':str(p['shiny']).lower(),'variant':p['variant'],'luck':p['luck']}
 return '{'+','.join('.'+k+'='+str(v)for k,v in fields.items())+'}'

def template(t):
 assert set(t)=={'size','strength','kind','subSizes','members'}
 integer(t['size'],1,6);integer(t['strength'],0,5);integer(t['kind'],0,3)
 assert len(t['members'])==t['size'] and type(t['subSizes'])==list
 offsets=[0]*t['size']
 if t['subSizes']:
  assert sum(t['subSizes'])==t['size'];start=0
  for n in t['subSizes']:
   integer(n,1,6);offsets[start:start+n]=[start]*n;start+=n
 strengths=[];members=[]
 for m in t['members']:
  assert set(m)=={'strength','kind','same','balanced'}
  integer(m['strength'],0,5);integer(m['kind'],0,3);boolean(m['same']);boolean(m['balanced'])
  strengths.append(m['strength']);members.append((m['kind']<<2)|(int(m['balanced'])<<1)|int(m['same']))
 return '{.size='+str(t['size'])+',.strength='+str(t['strength'])+',.evo_kind='+str(t['kind'])+',.strengths='+arr(strengths)+',.member='+arr(members)+',.offsets='+arr(offsets)+'}'

capture=Path(sys.argv[1]);data=capture.read_bytes();data=gzip.decompress(data)if capture.suffix=='.gz'else data
x=json.loads(data,object_pairs_hook=unique)
assert set(x)=={'version','upstream','rows','parties'}and type(x['version'])==int and x['version']==2
assert x['upstream']=='8555c08c823b856cbec4eb99ca84ea52a955836d'
assert len(x['rows'])==1400 and len(x['parties'])==856
# Original 696 actual constructor observations must remain intact.
assert hashlib.sha256(json.dumps(x['rows'][:696],sort_keys=True,separators=(',',':')).encode()).hexdigest()=='2edc711df155e8bcaf78a3d20f5147c8acfc3a5a970daa63cd5a19be9b7c618d'
expected=[(t,w,l)for t in[63,11]for w in[11,99]for l in[5,24,25,69,70,100]for _ in range(8)]
expected +=[(t,w,l)for t in range(1,64)for w in[11,99]for l in[5,24]for _ in range(2)]
higher=[1,11,6,36,58,59,62,63]
expected +=[(t,w,l)for t in higher for w in[11,99]for l in[24,25,39,40,41,54,55,69,70,71,79,80,100,101,120,121,160,161,169,170,200,201]for _ in range(2)]
party_expected=[(t,w,l)for t in range(1,64)for w in[11,99]for l in[5,24]for _ in range(2)]
party_expected +=[(t,w,l)for t in higher for w in[11,99]for l in[25,40,41,55,71,80,101,121,161,170,201]for _ in range(2)]
assert hashlib.sha256(json.dumps(x['parties'][:504],sort_keys=True,separators=(',',':')).encode()).hexdigest()=='124027450c5146ecdda3e896f737fe5aca10474109978f1a8ef9d9a3610059cf'
member_rows=[];group_rows=[]
for full,groups in [(False,x['rows']),(True,x['parties'])]:
 for index,r in enumerate(groups):
  assert set(r)==({'index','type','wave','level','double','template','environment','ambient','members','refreshed'}if full else {'index','type','wave','level','double','template','environment','ambient','atMoves','atConstructor','finished'})
  assert r['index']==index and type(r['index'])==int
  assert (r['type'],r['wave'],r['level'])==(party_expected[index]if full else expected[index])
  integer(r['type'],1,63);integer(r['wave'],1,200);integer(r['level'],1,201)
  boolean(r['double']);t=template(r['template']);ambient=rng(r['ambient']);e=r['environment']
  assert set(e)=={'trainerId','secretId','biome','cycle','weather','terrain','allowEggMoves','eventActive','eventChance','eventMultiplier'}
  assert (e['trainerId'],e['secretId'],e['biome'],e['weather'],e['terrain'],e['eventActive'],e['eventChance'],e['eventMultiplier'])==(12345,54321,0,0,0,False,0,1)
  for key in ['trainerId','secretId','biome','weather','terrain']:integer(e[key],0,65535)
  for key in ['eventChance','eventMultiplier']:assert type(e[key])in(int,float)and math.isfinite(e[key])
  integer(e['cycle'],0,35);assert e['cycle']%5==0;assert e['allowEggMoves'] is False;boolean(e['eventActive'])
  members=r['members']if full else [r];n=len(members);start=len(member_rows)
  assert n==(r['template']['size']if full else 1)
  if full: assert len(r['refreshed'])==n
  for member,m in enumerate(members):
   if full:
    assert set(m)=={'member','atMoves','atConstructor','finished'}and m['member']==member
    integer(m['member'],0,5)
   a,c=m['atMoves'],m['atConstructor'];assert set(a)=={'before','input','after','output'}and set(c)=={'before','after','output'}
   frame(a['input'],True);frame(a['output']);frame(c['output']);frame(m['finished']);assert c['output']==m['finished']
   p=a['input'];assert (p['level'],p['slot'])==(r['level'],2 if r['double']and member%2 else 1)
   assert {k:v for k,v in a['output'].items()if k!='moves'}=={k:v for k,v in p.items()if k!='moves'}
   assert c['output']['moves']==a['output']['moves']
   refreshed=r['refreshed'][member]if full else m['finished'];frame(refreshed)
   assert {k:v for k,v in refreshed.items()if k not in ['stats','hp']}=={k:v for k,v in m['finished'].items()if k not in ['stats','hp']}
   moves=a['output']['moves'];field=[str(p['species']),str(p['level']),str(p['slot']),str(p['ability']),str(p['tera']),gen(p),gen(m['finished']),arr(p['stats']),str(p['hp']),arr(refreshed['stats']),str(refreshed['hp']),str(len(moves)),arr([v['move']for v in moves]),arr([v['ppUp']for v in moves]),rng(c['before']),rng(a['before']),rng(a['after']),rng(c['after'])]
   member_rows.append('{'+','.join(field)+'},')
  fields=[str(index),str(r['type']),str(r['wave']),str(r['level']),str(r['double']).lower(),str(full).lower(),str(e['cycle']),str(e['allowEggMoves']).lower(),t,ambient,str(start),str(n)]
  group_rows.append('{'+','.join(fields)+'},')
assert len(member_rows)==3574
out=['/* Native Trainer.genPartyMember transport; generated, do not edit. */','static const trainer_member_case trainer_members[]={',*member_rows,'};','static const trainer_party_case trainer_parties[]={',*group_rows,'};']
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
print('Native transport: all 1880 old observations retained; 1400 first members + 856 whole parties / 2174 members')
