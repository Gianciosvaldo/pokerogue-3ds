#!/usr/bin/env python3
"""Compile owner faults and require native comparisons to reject each one."""
import argparse,pathlib,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,default=pathlib.Path('build'));a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2];gc=root/'game-core';b=a.build.resolve()
libs=[b/'libgc.a',b/'reward-core/libreward.a',b/'ai-core/libai.a',b/'run-core/librun.a',b/'run-core/battle-core/libprcore.a']
base=['cc','-O1','-std=c99','-ffp-contract=off']
for d in ['game-core/src','game-core/include','run-core/include','battle-core/include','battle-core/gen/out','reward-core/include','reward-core/gen/out','ai-core/include']:base+=['-I'+str(root/d)]
pillar=gc/'src/gc_pillar.c';pair=root/'run-core/src/trainerparty.c';entry=gc/'src/gc_encounter.c';battle=gc/'src/gc_battle.c'
mutants=[
 ('selector-stream',pillar,'&run->run,run->wave,NULL','&run->run,run->wave+1,NULL'),
 ('double-variant',pillar,'const bool doubles=!pillar&&variants[option]==TRAINER_VARIANT_DOUBLE;','const bool doubles=false;'),
 ('name-index',pillar,'count>1?pr_rng_seed_int(&run->run.rng,count,0):0','0'),
 ('option-stream',pillar,'{10, 100, 1000}','{10, 10, 10}'),
 ('constructor-wave',pillar,'pillar?run->wave-1:run->wave','run->wave-1'),
 ('easy-reward',pillar,'"TM_COMMON", "TM_GREAT", "MEMORY_MUSHROOM"','"TM_COMMON", "TM_GREAT", "RARE_CANDY"'),
 ('hard-reward',pillar,'{2, 2, 1, 1}','{1, 1, 1, 1}'),
 ('pair-slot',pair,'ec.trainer_slot=func_key+1','ec.trainer_slot=0'),
 ('pair-evolution',pair,'true,kind,true,ctx->classic_wave20_trainer','true,PR_EVO_KIND_WILD,true,ctx->classic_wave20_trainer'),
 ('pair-pool',pair,'SPECIES_ID_METAPOD,SPECIES_ID_LEDYBA','SPECIES_ID_MAGIKARP,SPECIES_ID_LEDYBA'),
 ('entry-opt-in',entry,'tc.enable_gym_e4=run->mystery_type==GC_ME_MYSTERIOUS_CHALLENGERS;','tc.enable_gym_e4=false;'),
 ('fusion-field',battle,'for(int k=0;k<(bt->double_battle?2:1)&&k<bt->n_party[0];k++)bt->field[k]=bt->party[0][k];',''),
 ('summon-state',battle,'ev->summon_snapshot=true;','ev->summon_snapshot=false;'),
]
# Each projection must first pass with the pristine owner compiled the same way.
with tempfile.TemporaryDirectory(prefix='challengers-owner-') as d:
 d=pathlib.Path(d)
 def run(name,owner,source,expect_pass):
  target=d/'owner.c';target.write_text(source);binary=d/'owner-test'
  is_pair=owner==pair;is_state=owner==battle
  test=gc/'tests/post_fusion_cleanup_test.c' if is_state else root/'run-core/tests/pair_trainer_test.c' if is_pair else gc/'tests/challengers_test.c'
  wrapper='pr_run_enemy_pokemon_base' if is_pair else 'gc_gen_trainer_party'
  wrap=[] if is_state else ['-Wl,--wrap='+wrapper]
  cmd=base+[str(test),str(target),*[str(x) for x in libs],*wrap,'-lm','-o',str(binary)]
  subprocess.run(cmd,check=True,capture_output=True,text=True)
  r=subprocess.run([str(binary)],capture_output=True,text=True)
  if expect_pass and r.returncode:raise SystemExit(f'pristine {name} failed: {r.stdout}{r.stderr}')
  if not expect_pass and (not r.returncode or ('fusion host line' if is_state else 'Pair factory case' if is_pair else 'Challengers case') not in r.stderr):raise SystemExit(f'fault {name} not detected by assertions: {r.stdout}{r.stderr}')
  print(name+(': pristine passes' if expect_pass else ': native comparison rejects'))
 for owner in (pillar,pair,entry,battle):run(owner.name,owner,owner.read_text(),True)
 for name,owner,old,new in mutants:
  s=owner.read_text()
  if s.count(old)!=1:raise SystemExit(f'fault anchor drift: {name} count {s.count(old)}')
  run(name,owner,s.replace(old,new),False)
print(f'Challengers/pair owner faults: {len(mutants)} compiled regressions rejected')
