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
with tempfile.TemporaryDirectory(prefix='turn-order-proof-')as td:
 temp=Path(td);captures={}
 for stem in ['turn-order','turn-order-command']:
  raw=gzip.decompress((CORE/f'tests/fixtures/{stem}.native.json.gz').read_bytes());captures[stem]=json.loads(raw);src=temp/f'{stem}.json';src.write_bytes(raw);out=temp/f'{stem}.inc'
  run([sys.executable,CORE/f'tools/build-{stem}-proof.py',src,out]);assert out.read_bytes()==(CORE/f'tests/{stem}.native.inc').read_bytes()
 print(run(['make','-C',CORE,'turn-order-host-test']),flush=True)
 print(run(['make','-C',ROOT/'game-core','turn-order-host-test']),flush=True)
 bad=[]
 def malformed(stem,label,edit):
  data=copy.deepcopy(captures[stem]);edit(data);bad.append((stem,label,json.dumps(data)))
 malformed('turn-order','missing case',lambda d:d['rows'].pop())
 malformed('turn-order','duplicate coverage',lambda d:d['rows'].__setitem__(1,d['rows'][0]))
 malformed('turn-order','missing stage',lambda d:d['rows'][0]['stages'].pop())
 malformed('turn-order','invalid actor',lambda d:d['rows'][24]['order'].__setitem__(0,12))
 malformed('turn-order','invalid unsigned ID',lambda d:d['ids'].__setitem__(1,-1))
 malformed('turn-order','invalid result',lambda d:d['rows'][0]['initial'][0].__setitem__('result',99))
 malformed('turn-order','missing observation',lambda d:d['rows'][0]['stages'][0]['values'].pop())
 malformed('turn-order','invalid RNG',lambda d:d.__setitem__('globalRng','!rnd,-1,0,0,0'))
 malformed('turn-order','nonboolean applied',lambda d:d['rows'][0]['stages'][0]['values'][0].__setitem__('applied',1))
 malformed('turn-order-command','missing actor',lambda d:d['after']['actors'].pop())
 malformed('turn-order-command','missing action',lambda d:d['order'].pop())
 malformed('turn-order-command','fractional HP',lambda d:d['after']['actors'][0].__setitem__('hp',1.5))
 malformed('turn-order-command','invalid history mode',lambda d:d['after']['actors'][0]['history'][0].__setitem__('useMode',99))
 raw=gzip.decompress((CORE/'tests/fixtures/turn-order.native.json.gz').read_bytes()).decode();bad.append(('turn-order','duplicate key',raw.replace('"version":1','"version":1,"version":1',1)))
 for n,(stem,label,payload)in enumerate(bad):
  src=temp/f'bad-{n}.json';src.write_text(payload);run([sys.executable,CORE/f'tools/build-{stem}-proof.py',src,temp/'bad.inc'],False)
 faults=[
 ('history.c','\n    h->n_before = h->n_recent;','\n    h->n_before = 0;','lose preceding actors'),
 ('history.c','    h->before_complete = h->complete;','    h->before_complete = true;','hide missing suffix'),
 ('history.c','    if (at == h->n_recent) {','    if (true) {','retain duplicate identities'),
 ('history.c','    } else if (bt->n_last_turn_order == PR_MAX_TURN_ORDER + 1) {','    } else if (false) {','lose raw nine coverage'),
 ('history.c','    bt->n_last_turn_order = 0;','    ;','lose order reset'),
 ('move_attrs.c','const pr_pokemon *p = actors[k];','const pr_pokemon *p = actors[k];if(p!=bt->field[0]&&p!=bt->field[1]&&p!=bt->field[2]&&p!=bt->field[3])p=NULL;','lose bench identity'),
 ('move_attrs.c','if (last->result != MOVE_RESULT_FAIL) {','if (last->result == MOVE_RESULT_SUCCESS) {','ignore pending miss and other blockers'),
 ('move_attrs.c','if (!stopped && !complete) pr_note_unported(bt, PR_FEAT_TURN_ORDER);',';','hide unknown order'),
 ('engine.c','if (epoch != p->history_epoch) return false;','if (false) return false;','retain stale epoch aliases'),
 ('engine.c','if (serial && serial <= p->history_serial && p->history_serial - serial < kept) return true;','return true;','never reclaim aliases'),
 ('engine.c','if ((p->history_last_present & (1u << q)) && p->history_last_serial[q] == serial) return true;','if (false) return true;','drop cached selector aliases'),
 ('engine.c','return (p->history_all_present & 2) && p->history_aim_serial == serial;','return false;','drop aim alias'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=temp/f'fault-{n}';folder.mkdir();body=(CORE/'src'/file).read_text();assert body.count(old)==1,(label,body.count(old));source=folder/file;source.write_text(body.replace(old,new));obj=folder/'fault.o';run([cc,*flags,'-c',source,'-o',obj])
  failed=False
  for name in ['turn-order-test.c','turn-order-guards-test.c','turn-order-command-test.c']:
   test=CORE/'tests'/name;args=[obj];link=[]
   if name=='turn-order-test.c':
    local=folder/name;s=test.read_text().replace('#include "../src/engine.c"','#include "'+str(source if file=='engine.c'else CORE/'src/engine.c')+'"');local.write_text(s);test=local
    if file=='engine.c':args=[]
   if name=='turn-order-command-test.c':link=['-Wl,--wrap=pr_phase_run']
   exe=folder/name.replace('.c','');run([cc,*flags,'-I'+str(CORE/'tests'),test,*args,CORE/'build/libprcore.a',*link,'-lm','-o',exe])
   result=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
   if result.returncode:failed=True;break
  assert failed,('undetected compiled fault',label)
  print('Caught compiled fault:',label,flush=True)
 # Valid but forged native answers must fail the C comparison, rather than the builder.
 for n,(stem,label)in enumerate([('turn-order','power'),('turn-order-command','HP'),('turn-order-command','RNG')]):
  data=copy.deepcopy(captures[stem]);folder=temp/f'forged-{n}';folder.mkdir()
  if label=='power':v=data['rows'][0]['stages'][0]['values'][0];v['power']=3-v['power']
  elif label=='HP':data['after']['actors'][0]['hp']+=1
  else:
   f=data['after']['battle'].split(',');f[1]=str(int(f[1])+1);data['after']['battle']=','.join(f)
  src=folder/'raw.json';src.write_text(json.dumps(data));inc=folder/f'{stem}.native.inc';run([sys.executable,CORE/f'tools/build-{stem}-proof.py',src,inc])
  name=stem+'-test.c';body=(CORE/'tests'/name).read_text().replace('#include "../src/engine.c"','#include "'+str(CORE/'src/engine.c')+'"').replace('#include "'+stem+'.native.inc"','#include "'+str(inc)+'"');test=folder/name;test.write_text(body);exe=folder/'compare';link=['-Wl,--wrap=pr_phase_run']if stem.endswith('command')else[]
  run([cc,*flags,test,CORE/'build/libprcore.a',*link,'-lm','-o',exe]);run([exe],False);print('Rejected forged observation:',label,flush=True)
 print(f'Turn-order proof: {len(faults)} compiled faults, {len(bad)} malformed captures and 3 forged observations rejected')
