#!/usr/bin/env python3
"""Strict transport of real plain Gym constructors, including every native call."""
import gzip,hashlib,json,math,sys
from pathlib import Path

def unique(pairs):
 d={}
 for k,v in pairs:assert k not in d;d[k]=v
 return d

def integer(v,lo,hi):assert type(v)==int and lo<=v<=hi;return v

def boolean(v):assert type(v)==bool

def arr(v):return '{'+','.join(map(str,v))+'}'

def rng(v):
 assert type(v)==list and len(v)==4;integer(v[0],0,2097152)
 assert all(type(t)in(int,float)and math.isfinite(t)and 0<=t<1 for t in v[1:])
 return arr(v+[0])

def validate(p,member,initial=False):
 base={'slot','onField','trainer','boss','passive','willTera','species','form','level','id','abilityIndex','ability','gender','nature','ivs','stats','fusion','tera','hp','friendship','shiny','variant','luck','moves'}
 rich={'initialTeamIndex','passiveAbility','bossSegments','bossSegmentIndex','storedPassive'}
 assert set(p)<=base|rich and base|{'initialTeamIndex','passiveAbility'}<=set(p)
 assert p['initialTeamIndex']==member and type(p['initialTeamIndex'])==int
 integer(p['passiveAbility'],0,65535)
 for k in ['bossSegments','bossSegmentIndex']:
  if k in p:assert type(p[k])==int and p[k]==0
 if 'storedPassive'in p:assert p['storedPassive']is False
 for k in ['onField','trainer','boss','passive','willTera','shiny']:boolean(p[k])
 assert p['trainer']and not any(p[k]for k in ['onField','boss','passive','willTera'])
 integer(p['slot'],1,2);integer(p['species'],1,65535);integer(p['form'],0,255);integer(p['level'],1,201)
 integer(p['id'],0,4294967295);integer(p['abilityIndex'],0,2);integer(p['ability'],0,65535)
 integer(p['gender'],-1,1);integer(p['nature'],0,24);assert p['fusion']==-1
 integer(p['tera'],0,18);integer(p['hp'],1,2147483647);integer(p['friendship'],0,255)
 integer(p['variant'],0,2);integer(p['luck'],0,3)
 assert len(p['ivs'])==len(p['stats'])==6
 for v in p['ivs']:integer(v,0,31)
 for v in p['stats']:integer(v,1,2147483647)
 if initial=='api':assert p['moves']==[]
 elif initial:assert p['moves']is None
 else:
  assert type(p['moves'])==list and len(p['moves'])<=4
  for m in p['moves']:
   assert set(m)in[{'move','ppUsed','ppUp'},{'move','ppUsed','ppUp','maxPp'}]
   integer(m['move'],1,920);assert m['ppUsed']==0;integer(m['ppUp'],-4,3)
   if 'maxPp'in m:assert type(m['maxPp'])==int and m['maxPp']==-1

def gen(p):
 fields={'id':str(p['id'])+'u','form':p['form'],'gender':p['gender'],'nature':p['nature'],'ability_index':p['abilityIndex'],'tera_type':p['tera'],'fusion_species':p['fusion'],'ivs':arr(p['ivs']),'friendship':p['friendship'],'shiny':str(p['shiny']).lower(),'variant':p['variant'],'luck':p['luck']}
 return '{'+','.join('.'+k+'='+str(v)for k,v in fields.items())+'}'

def template(t):
 assert set(t)=={'size','strength','kind','subSizes','members'}
 integer(t['size'],1,6);integer(t['strength'],0,5);integer(t['kind'],0,3)
 assert len(t['members'])==t['size']and type(t['subSizes'])==list
 offsets=[0]*t['size']
 if t['subSizes']:
  assert sum(t['subSizes'])==t['size'];start=0
  for n in t['subSizes']:integer(n,1,6);offsets[start:start+n]=[start]*n;start+=n
 strengths=[];members=[]
 for m in t['members']:
  assert set(m)=={'strength','kind','same','balanced'}
  integer(m['strength'],0,5);integer(m['kind'],0,3);boolean(m['same']);boolean(m['balanced'])
  strengths.append(m['strength']);members.append((m['kind']<<2)|(int(m['balanced'])<<1)|int(m['same']))
 return '{.size='+str(t['size'])+',.strength='+str(t['strength'])+',.evo_kind='+str(t['kind'])+',.strengths='+arr(strengths)+',.member='+arr(members)+',.offsets='+arr(offsets)+'}'

path=Path(sys.argv[1]);data=path.read_bytes();x=json.loads(gzip.decompress(data)if path.suffix=='.gz'else data,object_pairs_hook=unique)
assert set(x)=={'version','scope','upstream','rows','parties','api'}and x['version']==2 and type(x['version'])==int
assert x['scope']=='default plain Gym trainer constructors with egg moves'
assert x['upstream']=='8555c08c823b856cbec4eb99ca84ea52a955836d'
expected=[(t,w,l,False)for t in[200,216,222,223]for w in[31,91,131]for l in[79,80,120,121,169,170,200,201]for _ in range(2)]
expected +=[(t,w,l,True)for t in[222,223]for w in[31,91,131]for l in[79,80,120,121,169,170,200,201]for _ in range(2)]
assert len(x['rows'])==len(x['parties'])==len(expected)==288
assert len(x['api'])==18
assert hashlib.sha256(json.dumps(x['rows'],sort_keys=True,separators=(',',':')).encode()).hexdigest()=='70d5ef50daad60849e9e2e98d57e3910955e1b1f9251ee32b4007abfcd3b66a4'
assert hashlib.sha256(json.dumps(x['parties'],sort_keys=True,separators=(',',':')).encode()).hexdigest()=='2c00df7ab09bb2585804355b5307bf31239b818a47f4f8cce8fd01090da32e4f'
member_rows=[];group_rows=[]
for full,groups in[(False,x['rows']),(True,x['parties'])]:
 for index,r in enumerate(groups):
  assert set(r)==({'index','type','wave','level','double','template','environment','ambient','members','refreshed'}if full else {'index','type','wave','level','double','template','environment','ambient','atMoves','atConstructor','finished'})
  assert r['index']==index and type(r['index'])==int
  for k in ['type','wave','level']:integer(r[k],1,65535)
  boolean(r['double']);assert (r['type'],r['wave'],r['level'],r['double'])==expected[index]
  t=template(r['template']);ambient=rng(r['ambient']);e=r['environment']
  assert set(e)=={'trainerId','secretId','biome','cycle','weather','terrain','allowEggMoves','configBoss','staticParty','instantTeras','eventActive','eventChance','eventMultiplier'}
  assert e['allowEggMoves']is True and e['configBoss']is True and e['staticParty']is True and e['instantTeras']==[]
  assert (e['trainerId'],e['secretId'],e['biome'],e['weather'],e['terrain'],e['eventActive'],e['eventChance'],e['eventMultiplier'])==(12345,54321,0,0,0,False,0,1)
  for k in ['trainerId','secretId','biome','weather','terrain']:integer(e[k],0,65535)
  for k in ['eventChance','eventMultiplier']:assert type(e[k])in(int,float)and math.isfinite(e[k])
  assert e['eventActive']is False;integer(e['cycle'],0,35);assert e['cycle']%5==0
  members=r['members']if full else[r];n=len(members);start=len(member_rows)
  assert n==(r['template']['size']if full else 1)
  if full:assert len(r['refreshed'])==n
  for member,m in enumerate(members):
   if full:assert set(m)=={'member','atMoves','atConstructor','finished'}and m['member']==member and type(m['member'])==int
   a,c=m['atMoves'],m['atConstructor']
   assert set(a)=={'before','input','after','output'}and set(c)=={'before','input','after','generations','output'}
   assert c['generations']==[a]
   p=a['input'];arg=c['input'];assert set(arg)=={'species','level','slot','boss','shinyLock','dataSource','postProcess','forRival'}
   assert arg=={'species':p['species'],'level':r['level'],'slot':p['slot'],'boss':False,'shinyLock':False,'dataSource':False,'postProcess':False,'forRival':False}
   for k in ['boss','shinyLock','dataSource','postProcess','forRival']:boolean(arg[k])
   integer(arg['species'],1,65535);integer(arg['level'],1,201);integer(arg['slot'],1,2)
   validate(p,member,True);validate(a['output'],member);validate(c['output'],member);validate(m['finished'],member)
   assert (p['level'],p['slot'])==(r['level'],2 if r['double']and member%2 else 1)
   assert c['output']==m['finished']and c['output']['moves']==a['output']['moves']
   assert {k:v for k,v in a['output'].items()if k!='moves'}=={k:v for k,v in p.items()if k!='moves'}
   refreshed=r['refreshed'][member]if full else m['finished'];validate(refreshed,member)
   assert {k:v for k,v in refreshed.items()if k not in ['stats','hp']}=={k:v for k,v in m['finished'].items()if k not in ['stats','hp']}
   moves=a['output']['moves'];fields=[p['species'],p['level'],p['slot'],p['ability'],p['tera'],gen(p),gen(m['finished']),arr(p['stats']),p['hp'],arr(refreshed['stats']),refreshed['hp'],len(moves),arr([v['move']for v in moves]),arr([v['ppUp']for v in moves]),rng(c['before']),rng(a['before']),rng(a['after']),rng(c['after'])]
   member_rows.append('{'+','.join(map(str,fields))+'},')
  fields=[index,r['type'],r['wave'],r['level'],str(r['double']).lower(),str(full).lower(),e['cycle'],'true',t,ambient,start,n]
  group_rows.append('{'+','.join(map(str,fields))+'},')
assert len(member_rows)==1824
api_rows=[]
api_expected=[(form,level)for form in[2,3,5]for level in[80,170,201]for _ in range(2)]
for index,r in enumerate(x['api']):
 assert set(r)=={'index','type','wave','level','form','environment','atMoves'}
 assert r['index']==index and type(r['index'])==int and (r['form'],r['level'])==api_expected[index]
 for k in ['type','wave','level','form']:integer(r[k],1,65535)
 assert r['type']==200 and r['wave']==91
 e=r['environment'];assert set(e)=={'trainerId','secretId','biome','cycle','weather','terrain','allowEggMoves','configBoss','staticParty','instantTeras','eventActive','eventChance','eventMultiplier'}
 assert e['allowEggMoves']is True and e['configBoss']is True and e['staticParty']is True and e['instantTeras']==[]
 assert e['eventActive']is False and e['eventChance']==0 and e['eventMultiplier']==1
 assert e['trainerId']==12345 and e['secretId']==54321 and e['biome']==e['weather']==e['terrain']==0
 for k in ['trainerId','secretId','biome','weather','terrain']:integer(e[k],0,65535)
 for k in ['eventChance','eventMultiplier']:assert type(e[k])in(int,float)and math.isfinite(e[k])
 integer(e['cycle'],0,35);assert e['cycle']%5==0
 a=r['atMoves'];assert set(a)=={'before','input','after','output'}
 p=a['input'];o=a['output'];validate(p,0,'api');validate(o,0)
 assert p['species']==966 and p['form']==r['form']and p['level']==r['level']and p['slot']==1 and p['abilityIndex']==0
 assert {k:v for k,v in p.items()if k!='moves'}=={k:v for k,v in o.items()if k!='moves'}
 moves=o['moves'];fields=[p['species'],p['level'],p['slot'],p['ability'],p['tera'],gen(p),gen(o),arr(p['stats']),p['hp'],arr(o['stats']),o['hp'],len(moves),arr([v['move']for v in moves]),arr([v['ppUp']for v in moves]),rng(a['before']),rng(a['before']),rng(a['after']),rng(a['after'])]
 api_rows.append('{'+str(r['type'])+','+str(r['wave'])+',{'+','.join(map(str,fields))+'}},')
Path(sys.argv[2]).write_text('\n'.join(['/* Actual native plain Gym constructors; generated transport. */','static const trainer_member_case trainer_members[]={',*member_rows,'};','static const trainer_party_case trainer_parties[]={',*group_rows,'};','static const struct {int type,wave;trainer_member_case member;} gym_api_cases[]={',*api_rows,'};'])+'\n')
print('Native plain Gym transport: 288 first members + 288 full parties / 1536 members, 18 direct generator controls')
