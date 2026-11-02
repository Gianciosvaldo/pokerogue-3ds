#!/usr/bin/env python3
"""Compile wrong production owners and require the actual-native comparison to fail."""
import argparse,pathlib,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,default=pathlib.Path('build'));a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2];gc=root/'game-core';b=a.build.resolve()
libs=[b/'libgc.a',b/'reward-core/libreward.a',b/'ai-core/libai.a',b/'run-core/librun.a',b/'run-core/battle-core/libprcore.a']
base=['cc','-O1','-std=c99','-ffp-contract=off','-Wl,--strip-debug','-I'+str(b)]
for d in ['game-core/src','game-core/include','run-core/include','battle-core/include','battle-core/gen/out','reward-core/include','reward-core/gen/out','ai-core/include']:base+=['-I'+str(root/d)]
move=gc/'src/gc_movegen.c';entry=gc/'src/gc_encounter.c';party=root/'run-core/src/trainerparty.c'
mutants=[
 ('no-trainer-diversity',move,'if (c->trainer) {','if (false) {'),
 ('no-trainer-weights',move,'if (c.trainer) {','if (false) {'),
 ('no-superceding',move,'if (c.boss || c.trainer) filter_superceded(&pool);','if (c.boss) filter_superceded(&pool);'),
 ('omit-trainer-pools',move,'if (c.trainer && !trainer_pools(&c, &pool)) return false;','if (false) return false;'),
 ('wrong-trainer-stream',entry,'gc_generate_moveset(state->run, rng, m)','gc_generate_moveset(state->run, &state->run->bt.battle_rng, m)'),
 ('double-finish',entry,'if (state.ready[e]) {','if (false) {'),
 ('disabled-owner',entry,'finish_trainer_constructor, &state','NULL, NULL'),
 ('partner-slot',party,'ec.trainer_slot = !ctx->is_double || !(e % 2) ? 1 : 2;','ec.trainer_slot = 1;'),
 ('couple-pool',party,'pool=couple[func_key];count=8','pool=couple[1-func_key];count=8'),
 ('same-seed',party,'((ts->same_seed ? 0 : e) + 1)','((e) + 1)'),
 ('ambient-restore',party,'run->rng = saved;   /* each member rolled','/* fault */   /* each member rolled'),
 ('decline-output',party,'if (!handled) { run->rng = base_rng; out->mons[e] = base; }','if (!handled) { run->rng = base_rng; }'),
 ('reject-old-moves',move,'m->b.level < 1 || m->b.n_moves','m->b.level < 1'),
 ('no-form-guard',move,'if (!sp || (trainer && (sp->form_change_triggers','if (!sp || (false && (sp->form_change_triggers'),
 ('omit-relearner-weight',move,'weight = c.trainer && c.p.level >= gc_mg_const.relearn_level ? gc_mg_const.relearn_move_weight : 0;','weight = 0;'),
 ('delay-common-tms',move,'if (c->p.level >= gc_mg_trainer_const.tm_level[0]) {','if (c->p.level >= gc_mg_trainer_const.tm_level[1]) {'),
 ('reverse-tm-order',move,'for (int k = 0; k < list->n; k++) {','for (int k = list->n - 1; k >= 0; k--) {'),
 ('lose-tm-weight',move,'(mg_ent){move, gc_mg_trainer_const.tm_weight[tier]}','(mg_ent){move, 0}'),
 ('allow-forbidden-tms',move,'sorted_has(gc_mg_tm_forbidden, gc_mg_tm_forbidden_count, move)','false'),
 ('unlimited-tm-cap',move,'nonlevel >= tm_cap && c->source_of[move] == 1','false'),
 ('lose-tm-count',move,'if (c->source_of[move] == 1) c->tm_count++;','if (c->source_of[move] == 1) {}'),
 ('lose-tm-cross-superceding',move,'(a && pool_find(a, gc_mg_superceded_moves[row->first + k]) >= 0)','false'),

]
with tempfile.TemporaryDirectory(prefix='trainer-owner-')as directory:
 d=pathlib.Path(directory)
 def run(name,owner,text,expect_pass):
  source=d/'owner.c';source.write_text(text);binary=d/'owner-test'
  cmd=base+[str(gc/'tests/trainer_constructor_test.c'),str(source),*[str(v)for v in libs],'-Wl,--wrap=pr_run_enemy_pokemon_base,--wrap=gc_generate_moveset,--wrap=pr_run_enemy_pokemon_finish,--wrap=pr_calculate_stats','-lm','-o',str(binary)]
  subprocess.run(cmd,check=True,capture_output=True,text=True)
  r=subprocess.run([str(binary)],capture_output=True,text=True)
  assert (not r.returncode)if expect_pass else (r.returncode and 'Trainer constructor group'in r.stderr),name+': '+r.stdout+r.stderr
  print(name+(': pristine passes'if expect_pass else ': native assertions reject'))
 for owner in [move,entry,party]:run(owner.name,owner,owner.read_text(),True)
 for name,owner,old,new in mutants:
  source=owner.read_text();count=source.count(old)
  assert count==(2 if name=='partner-slot'else 1),(name,count)
  run(name,owner,source.replace(old,new),False)
print(f'Trainer constructor owners: {len(mutants)} compiled faults rejected')
