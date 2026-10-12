#!/usr/bin/env python3
"""Compiled hit-count/order mutations against unchanged source turns and helpers."""
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent;trace,helpers=map(lambda p:Path(p).resolve(),sys.argv[1:])
bodies={n:(root/f'src/{n}.c').read_text() for n in ['ab_attrs','phases']}
a0=bodies['ab_attrs'].index('    case PR_AC_AddSecondStrikeAbAttr:');a1=bodies['ab_attrs'].index('    case PR_AC_ChangeMovePriorityInBracketAbAttr:',a0)
p0=bodies['phases'].index('    if (user->hits_left == -1) {');p1=bodies['phases'].index('    fx->entry.move =',p0)
mutants=[
 ('two-extra-strikes','ab_attrs','*pp->value += 1;','*pp->value += 2;'),
 ('replace-instead-of-add','ab_attrs','*pp->value += 1;','*pp->value = 2;'),
 ('ignored-eligibility','ab_attrs','GATE(pr_move_can_be_multi_strike_enhanced_ex(bt, move, p, true, o));','GATE(true);'),
 ('parent-simulation-skipped','ab_attrs','*pp->value += 1;','if(!pp->simulated)*pp->value += 1;'),
 ('wrong-skill-bucket','ab_attrs','*pp->value = 0;','*pp->value = 5;'),
 ('skill-simulation-skipped','ab_attrs','*pp->value = 0;','if(!pp->simulated)*pp->value = 0;'),
 ('missing-parent-caller','phases','pr_apply_ab_attrs(bt, PR_AC_AddSecondStrikeAbAttr, &second_strike);','(void)second_strike;'),
 ('missing-skill-caller','phases','pr_apply_ab_attrs(bt, PR_AC_MaxMultiHitAbAttr, &max_hits);','(void)max_hits;'),
 ('skip-skill-rng','phases','double rolled = pr_battle_rand_seed_int(bt, 20, 0);','double rolled = pr_has_ability_with_attr(bt,user,PR_AC_MaxMultiHitAbAttr)?0:pr_battle_rand_seed_int(bt,20,0);'),
 ('wrong-parent-move','phases','.move = move, .value = &count','.move = MOVE_ID_DOUBLE_SLAP, .value = &count'),
 ('skill-off-by-one-bucket','phases','v >= 3 ? 4 : 5','v >= 0 ? 4 : 5'),
 ('missing-last-strike-reduction','quarter','',''),
]
with tempfile.TemporaryDirectory(prefix='multi-hit-ability-mutants-') as work:
 tmp=Path(work);flags=[os.environ.get('CC','gcc'),'-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
 for name,target,a,b in [('intact','ab_attrs','','')]+mutants:
  changed=dict(bodies)
  if target=='quarter':
   anchor='    case PR_AC_MoveDamageBoostAbAttr: {';assert changed['ab_attrs'].count(anchor)==1
   changed['ab_attrs']=changed['ab_attrs'].replace(anchor,anchor+'\n        if(c->ability==ABILITY_ID_PARENTAL_BOND)return 0;',1)
  elif a:
   start,end=(a0,a1) if target=='ab_attrs' else (p0,p1);part=changed[target][start:end]
   assert part.count(a)==1,(name,a)
   changed[target]=changed[target][:start]+part.replace(a,b,1)+changed[target][end:]
  objects=[]
  for key in bodies:
   if name!='intact' and changed[key]==bodies[key]:continue
   p=tmp/f'{key}.c';p.write_text(changed[key]);obj=p.with_suffix('.o');objects.append(str(obj))
   subprocess.run(flags+['-c',str(p),'-o',str(obj)],check=True)
  statuses=[]
  for src,input_file in [('multi-hit-abilities-test.c',trace),('multi-hit-abilities-helper-test.c',helpers)]:
   exe=tmp/src[:-2]
   subprocess.run(flags+[str(root/'tests'/src)]+objects+[str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
   r=subprocess.run([str(exe),str(input_file)],capture_output=True,text=True);statuses.append(r.returncode)
  print(name,'turn/helper exits',statuses,flush=True)
  if name=='intact':
   if statuses!=[0,0]:raise SystemExit('intact must pass')
  elif any(s not in (0,1) for s in statuses) or 1 not in statuses:raise SystemExit('mutation not detected')
