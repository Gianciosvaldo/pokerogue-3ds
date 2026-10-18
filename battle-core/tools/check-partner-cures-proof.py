#!/usr/bin/env python3
"""Compile partner healing faults and reject malformed native observations."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];core=root/'battle-core';out=core/'build/partner-cures-guards';out.mkdir(parents=True,exist_ok=True)
assert os.environ.get('TURNTEST_LOCALES')
code={name:(core/'src'/f'{name}.c').read_text() for name in ['move_effects','ab_attrs']}
faults={}
def change(name,file,old,new):
 s=code[file];assert s.count(old)==1,(name,s.count(old));faults[name]=(file,s.replace(old,new))
change('skip-doubles','move_effects','if (bt->n_party[side] < 1)', 'if (bt->double_battle || bt->n_party[side] < 1)')
change('pointer-user-exemption','move_effects','p->id == user->id','p == user')
change('revive-user-reserves','move_effects','pr_reset_status_phase(bt, e, p, false);','pr_reset_status_phase(bt, e, p, true);')
change('skip-active-revive','move_effects','else pr_reset_status_phase(bt, e, p, true);','else pr_reset_status_phase(bt, e, p, false);')
change('ignore-cure-immunity','move_effects','pr_has_ability_active(bt, p, x->abilityCondition)','false')
change('ignore-disabled-passive-display','move_effects','pr_get_passive_ability(p) == x->abilityCondition','p->passive_flag && pr_get_passive_ability(p) == x->abilityCondition')
change('skip-hide','move_effects','                pr_queue_ability_display(bt, e, p, passive, false);','')
change('wrong-display-order','move_effects','pr_queue_ability_display(bt, e, p, passive, true);\n                pr_queue_ability_display(bt, e, p, passive, false);','pr_queue_ability_display(bt, e, p, passive, false);\n                pr_queue_ability_display(bt, e, p, passive, true);')
change('cure-parent-hp-gate','move_effects','        /* Custom canApply deliberately omits target HP. */','        if (!effect_can_apply(ef, user, target)) return;')
change('skip-pollen','move_effects','    case PR_AC_HealOnAllyAttr:\n','')
change('pollen-nonally','move_effects','pr_is_opponent(bt, user, target) || target != pr_get_ally(bt, user)','pr_is_opponent(bt, user, target)')
change('pollen-user-hp','move_effects','if (pr_is_opponent(bt, user, target) || target != pr_get_ally(bt, user))','if (user->hp <= 0 || pr_is_opponent(bt, user, target) || target != pr_get_ally(bt, user))')
change('pollen-full-hp','move_effects','ratio = x->healRatio; self_target = x->selfTarget; show_anim = x->showAnim;\n        } else if (variable)', 'if (target->hp >= pr_get_stat(target, STAT_HP, false)) return;\n            ratio = x->healRatio; self_target = x->selfTarget; show_anim = x->showAnim;\n        } else if (variable)')
change('floor-healing','move_effects','js_round(pr_get_stat(on, STAT_HP, false) * ratio)','floor(pr_get_stat(on, STAT_HP, false) * ratio)')
change('missing-heal-boost','ab_attrs','    case PR_AC_MoveHealBoostAbAttr: {','    case PR_AC_MoveHealBoostAbAttr: return 0; {')
change('wrong-heal-multiplier','ab_attrs','*pp->value *= x->healMulti;','*pp->value *= 2;')
change('ignore-pulse-condition','ab_attrs','GATE(call_bool(bt, x->boostCondition, a->cls, p, o, move));','GATE(true);')
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(core/'include'),'-I'+str(core/'gen/out')]
for name,(file,source) in faults.items():
 src,obj,exe=(out/(name+suffix) for suffix in ['.c','.o','']);src.write_text(source)
 subprocess.run(['gcc',*flags,'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run(['gcc',*flags,str(core/'tests/partner-cures-test.c'),str(obj),str(core/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
 r=subprocess.run([str(exe),str(a.trace.resolve())],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
rows=a.trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':['PARTNERCURES 2 12800 777']+rows[1:],'wrong-footer':rows[:-1]+['PARTNERCURES_COMPLETE 1']}
for name,prefix,col,value in [('fractional-case','C ',1,'0.5'),('invalid-mode','C ',2,'4'),('invalid-holder','C ',3,'12'),('invalid-target','C ',4,'-1'),('invalid-double','C ',5,'2'),('invalid-party','C ',6,'7'),('invalid-swap','C ',8,'2'),('invalid-gas','G ',1,'3'),('overflow-owner','G ',2,'4294967296'),('invalid-slot','I ',1,'12'),('invalid-hp','I ',8,'-1'),('nonfinite-hp','I ',8,'nan'),('invalid-tags','IX ',2,'16'),('invalid-counter','IX ',3,'128'),('invalid-result','RESULT ',1,'2'),('invalid-condition','COND ',1,'2'),('invalid-queue-count','QCOUNT ',1,'129'),('invalid-level-count','QCOUNT ',2,'0'),('invalid-deferred','QCOUNT ',3,'2'),('queue-overflow','Q ',2,'4294967296'),('queue-fraction','Q ',5,'0.5'),('queue-invalid-person','Q ',6,'12'),('queue-nonfinite','Q ',7,'nan'),('invalid-world','WORLD ',1,'2')]:
 j=next(i for i,row in enumerate(rows) if row.startswith(prefix));parts=rows[j].split();parts[col]=value;lines=rows.copy();lines[j]=' '.join(parts);bad[name]=lines
for name,lines in bad.items():
 path=out/(name+'.txt');path.write_text('\n'.join(lines)+'\n');r=subprocess.run([str(core/'build/partner-cures-test'),str(path)],capture_output=True,text=True);assert r.returncode==2,(name,r.returncode);print(name,'rejected',flush=True)
print(f'{len(faults)} compiled faults caught; {len(bad)} malformed captures rejected')
