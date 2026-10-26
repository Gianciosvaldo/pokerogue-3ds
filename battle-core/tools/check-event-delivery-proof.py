#!/usr/bin/env python3
"""Actual source observations, transport boundaries and compiled fault controls."""
from pathlib import Path
import copy,gzip,json,os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[2];BC=ROOT/'battle-core';GC=ROOT/'game-core'
def run(args,good=True):
 p=subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if (p.returncode==0)!=good:raise RuntimeError(str(args)+'\n'+p.stdout[-5000:])
 return p.stdout
cc=os.environ.get('CC','cc');flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['battle-core/include','battle-core/gen/out','battle-core/tests','game-core/include','game-core/src','game-core/tests','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out']]]
with tempfile.TemporaryDirectory(prefix='event-delivery-proof-')as td:
 temp=Path(td);raw=gzip.decompress((BC/'tests/fixtures/event-delivery.native.json.gz').read_bytes());capture=temp/'raw.json';capture.write_bytes(raw);inc=temp/'native.inc';run(['python3',BC/'tools/build-event-delivery-proof.py',capture,inc]);assert inc.read_bytes()==(BC/'tests/event-delivery.native.inc').read_bytes()
 print(run(['make','-C',BC,'event-delivery-host-test']),flush=True);print(run(['make','-C',GC,'event-delivery-host-test']),flush=True)
 j=json.loads(raw);bad=[]
 def malformed(label,edit):
  d=copy.deepcopy(j);edit(d);bad.append((label,json.dumps(d)))
 malformed('missing hit',lambda d:d['hits'].pop())
 malformed('missing text',lambda d:d['texts'].pop())
 malformed('missing phase',lambda d:d['phases'].pop())
 malformed('wrong order',lambda d:d['order'].__setitem__(0,1))
 malformed('missing actor',lambda d:d['before']['actors'].pop())
 malformed('wrong identity',lambda d:d['after']['actors'][0].__setitem__('id',-1))
 malformed('missing environment',lambda d:d['before'].pop('weather'))
 malformed('boolean version',lambda d:d.__setitem__('version',True))
 malformed('invalid RNG',lambda d:d['after'].__setitem__('battle','!rnd,-1,0,0,0'))
 malformed('fractional damage',lambda d:d['records'][0].__setitem__('damage',1.5))
 malformed('nonboolean critical',lambda d:d['hits'][0].__setitem__('critical',1))
 malformed('missing locale',lambda d:d['locales'].pop('move-trigger'))
 bad.append(('duplicate key',raw.decode().replace('"version":1','"version":1,"version":1',1)))
 for n,(label,payload)in enumerate(bad):
  src=temp/f'bad-{n}.json';src.write_text(payload);run(['python3',BC/'tools/build-event-delivery-proof.py',src,temp/'bad.inc'],False)
 faults=[
 ('battle-core/src/events.c','if (pending) bt->event_sink(e->events + e->n_events_acknowledged, pending, bt->event_sink_ctx);',';','drop full chunks'),
 ('battle-core/src/events.c','if (pending) bt->event_sink(e->events + e->n_events_acknowledged, pending, bt->event_sink_ctx);','if(bt->event_sink)bt->event_sink(e->events,e->n_events,bt->event_sink_ctx);','send acknowledged events twice'),
 ('battle-core/src/events.c','e->n_events = e->n_events_acknowledged = 0;','e->n_events = 0;','retain stale acknowledgement'),
 ('battle-core/src/events.c','{ e->events_overflow = true; return NULL; }','{ return NULL; }','hide default overflow'),
 ('battle-core/src/events.c','if (e->n_events_acknowledged > e->n_events) e->n_events_acknowledged = 0;',';','lose direct-reset compatibility'),
 ('battle-core/src/phases.c','event->actor_ids_known=true;','event->actor_ids_known=false;','lose Tera annotation after flush'),
 ('game-core/src/gc_battle.c','bt->event_sink = battle_event_chunk; bt->event_sink_ctx = run;','bt->event_sink = NULL; bt->event_sink_ctx = run;','omit real controller sink'),
 ('game-core/src/gc_battle.c','gc_ev_battle_chunk(run, events, count);','gc_ev_battle_chunk(run, events, count-1);','truncate controller chunk'),
 ('game-core/src/gc_battle.c','note_revealed_chunk(run, events, count);',';','lose early ability reveal'),
 ('game-core/src/gc_run.c','pr_event_acknowledge(e);',';','omit final acknowledgement'),
 ('game-core/src/gc_run.c','int first = e->n_events_acknowledged;','int first = 0;','repeat an acknowledged final batch'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=temp/f'fault-{n}';folder.mkdir();source=(ROOT/file).read_text();assert source.count(old)==1,(label,source.count(old));src=folder/Path(file).name;src.write_text(source.replace(old,new));obj=folder/'fault.o';run([cc,*flags,'-c',src,'-o',obj])
  tests=['host']if file.startswith('game-core')else['native','guards'];caught=False
  for kind in tests:
   exe=folder/kind
   if kind=='host':run([cc,*flags,GC/'tests/event_delivery_host_test.c',obj,GC/'build/libgc.a',GC/'build/reward-core/libreward.a',GC/'build/ai-core/libai.a',GC/'build/run-core/librun.a',GC/'build/run-core/battle-core/libprcore.a','-lm','-o',exe])
   else:run([cc,*flags,*(['-DEVENT_DELIVERY_STREAM']if kind=='native'else[]),BC/('tests/event-delivery-test.c'if kind=='native'else'tests/event-delivery-guards-test.c'),obj,BC/'build/libprcore.a',*(['-Wl,--wrap=pr_phase_run']if kind=='native'else[]),'-lm','-o',exe])
   p=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
   if p.returncode:caught=True;break
  assert caught,('undetected compiled fault',label)
  print('Caught compiled fault:',label,flush=True)
 for n,label in enumerate(['last message','damage snapshot','battle RNG']):
  d=copy.deepcopy(j)
  if n==0:d['texts'][-1]+='!'
  elif n==1:d['hits'][-1]['hp']+=1
  else:
   f=d['after']['battle'].split(',');f[1]=str(int(f[1])+1);d['after']['battle']=','.join(f)
  folder=temp/f'forged-{n}';folder.mkdir();src=folder/'input.json';src.write_text(json.dumps(d));inc=folder/'fixture.inc';run(['python3',BC/'tools/build-event-delivery-proof.py',src,inc]);test=folder/'test.c';test.write_text((BC/'tests/event-delivery-test.c').read_text().replace('#include "event-delivery.native.inc"','#include "'+str(inc)+'"'));exe=folder/'test';run([cc,*flags,'-DEVENT_DELIVERY_STREAM',test,BC/'build/libprcore.a','-Wl,--wrap=pr_phase_run','-lm','-o',exe]);run([exe],False);print('Rejected forged observation:',label,flush=True)
 print(f'Event delivery proof: {len(faults)} compiled faults, {len(bad)} malformed captures and 3 forged observations rejected')
