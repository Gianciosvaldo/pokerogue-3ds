#!/usr/bin/env python3
"""Retained native answers, raw guards, real owners and deliberate failures."""
from pathlib import Path
import concurrent.futures,copy,gzip,json,os,re,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[2];CORE=ROOT/'battle-core'
def run(args,good=True,log=None):
 p=subprocess.run([str(x)for x in args],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if log:Path(log).write_text(p.stdout)
 if (p.returncode==0)!=good:raise RuntimeError(str(args)+'\n'+p.stdout[-4000:])
 return p.stdout
with tempfile.TemporaryDirectory(prefix='received-proof-')as td:
 temp=Path(td)
 for stem in ['received-history','received-command']:
  raw=gzip.decompress((CORE/f'tests/fixtures/{stem}.native.json.gz').read_bytes());source=temp/f'{stem}.json';source.write_bytes(raw);out=temp/f'{stem}.inc'
  run([sys.executable,CORE/f'tools/build-{stem}-proof.py',source,out])
  assert out.read_bytes()==(CORE/f'tests/{stem}.native.inc').read_bytes()
 output=run(['make','-C',CORE,'received-history-host-test'])
 assert '384 native cases, 1920 stages, 59520 observations; 0 mismatches/uncovered' in output
 assert re.search(r'guards: (\d+) checks passed',output) and int(re.search(r'guards: (\d+) checks passed',output)[1])>=180
 assert re.search(r'RNG; (\d+) checks passed',output) and int(re.search(r'RNG; (\d+) checks passed',output)[1])>=42
 print('\n'.join(line for line in output.splitlines() if line.startswith('Received')),flush=True)
 output=run(['make','-C',ROOT/'game-core','received-history-host-test'])
 assert re.search(r'save; (\d+) checks passed',output) and int(re.search(r'save; (\d+) checks passed',output)[1])>=29
 print('\n'.join(line for line in output.splitlines() if line.startswith('Received')),flush=True)
 j=json.loads((temp/'received-history.json').read_text());bad=[]
 def malformed(name,edit):
  data=copy.deepcopy(j);edit(data);bad.append((name,json.dumps(data)))
 malformed('missing row',lambda d:d['rows'].pop())
 malformed('duplicate coverage',lambda d:d['rows'].__setitem__(1,d['rows'][0]))
 malformed('missing stage',lambda d:d['rows'][0]['stages'].pop())
 malformed('wrong unsigned ID',lambda d:d['ids'].__setitem__(1,-1))
 malformed('missing counter',lambda d:d['rows'][0]['stages'][0]['counters'].pop())
 malformed('counter bool as int',lambda d:d['rows'][0]['stages'][0]['counters'][0].__setitem__('condition',1))
 malformed('counter fractional',lambda d:d['rows'][0]['stages'][0]['counters'][0].__setitem__('damage',1.5))
 malformed('counter nonfinite',lambda d:d['rows'][0]['stages'][0]['counters'][0].__setitem__('damage',float('nan')))
 malformed('missing raw field',lambda d:d['rows'][0]['first'].pop('critical'))
 malformed('missing RNG',lambda d:d['rows'][0]['stages'][0]['rng'].pop())
 malformed('invalid RNG',lambda d:d['rows'][0]['stages'][0]['rng'].__setitem__(0,'!rnd,-1,0,0,0'))
 malformed('unknown version',lambda d:d.__setitem__('version',3))
 malformed('negative source ID',lambda d:d['rows'][0]['first'].__setitem__('sourceId',-1))
 malformed('missing owner template',lambda d:d['templates'].pop())
 bad.append(('duplicate JSON key',(temp/'received-history.json').read_text().replace('"version":2','"version":2,"version":2',1)))
 for n,(name,payload)in enumerate(bad):
  src=temp/f'malformed-{n}.json';src.write_text(payload);run([sys.executable,CORE/'tools/build-received-history-proof.py',src,temp/'bad.inc'],good=False)
 faults=[
 ('history.c','    received_remember(&h, &record, p->n_attacks_received == 0);','    if(p->n_attacks_received<32)received_remember(&h, &record, p->n_attacks_received == 0);','drop appended summaries'),
 ('history.c','    if (first) {','    if (true) {','use newest for Rage'),
 ('history.c','    return received_any(bt, p, RECEIVED_SUPER);','    return false;','lose Enigma'),
 ('history.c','    return received_any(bt, p, RECEIVED_POSITIVE);','    return false;','lose Focus Punch'),
 ('history.c','        if (pr_are_allies(readers[context], r->source_battler_index)) continue;','        if (false) continue;','Counter includes allies'),
 ('history.c','            h->counter[q] = *r;','            double previous=h->counter[q].damage;h->counter[q]=*r;h->counter[q].damage+=previous;','sum Counter damage'),
 ('history.c','h->damage_sources[h->n_sources++] = r->source_id;','h->damage_sources[h->n_sources++] = (int32_t)r->source_id;','truncate unsigned identities'),
 ('phases.c','    pr_reset_received_history(p);','    ;','lose turn reset'),
 ('history.c','    int context = self < 0 ? 2 : self < 2 ? 0 : 1;','    int context = 0;','ignore reader context'),
 ('history.c','            else h->sources_complete = false;','            else h->sources_complete = true;','hide identity overflow'),
 ('history.c','    h->any_known = complete ? 7 : 0;','    h->any_known = 7;','invent missing-prefix answers'),
 ('history.c','    p->attacks_received[0] = record;','    p->attacks_received[0] = *r;','corrupt aliased append'),
 ('history.c','saved->count == p->n_attacks_received','true','reuse stale count'),
 ]
 cc=os.environ.get('CC','cc');flags=['-I'+str(CORE/'include'),'-I'+str(CORE/'gen/out'),'-O2','-g','-std=c99','-Wno-unused-parameter','-ffp-contract=off']
 def fault(job):
  n,(file,old,new,label)=job;folder=temp/f'fault-{n}';folder.mkdir();source=(CORE/'src'/file).read_text();assert source.count(old)==1,(label,source.count(old));mutated=folder/file;mutated.write_text(source.replace(old,new));obj=folder/'fault.o';extra=[]
  test=CORE/'tests/received-history-test.c'
  if file=='phases.c':
   test=folder/'test.c';test.write_text((CORE/'tests/received-history-test.c').read_text().replace('#include "../src/phases.c"','#include "'+str(mutated)+'"'));flags2=flags+['-I'+str(CORE/'tests')]
  else:
   run([cc,*flags,'-c',mutated,'-o',obj]);extra=[obj];flags2=flags
  exe=folder/'matrix';run([cc,*flags2,test,*extra,CORE/'build/libprcore.a','-lm','-o',exe])
  result=subprocess.run([str(exe)],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
  if result.returncode==0:
   guard=folder/'guards';run([cc,*flags,CORE/'tests/received-history-guards-test.c',*extra,CORE/'build/libprcore.a','-lm','-o',guard]);run([guard],good=False)
  return label
 with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:
  for label in pool.map(fault,enumerate(faults)):print('Caught compiled fault:',label,flush=True)
 # Well-formed forged observations must also fail the actual C comparison.
 sensitivity=[]
 forged=copy.deepcopy(j);forged['rows'][0]['stages'][0]['counters'][0]['damage']+=1
 sensitivity.append(('matrix damage','received-history',forged,'received-history-test.c'))
 command=json.loads((temp/'received-command.json').read_text())
 forged=copy.deepcopy(command);forged['after']['hp']+=1
 sensitivity.append(('command HP','received-command',forged,'received-command-test.c'))
 forged=copy.deepcopy(command);fields=forged['after']['battle'].split(',');fields[1]=str(int(fields[1])+1);forged['after']['battle']=','.join(fields)
 sensitivity.append(('command RNG','received-command',forged,'received-command-test.c'))
 for n,(label,stem,data,testname)in enumerate(sensitivity):
  folder=temp/f'forged-{n}';folder.mkdir();raw=folder/'input.json';raw.write_text(json.dumps(data));inc=folder/f'{stem}.native.inc'
  run([sys.executable,CORE/f'tools/build-{stem}-proof.py',raw,inc])
  test=folder/testname;body=(CORE/'tests'/testname).read_text().replace('#include "../src/phases.c"','#include "'+str(CORE/'src/phases.c')+'"').replace('#include "'+stem+'.native.inc"','#include "'+str(inc)+'"');test.write_text(body)
  exe=folder/'compare';run([cc,*flags,test,CORE/'build/libprcore.a','-lm','-o',exe]);run([exe],good=False);print('Rejected forged observation:',label,flush=True)
 print(f'Received proof: native matrix, raw guards, command and save owners; {len(faults)} compiled faults + {len(bad)} malformed captures + {len(sensitivity)} forged observations rejected')
