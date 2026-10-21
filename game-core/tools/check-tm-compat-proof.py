#!/usr/bin/env python3
"""Compile compatibility faults and reject malformed native TM captures."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[2]; src=(root/'game-core/src/gc_learnset.c').read_text(); trace=Path(sys.argv[1]).resolve(); exe=root/'game-core/build/tm_compat_diff_test'
a=src.index('int gc_mon_tm_state(');z=src.index('/* CanLearnMoveRequirement',a); part=src[a:z]
incs=['game-core/include','game-core/src','game-core/gen/out','battle-core/include','battle-core/gen/out','run-core/include','reward-core/include','reward-core/gen/out','ai-core/include']
flags=['-O2','-std=c99','-ffp-contract=off',*[f'-I{root/p}' for p in incs]]
libs=[root/'game-core/build'/p for p in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]
mutations=[('lost-known-priority','return 2;','return 1;'),('lost-fusion','|| (m->b.fusion_species && form_tms_have(m->b.fusion_species, m->b.fusion_form, move))','|| false'),('lost-primary-eligibility','return form_tms_have(m->b.species, m->b.form, move)','return false && form_tms_have(m->b.species, m->b.form, move)')]
with tempfile.TemporaryDirectory(dir=root/'work',prefix='tm-proof-') as tmp:
 d=Path(tmp)
 for name,old,new in mutations:
  assert part.count(old)==1
  fault=d/'gc_learnset.c';fault.write_text(src[:a]+part.replace(old,new,1)+src[z:]);obj=d/'fault.o';runner=d/'fault'
  subprocess.run(['gcc',*flags,'-c',fault,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,root/'game-core/tests/tm_compat_diff_test.c',obj,*libs,'-lm','-o',runner],check=True,capture_output=True)
  p=subprocess.run([runner,trace],capture_output=True);assert p.returncode==1,(name,p.returncode,p.stdout[-100:],p.stderr[-100:]);print(name,'caught',flush=True)
 data=trace.read_text();bad=d/'bad.txt'
 def reject(name,s):
  bad.write_text(s);p=subprocess.run([exe,bad],capture_output=True);assert p.returncode==1,(name,p.returncode);print(name,'rejected',flush=True)
 reject('version',data.replace('TM_COMPAT 1 ','TM_COMPAT 2 ',1));reject('count',data.replace('4500','4497',1));reject('footer',data[:data.rfind('TM_COMPAT_COMPLETE')]);reject('trailing',data+'JUNK\n')
 for col,val,name in [(1,'1','index'),(2,'9999','species'),(3,'1','form'),(4,'6','fusion'),(6,'2','profile'),(7,'3'+data.splitlines()[1].split()[7][1:],'state')]:
  lines=data.splitlines();row=lines[1].split();row[col]=val;lines[1]=' '.join(row);reject(name,'\n'.join(lines)+'\n')
print('3 compiled TM compatibility faults caught; 10 malformed captures rejected')
