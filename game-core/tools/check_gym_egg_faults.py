#!/usr/bin/env python3
"""Compile wrong plain Gym owners; require the actual-source constructor proof to reject them."""
import argparse,pathlib,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,default=pathlib.Path('build'));a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2];gc=root/'game-core';b=a.build.resolve()
libs=[b/'libgc.a',b/'reward-core/libreward.a',b/'ai-core/libai.a',b/'run-core/librun.a',b/'run-core/battle-core/libprcore.a']
base=['cc','-O1','-std=c99','-ffp-contract=off','-Wl,--strip-debug','-I'+str(b)]
for d in ['game-core/src','game-core/include','run-core/include','battle-core/include','battle-core/gen/out','reward-core/include','reward-core/gen/out','ai-core/include']:base+=['-I'+str(root/d)]
move=gc/'src/gc_movegen.c';entry=gc/'src/gc_encounter.c';party=root/'run-core/src/trainerparty.c'
egg_block='''    for (int k = 0; k < egg.n; k++) {
        combined.e[combined.n++] = egg.e[k];
        c->source_of[egg.e[k].move] = 2;
    }
'''
level_line='    for (int k = 0; k < level->n; k++) combined.e[combined.n++] = level->e[k];'
mutants=[
 ('disconnect-owner',entry,'finish_trainer_constructor, &state','NULL, NULL'),
 ('double-finish',entry,'if (state.ready[e]) {','if (false) {'),
 ('partner-slot',party,'ec.trainer_slot = !ctx->is_double || !(e % 2) ? 1 : 2;','ec.trainer_slot = 1;'),
 ('ambient-stream',party,'run->rng = saved;   /* each member rolled','/* fault */   /* each member rolled'),
 ('lose-eggs',move,'if (c->allow_egg && c->p.level >= gc_mg_trainer_const.egg_level)', 'if (false)'),
 ('early-eggs',move,'c->p.level >= gc_mg_trainer_const.egg_level','c->p.level > 0'),
 ('early-rare-eggs',move,'c->p.level < gc_mg_trainer_const.rare_egg_level','false'),
 ('rare-weight',move,'weight * (5.0 / 6.0)','weight'),
 ('egg-scale',move,'max * gc_mg_trainer_const.egg_to_level + .5','max * gc_mg_trainer_const.egg_to_level * .5 + .5'),
 ('egg-weight-cap',move,'gc_mg_trainer_const.egg_weight_max','30'),
 ('egg-duplicates',move,'if (!move || pool_find(level, move) >= 0','if (!move || false'),
 ('tm-egg-duplicates',move,'|| pool_find(&egg, move) >= 0','|| false'),
 ('egg-insertion-order',move,egg_block+level_line,level_line+'\n'+egg_block),
 ('shared-cap',move,'const int nonlevel = c->tm_count + c->egg_count;','const int nonlevel = c->tm_count;'),
 ('egg-cap',move,'nonlevel >= egg_cap && c->source_of[move] == 2','false'),
 ('egg-count',move,'else if (c->source_of[move] == 2) c->egg_count++;','else if (c->source_of[move] == 2) {}'),
 ('egg-superceding-tm',move,'(b && pool_find(b, gc_mg_superceded_moves[row->first + k]) >= 0)','false'),
 ('no-tate-liza',move,'|| type == TRAINER_TYPE_TATE || type == TRAINER_TYPE_LIZA',''),
 ('unreviewed-profiles',move,'return type == TRAINER_TYPE_BROCK || type == TRAINER_TYPE_ROXANNE','return true || type == TRAINER_TYPE_BROCK || type == TRAINER_TYPE_ROXANNE'),
 ('unreviewed-doubles',move,'egg_gym && run->double_battle && run->trainer_type != TRAINER_TYPE_TATE','false && run->double_battle && run->trainer_type != TRAINER_TYPE_TATE'),
 ('old-stat-failure',move,'if (c.bt.n_unported) { *rng = before; return false; }','if (false) { *rng = before; return false; }'),
]
with tempfile.TemporaryDirectory(prefix='gym-egg-owner-')as directory:
 d=pathlib.Path(directory)
 def run(name,owner,text,expect_pass):
  source=d/'owner.c';source.write_text(text);binary=d/'owner-test'
  cmd=base+[str(gc/'tests/gym_egg_constructor_test.c'),str(source),*[str(v)for v in libs],'-Wl,--wrap=pr_run_enemy_pokemon_base,--wrap=gc_generate_moveset,--wrap=pr_run_enemy_pokemon_finish,--wrap=pr_calculate_stats','-lm','-o',str(binary)]
  subprocess.run(cmd,check=True,capture_output=True,text=True)
  r=subprocess.run([str(binary)],capture_output=True,text=True)
  assert (not r.returncode)if expect_pass else (r.returncode and 'Trainer constructor group'in r.stderr),name+': '+r.stdout+r.stderr
  print(name+(': pristine passes'if expect_pass else ': native assertions reject'),flush=True)
 for owner in [move,entry,party]:run(owner.name,owner,owner.read_text(),True)
 for name,owner,old,new in mutants:
  source=owner.read_text();count=source.count(old)
  assert count==(2 if name=='partner-slot'else 1),(name,count)
  run(name,owner,source.replace(old,new),False)
print(f'Plain Gym constructor owners: {len(mutants)} compiled faults rejected')
