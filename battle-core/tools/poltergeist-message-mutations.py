#!/usr/bin/env python3
"""Restore the missing runtime announcement; intact source traces must reject it."""
from pathlib import Path
import os, subprocess, sys, tempfile
root=Path(__file__).resolve().parent.parent
fixtures, *turns=map(lambda s:Path(s).resolve(),sys.argv[1:])
if len(turns)!=4 or not os.environ.get('TURNTEST_LOCALES'):raise SystemExit(2)
with tempfile.TemporaryDirectory(prefix='poltergeist-runtime-mutation-') as work:
 tmp=Path(work)
 source=(root/'src/phases.c').read_text()
 anchor='''        *out = pr_msg2("moveTriggers:attackedByItem",
            pr_arg_pokemon(bt, "pokemonName", target), pr_arg_key("itemName", key));'''
 assert source.count(anchor)==1
 (tmp/'phases.c').write_text(source.replace(anchor,'        *out = pr_msg0(NULL);',1))
 flags=[os.environ.get('CC','gcc'),'-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
 subprocess.run(flags+['-c',str(tmp/'phases.c'),'-o',str(tmp/'phases.o')],check=True)
 for stem,traces in [('poltergeist-message-test',[fixtures]),('turntest',turns)]:
  mutant=tmp/stem
  subprocess.run(flags+['-I'+str(root/'tests'),str(root/f'tests/{stem}.c'),str(tmp/'phases.o'),str(root/'build/libprcore.a'),'-lm','-o',str(mutant)],check=True)
  for trace in traces:
   for name,exe,wanted in [('intact',root/f'build/{stem}',0),('missing-announcement',mutant,1)]:
    r=subprocess.run([str(exe),str(trace)],capture_output=True,text=True)
    print(name,stem,trace.name,'exit',r.returncode)
    for line in r.stdout.splitlines():
     if line.startswith(('Poltergeist messages','covered ','texts of covered')):print(line)
    if r.returncode!=wanted:raise SystemExit('unexpected mutation status: '+r.stdout[-2000:]+r.stderr)
