#!/usr/bin/env python3
"""Owning mutations of production phase, formula, stat refresh and modifiers."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1];trace=Path(sys.argv[1]).resolve()
phase=(root/'src/phases.c').read_text();pokemon=(root/'src/pokemon.c').read_text()
start=phase.index('/* AttemptRunPhase.calculateEscapeChance');end=phase.index('/* ToggleDoublePositionPhase',start)
body=phase[start:end]
changes={
 'endless-always-biome':('phase','if (pr_is_new_biome(bt)) {','if (pr_is_new_biome(bt) || bt->game_mode == GAME_MODES_ENDLESS || bt->game_mode == GAME_MODES_SPLICED_ENDLESS) {'),
 'missing-biome':('phase','if (pr_is_new_biome(bt)) {','if (false) {'),
 'skip-inactive-enemy-speed':('phase','if (p) {\n            enemy_speed','if (p && p->hp > 0) {\n            enemy_speed'),
 'player-speed-first-only':('phase','for (int i = 0; i < 2; i++) {','for (int i = 0; i < 1; i++) {'),
 'effective-instead-of-raw-speed':('phase','player_speed += pr_get_stat(p, STAT_SPD, true);','player_speed += pr_get_effective_stat(bt,p,STAT_SPD,NULL,-1,false,false);'),
 'wrong-boss-slope':('phase','double max_chance = is_boss ? 25.0 : 95.0;','double max_chance = is_boss ? 26.0 : 95.0;'),
 'wrong-attempt-bonus':('phase','double escape_bonus = is_boss ? 2.0 : 10.0;','double escape_bonus = is_boss ? 3.0 : 10.0;'),
 'missing-attempt-increment':('phase','int current_attempts = bt->escape_attempts++;','int current_attempts = bt->escape_attempts;'),
 'extra-RNG':('phase','int escape_roll = pr_battle_rand_seed_int(bt, 100, 0);','(void)pr_battle_rand_seed_int(bt, 100, 0); int escape_roll = pr_battle_rand_seed_int(bt, 100, 0);'),
 'missing-Run-Away-caller':('phase','pr_apply_ab_attrs(bt, PR_AC_RunSuccessAbAttr, &pp);','/* missing ability caller */'),
 'missing-failed-flag':('phase','pp->failed_run_away = true;','pp->failed_run_away = false;'),
 'missing-status-reset':('phase','ep->status_toxic_turns = 0;','/* missing status reset */'),
 'missing-item-cleanup':('phase','bt->n_mods[1] = w;','/* missing item cleanup */'),
 'missing-virtual-reset':('phase','if (pr_mod_is(&bt->mods[1][i], PR_MOD_PERSISTENT)) bt->mods[1][i].virtual_stack_count = 0;','/* missing virtual reset */'),
 'missing-zero-stack-cleanup':('phase','if (pr_mod_get_stack_count(&bt->mods[1][i])) bt->mods[1][w++] = bt->mods[1][i];','bt->mods[1][w++] = bt->mods[1][i];'),
 'missing-stats-refresh':('phase','if (bt->party[1][k]) pr_recalculate_stats(bt, bt->party[1][k]);','/* missing refresh */'),
 'wrong-boundary-order':('phase','push_new(bt, e, PH_BATTLE_END, -1);','push_new(bt, e, PH_NEW_BATTLE, -1);'),
 'missing-spliced-halving':('pokemon','base[s] = ceil(f->base_stats[s] / 2.0);','base[s] = f->base_stats[s];'),
}
flags=['gcc','-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
with tempfile.TemporaryDirectory(prefix='flee-phase-mutations-') as tmp:
 t=Path(tmp)
 for name,change in [('intact',None),*changes.items()]:
  texts={'phase':phase,'pokemon':pokemon}
  if change:
   target,a,b=change;s=body if target=='phase' else pokemon
   if s.count(a)!=1:raise SystemExit(f'{name}: anchor {s.count(a)}')
   texts[target]=phase[:start]+s.replace(a,b)+phase[end:] if target=='phase' else s.replace(a,b)
  objects=[]
  for target in ['phase','pokemon']:
   p=t/(target+'.c');p.write_text(texts[target]);obj=p.with_suffix('.o');objects.append(str(obj))
   subprocess.run(flags+['-c',str(p),'-o',str(obj)],capture_output=True,check=True)
  exe=t/name;subprocess.run(flags+[str(root/'tests/flee-phase-test.c'),*objects,str(root/'build/libprcore.a'),'-lm','-o',str(exe)],capture_output=True,check=True)
  r=subprocess.run([str(exe),str(trace)],capture_output=True,text=True)
  print(name,'exit',r.returncode,flush=True)
  if r.returncode!=(0 if name=='intact' else 1):raise SystemExit(r.stdout+r.stderr)
