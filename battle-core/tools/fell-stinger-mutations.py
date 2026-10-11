#!/usr/bin/env python3
"""Mutate only the real C CopyBiomeType dispatcher, rebuild and replay source evidence."""
from pathlib import Path
import shutil, subprocess, sys, tempfile, os
root=Path(__file__).resolve().parent.parent
trace=Path(sys.argv[1]).resolve();helper=Path(sys.argv[2]).resolve()
mutants={
 'last-history':('int victory_move = p->attacks_received[0].move;', 'int victory_move = p->attacks_received[p->n_attacks_received-1].move;'),
 'skip-stat-queue':('pr_apply_post_victory_stat_change(bt,e,a,source,victory_move);', '(void)source;')
}
with tempfile.TemporaryDirectory(prefix='fell-stinger-mutation-') as temp:
 for name,edit in [('intact',None),*mutants.items()]:
  dest=Path(temp)/name;shutil.copytree(root,dest,ignore=shutil.ignore_patterns('build'))
  if edit:
   p=dest/'src/phases.c';s=p.read_text();old,new=edit
   assert s.count(old)==1;s=s.replace(old,new,1);p.write_text(s)
  log=Path(temp)/(name+'.log')
  with log.open('w') as f:
   build=subprocess.run(['make','-s','-j2','-C',str(dest),'build/fell-stinger-test','build/fell-stinger-helper-test'],stdout=f,stderr=subprocess.STDOUT)
   if build.returncode:raise SystemExit(f'{name}: build failed')
   replay=subprocess.run([str(dest/('build/fell-stinger-helper-test' if name=='last-history' else 'build/fell-stinger-test')),str(helper if name=='last-history' else trace)],stdout=f,stderr=subprocess.STDOUT)
  print(f'{name}: exit {replay.returncode}')
  for line in log.read_text().splitlines():
   if line.startswith(('covered ','texts of covered','damage probes','Fell callback')):print(line)
  if replay.returncode!=(0 if name=='intact' else 1):raise SystemExit(f'{name}: unexpected status')
