#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys,tempfile
exe,trace=sys.argv[1:];s=Path(trace).read_text();lines=s.splitlines(True)
variants={'missing-footer':''.join(lines[:-1]),'wrong-header':s.replace('SOOTHEACQ_CALLER 1 2000','SOOTHEACQ_CALLER 1 2001',1),'zero-count':s.replace('SOOTHEACQ_CALLER 1 2000','SOOTHEACQ_CALLER 1 0',1),'fractional-count':s.replace('SOOTHEACQ_CALLER 1 2000','SOOTHEACQ_CALLER 1 2000.5',1),'duplicate-case':s.replace('C 1 ','C 0 ',1),'out-of-order':s.replace('C 1 ','C 3 ',1),'truncated':s[:-8],'trailing':s+'C 2000\n','invalid-owner':s.replace(' -1 0 0 0\n',' -2 0 0 0\n',1)}
for prefix in ['C ','R ','P ','I ','H ','A ','T ','J ','K ','S ','E ']:
 i=next(i for i,l in enumerate(lines) if l.startswith(prefix))
 variants['missing-'+prefix.strip()]=''.join(lines[:i]+lines[i+1:])
 variants['duplicate-'+prefix.strip()]=''.join(lines[:i]+[lines[i]]+lines[i:])
i=next(i for i,l in enumerate(lines) if l.startswith('P '))
for name,index,value in [('nan-party',1,'nan'),('invalid-species',2,'99999'),('invalid-level',6,'0'),('fractional-iv',43,'1.5')]:
 v=lines[i].split();v[index]=value;variants[name]=''.join(lines[:i]+[' '.join(v)+'\n']+lines[i+1:])
i=next(i for i,l in enumerate(lines) if l.startswith('T '))
for name,index,value in [('nan-after',1,'nan'),('invalid-after-species',2,'99999'),('invalid-after-level',6,'0')]:
 v=lines[i].split();v[index]=value;variants[name]=''.join(lines[:i]+[' '.join(v)+'\n']+lines[i+1:])
with tempfile.TemporaryDirectory(prefix='soothe-acquisition-malformed-') as tmp:
 for name,text in variants.items():
  p=Path(tmp)/name;p.write_text(text);r=subprocess.run([exe,str(p)],capture_output=True);print(name,r.returncode)
  if r.returncode!=2:raise SystemExit(f'{name}: expected2, got{r.returncode}')
print(len(variants),'malformed traces refused')
