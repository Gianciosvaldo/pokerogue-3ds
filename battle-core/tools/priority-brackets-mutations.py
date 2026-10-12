#!/usr/bin/env python3
"""Compile production mutations against the unchanged source turn trace."""
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent
trace=Path(sys.argv[1]).resolve()
names=['ab_attrs','engine','phases','battler_tags','closures_abilities']
bodies={n:(root/f'src/{n}.c').read_text() for n in names}
mutants=[
 ('last-becomes-first','ab_attrs','*pp->value = x->newModifier;','*pp->value = MOVE_PRIORITY_IN_BRACKET_FIRST;'),
 ('quick-draw-never','ab_attrs','< P(BypassSpeedChanceAbAttr, a)->chance','< 0'),
 ('status-draw-skipped','ab_attrs','GATE(pr_battle_rand_seed_int(bt, 100, 0) <','GATE(damage && pr_battle_rand_seed_int(bt, 100, 0) <'),
 ('quick-draw-status-allowed','ab_attrs','&& damage && pr_can_add_tag','&& pr_can_add_tag'),
 ('simulation-draws','ab_attrs','if (pp->simulated) return 0;','if (pp->simulated) (void)pr_battle_rand_seed_int(bt,100,0);'),
 ('prevent-allows','ab_attrs','*pp->flag = false;\n        return 1;\n    }\n    case PR_AC_WonderSkinAbAttr:','*pp->flag = true;\n        return 1;\n    }\n    case PR_AC_WonderSkinAbAttr:'),
 ('missing-draw-dispatch','phases','pr_apply_ab_attrs(bt, PR_AC_BypassSpeedChanceAbAttr, &params);','(void)params;'),
 ('missing-tag-dispatch','battler_tags','pr_apply_ab_attrs(bt, PR_AC_PreventBypassSpeedChanceAbAttr, &params);','(void)params;'),
 ('missing-bracket-dispatch','engine','pr_apply_ab_attrs(bt, PR_AC_ChangeMovePriorityInBracketAbAttr, &params);','(void)params;'),
 ('first-tag-becomes-normal','engine','if (pr_get_battler_tag(user, BATTLER_TAG_TYPE_BYPASS_SPEED)) return MOVE_PRIORITY_IN_BRACKET_FIRST;','if (pr_get_battler_tag(user, BATTLER_TAG_TYPE_BYPASS_SPEED)) return MOVE_PRIORITY_IN_BRACKET_NORMAL;'),
 ('mycelium-damage-condition','closures_abilities','return pr_moves[m].category == MOVE_CATEGORY_STATUS;','return pr_moves[m].category != MOVE_CATEGORY_STATUS;'),
 ('claw-before-draw','order','',''),
]
with tempfile.TemporaryDirectory(prefix='priority-brackets-mutants-') as work:
 tmp=Path(work);flags=[os.environ.get('CC','gcc'),'-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
 for name,target,a,b in [('intact','ab_attrs','','')]+mutants:
  changed=dict(bodies)
  if target=='order':
   call='        pr_apply_ab_attrs(bt, PR_AC_BypassSpeedChanceAbAttr, &params);'
   anchor='        pr_apply_bypass_speed_chance_modifiers(bt, e, p);'
   assert changed['phases'].count(call)==1 and changed['phases'].count(anchor)==1
   changed['phases']=changed['phases'].replace(call,'',1).replace(anchor,anchor+'\n'+call,1)
  elif a:
   assert a in changed[target],(name,a)
   changed[target]=changed[target].replace(a,b,1)
  objects=[]
  for key in names:
   if name!='intact' and changed[key]==bodies[key]:continue
   p=tmp/f'{key}.c';p.write_text(changed[key]);obj=p.with_suffix('.o');objects.append(str(obj))
   subprocess.run(flags+['-c',str(p),'-o',str(obj)],check=True)
  exe=tmp/'replay'
  subprocess.run(flags+['-I'+str(root/'build'),str(root/'tests/priority-brackets-test.c')]+objects+[str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
  r=subprocess.run([str(exe),str(trace)],capture_output=True,text=True)
  print(name,'exit',r.returncode,flush=True)
  if r.returncode!=(0 if name=='intact' else 1):
   print(r.stdout[-5000:],r.stderr[-5000:]);raise SystemExit('mutation contract failed')
