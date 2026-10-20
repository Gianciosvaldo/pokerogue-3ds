#!/usr/bin/env python3
"""Prove the complete-turn wrapper refuses a real missing Costar implementation."""
from pathlib import Path
import os,subprocess,sys,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();assert os.environ.get('TURNTEST_LOCALES')
s=(r/'battle-core/src/ab_attrs.c').read_text();a=s.index('    case PR_AC_PostSummonCopyAllyStatsAbAttr: {');b=s.index('\n    case PR_AC_',a+1);s=s[:a]+s[b:]
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(r/'battle-core'/p) for p in ['include','gen/out','tests']]]
with tempfile.TemporaryDirectory(prefix='complete-turns-guard-',dir=r/'battle-core/build') as tmp:
 d=Path(tmp);src=d/'ab_attrs.c';src.write_text(s);obj=d/'ab_attrs.o';exe=d/'missing-costar'
 subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
 subprocess.run(['gcc',*flags,r/'battle-core/tests/doublestest.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
 partial=subprocess.run([exe,trace],text=True,capture_output=True)
 assert partial.returncode==0 and 'covered 4096: pass 4096, fail 0; not covered 4096' in partial.stdout,(partial.returncode,partial.stdout[:4000],partial.stderr[-1000:])
 full=subprocess.run([sys.executable,r/'battle-core/tools/check-complete-turns.py',exe,trace],text=True,capture_output=True)
 assert full.returncode==1 and 'Incomplete native turn proof:' in full.stderr,(full.returncode,full.stdout[:4000],full.stderr[-1000:])
 wide=d/'wide-missing-costar'
 subprocess.run(['gcc',*flags,r/'battle-core/build/wide-combat-replay.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',wide],check=True,capture_output=True)
 prior=subprocess.run([wide,trace],text=True,capture_output=True)
 assert prior.returncode==1 and 'Wide combat requires all 8192 native turns, compared 4096' in prior.stderr,(prior.returncode,prior.stderr[-1000:])
 print('Real missing-Costar general runner compares only 4096/8192; complete-proof wrapper rejects it')
 print('Existing wide runner independently retains its all-cases requirement')
 data=trace.read_bytes()
 for name,changed in [('duplicate native ID',data.replace(b'\nCASE 1\n',b'\nCASE 0\n',1)),('wrong source count',data.replace(b'# DOUBLES_CONTRACT 1 8192',b'# DOUBLES_CONTRACT 1 8191',1))]:
  bad=d/'bad.txt';bad.write_bytes(changed)
  p=subprocess.run([sys.executable,r/'battle-core/tools/check-complete-turns.py',r/'battle-core/build/wide-combat-test',bad],text=True,capture_output=True)
  assert p.returncode==1 and 'Incomplete native turn proof:' in p.stderr,(name,p.returncode,p.stderr[-1000:])
  print(name,'rejected')
