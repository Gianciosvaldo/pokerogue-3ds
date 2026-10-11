#!/usr/bin/env python3
"""Mutate only the real C AverageStats dispatcher, rebuild and replay source evidence."""
from pathlib import Path
import shutil, subprocess, sys, tempfile, os
root=Path(__file__).resolve().parent.parent
trace=Path(sys.argv[1]).resolve()
site='''double average = floor((pr_get_stat(user, stat, false)
                                  + pr_get_stat(target, stat, false)) / 2.0);'''
mutants={}
for name,move in [('power-round-up','MOVE_ID_POWER_SPLIT'),('guard-round-up','MOVE_ID_GUARD_SPLIT')]:
 mutants[name]=(site,site+f'\n            if (move == {move}) average = ceil((pr_get_stat(user,stat,false)+pr_get_stat(target,stat,false))/2.0);')
mutants['permanent-stats']=('if (average > 0) user->summon_stats[stat] = target->summon_stats[stat] = average;', 'if (average > 0) user->stats[stat] = target->stats[stat] = average;')
with tempfile.TemporaryDirectory(prefix='average-stats-mutation-') as temp:
 for name,edit in [('intact',None),*mutants.items()]:
  dest=Path(temp)/name;shutil.copytree(root,dest,ignore=shutil.ignore_patterns('build'))
  if edit:
   p=dest/'src/move_effects.c';s=p.read_text();old,new=edit
   assert s.count(old)==1;s=s.replace(old,new,1);p.write_text(s)
  log=Path(temp)/(name+'.log')
  with log.open('w') as f:
   build=subprocess.run(['make','-s','-j2','-C',str(dest),'build/average-stats-test'],stdout=f,stderr=subprocess.STDOUT)
   if build.returncode:raise SystemExit(f'{name}: build failed')
   replay=subprocess.run([str(dest/'build/average-stats-test'),str(trace)],stdout=f,stderr=subprocess.STDOUT)
  print(f'{name}: exit {replay.returncode}')
  for line in log.read_text().splitlines():
   if line.startswith(('covered ','texts of covered','damage probes')):print(line)
  if replay.returncode!=(0 if name=='intact' else 1):raise SystemExit(f'{name}: unexpected status')
