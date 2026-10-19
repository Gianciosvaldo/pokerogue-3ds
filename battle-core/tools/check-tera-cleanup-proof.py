#!/usr/bin/env python3
"""Counter reset faults use source-observed cleanup results, not a local model."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve()
subprocess.run([r/'battle-core/build/tera-cleanup-test',trace],check=True)
s=(r/'battle-core/src/engine.c').read_text()
a='if(p->species==SPECIES_ID_TERAPAGOS || p->fusion_species==SPECIES_ID_TERAPAGOS)'
assert s.count(a)==1
faults=[('missing-Terapagos-reset',s.replace(a,'if(false)',1)),('missing-fusion-reset',s.replace(a,'if(p->species==SPECIES_ID_TERAPAGOS)',1)),('ordinary-trainer-resets-counter',s.replace(a,'if(true)',1))]
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out']]]
with tempfile.TemporaryDirectory(prefix='tera-reset-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,changed in faults:
  f=d/'engine.c';f.write_text(changed);o=d/'engine.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',f,'-o',o],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/tests/tera-cleanup-test.c',o,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  p=subprocess.run([exe,trace],capture_output=True);assert p.returncode==1,(name,p.returncode,p.stderr[-400:]);print(name,'caught',flush=True)
 data=trace.read_bytes()
 for name,bad in [('version',data.replace(b'TERA_CLEANUP 1 ',b'TERA_CLEANUP 2 ',1)),('trailing',data+b'JUNK\n'),('truncated',data.rsplit(b'TERA_CLEANUP_COMPLETE',1)[0])]:
  f=d/'bad';f.write_bytes(bad);p=subprocess.run([r/'battle-core/build/tera-cleanup-test',f],capture_output=True);assert p.returncode==2,(name,p.returncode);print(name,'rejected',flush=True)
 print('3 cleanup faults caught; 3 malformed captures rejected')
