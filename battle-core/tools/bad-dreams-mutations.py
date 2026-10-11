#!/usr/bin/env python3
"""Mutate only the real C CopyBiomeType dispatcher, rebuild and replay source evidence."""
from pathlib import Path
import shutil, subprocess, sys, tempfile, os
root=Path(__file__).resolve().parent.parent
trace=Path(sys.argv[1]).resolve()
mutants={
 'wrong-blocker-holder':('pr_ab_params bp={.pokemon=p,.move=-1,.simulated=pp->simulated,.flag=&cancelled};','pr_ab_params bp={.pokemon=v,.move=-1,.simulated=pp->simulated,.flag=&cancelled};'),
 'round-up':('to_dmg_value(pr_get_stat(v,STAT_HP,true)/8.0)','to_dmg_value(ceil(pr_get_stat(v,STAT_HP,true)/8.0))')
}
with tempfile.TemporaryDirectory(prefix='bad-dreams-mutation-') as temp:
 for name,edit in [('intact',None),*mutants.items()]:
  dest=Path(temp)/name;shutil.copytree(root,dest,ignore=shutil.ignore_patterns('build'))
  if edit:
   p=dest/'src/ab_attrs.c';s=p.read_text();old,new=edit
   assert s.count(old)==1;s=s.replace(old,new,1);p.write_text(s)
  log=Path(temp)/(name+'.log')
  with log.open('w') as f:
   build=subprocess.run(['make','-s','-j2','-C',str(dest),'build/bad-dreams-test'],stdout=f,stderr=subprocess.STDOUT)
   if build.returncode:raise SystemExit(f'{name}: build failed')
   replay=subprocess.run([str(dest/'build/bad-dreams-test'),str(trace)],stdout=f,stderr=subprocess.STDOUT)
  print(f'{name}: exit {replay.returncode}')
  for line in log.read_text().splitlines():
   if line.startswith(('covered ','texts of covered','damage probes')):print(line)
  if replay.returncode!=(0 if name=='intact' else 1):raise SystemExit(f'{name}: unexpected status')
