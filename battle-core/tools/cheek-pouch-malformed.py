#!/usr/bin/env python3
"""Separate malformed-transport checks; production/source expectations stay intact."""
from pathlib import Path
import subprocess, sys, tempfile, os
if not os.environ.get("TURNTEST_LOCALES"):raise SystemExit("TURNTEST_LOCALES required: refuse to run vacuous negative checks")
exe,trace=sys.argv[1:];
if subprocess.run([exe,trace],capture_output=True).returncode!=0:raise SystemExit("intact trace must pass before malformed checks")
s=Path(trace).read_text();lines=s.splitlines(True)
variants={'invalid-end-phase':s.replace('END stop=TurnInitPhase','END stop=NoSuchPhase',1),'missing-footer':''.join(lines[:-1]),'truncated-record':s[:-8], 'fractional-count':s.replace('CONTRACT 1 2000','CONTRACT 1 2000.5',1), 'wrong-count':s.replace('COMPLETE 2000','COMPLETE 1999'), 'duplicate-case':s.replace('CASE 1\n','CASE 0\n',1), 'out-of-order-case':s.replace('CASE 1\n','CASE 3\n',1), 'trailing-case':s+'CASE 2000\n'}
for prefix in ['TRACE ','TEXTS ','QUEUE ','MON ','TURN ','MOVES ','HIST ','MQ ','EFFECTIVE ','PARTY ','ARENA ','RNG ','RUN\n']:
 i=next(i for i,l in enumerate(lines) if l.startswith(prefix))
 variants['missing-'+prefix.strip()]=''.join(lines[:i]+lines[i+1:])
 variants['duplicate-'+prefix.strip()]=''.join(lines[:i]+[lines[i]]+lines[i:])
for key,bad in [('types','999'),('added','999'),('sp','99999'),('id','-2'),('abil','99999'),('st','99999'),('ss','nan,0,0,0,0,0'),('stages','7,0,0,0,0,0,0'),('stats','0,1,2,3,4,5')]:
 i=next(i for i,l in enumerate(lines) if l.startswith('MON '));tokens=lines[i].split()
 tokens=[key+'='+bad if t.startswith(key+'=') else t for t in tokens]
 variants['invalid-'+key]=''.join(lines[:i]+[' '.join(tokens)+'\n']+lines[i+1:])
for key in ['terrain','biome']:
 i=next(i for i,l in enumerate(lines) if l.startswith('ARENA '));tokens=lines[i].split();tokens=[key+'=999' if t.startswith(key+'=') else t for t in tokens]
 variants['invalid-'+key]=''.join(lines[:i]+[' '.join(tokens)+'\n']+lines[i+1:])
i=next(i for i,l in enumerate(lines) if l.startswith('MOD '))
for key,bad in [('btype','999'),('stack','4'),('pid','-1'),('side','2')]:
 tokens=lines[i].split();tokens=[key+'='+bad if t.startswith(key+'=') else t for t in tokens]
 variants['invalid-mod-'+key]=''.join(lines[:i]+[' '.join(tokens)+'\n']+lines[i+1:])
with tempfile.TemporaryDirectory(prefix='cheek-pouch-malformed-') as d:
 for name,text in variants.items():
  p=Path(d)/name;p.write_text(text)
  r=subprocess.run([exe,str(p)],capture_output=True)
  print(f'{name}: exit {r.returncode}')
  if r.returncode!=2:raise SystemExit(f'{name}: expected parse refusal 2')
print(f'{len(variants)} malformed traces refused')
