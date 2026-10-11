#!/usr/bin/env python3
"""Rebuild real encounter input mutants against untouched source traces."""
from pathlib import Path
import shutil,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent;trace=Path(sys.argv[1]).resolve()
site='if (!gc_lure_count(run, &ctx.n_lures)) gc_stub(run, GC_STUB_REWARD_INPUT);'
mutants={'missing-count':(site,'ctx.n_lures = 0;'),'stacks-not-records':(site,site+'\n    for (int j=0;j<run->n_items;j++) if (!strcmp(run->items[j].key,"LURE") || !strcmp(run->items[j].key,"SUPER_LURE") || !strcmp(run->items[j].key,"MAX_LURE")) ctx.n_lures += run->items[j].stack - 1;')}
with tempfile.TemporaryDirectory(prefix='lure-encounter-mutations-') as tmp:
 for core in ['battle-core','run-core','ai-core','reward-core']:(Path(tmp)/core).symlink_to(root.parent/core,target_is_directory=True)
 for name,edit in [('intact',None),*mutants.items()]:
  dest=Path(tmp)/'game-core';shutil.copytree(root,dest,ignore=shutil.ignore_patterns('build'))
  if edit:
   p=dest/'src/gc_encounter.c';s=p.read_text();assert s.count(edit[0])==1;p.write_text(s.replace(*edit,1))
  log=Path(tmp)/(name+'.log')
  with log.open('w') as f:
   r=subprocess.run(['make','-s','-j2','-C',str(dest),'build/lure_encounter_test'],stdout=f,stderr=subprocess.STDOUT)
   if r.returncode:raise SystemExit(f'{name}: build failed; {log.read_text()[-3000:]}')
   r=subprocess.run([str(dest/'build/lure_encounter_test'),str(trace)],stdout=f,stderr=subprocess.STDOUT)
  print(f'{name}: exit {r.returncode}',flush=True)
  for line in log.read_text().splitlines():
   if line.startswith(('Lure encounter','FAIL')):print(line,flush=True)
  if r.returncode!=(0 if name=='intact' else 1):raise SystemExit('unexpected mutation status')
  shutil.rmtree(dest)
