#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,sys,tempfile
exe,trace=sys.argv[1:];s=Path(trace).read_text();ls=s.splitlines(True)
if not os.environ.get('GC_LOCALES'):raise SystemExit('GC_LOCALES required')
if subprocess.run([exe,trace],capture_output=True).returncode:raise SystemExit('intact must pass')
n=int(ls[0].split()[-1]);variants={'footer':''.join(ls[:-1]),'wrong-count':s.replace(f'Z {n}\n',f'Z {n-1}\n'),'fractional-count':ls[0].rstrip()+'.5\n'+''.join(ls[1:]),'trailing':s+'C 1\n','case-order':s.replace('C 1 ','C 0 ',1),'truncated':s[:-4]}
for tag in ['C','R','F','P','I','H','A','M','T','J','K','S','E']:
 i=next(i for i,l in enumerate(ls) if l.startswith(tag+' '));variants['missing-'+tag]=''.join(ls[:i]+ls[i+1:]);variants['duplicate-'+tag]=''.join(ls[:i]+[ls[i]]+ls[i:]);a=ls[i].split();a[-1]='nan';variants['nan-'+tag]=''.join(ls[:i]+[' '.join(a)+'\n']+ls[i+1:])
for name,tag,field,val in [('party','C',2,'7'),('kind','C',3,'3'),('path','C',4,'2'),('eligibility','F',1,'2'),('owner','H',3,'9'),('stock-count','I',1,'999'),('fractional-ID','P',1,'1.5')]:
 i=next(i for i,l in enumerate(ls) if l.startswith(tag+' '));a=ls[i].split();a[field]=val;variants[name]=''.join(ls[:i]+[' '.join(a)+'\n']+ls[i+1:])
with tempfile.TemporaryDirectory() as d:
 for name,content in variants.items():
  p=Path(d)/name;p.write_text(content);r=subprocess.run([exe,str(p)],capture_output=True);print(name,r.returncode)
  if r.returncode!=2:raise SystemExit('expected parse refusal2')
print(len(variants),'malformed refused')
