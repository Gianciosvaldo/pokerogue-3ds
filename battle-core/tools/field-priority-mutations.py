#!/usr/bin/env python3
"""Compiled caller/leaf mutations must fail unchanged actual-source turns."""
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent;trace=Path(sys.argv[1]).resolve()
bodies={n:(root/f'src/{n}.c').read_text() for n in ['phases','ab_attrs']}
p0=bodies['phases'].index('    bool failed = fails_conditions || terrain;');p1=bodies['phases'].index('    if (failed) {',p0)
a0=bodies['ab_attrs'].index('    case PR_AC_FieldPriorityMoveImmunityAbAttr: {');a1=bodies['ab_attrs'].index('    case PR_AC_FullHpResistTypeAbAttr:',a0)
mutants=[
 ('missing-dispatch','phases','pr_apply_ab_attrs(bt, PR_AC_FieldPriorityMoveImmunityAbAttr, &params);','(void)params;'),
 ('ignore-cancellation','phases','failed = cancelled;','failed = false;'),
 ('wrong-field','phases','(i < 2) != user->is_player','(i < 2) == user->is_player'),
 ('wrong-attacker','phases','.opponent = user,','.opponent = o,'),
 ('wrong-move','phases','.move = m->move.id,','.move = MOVE_ID_TACKLE,'),
 ('simulated-real-caller','phases','.flag = &cancelled','.flag = &cancelled, .simulated = true'),
 ('zero-priority-blocked','ab_attrs','> 0 &&','>= 0 &&'),
 ('negative-priority-blocked','ab_attrs','> 0 &&','> -2 &&'),
 ('duplicate-cancellation','ab_attrs','!*pp->flag &&',''),
 ('no-leaf-cancel','ab_attrs','*pp->flag = true;','*pp->flag = false;'),
]
with tempfile.TemporaryDirectory(prefix='field-priority-mutants-') as work:
 tmp=Path(work);flags=[os.environ.get('CC','gcc'),'-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
 for name,target,a,b in [('intact','phases','','')]+mutants:
  changed=dict(bodies)
  if a:
   start,end=(p0,p1) if target=='phases' else (a0,a1);part=changed[target][start:end]
   assert part.count(a)==1,(name,a)
   changed[target]=changed[target][:start]+part.replace(a,b,1)+changed[target][end:]
  objects=[]
  for key in bodies:
   if name!='intact' and changed[key]==bodies[key]:continue
   p=tmp/f'{key}.c';p.write_text(changed[key]);obj=p.with_suffix('.o');objects.append(str(obj))
   subprocess.run(flags+['-c',str(p),'-o',str(obj)],check=True)
  exe=tmp/'replay'
  subprocess.run(flags+[str(root/'tests/field-priority-test.c')]+objects+[str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
  r=subprocess.run([str(exe),str(trace)],capture_output=True,text=True)
  print(name,'exit',r.returncode,flush=True)
  if r.returncode!=(0 if name=='intact' else 1):raise SystemExit('mutation not detected')
