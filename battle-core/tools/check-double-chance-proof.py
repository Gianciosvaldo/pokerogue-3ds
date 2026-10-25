#!/usr/bin/env python3
"""Compiled faults and strict malformed-input guards for double encounter chance."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];raw,generation,trainer=map(lambda v:Path(v).resolve(),sys.argv[1:4]);env=os.environ.copy();assert env.get('TURNTEST_LOCALES');battle=r/'battle-core';runner=battle/'build/double-chance-test';genrunner=battle/'build/double-generation-test'
for exe,trace in [(runner,raw),(genrunner,generation),(genrunner,trainer)]:subprocess.run([exe,trace],check=True,env=env)
faults=[]
def variants(filename,start,end,changes,trace,kind):
 source=(r/filename).read_text();a=source.index(start);b=source.index(end,a+len(start));body=source[a:b]
 for name,old,new in changes:
  assert body.count(old)==1,(name,body.count(old));faults.append((name,filename,source[:a]+body.replace(old,new,1)+source[b:],trace,kind))
variants('battle-core/src/ab_attrs.c','    case PR_AC_DoubleBattleChanceAbAttr:','    case PR_AC_StatMultiplierAbAttr:',[
 ('wrong-divisor','*pp->value /= 4','*pp->value /= 2'),('missing-simulated-holder','*pp->value /= 4','if(!pp->simulated)*pp->value /= 4'),('missing-holder','*pp->value /= 4','*pp->value /= 1'),('lost-activation-history','        return 1;','        return 0;')],raw,'raw')
variants('battle-core/src/engine.c','double pr_get_double_battle_chance(','/* NewBiomeEncounterPhase.doEncounter',[
 ('wrong-boss-base','wave % 10 == 0 ? 32 : 8','wave % 10 == 0 ? 8 : 32'),('missing-lures','i < n_lures','i < 0'),('only-first-slot','bt->double_battle ? 2 : 1','1'),('include-reserves','bt->double_battle ? 2 : 1','4'),('forbidden-on-field-filter','pr_apply_ab_attrs(bt, PR_AC_DoubleBattleChanceAbAttr, &pp);','if(pp.pokemon->on_field)pr_apply_ab_attrs(bt, PR_AC_DoubleBattleChanceAbAttr, &pp);'),('missing-final-clamp','return chance < 1 ? 1 : chance;','return chance;'),('premature-clamp','int n = bt->double_battle ? 2 : 1;','if(chance<1)chance=1;int n = bt->double_battle ? 2 : 1;')],raw,'raw')
variants('run-core/src/newbattle.c','int pr_run_new_battle(','/* randomString(',[(
 'missing-wild-callback','out->double_battle = pr_rng_seed_int(&run->rng, (int32_t)get_double_chance(wave, ctx), 0) == 0;','out->double_battle = pr_rng_seed_int(&run->rng, (int32_t)pr_run_get_double_battle_chance(wave,ctx->n_lures,0),0)==0;'),(
 'missing-trainer-callback','else dbl = pr_rng_seed_int(&run->rng, (int32_t)get_double_chance(wave, ctx), 0) == 0;','else dbl = pr_rng_seed_int(&run->rng, (int32_t)pr_run_get_double_battle_chance(wave,ctx->n_lures,0),0)==0;')],generation,'generation')
# The trainer-specific fault needs the native Classic/Challenge stream.
faults[-1]=(*faults[-1][:3],trainer,faults[-1][4])
variants('game-core/src/gc_encounter.c','static double scene_double_chance(','void gc_new_battle_start(',[
 ('missing-game-callback','ctx.double_chance = scene_double_chance;','ctx.double_chance = NULL;'),('wrong-party-order','run->party[run->party_order[k]].b','run->party[k].b'),('missing-passive-refresh','pr_update_passive(bt, p);','p->passive=0;'),('lost-game-lures','ctx->n_lures);','0);')],None,'host')
# Moving the clamp must also remove the final clamp; merely adding an early
# clamp is observationally equivalent when the final clamp remains.
faults=[(name,filename,changed.replace('return chance < 1 ? 1 : chance;','return chance;') if name=='premature-clamp' else changed,trace,kind) for name,filename,changed,trace,kind in faults]
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(battle/p) for p in ['include','gen/out','build']],'-I'+str(r/'run-core/include')]
with tempfile.TemporaryDirectory(prefix='double-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,filename,changed,trace,kind in faults:
  src=d/Path(filename).name;src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  f=flags+(['-I'+str(r/'game-core/src'),*['-I'+str(r/p) for p in ['game-core/include','ai-core/include','reward-core/include','reward-core/gen/out']]] if kind=='host' else [])
  subprocess.run(['gcc',*f,'-c',src,'-o',obj],check=True,capture_output=True)
  if kind=='host':
   gb=r/'game-core/build';args=[r/'game-core/tests/double_chance_host_test.c',obj,gb/'libgc.a',gb/'reward-core/libreward.a',gb/'ai-core/libai.a',gb/'run-core/librun.a',gb/'run-core/battle-core/libprcore.a','-Wl,--wrap=pr_run_new_battle'];cmd=[exe]
  else:
   args=[battle/'tests'/('double-chance-test.c' if kind=='raw' else 'double-generation-test.c'),obj]
   if kind=='generation':args+=[battle/'build/double-chance-run/librun.a']
   args+=[battle/'build/libprcore.a'];cmd=[exe,trace]
  subprocess.run(['gcc',*f,*args,'-lm','-o',exe],check=True,capture_output=True)
  p=subprocess.run(cmd,env=env,capture_output=True);assert p.returncode==1,(name,p.returncode,p.stdout[-400:],p.stderr[-400:]);print(name,'caught',flush=True)
 count=0
 def reject(name,data,exe):
  global count
  bad=d/'bad.txt';bad.write_bytes(data);p=subprocess.run([exe,bad],env=env,capture_output=True);assert p.returncode==2,(name,p.returncode,p.stdout[-200:],p.stderr[-200:]);count+=1;print(name,'rejected',flush=True)
 def mutation(data,tag,col,value):
  a=data.index(b'\n'+tag)+1;b=data.index(b'\n',a);row=data[a:b].split();row[col]=value;return data[:a]+b' '.join(row)+data[b:]
 data=raw.read_bytes()
 reject('raw-version',data.replace(b'DOUBLE_CHANCE 1 ',b'DOUBLE_CHANCE 3 ',1),runner)
 reject('raw-small',data.replace(b'16384 ',b'16383 ',1),runner)
 reject('raw-footer',data.rsplit(b'DOUBLE_CHANCE_COMPLETE ',1)[0]+b'DOUBLE_CHANCE_COMPLETE 1\n',runner)
 reject('raw-trailing',data+b'JUNK\n',runner)
 for name,tag,col,value in [('case-fraction',b'C ',1,b'0.5'),('user',b'C ',2,b'8'),('simulation',b'C ',3,b'2'),('owner',b'C ',4,b'2'),('single',b'C ',5,b'2'),('wave',b'C ',6,b'1000001'),('lures',b'C ',7,b'4'),('holder-negative',b'C ',8,b'-1'),('holder-nan',b'C ',8,b'NaN')]:reject(name,mutation(data,tag,col,value),runner)
 data=generation.read_bytes()
 reject('generation-small',data.replace(b'32768 ',b'32767 ',1),genrunner)
 reject('generation-footer',data.rsplit(b'DOUBLE_GENERATION_COMPLETE ',1)[0]+b'DOUBLE_GENERATION_COMPLETE 1\n',genrunner)
 for name,tag,col,value in [('generation-mode',b'C ',6,b'999'),('generation-lures',b'C ',5,b'4'),('callback-count',b'A ',1,b'2'),('generation-level-count',b'G ',4,b'13')]:reject(name,mutation(data,tag,col,value),genrunner)
 print(f'{len(faults)} compiled faults caught; {count} malformed inputs rejected',flush=True)
