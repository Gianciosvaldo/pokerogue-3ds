#!/usr/bin/env python3
from pathlib import Path
import subprocess, sys, tempfile
exe,trace=sys.argv[1:];s=Path(trace).read_text();lines=s.splitlines(True)
if subprocess.run([exe,trace],capture_output=True).returncode:raise SystemExit('intact control must pass')
variants={
 'missing-footer':''.join(lines[:-1]),
 'wrong-version':s.replace('HISTORY_CONTRACT 1 ', 'HISTORY_CONTRACT 2 ',1),
 'fractional-count':s.replace('HISTORY_CONTRACT 1 672','HISTORY_CONTRACT 1 672.5',1),
 'case-order':s.replace('C 1 ','C 0 ',1),
 'missing-case':''.join(lines[:1]+lines[2:]),
 'trailing-case':s+lines[1],
 'wrong-footer':s.replace('HISTORY_COMPLETE 672','HISTORY_COMPLETE 671',1),
 'bad-text-encoding':s.replace(' 1 - 1 - ',' 1 zz 1 - ',1),
}
with tempfile.TemporaryDirectory(prefix='history-malformed-') as work:
 for name,data in variants.items():
  if data==s:raise SystemExit('mutation anchor absent: '+name)
  p=Path(work)/name;p.write_text(data);r=subprocess.run([exe,str(p)],capture_output=True).returncode
  print(name,'exit',r)
  if r!=2:raise SystemExit('unexpected malformed status')
print('history malformed refusals8 after intact0 control')
