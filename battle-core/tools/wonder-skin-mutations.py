#!/usr/bin/env python3
"""Production mutations must fail unchanged source turns or dispatch helpers."""
from pathlib import Path
import os, subprocess, sys, tempfile
root=Path(__file__).resolve().parent.parent;trace,helpers=map(lambda p:Path(p).resolve(),sys.argv[1:])
attrs=(root/'src/ab_attrs.c').read_text();phases=(root/'src/phases.c').read_text()
a0=attrs.index('    case PR_AC_WonderSkinAbAttr:');a1=attrs.index('    case PR_AC_StatMultiplierAbAttr:',a0);leaf=attrs[a0:a1]
mutants=[
 ('cap-45','attrs','*pp->value = 50;','*pp->value = 45;'),
 ('damage-also-capped','attrs','pr_moves[move].category == MOVE_CATEGORY_STATUS &&',''),
 ('threshold-above-50','attrs','*pp->value >= 50','*pp->value > 50'),
 ('below-50-also-capped','attrs','*pp->value >= 50','*pp->value >= 0'),
 ('simulation-skipped','attrs','        *pp->value = 50;','        if(pp->simulated)return 1;\n        *pp->value = 50;'),
 ('missing-caller','phases','pr_apply_ab_attrs(bt, PR_AC_WonderSkinAbAttr, &wonder);','(void)wonder;'),
 ('wrong-holder','phases','.pokemon = target, .opponent = user, .move = move, .value = &acc','.pokemon = user, .opponent = target, .move = move, .value = &acc'),
 ('after-wide-lens','order','',''),
]
with tempfile.TemporaryDirectory(prefix='wonder-skin-mutants-') as work:
 tmp=Path(work);flags=[os.environ.get('CC','gcc'),'-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
 for name,target,a,b in [('intact','attrs','','')]+mutants:
  bodies={'attrs':attrs,'phases':phases}
  if target=='order':
   call='    pr_apply_ab_attrs(bt, PR_AC_WonderSkinAbAttr, &wonder);';anchor='        pr_apply_accuracy_booster_modifiers(bt, user, &acc);'
   assert phases.count(call)==1 and phases.count(anchor)==1
   bodies['phases']=phases.replace(call,'',1).replace(anchor,anchor+'\n'+call,1)
  elif a:
   part=leaf if target=='attrs' else phases;assert a in part,(name,a);changed=part.replace(a,b,1)
   bodies[target]=attrs[:a0]+changed+attrs[a1:] if target=='attrs' else changed
  objects=[]
  for key,filename in [('attrs','ab_attrs.c'),('phases','phases.c')]:
   p=tmp/filename;p.write_text(bodies[key]);obj=p.with_suffix('.o');objects.append(str(obj));subprocess.run(flags+['-c',str(p),'-o',str(obj)],check=True)
  statuses=[]
  for src,input_file in [('wonder-skin-test.c',trace),('wonder-skin-helper-test.c',helpers)]:
   exe=tmp/src[:-2];subprocess.run(flags+['-I'+str(root/'build'),str(root/'tests'/src)]+objects+[str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
   r=subprocess.run([str(exe),str(input_file)],capture_output=True,text=True);statuses.append(r.returncode)
  print(name,'turn/helper exits',statuses,flush=True)
  if name=='intact':
   if statuses!=[0,0]:raise SystemExit('intact must pass')
  elif any(s not in (0,1) for s in statuses) or 1 not in statuses:raise SystemExit('mutation not detected')
