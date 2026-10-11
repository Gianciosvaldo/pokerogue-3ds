#!/usr/bin/env python3
"""Require a passing control before checking owning strict transport refusals."""
from pathlib import Path
import subprocess,sys,tempfile
exe,trace=sys.argv[1:];s=Path(trace).read_text();ls=s.splitlines(True)
if subprocess.run([exe,trace],capture_output=True).returncode:raise SystemExit('intact must pass')
n=int(ls[0].split()[-1])
v={'footer':''.join(ls[:-1]),'wrong-count':s.replace(f'Z {n}\n',f'Z {n-1}\n'),'fractional-count':ls[0].rstrip()+'.5\n'+''.join(ls[1:]),'trailing':s+'C 1\n','case-order':s.replace('C 1 ','C 0 ',1),'truncated':s[:-4]}
for tag in ['C','R','P','I','H','T','A','S','E','F']:
 i=next(i for i,l in enumerate(ls) if l.startswith(tag+' '));v['missing-'+tag]=''.join(ls[:i]+ls[i+1:]);v['duplicate-'+tag]=''.join(ls[:i]+[ls[i]]+ls[i:]);a=ls[i].split();a[-1]='nan';v['nan-'+tag]=''.join(ls[:i]+[' '.join(a)+'\n']+ls[i+1:])
for tag,field,val in [('C',2,'7'),('C',3,'3'),('H',1,'9'),('H',2,'4'),('P',16,'256'),('P',55,'0.5')]:
 i=next(i for i,l in enumerate(ls) if l.startswith(tag+' '));a=ls[i].split();a[field]=val;v[f'range-{tag}-{field}']=''.join(ls[:i]+[' '.join(a)+'\n']+ls[i+1:])
with tempfile.TemporaryDirectory() as d:
 for name,content in v.items():
  p=Path(d)/name;p.write_text(content);r=subprocess.run([exe,str(p)],capture_output=True);print(name,r.returncode)
  if r.returncode!=2:raise SystemExit('expected parse refusal2')
print(len(v),'malformed refused')
