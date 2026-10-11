#!/usr/bin/env python3
"""Real game caller mutations; unchanged source trace remains the expectation."""
from pathlib import Path
import os,shutil,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent;trace=Path(sys.argv[1]).resolve()
site='int status=pr_reward_add_canonical_orb_item(&c,item,target);'
mutants={'allow-opposite':(site,'for(int j=0;j<c.n_items;j++)if(is_orb(c.items[j].item)&&c.items[j].item!=item&&c.items[j].owner==target)c.items[j].owner=-1;\n '+site), 'wrong-storage-owner':(site,'target=storage;\n '+site)}
with tempfile.TemporaryDirectory(prefix='orb-acquisition-mutations-') as tmp:
 for core in ['battle-core','run-core','ai-core','reward-core']:(Path(tmp)/core).symlink_to(root.parent/core,target_is_directory=True)
 for name,edit in [('intact',None),*mutants.items()]:
  dest=Path(tmp)/'game-core';shutil.copytree(root,dest,ignore=shutil.ignore_patterns('build'))
  if edit:
   p=dest/'src/gc_orb_add.c';s=p.read_text();assert s.count(edit[0])==1;p.write_text(s.replace(*edit,1))
  log=Path(tmp)/(name+'.log')
  with log.open('w') as f:
   r=subprocess.run(['make','-s','-j2','-C',str(dest),'build/orb_acquisition_diff_test'],stdout=f,stderr=subprocess.STDOUT)
   if r.returncode:raise SystemExit(f'{name}: build failed; {log.read_text()[-3000:]}')
   r=subprocess.run([str(dest/'build/orb_acquisition_diff_test'),str(trace)],stdout=f,stderr=subprocess.STDOUT)
  print(f'{name}: exit {r.returncode}',flush=True)
  for line in log.read_text().splitlines():
   if line.startswith(('Game Orb','Orb identities','FAIL')):print(line,flush=True)
  if r.returncode!=(0 if name=='intact' else 1):raise SystemExit('unexpected mutation status')
  shutil.rmtree(dest)
