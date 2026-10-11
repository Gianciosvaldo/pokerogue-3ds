#!/usr/bin/env python3
"""Real game caller mutations; unchanged source trace remains the expectation."""
from pathlib import Path
import os,shutil,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent;trace=Path(sys.argv[1]).resolve()
site='for(int j=0;j<run->n_items;j++)run->items[j].battle_count=c.items[j].battle_count;'
mutants={'increment-refresh-stack':(site,'for(int j=0;j<run->n_items;j++){if(run->items[j].battle_count!=c.items[j].battle_count)run->items[j].stack++;run->items[j].battle_count=c.items[j].battle_count;}'),'wrong-lure-kind':('int result=pr_reward_add_lure(&c,item);','item=item==PR_ITEM_LURE?PR_ITEM_SUPER_LURE:PR_ITEM_LURE;int result=pr_reward_add_lure(&c,item);')}
with tempfile.TemporaryDirectory(prefix='lure-acquisition-mutations-') as tmp:
 for core in ['battle-core','run-core','ai-core','reward-core']:(Path(tmp)/core).symlink_to(root.parent/core,target_is_directory=True)
 for name,edit in [('intact',None),*mutants.items()]:
  dest=Path(tmp)/'game-core';shutil.copytree(root,dest,ignore=shutil.ignore_patterns('build'))
  if edit:
   p=dest/'src/gc_lure.c';s=p.read_text();assert s.count(edit[0])==1;p.write_text(s.replace(*edit,1))
  log=Path(tmp)/(name+'.log')
  with log.open('w') as f:
   r=subprocess.run(['make','-s','-j2','-C',str(dest),'build/lure_acquisition_test'],stdout=f,stderr=subprocess.STDOUT)
   if r.returncode:raise SystemExit(f'{name}: build failed; {log.read_text()[-3000:]}')
   r=subprocess.run([str(dest/'build/lure_acquisition_test'),str(trace)],stdout=f,stderr=subprocess.STDOUT)
  print(f'{name}: exit {r.returncode}',flush=True)
  for line in log.read_text().splitlines():
   if line.startswith(('Lure acquisition','FAIL')):print(line,flush=True)
  if r.returncode!=(0 if name=='intact' else 1):raise SystemExit('unexpected mutation status')
  shutil.rmtree(dest)
