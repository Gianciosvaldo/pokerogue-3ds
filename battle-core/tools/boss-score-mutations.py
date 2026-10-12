#!/usr/bin/env python3
"""Compile actual production score/caller mutations; preserve the source oracle."""
from pathlib import Path
import os, subprocess, sys, tempfile
root=Path(__file__).resolve().parents[1]
trace=Path(sys.argv[1]).resolve()
s=(root/'src/phases.c').read_text()
changes={
 'missing-capture-caller':('add_fainted_enemy_score(bt, pokemon);','/* missing capture score */'),
 'missing-faint-caller':('add_fainted_enemy_score(bt, p);','/* missing faint score */'),
 'missing-boss-factor':('score *= sqrt(pokemon->boss_segments);','score *= 1;'),
 'segments-instead-of-root':('score *= sqrt(pokemon->boss_segments);','score *= pokemon->boss_segments;'),
 'round-before-root':('score *= sqrt(pokemon->boss_segments);','score = ceil(score) * sqrt(pokemon->boss_segments);'),
 'round-before-item':('score *= 1.2;','score = ceil(score) * 1.2;'),
 'wrong-rounding':('bt->battle_score += ceil(score);','bt->battle_score += floor(score);'),
 'stacked-score-factor':('score *= 1.2;','score *= pow(1.2, m->stack_count);'),
 'wrong-owner':('m->pokemon_id != (int64_t)pokemon->id) continue;','m->pokemon_id == (int64_t)pokemon->id) continue;'),
 'missing-items':('for (int i = 0; i < bt->n_mods[1]; i++) {\n        const pr_modifier *m = &bt->mods[1][i];','for (int i = 0; i < 0; i++) {\n        const pr_modifier *m = &bt->mods[1][i];'),
 'wrong-vitamin-factor':('score *= 1.1;','score *= 1.2;'),
 'extra-score-RNG':('bt->battle_score += ceil(score);','(void)pr_battle_rand_seed_int(bt,100,0); bt->battle_score += ceil(score);'),
}
flags=['gcc','-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
with tempfile.TemporaryDirectory(prefix='boss-score-mutations-') as tmp:
 t=Path(tmp)
 for name,pair in [('intact',None),*changes.items()]:
  source=s
  if pair:
   a,b=pair
   if s.count(a)!=1:raise SystemExit(f'{name}: ambiguous anchor {s.count(a)}')
   source=s.replace(a,b)
  p=t/(name+'.c');p.write_text(source);obj=t/(name+'.o');exe=t/name
  subprocess.run(flags+['-c',str(p),'-o',str(obj)],check=True,capture_output=True)
  subprocess.run(flags+[str(root/'tests/boss-score-test.c'),str(obj),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True,capture_output=True)
  r=subprocess.run([str(exe),str(trace)],capture_output=True,text=True)
  print(name,'exit',r.returncode,flush=True)
  if r.returncode!=(0 if name=='intact' else 1):raise SystemExit(r.stdout+r.stderr)
