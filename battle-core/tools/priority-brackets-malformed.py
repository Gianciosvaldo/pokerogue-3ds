#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys,tempfile
exe,trace=sys.argv[1:];s=Path(trace).read_text();lines=s.splitlines(True);wanted=int(lines[0].split()[-1])
if subprocess.run([exe,trace],capture_output=True).returncode:raise SystemExit('intact control must pass')
variants={'missing-footer':''.join(lines[:-1]),'wrong-count':s.replace(f'# BRACKETS_COMPLETE {wanted}',f'# BRACKETS_COMPLETE {wanted-1}',1),'version':s.replace('# BRACKETS_CONTRACT 1 ','# BRACKETS_CONTRACT 2 ',1),'fractional-count':s.replace(f'# BRACKETS_CONTRACT 1 {wanted}',f'# BRACKETS_CONTRACT 1 {wanted}.5',1),'case-order':s.replace('CASE 1\n','CASE 0\n',1),'trailing':s+'CASE 0\n','duplicate-RUN':s.replace('RUN\n','RUN\nRUN\n',1),'missing-RUN':s.replace('RUN\n','',1),'missing-END':''.join(l for i,l in enumerate(lines) if i!=next(i for i,l in enumerate(lines) if l.startswith('END ')))}
variants['missing-bracket']=s.replace(next(l for l in lines if l.startswith('BRACKET ')), '', 1)
variants['duplicate-bracket']=s.replace(next(l for l in lines if l.startswith('BRACKET ')), next(l for l in lines if l.startswith('BRACKET '))*2, 1)
first=next(l for l in lines if l.startswith('BRACKET '));value=first.split('value=')[1].strip()
variants['nonfinite-bracket']=s.replace(first,first.replace('value='+value,'value=nan'),1)
with tempfile.TemporaryDirectory(prefix='priority-brackets-malformed-') as work:
 for name,data in variants.items():
  p=Path(work)/name;p.write_text(data);r=subprocess.run([exe,str(p)],capture_output=True).returncode
  print(name,'exit',r)
  if r!=2:raise SystemExit('unexpected parse refusal')
print('Priority brackets malformed refusals12 after intact0 control')
