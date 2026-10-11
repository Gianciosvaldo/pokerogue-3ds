#!/usr/bin/env python3
"""Mutate only the real C CopyBiomeType dispatcher, rebuild and replay source evidence."""
from pathlib import Path
import shutil, subprocess, sys, tempfile, os
root=Path(__file__).resolve().parent.parent
trace=Path(sys.argv[1]).resolve()
mutants={
 'user-only':('for (int k = 0; k < bt->n_party[side]; k++)\n            pr_reset_status_phase(bt, e, bt->party[side][k], false);','pr_reset_status_phase(bt, e, user, false);'),
 'wrong-bench-immunity':('pr_reset_status_phase(bt, e, bt->party[side][k], false);','if (k == 0 || !pr_has_ability_active(bt, bt->party[side][k], ABILITY_ID_SOUNDPROOF)) pr_reset_status_phase(bt, e, bt->party[side][k], false);')
}
with tempfile.TemporaryDirectory(prefix='party-status-cure-mutation-') as temp:
 for name,edit in [('intact',None),*mutants.items()]:
  dest=Path(temp)/name;shutil.copytree(root,dest,ignore=shutil.ignore_patterns('build'))
  if edit:
   p=dest/'src/move_effects.c';s=p.read_text();old,new=edit
   assert s.count(old)==1;s=s.replace(old,new,1);p.write_text(s)
  log=Path(temp)/(name+'.log')
  with log.open('w') as f:
   build=subprocess.run(['make','-s','-j2','-C',str(dest),'build/party-status-cure-test'],stdout=f,stderr=subprocess.STDOUT)
   if build.returncode:raise SystemExit(f'{name}: build failed')
   replay=subprocess.run([str(dest/'build/party-status-cure-test'),str(trace)],stdout=f,stderr=subprocess.STDOUT)
  print(f'{name}: exit {replay.returncode}')
  for line in log.read_text().splitlines():
   if line.startswith(('covered ','texts of covered','damage probes')):print(line)
  if replay.returncode!=(0 if name=='intact' else 1):raise SystemExit(f'{name}: unexpected status')
