#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys,tempfile
exe,trace=sys.argv[1:];s=Path(trace).read_text();ls=s.splitlines(True)
if subprocess.run([exe,trace],capture_output=True).returncode:raise SystemExit('intact must pass')
n=int(ls[0].split()[-1]);v={'footer':''.join(ls[:-1]),'wrong-count':s.replace(f'Z {n}\n',f'Z {n-1}\n'),'fractional-count':ls[0].rstrip()+'.5\n'+''.join(ls[1:]),'trailing':s+'C 1\n','case-order':s.replace('C 1 ','C 0 ',1),'truncated':s[:-4]}
for tag in ['C','R','P','I','H','J','K','A','B','D','S','T','E']:
 i=next(i for i,l in enumerate(ls) if l.startswith(tag+' '));v['missing-'+tag]=''.join(ls[:i]+ls[i+1:]);v['duplicate-'+tag]=''.join(ls[:i]+[ls[i]]+ls[i:]);a=ls[i].split();a[-1]='nan';v['nan-'+tag]=''.join(ls[:i]+[' '.join(a)+'\n']+ls[i+1:])
for name,tag,field,val in [('records','A',1,'4'),('decision','A',3,'2'),('draw-count','D',1,'1'),('fractional-ID','P',1,'1.5'),('stack','H',2,'3')]:
 i=next(i for i,l in enumerate(ls) if l.startswith(tag+' '));a=ls[i].split();a[field]=val;v[name]=''.join(ls[:i]+[' '.join(a)+'\n']+ls[i+1:])
i=next(i for i,l in enumerate(ls) if l.startswith('H '));a=ls[i].split();a[1]='UNKNOWN';v['unknown-class']=''.join(ls[:i]+[' '.join(a)+'\n']+ls[i+1:])
with tempfile.TemporaryDirectory() as d:
 for name,content in v.items():
  p=Path(d)/name;p.write_text(content);r=subprocess.run([exe,str(p)],capture_output=True);print(name,r.returncode)
  if r.returncode!=2:raise SystemExit('expected parse refusal2')
print(len(v),'malformed refused')
