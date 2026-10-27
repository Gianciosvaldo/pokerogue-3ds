#!/usr/bin/env python3
"""Native fusion item owners, source transport, fault controls and sanitizers."""
from pathlib import Path
import copy,gzip,json,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core'
def execute(args):return subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
def command(args):
 r=execute(args)
 if r.returncode:raise RuntimeError(str(args)+'\n'+r.stdout[-5000:])
 return r.stdout
flags=['-O1','-std=c99','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','game-core/tests','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
cc=os.environ.get('CC','cc');builder=['python3',GC/'tools/build-fusion-item-evolution-proof.py'];test=GC/'tests/fusion_item_evolution_host_test.c'
with tempfile.TemporaryDirectory(prefix='fusion-item-evolution-')as td:
 temp=Path(td);raw=gzip.decompress((GC/'tests/fixtures/fusion-item-evolution.native.json.gz').read_bytes());capture=temp/'native.json';capture.write_bytes(raw);inc=temp/'native.inc';command([*builder,capture,inc]);assert inc.read_bytes()==(GC/'tests/fixtures/fusion-item-evolution.native.inc').read_bytes()
 print(command(['make','-C',GC,'fusion-item-evolution-host-test']),flush=True)
 if os.environ.get('FUSION_ITEM_EVOLUTION_SANITIZE')=='1':
  san=flags+['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'];objects=[]
  for file in ['gc_mon.c','gc_reward_lifecycle.c','gc_phases.c']:
   obj=temp/(file+'.o');command([cc,*san,'-c',GC/'src'/file,'-o',obj]);objects.append(obj)
  exe=temp/'san';command([cc,*san,test,*objects,*libs,'-lm','-o',exe]);print(command([exe]),flush=True)
  print('Scoped ASan/UBSan/LSan: mon evolution, reward lifecycle and phase execution; supporting archives unsanitized');raise SystemExit(0)
 faults=[
 ('gc_reward.c','const int lifecycle = gc_reward_lifecycle_apply(run, o, party, why);','const int lifecycle = GC_APPLY_UNHANDLED;','disconnect reward lifecycle owner'),
 ('gc_reward_lifecycle.c','    bool fusion = false;','    if (m->b.fusion_species) return GC_APPLY_UNHANDLED;\n    bool fusion = false;','restore blanket fusion rejection'),
 ('gc_mon.c','int evo = gc_mon_item_evolution(run, m, item);\n    if (evo >= 0 || !m->b.fusion_species) return evo;','return gc_mon_item_evolution(run, m, item);\n    int evo = -1;','omit secondary item selection'),
 ('gc_mon.c','int evo = gc_mon_item_evolution(run, m, item);','int evo = -1;','skip primary priority'),
 ('gc_mon.c','(fusion ? p->fusion_gender : p->gender) == c->v[0]','p->gender == c->v[0]','use primary gender'),
 ('gc_mon.c','    *fusion = false;\n    int evo = gc_mon_item_evolution','    *fusion = false;\n    if (m->pause_evolutions) return -1;\n    int evo = gc_mon_item_evolution','reject explicit items while paused'),
 ('gc_reward_lifecycle.c','request.d = fusion;','request.d = 0;','lose queued secondary flag'),
 ('gc_phases.c','const bool fusion = p->d == 1;','const bool fusion = false;','ignore selected half on confirm'),
 ('gc_mon.c','if (fusion) p->fusion_species = e->species;','if (fusion) p->species = e->species;','mutate primary half'),
 ('gc_mon.c','if (fusion) p->fusion_ability_index = (int8_t)index;','if (fusion) p->fusion_ability_index = 0;','reset secondary ability index'),
 ('gc_reward_lifecycle.c','request.d = fusion;','request.d = fusion;pr_rng_seed_int(&run->run.rng,100,0);','consume item selection RNG'),
 ('gc_mon.c','if (e->item != item || m->b.level < e->level || !pre_form_identity(m, e, true)) continue;','if (e->item == item || m->b.level < e->level || !pre_form_identity(m, e, true)) continue;','select a different item'),
 ('gc_mon.c','m->pause_evolutions = false;','m->pause_evolutions = true;','leave evolved Pokemon paused'),
 ]
 for file,old,new,label in faults:
  source=(GC/'src'/file).read_text();assert source.count(old)==1,(label,source.count(old));p=temp/file;p.write_text(source.replace(old,new));obj=temp/'fault.o';exe=temp/'fault';command([cc,*flags,'-c',p,'-o',obj]);command([cc,*flags,test,obj,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'fusion item case'in r.stdout,'surviving '+label;print('caught compiled fault: '+label,flush=True)
 # The unknown-gender guard exists independently in both level and item owners.
 source=(GC/'src/gc_mon.c').read_text();start=source.index('int gc_mon_item_evolution_target(');end=source.index('int gc_mon_item_evolutions(',start);chunk=source[start:end];old='if (!m->b.fusion_gender_known)';assert chunk.count(old)==1;source=source[:start]+chunk.replace(old,'if (false)')+source[end:];p=temp/'gc_mon.c';p.write_text(source);obj=temp/'fault.o';exe=temp/'fault';command([cc,*flags,'-c',p,'-o',obj]);command([cc,*flags,test,obj,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'fusion item case'in r.stdout,'surviving unknown gender guess';print('caught compiled fault: guess unknown secondary gender',flush=True)
 data=json.loads(raw)
 bad=[('missing case',lambda d:d['rows'].pop()),('wrong matrix',lambda d:d['rows'].reverse()),('boolean level',lambda d:d['rows'][0]['before'].__setitem__('level',True)),('boolean accepted',lambda d:d['rows'][0].__setitem__('accepted',1)),('unknown field',lambda d:d.__setitem__('extra',0)),('bad RNG',lambda d:d['rows'][0]['after'].__setitem__('global','!rnd,1,1.1,0,0')),('out of range nature',lambda d:d['rows'][0]['before'].__setitem__('nature',25)),('wrong input gender',lambda d:d['rows'][0]['before'].__setitem__('fusionGender',-1)),('invalid PP',lambda d:d['rows'][0]['after']['moves'][0].__setitem__('up',4)),('invalid queue level',lambda d:d['rows'][0]['selected'].__setitem__('lastLevel',1)),('nonboolean fusion',lambda d:d['rows'][0]['selected'].__setitem__('fusion',1)),('boolean root item',lambda d:d['items'].__setitem__(5,True))]
 for label,edit in bad:
  d=copy.deepcopy(data);edit(d);p=temp/'bad.json';p.write_text(json.dumps(d));r=execute([*builder,p,inc]);assert r.returncode!=0,'accepted malformed '+label;print('rejected malformed '+label,flush=True)
 for key in ['target','half','HP','ability','RNG']:
  d=copy.deepcopy(data);r=d['rows'][0]
  if key=='target':r['selected']['species']=151
  elif key=='half':r['selected']['fusion']=False
  elif key=='HP':r['after']['hp']+=1
  elif key=='ability':r['after']['ability']+=1
  else:
   v=r['after']['global'].split(',');v[2]=str((float(v[2])+.125)%1);r['after']['global']=','.join(v)
  p=temp/'forged.json';p.write_text(json.dumps(d));command([*builder,p,inc]);c=temp/'forged.c';c.write_text(test.read_text().replace('#include "../src/internal.h"','#include "internal.h"').replace('#include "fixtures/fusion-item-evolution.native.inc"','#include "'+str(inc)+'"'));exe=temp/'forged';command([cc,*flags,c,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'fusion item case'in r.stdout,'surviving forged '+key;print('caught forged '+key,flush=True)
 print('fusion item evolution: 14 compiled faults, 12 malformed captures, 5 forged observations caught')
