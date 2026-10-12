#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys,tempfile
exe=Path(sys.argv[1]).resolve();s=Path(sys.argv[2]).read_text();rows=s.splitlines()
cases={
 'missing-footer':'\n'.join(rows[:-1])+'\n',
 'wrong-footer':s.replace('FLEE_PHASE_COMPLETE 2000','FLEE_PHASE_COMPLETE 1999'),
 'fractional-header':s.replace('FLEE_PHASE 1 2000','FLEE_PHASE 1 2000.5'),
 'wrong-version':s.replace('FLEE_PHASE 1 ','FLEE_PHASE 2 ',1),
 'trailing':s+'E\n',
 'duplicate-case':s.replace('C 1 ','C 0 ',1),
 'missing-end':s.replace('\nE\n','\n',1),
 'missing-stats':s.replace(next(x for x in rows if x.startswith('META '))+'\n','',1),
 'missing-text':s.replace(next(x for x in rows if x.startswith('TEXT '))+'\n','',1),
 'bad-queue-type':s.replace('MessagePhase A -1','UnknownPhase A -1',1),
 'missing-doubles-half':s.replace('C 1000 1 ','C 1000 0 ',1),
}
if subprocess.run([str(exe),sys.argv[2]],capture_output=True).returncode:raise SystemExit('intact failed')
with tempfile.TemporaryDirectory(prefix='flee-phase-malformed-') as tmp:
 for name,text in cases.items():
  if text==s:raise SystemExit('ineffective mutation '+name)
  p=Path(tmp)/name;p.write_text(text);r=subprocess.run([str(exe),str(p)],capture_output=True)
  print(name,'exit',r.returncode)
  if r.returncode!=2:raise SystemExit('malformed refusal failed')
