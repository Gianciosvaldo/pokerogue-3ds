#!/usr/bin/env python3
"""Compile actual callback/phase faults and require strict native replay failures."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/form-block-damage-test';subprocess.run([runner,trace],check=True,env=env);subprocess.run([r/'battle-core/build/wide-combat-test',turns],check=True,env=env)
s=(r/'battle-core/src/ab_attrs.c').read_text();a=s.index('    case PR_AC_FormBlockDamageAbAttr: {');b=s.index('    case PR_AC_AlliedFieldDamageReductionAbAttr:',a);leaf=s[a:b];faults=[]
def fault(name,old,new):
 assert leaf.count(old)==1,(name,leaf.count(old));faults.append((name,'ab_attrs.c',s[:a]+leaf.replace(old,new,1)+s[b:],False))
fault('missing-zero-damage','*pp->value = 0;','/* no damage block */')
fault('missing-simulation-gate','if (pp->simulated) return 1;','/* no simulation guard */')
fault('missing-form-gate','p->form == x->formIndex','true')
fault('wrong-form-gate','p->form == x->formIndex','p->form != x->formIndex')
fault('missing-damage-gate','*pp->value > 0','true')
fault('missing-category-condition','call_bool(bt, x->condition, a->cls, p, o, move)','true')
fault('missing-substitute-gate','!pr_move_hits_substitute(bt, move, o, p)','true')
fault('extra-hp-gate','GATE(*pp->value > 0','GATE(p->hp>0 && *pp->value > 0')
fault('extra-species-gate','GATE(*pp->value > 0','GATE((p->species==SPECIES_ID_MIMIKYU||p->species==SPECIES_ID_EISCUE) && *pp->value > 0')
fault('wrong-max-hp','to_dmg_value(pr_get_stat(p, STAT_HP, true) * x->recoil), HIT_RESULT_INDIRECT','to_dmg_value(pr_get_stat(p, STAT_HP, false) * x->recoil), HIT_RESULT_INDIRECT')
fault('wrong-recoil-amount','to_dmg_value(pr_get_stat(p, STAT_HP, true) * x->recoil), HIT_RESULT_INDIRECT','to_dmg_value(pr_get_stat(p, STAT_HP, true) * x->recoil * 2), HIT_RESULT_INDIRECT')
fault('missing-recoil','if (x->recoil > 0)\n            pr_damage_and_update_segments','if (false)\n            pr_damage_and_update_segments')
fault('clamped-boss-recoil','HIT_RESULT_INDIRECT, true, true, NULL','HIT_RESULT_INDIRECT, false, true, NULL')
fault('queued-faint','HIT_RESULT_INDIRECT, true, true, NULL','HIT_RESULT_INDIRECT, true, false, NULL')
fault('wrong-result','HIT_RESULT_INDIRECT, true, true, NULL','HIT_RESULT_EFFECTIVE, true, true, NULL')
fault('missing-form-queue','pr_form_change(bt, p, PR_FCT_SpeciesFormChangeAbilityTrigger);','/* no form change */')
old='''        if (x->recoil > 0)
            pr_damage_and_update_segments(bt, c->e, (pr_pokemon *)p,
                to_dmg_value(pr_get_stat(p, STAT_HP, true) * x->recoil), HIT_RESULT_INDIRECT, true, true, NULL);
        pr_form_change(bt, p, PR_FCT_SpeciesFormChangeAbilityTrigger);'''
fault('wrong-form-recoil-order',old,'''        pr_form_change(bt, p, PR_FCT_SpeciesFormChangeAbilityTrigger);
        if (x->recoil > 0)
            pr_damage_and_update_segments(bt, c->e, (pr_pokemon *)p,
                to_dmg_value(pr_get_stat(p, STAT_HP, true) * x->recoil), HIT_RESULT_INDIRECT, true, true, NULL);''')
faults.append(('missing-owning-block','ab_attrs.c',s[:a]+leaf.replace('GATE(*pp->value > 0','GATE(false && *pp->value > 0',1)+s[b:],True))
p=(r/'battle-core/src/phases.c').read_text()
for name,old,new in [('missing-postdamage','if (!source || source->hit_count <= 1) pr_post_damage_ab(bt, p, damage, source);','/* no postdamage */'),('wrong-form-target','if(p->form<2)*target=1-p->form;','if(p->form<2)*target=0;'),('deferred-form','else pr_phase_unshift(bt, e, ph);','else pr_phase_push(bt, e, ph);')]:
 assert p.count(old)==1,(name,p.count(old));faults.append((name,'phases.c',p.replace(old,new,1),False))
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='form-block-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,file,changed,own in faults:
  src=d/'fault.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault';subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test=r/('battle-core/build/wide-combat-replay.c' if own else 'battle-core/tests/form-block-damage-test.c');subprocess.run(['gcc',*flags,test,obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns if own else trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-400:],result.stderr[-400:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['FORM_BLOCK_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['FORM_BLOCK 3 8704 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('user','C ',2,'4'),('target','C ',3,'4'),('move','C ',4,'99999'),('simulation','C ',5,'2'),('damage','C ',6,'NaN'),('ignore','IGNORE ',1,'2'),('source','IGNORE ',2,'4'),('id','I ',5,'4294967296'),('level','I ',6,'2147483648'),('nan','I ',26,'NaN'),('context','QIN ',2,'2'),('segments','IB ',3,'9'),('result','RESULT ',1,'NaN'),('queue','QCOUNT ',1,'33'),('dynamic','DCOUNT ',1,'17'),('stat','CHANGE ',1,'8')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  f=d/'bad.txt';f.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,f],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')
