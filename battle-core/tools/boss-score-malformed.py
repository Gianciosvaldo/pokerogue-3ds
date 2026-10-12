#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys,tempfile
exe=Path(sys.argv[1]).resolve();s=Path(sys.argv[2]).read_text();rows=s.splitlines()
cases={
 'missing-footer':'\n'.join(rows[:-1])+'\n',
 'wrong-footer':s.replace('BOSS_SCORE_COMPLETE 1000','BOSS_SCORE_COMPLETE 999'),
 'fractional-header':s.replace('BOSS_SCORE 1 1000','BOSS_SCORE 1 1000.5'),
 'wrong-version':s.replace('BOSS_SCORE 1 ','BOSS_SCORE 2 ',1),
 'trailing':s+'E\n',
 'duplicate-case':s.replace('C 1 ','C 0 ',1),
 'missing-end':s.replace('\nE\n','\n',1),
 'missing-ivs':s.replace(next(x for x in rows if x.startswith('I '))+'\n','',1),
 'unsupported-level':s.replace(rows[1],' '.join(rows[1].split()[:4]+['256']+rows[1].split()[5:]),1),
}
if subprocess.run([str(exe),sys.argv[2]],capture_output=True).returncode:raise SystemExit('intact failed')
with tempfile.TemporaryDirectory(prefix='boss-score-malformed-') as tmp:
 for name,text in cases.items():
  if text==s:raise SystemExit('ineffective mutation '+name)
  p=Path(tmp)/name;p.write_text(text);r=subprocess.run([str(exe),str(p)],capture_output=True)
  print(name,'exit',r.returncode)
  if r.returncode!=2:raise SystemExit('malformed refusal failed')
