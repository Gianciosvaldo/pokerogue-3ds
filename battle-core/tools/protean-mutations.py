#!/usr/bin/env python3
"""Detect compiled runtime mutations with unchanged source turns/helpers."""
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent
trace,helpers=map(lambda p:Path(p).resolve(),sys.argv[1:])
attrs=(root/'src/ab_attrs.c').read_text();phases=(root/'src/phases.c').read_text()
a0=attrs.index('    case PR_AC_PokemonTypeChangeAbAttr: {');a1=attrs.index('    /* ---- stats ---- */',a0);leaf=attrs[a0:a1]
mutants=[
 ('wrong-type','attrs','int8_t one = (int8_t)pr_get_move_type(bt, p, move);','int8_t one = POKEMON_TYPE_NORMAL;'),
 ('accept-already-same','attrs','if (same) return 0;','(void)same;'),
 ('tera-allowed','attrs','p->terastallized || ',' '),
 ('struggle-allowed','attrs','move == MOVE_ID_STRUGGLE','false'),
 ('called-moves-allowed','attrs','|| pr_move_has_attr((uint16_t)move, PR_AC_CallMoveAttr)\n            || pr_move_has_attr((uint16_t)move, PR_AC_NaturePowerAttr)',''),
 ('simulation-writes','attrs','if (!pp->simulated) {','if (true) {'),
 ('added-type-cleared','attrs','pr_set_summon_types(bt, (pr_pokemon *)p, &one, 1);','pr_set_summon_types(bt, (pr_pokemon *)p, &one, 1);\n            ((pr_pokemon *)p)->added_type=0;'),
 ('invented-once-per-summon','attrs','int type = pr_get_move_type(bt, p, move);','if(p->n_ab_applied_summon)return 0;\n        int type = pr_get_move_type(bt, p, move);'),
 ('missing-normal-caller','phases','pr_apply_ab_attrs(bt, PR_AC_PokemonTypeChangeAbAttr, &type_change);','(void)type_change;'),
 ('missing-failure-caller','phases','pr_apply_ab_attrs(bt, PR_AC_PokemonTypeChangeAbAttr, &params);','(void)params;'),
 ('wrong-message-type','attrs','c->msg_type = type;','c->msg_type = POKEMON_TYPE_GRASS;'),
 ('raw-move-type','raw','',''),
]
with tempfile.TemporaryDirectory(prefix='protean-mutants-') as work:
 tmp=Path(work);flags=[os.environ.get('CC','gcc'),'-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
 for name,target,a,b in [('intact','attrs','','')]+mutants:
  bodies={'attrs':attrs,'phases':phases}
  if target=='raw':bodies['attrs']=attrs[:a0]+leaf.replace('pr_get_move_type(bt, p, move)','pr_moves[move].type')+attrs[a1:]
  elif a:
   part=leaf if target=='attrs' else phases;assert a in part,(name,a)
   changed=part.replace(a,b,1)
   bodies[target]=attrs[:a0]+changed+attrs[a1:] if target=='attrs' else changed
  objects=[]
  for key,filename in [('attrs','ab_attrs.c'),('phases','phases.c')]:
   if name!='intact' and bodies[key]==(attrs if key=='attrs' else phases):continue
   p=tmp/filename;p.write_text(bodies[key]);obj=p.with_suffix('.o');objects.append(str(obj))
   subprocess.run(flags+['-c',str(p),'-o',str(obj)],check=True)
  statuses=[]
  for src,input_file in [('protean-test.c',trace),('protean-helper-test.c',helpers)]:
   exe=tmp/src[:-2]
   subprocess.run(flags+[str(root/'tests'/src)]+objects+[str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
   r=subprocess.run([str(exe),str(input_file)],capture_output=True,text=True);statuses.append(r.returncode)
  print(name,'turn/helper exits',statuses,flush=True)
  if name=='intact':
   if statuses!=[0,0]:raise SystemExit('intact must pass')
  elif any(s not in (0,1) for s in statuses) or 1 not in statuses:raise SystemExit('mutation not detected')
