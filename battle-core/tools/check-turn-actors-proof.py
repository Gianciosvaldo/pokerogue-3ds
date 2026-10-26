#!/usr/bin/env python3
"""Retained native answers, actual owners and deliberate failure controls."""
from pathlib import Path
import copy,gzip,json,os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[2];CORE=ROOT/'battle-core'
def run(args,good=True):
 p=subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if (p.returncode==0)!=good:raise RuntimeError(str(args)+'\n'+p.stdout[-4000:])
 return p.stdout
cc=os.environ.get('CC','cc');flags=['-I'+str(CORE/'include'),'-I'+str(CORE/'gen/out'),'-O2','-g','-std=c99','-Wno-unused-parameter','-ffp-contract=off']
with tempfile.TemporaryDirectory(prefix='turn-actors-proof-')as td:
 temp=Path(td);captures={}
 for stem in ['turn-actors','turn-actor-command']:
  raw=gzip.decompress((CORE/f'tests/fixtures/{stem}.native.json.gz').read_bytes());captures[stem]=json.loads(raw);src=temp/f'{stem}.json';src.write_bytes(raw);out=temp/f'{stem}.inc'
  run([sys.executable,CORE/f'tools/build-{stem}-proof.py',src,out]);assert out.read_bytes()==(CORE/f'tests/{stem}.native.inc').read_bytes()
 print(run(['make','-C',CORE,'turn-actors-host-test']),flush=True)
 print(run(['make','-C',ROOT/'game-core','turn-actors-host-test']),flush=True)
 bad=[]
 def malformed(stem,label,edit):
  data=copy.deepcopy(captures[stem]);edit(data);bad.append((stem,label,json.dumps(data)))
 malformed('turn-actors','missing case',lambda d:d['rows'].pop())
 malformed('turn-actors','duplicate coverage',lambda d:d['rows'].__setitem__(1,d['rows'][0]))
 malformed('turn-actors','missing stage',lambda d:d['rows'][0]['stages'].pop())
 malformed('turn-actors','invalid actor',lambda d:d['rows'][24]['order'].__setitem__(0,12))
 malformed('turn-actors','invalid ID group',lambda d:d['rows'][0]['ids'].__setitem__(1,-1))
 malformed('turn-actors','invalid result',lambda d:d['rows'][0]['initial'][0].__setitem__('result',99))
 malformed('turn-actors','missing observation',lambda d:d['rows'][0]['stages'][0]['values'].pop())
 malformed('turn-actors','invalid RNG',lambda d:d.__setitem__('globalRng','!rnd,-1,0,0,0'))
 malformed('turn-actor-command','missing constructor state',lambda d:d['witness']['states'].pop())
 malformed('turn-actor-command','invalid constructor state',lambda d:d['witness']['states'].__setitem__(0,'!rnd,-1,0,0,0'))
 malformed('turn-actor-command','missing action',lambda d:d['order'].pop())
 malformed('turn-actor-command','fractional HP',lambda d:d['after']['actors'][2].__setitem__('hp',1.5))
 raw=gzip.decompress((CORE/'tests/fixtures/turn-actors.native.json.gz').read_bytes()).decode();bad.append(('turn-actors','duplicate key',raw.replace('"version":1','"version":1,"version":1',1)))
 for n,(stem,label,payload)in enumerate(bad):
  src=temp/f'bad-{n}.json';src.write_text(payload);run([sys.executable,CORE/f'tools/build-{stem}-proof.py',src,temp/'bad.inc'],False)
 faults=[
 ('engine.c','pr_record_turn_actor(bt, e->pool[ph].pokemon);','pr_record_turn_order(bt, e->pool[ph].pokemon->id);','queue reduces objects to IDs'),
 ('history.c','return a.actor && b.actor ? a.actor == b.actor : !a.actor && !b.actor && a.id == b.id;','return a.id == b.id;','merge distinct objects sharing an ID'),
 ('history.c','out[i] = h.before[i].actor ? h.before[i].actor : unique_order_actor(bt, h.before[i].id);','out[i] = unique_order_actor(bt, h.before[i].id);','look up live references by stale ID'),
 ('history.c','if (saved->valid && saved->count == bt->n_last_turn_order','if (bt->n_last_turn_order > PR_MAX_TURN_ORDER && saved->valid && saved->count == bt->n_last_turn_order','lose short-order reference cache'),
 ('history.c','&& !memcmp(saved->prefix, bt->last_turn_order, kept * sizeof *saved->prefix)','&& true','ignore authoritative raw prefix edits'),
 ('history.c','if (found) return NULL;','if (found) return found;','invent an actor for ambiguous raw IDs'),
 ('history.c','record_turn_actor(bt, (pr_turn_actor){actor->id, actor});','record_turn_actor(bt, (pr_turn_actor){actor->id, unique_order_actor(bt, actor->id)});','discard native actor reference'),
 ('move_attrs.c','const pr_pokemon *p = actors[k];','const pr_pokemon *p = actors[k];if(p)p=pr_pokemon_by_id(bt,p->id);','replace queued object with scene lookup'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=temp/f'fault-{n}';folder.mkdir();body=(CORE/'src'/file).read_text();assert body.count(old)==1,(label,body.count(old));source=folder/file;source.write_text(body.replace(old,new));obj=folder/'fault.o';run([cc,*flags,'-c',source,'-o',obj])
  failed=False
  for name in ['turn-actors-test.c','turn-actors-guards-test.c','turn-actor-command-test.c']:
   test=CORE/'tests'/name;args=[obj];link=[]
   if name=='turn-actors-test.c':
    local=folder/name;s=test.read_text().replace('#include "../src/engine.c"','#include "'+str(source if file=='engine.c'else CORE/'src/engine.c')+'"');local.write_text(s);test=local
    if file=='engine.c':args=[]
   if name=='turn-actor-command-test.c':link=['-Wl,--wrap=pr_phase_run']
   exe=folder/name.replace('.c','');run([cc,*flags,'-I'+str(CORE/'tests'),test,*args,CORE/'build/libprcore.a',*link,'-lm','-o',exe])
   result=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
   if result.returncode:failed=True;break
  assert failed,('undetected compiled fault',label)
  print('Caught compiled fault:',label,flush=True)
 # Valid but forged native answers must fail the C comparison, rather than the builder.
 for n,(stem,label)in enumerate([('turn-actors','power'),('turn-actor-command','HP'),('turn-actor-command','RNG')]):
  data=copy.deepcopy(captures[stem]);folder=temp/f'forged-{n}';folder.mkdir()
  if label=='power':v=data['rows'][0]['stages'][0]['values'][0];v['power']=3-v['power']
  elif label=='HP':data['after']['actors'][2]['hp']+=1
  else:
   f=data['after']['battle'].split(',');f[1]=str(int(f[1])+1);data['after']['battle']=','.join(f)
  src=folder/'raw.json';src.write_text(json.dumps(data));inc=folder/f'{stem}.native.inc';run([sys.executable,CORE/f'tools/build-{stem}-proof.py',src,inc])
  name=stem+'-test.c';body=(CORE/'tests'/name).read_text().replace('#include "../src/engine.c"','#include "'+str(CORE/'src/engine.c')+'"').replace('#include "'+stem+'.native.inc"','#include "'+str(inc)+'"');test=folder/name;test.write_text(body);exe=folder/'compare';link=['-Wl,--wrap=pr_phase_run']if stem.endswith('command')else[]
  run([cc,*flags,test,CORE/'build/libprcore.a',*link,'-lm','-o',exe]);run([exe],False);print('Rejected forged observation:',label,flush=True)
 print(f'Turn-actor proof: {len(faults)} compiled faults, {len(bad)} malformed captures and 3 forged observations rejected')
