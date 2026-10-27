#!/usr/bin/env python3
"""Native power transport, public query, fault controls and scoped sanitizers."""
from pathlib import Path
import copy,gzip,json,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core'
def execute(args):return subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
def command(args):
 r=execute(args)
 if r.returncode:raise RuntimeError(str(args)+'\n'+r.stdout[-5000:])
 return r.stdout
flags=['-O1','-std=gnu11','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','game-core/tests','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
cc=os.environ.get('CC','cc');builder=['python3',GC/'tools/build-fusion-power-proof.py'];test=GC/'tests/fusion_power_host_test.c';source=(GC/'src/gc_movegen.c').read_text()
with tempfile.TemporaryDirectory(prefix='fusion-power-')as td:
 temp=Path(td);inputs=[]
 for n in ['focused','constructors']:
  p=temp/(n+'.json');p.write_bytes(gzip.decompress((GC/('tests/fixtures/fusion-power.'+n+'.json.gz')).read_bytes()));inputs.append(p)
 inc=temp/'native.inc';command([*builder,*inputs,inc]);assert inc.read_bytes()==(GC/'tests/fixtures/fusion-power.native.inc').read_bytes()
 print(command(['make','-C',GC,'fusion-power-host-test']),flush=True)
 if os.environ.get('FUSION_POWER_SANITIZE')=='1':
  san=flags+['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'];obj=temp/'movegen.o';exe=temp/'san'
  command([cc,*san,'-c',GC/'src/gc_movegen.c','-o',obj]);command([cc,*san,test,obj,*libs,'-lm','-o',exe]);print(command([exe]),flush=True)
  print('Scoped ASan/UBSan/LSan: gc_movegen; supporting archives unsanitized');raise SystemExit(0)
 faults=[
  ('    if (!alt) {\n        const int type = pr_get_move_type','    if (!alt && mg_flag(move, GC_MGF_VARIABLE_TYPE)) {\n        const int type = pr_get_move_type','old variable-only lookup'),
  ('const int type = pr_get_move_type(&c->bt, &c->p, move);','const int type = pr_moves[move].type;','ignore ability type changes'),
  ('alt = find_pow(ab, move, 4, type);','alt = find_pow(ab, move, 4, POKEMON_TYPE_NORMAL);','wrong type lookup'),
  ('ability_stats_of(c, move, c->passive, &pm, &am);','ability_stats_of(c, move, c->ability, &pm, &am);','replace passive with active'),
  ('for (int i = 0; i < n; i++) out[i] = effective_power(&c, moves[i]);','pr_rng_seed_int(&run->run.rng,100,0);for (int i = 0; i < n; i++) out[i] = effective_power(&c, moves[i]);','consume caller RNG'),
  ('for (int i = 0; i < n; i++) out[i] = effective_power(&c, moves[i]);','((gc_mon*)m)->b.pp_used[0]++;for (int i = 0; i < n; i++) out[i] = effective_power(&c, moves[i]);','mutate queried actor'),
 ]
 for old,new,label in faults:
  assert source.count(old)==1,(label,source.count(old));p=temp/'fault.c';p.write_text(source.replace(old,new));obj=temp/'fault.o';exe=temp/'fault'
  command([cc,*flags,'-c',p,'-o',obj]);command([cc,*flags,test,obj,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0,'surviving '+label;assert 'fusion power case' in r.stdout,(label,r.stdout)
  print('caught compiled fault: '+label,flush=True)
 table=(GC/'gen/out/gc_movegen_data.c').read_text();old='{199, 574, 4, 10, 2.0, 1.0, 0, 0}';assert table.count(old)==1
 p=temp/'table.c';p.write_text(table.replace(old,'{199, 574, 4, 10, 1.0, 1.0, 0, 0}'));obj=temp/'table.o';exe=temp/'table'
 command([cc,*flags,'-c',p,'-o',obj]);command([cc,*flags,test,obj,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'fusion power case'in r.stdout,'surviving table fault';print('caught compiled fault: lost sound-to-water power row',flush=True)
 x=json.loads(inputs[0].read_text());y=json.loads(inputs[1].read_text())
 def rejected(change,label):
  d=copy.deepcopy(x);e=copy.deepcopy(y);change(d,e);inputs[0].write_text(json.dumps(d));inputs[1].write_text(json.dumps(e));r=execute([*builder,*inputs,inc]);assert r.returncode!=0,'accepted malformed '+label;print('rejected malformed '+label,flush=True)
 rejected(lambda d,e:d['rows'].pop(),'missing focused case')
 rejected(lambda d,e:e['rows'].pop(),'missing constructor case')
 rejected(lambda d,e:d['rows'][0]['generation']['input'].__setitem__('fusion',19),'wrong focused species')
 rejected(lambda d,e:d['rows'][0]['generation']['input'].__setitem__('boss',1),'integer boss boolean')
 rejected(lambda d,e:d['rows'][0]['generation']['input'].__setitem__('level',True),'boolean level')
 rejected(lambda d,e:d['rows'][0]['generation']['powers'].pop(),'missing power query')
 rejected(lambda d,e:d['rows'][0]['generation']['powers'][0].__setitem__(1,float('nan')),'nonfinite power')
 rejected(lambda d,e:d['rows'][0]['generation']['implemented'].__setitem__(0,1),'integer implemented boolean')
 rejected(lambda d,e:e['rows'][0].__setitem__('sample',999),'wrong constructor matrix')
 rejected(lambda d,e:d['rows'][0]['generation'].__setitem__('before','!rnd,1,1.1,0,0'),'invalid RNG')
 rejected(lambda d,e:d['rows'][0]['generation']['powers'][2].__setitem__(0,True),'boolean power move id')
 rejected(lambda d,e:e['rows'][0].__setitem__('species',True),'boolean constructor species')
 inputs[0].write_text(json.dumps(x));inputs[1].write_text(json.dumps(y));command([*builder,*inputs,inc]);original=inc.read_text();harness=test.read_text()
 # A structurally legal forged expectation must reach, and fail, the real C query.
 for field,value,label in [('powers',40,'power'),('ability',1,'ability'),('passiveAbility',1,'passive')]:
  d=copy.deepcopy(x)
  if field=='powers':
   t=next(t for t in d['rows'][0]['generation']['powers']if t[0]==574);assert t[1]==80;t[1]=value
  else:d['rows'][0]['generation']['input'][field]=value
  inputs[0].write_text(json.dumps(d));command([*builder,*inputs,inc]);p=temp/'forged.c';p.write_text(harness.replace('#include "../src/internal.h"','#include "internal.h"').replace('#include "fixtures/fusion-power.native.inc"','#include "'+str(inc)+'"'));exe=temp/'forged';command([cc,*flags,p,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'fusion power case'in r.stdout,'surviving forged '+label;print('caught forged '+label,flush=True)
 print('fusion power proof: 7 compiled faults, 12 malformed captures, 3 forged observations caught')
