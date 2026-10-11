#!/usr/bin/env python3
"""Separate malformed-transport checks; production/source expectations stay intact."""
from pathlib import Path
import subprocess, sys, tempfile
exe,trace=sys.argv[1:];s=Path(trace).read_text();lines=s.splitlines(True)
variants={'missing-footer':''.join(lines[:-1]),'truncated-record':s[:-8], 'fractional-count':s.replace('CONTRACT 1 2000','CONTRACT 1 2000.5',1), 'wrong-count':s.replace('COMPLETE 2000','COMPLETE 1999'), 'duplicate-case':s.replace('CASE 1\n','CASE 0\n',1), 'out-of-order-case':s.replace('CASE 1\n','CASE 3\n',1), 'trailing-case':s+'CASE 2000\n'}
for prefix in ['TRACE ','TEXTS ','QUEUE ','MON ','TURN ','MOVES ','HIST ','MQ ','EFFECTIVE ','PARTY ','ARENA ','RNG ','RUN\n','DAMAGEPROBE ']:
 i=next(i for i,l in enumerate(lines) if l.startswith(prefix))
 variants['missing-'+prefix.strip()]=''.join(lines[:i]+lines[i+1:])
 variants['duplicate-'+prefix.strip()]=''.join(lines[:i]+[lines[i]]+lines[i:])
for key,bad in [('sp','99999'),('id','-2'),('abil','99999'),('st','99999'),('ss','nan,0,0,0,0,0'),('stages','7,0,0,0,0,0,0'),('stats','0,1,2,3,4,5')]:
 i=next(i for i,l in enumerate(lines) if l.startswith('MON '));tokens=lines[i].split()
 tokens=[key+'='+bad if t.startswith(key+'=') else t for t in tokens]
 variants['invalid-'+key]=''.join(lines[:i]+[' '.join(tokens)+'\n']+lines[i+1:])
with tempfile.TemporaryDirectory(prefix='average-stats-malformed-') as d:
 for name,text in variants.items():
  p=Path(d)/name;p.write_text(text)
  r=subprocess.run([exe,str(p)],capture_output=True)
  print(f'{name}: exit {r.returncode}')
  if r.returncode!=2:raise SystemExit(f'{name}: expected parse refusal 2')
print(f'{len(variants)} malformed traces refused')
