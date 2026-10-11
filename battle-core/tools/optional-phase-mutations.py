#!/usr/bin/env python3
"""Rebuild independent C metadata mapping; production and expectations unchanged."""
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent;typed,fixtures=map(lambda s:Path(s).resolve(),sys.argv[1:]);lib=root/'build/libprcore.a'
with tempfile.TemporaryDirectory(prefix='optional-phase-mutations-') as scratch:
 tmp=Path(scratch)
 for name,mutated in [('intact',False),('absent-as-integer-minus-one',True)]:
  dest=tmp/name;dest.mkdir();mapping=(root/'tests/optional-phase-map.inc').read_text()
  if mutated:
   assert mapping.count('case PH_SWITCH_SUMMON:')==1;mapping=mapping.replace('case PH_SWITCH_SUMMON:','',1)
  (dest/'optional-phase-map.inc').write_text(mapping)
  for f in ['optional-phase-test.c','optional-phase-fixtures-test.c']:(dest/f).write_text((root/'tests'/f).read_text())
  subprocess.run(['python3',str(root/'tools/build-optional-phase-replay.py'),str(root/'tests/turntest.c'),str(dest/'optional-phase-replay.inc')],check=True)
  for stem,trace in [('optional-phase-test',typed),('optional-phase-fixtures-test',fixtures)]:
   exe=dest/stem;log=dest/(stem+'.log')
   cmd=[os.environ.get('CC','gcc'),'-O2','-g','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out'),'-I'+str(root/'tests'),'-I'+str(dest),str(dest/(stem+'.c')),str(lib),'-lm','-o',str(exe)]
   with log.open('w') as f:
    r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
    if r.returncode:raise SystemExit('mutation build failed: '+log.read_text()[-3000:])
    r=subprocess.run([str(exe),str(trace)],stdout=f,stderr=subprocess.STDOUT)
   print(name,stem,'exit',r.returncode,flush=True)
   for l in log.read_text().splitlines():
    if l.startswith(('FAIL CASE','covered ','Optional ','fixture')):print(l,flush=True)
   if r.returncode!=(1 if mutated else 0):raise SystemExit('unexpected mutation status')
