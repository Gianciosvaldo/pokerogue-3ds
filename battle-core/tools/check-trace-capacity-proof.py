#!/usr/bin/env python3
"""Retain the native long turn and reject compiled trace-limit regressions."""
from pathlib import Path
import copy,gzip,json,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];BC=ROOT/'battle-core'
def run(args,good=True):
 p=subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if (p.returncode==0)!=good:raise RuntimeError(str(args)+'\n'+p.stdout[-5000:])
 return p.stdout
cc=os.environ.get('CC','cc');flags=['-O2','-g','-std=c99','-ffp-contract=off','-I'+str(BC/'include'),'-I'+str(BC/'gen/out')]
with tempfile.TemporaryDirectory(prefix='trace-capacity-proof-')as td:
 temp=Path(td);raw=gzip.decompress((BC/'tests/fixtures/trace-capacity.native.json.gz').read_bytes());capture=temp/'raw.json';capture.write_bytes(raw)
 inc=temp/'native.inc';run(['python3',BC/'tools/build-trace-capacity-proof.py',capture,inc]);assert inc.read_bytes()==(BC/'tests/trace-capacity.native.inc').read_bytes()
 print(run(['make','-C',BC,'trace-capacity-host-test']),flush=True)
 source=(BC/'src/engine.c').read_text()
 faults=[
 ('if (e->n_trace >= PR_MAX_TRACE) { e->trace_overflow = true; return; }','if (e->n_trace >= PR_MAX_TRACE) { e->overflow = true; return; }','treat diagnostic exhaustion as engine failure'),
 ('        ph = shift_phase(bt, e);\n    }\n    bt->engine = NULL;','        if(e->n_trace>=PR_MAX_TRACE){e->overflow=true;continue;}\n        ph = shift_phase(bt, e);\n    }\n    bt->engine = NULL;','stop at a full diagnostic prefix'),
 ('if (e->n_trace >= PR_MAX_TRACE) { e->trace_overflow = true; return; }','if (e->n_trace >= PR_MAX_TRACE) { return; }','hide trace truncation'),
 ('    else e->trace_overflow = true;','    else { /* silently lose the Victory entry */ }','hide continuation truncation'),
 ('char *out = e->trace[e->n_trace++];','char *out = e->trace[0];e->n_trace++;','overwrite the retained prefix'),
 ]
 for n,(old,new,label)in enumerate(faults):
  assert source.count(old)==1,(label,source.count(old));folder=temp/f'fault-{n}';folder.mkdir();src=folder/'engine.c';src.write_text(source.replace(old,new));obj=folder/'engine.o';run([cc,*flags,'-c',src,'-o',obj]);caught=False
  for test in ['trace-capacity-test','trace-capacity-guards-test']:
   exe=folder/test;run([cc,*flags,BC/f'tests/{test}.c',obj,BC/'build/libprcore.a',*(['-Wl,--wrap=pr_phase_run']if test=='trace-capacity-test'else[]),'-lm','-o',exe]);p=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
   if p.returncode:caught=True;break
  assert caught,('undetected compiled fault',label)
  print('Caught compiled fault:',label,flush=True)
 j=json.loads(raw);bad=[]
 def malformed(label,edit):
  d=copy.deepcopy(j);edit(d);bad.append((label,json.dumps(d)))
 malformed('missing hit',lambda d:d['hits'].pop())
 malformed('missing text',lambda d:d['texts'].pop())
 malformed('missing phase',lambda d:d['phases'].pop())
 malformed('non-string phase',lambda d:d['phases'].__setitem__(5,123))
 malformed('wrong order',lambda d:d['order'].__setitem__(0,1))
 malformed('missing actor',lambda d:d['before']['actors'].pop())
 malformed('missing passive',lambda d:d['before']['actors'][2].pop('passive'))
 malformed('invalid passive',lambda d:d['before']['actors'][2].__setitem__('passive',True))
 malformed('missing environment',lambda d:d['before'].pop('weather'))
 malformed('boolean version',lambda d:d.__setitem__('version',True))
 malformed('invalid RNG',lambda d:d['after'].__setitem__('battle','!rnd,-1,0,0,0'))
 malformed('fractional damage',lambda d:d['records'][0].__setitem__('damage',1.5))
 malformed('nonboolean critical',lambda d:d['hits'][0].__setitem__('critical',1))
 malformed('missing locale',lambda d:d['locales'].pop('move-trigger'))
 bad.append(('duplicate key',raw.decode().replace('"version":1','"version":1,"version":1',1)))
 for n,(label,payload)in enumerate(bad):
  src=temp/f'bad-{n}.json';src.write_text(payload);run(['python3',BC/'tools/build-trace-capacity-proof.py',src,temp/'bad.inc'],False)
 for n,label in enumerate(['late phase','last message','battle RNG']):
  d=copy.deepcopy(j)
  if n==0:d['phases'][-2]='MessagePhase'
  elif n==1:d['texts'][-1]+='!'
  else:
   f=d['after']['battle'].split(',');f[1]=str(int(f[1])+1);d['after']['battle']=','.join(f)
  folder=temp/f'forged-{n}';folder.mkdir();src=folder/'input.json';src.write_text(json.dumps(d));inc=folder/'fixture.inc';run(['python3',BC/'tools/build-trace-capacity-proof.py',src,inc]);test=folder/'test.c';test.write_text((BC/'tests/trace-capacity-test.c').read_text().replace('#include "trace-capacity.native.inc"','#include "'+str(inc)+'"'));exe=folder/'test';run([cc,*flags,test,BC/'build/libprcore.a','-Wl,--wrap=pr_phase_run','-lm','-o',exe]);run([exe],False);print('Rejected forged observation:',label,flush=True)
 print(f'Trace capacity proof: {len(faults)} compiled faults, {len(bad)} malformed captures and 3 forged observations rejected')
