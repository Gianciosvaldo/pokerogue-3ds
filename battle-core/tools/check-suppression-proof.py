#!/usr/bin/env python3
"""Compile suppression/callback faults and reject malformed actual captures."""
from pathlib import Path
import argparse
import os
import re
import subprocess

p=argparse.ArgumentParser();p.add_argument('trace',type=Path);p.add_argument('turns',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];out=root/'battle-core/build/suppression-guards';out.mkdir(parents=True,exist_ok=True)
assert os.environ.get('TURNTEST_LOCALES')
effect=(root/'battle-core/src/move_effects.c').read_text();ab=(root/'battle-core/src/ab_attrs.c').read_text()
start=effect.index('    case PR_AC_SuppressAbilitiesAttr:');end=effect.index('    case PR_AC_AfterYouAttr:',start);leaf=effect[start:end]
primary='        lose.only = 1;\n        pr_apply_ab_attrs(bt, PR_AC_PreLeaveFieldAbAttr, &lose);'
passive='        pr_apply_ab_attrs(bt, PR_AC_PreLeaveFieldAbAttr, &lose);\n        lose.only = 1;'
mutations={
 'missing-base':('move_effects',effect[:start]+leaf.replace('    case PR_AC_SuppressAbilitiesAttr:\n','')+effect[end:]),
 'missing-core':('move_effects',effect[:start]+leaf.replace('    case PR_AC_SuppressAbilitiesIfActedAttr: {','    {')+effect[end:]),
 'missing-hp-gate':('move_effects',effect[:start]+leaf.replace('        if (!effect_can_apply(ef, user, target)) return;\n','')+effect[end:]),
 'base-condition-gate':('move_effects',effect[:start]+leaf.replace('        if (!effect_can_apply(ef, user, target)) return;','        if (!effect_can_apply(ef, user, target) || !pr_can_suppress_ability(target)) return;')+effect[end:]),
 'missing-acted':('move_effects',effect[:start]+leaf.replace('!target->acted || ', '')+effect[end:]),
 'missing-condition':('move_effects',effect[:start]+leaf.replace(' || !pr_can_suppress_ability(target)', '')+effect[end:]),
 'missing-message':('move_effects',effect[:start]+leaf.replace('        pr_queue_msg(bt, e, pr_msg1("moveTriggers:suppressAbilities", pr_arg_pokemon(bt, "pokemonName", target)));\n','')+effect[end:]),
 'wrong-affix':('move_effects',effect[:start]+leaf.replace('pr_arg_pokemon(', 'pr_arg_pokemon_name(')+effect[end:]),
 'missing-passive-loss':('move_effects',effect[:start]+leaf.replace(passive,'        lose.only = 1;')+effect[end:]),
 'missing-primary-loss':('move_effects',effect[:start]+leaf.replace(primary,'')+effect[end:]),
 'reversed-loss-order':('move_effects',effect[:start]+leaf.replace('.only=2','.only=1').replace('lose.only = 1','lose.only = 2')+effect[end:]),
 'early-suppression':('move_effects',effect[:start]+leaf.replace('        pr_ab_params lose', '        target->ability_suppressed = true;\n        pr_ab_params lose')+effect[end:]),
 'missing-suppression':('move_effects',effect[:start]+leaf.replace('target->ability_suppressed = true;', 'target->ability_suppressed = false;')+effect[end:]),
 'missing-weather-revert':('move_effects',effect[:start]+leaf.replace('        pr_weather_form_changes(bt, true, NULL);\n','')+effect[end:]),
 'ignore-NONE-passive':('ab_attrs',ab.replace('(p->passive_flag || p->passive)', '(p->passive)')),
}
# Restore the observed bug: scheduling ability displays before the complete canApply check.
start_ab=ab.index('    case PR_AC_PostSummonFormChangeByWeatherAbAttr:');end_ab=ab.index('    case PR_AC_PostSummonFormChangeAbAttr:',start_ab)
weather=ab[start_ab:end_ab]
old='''        GATE(wanted >= 0 && !p->ability_suppressed && !pr_weather_effect_suppressed(bt)
             && pr_has_ability_active(bt, p, wanted) && form >= 0 && form != p->form);'''
assert weather.count(old)==1
weather=weather.replace('        int form =', '''        GATE(wanted >= 0 && !p->ability_suppressed && !pr_weather_effect_suppressed(bt)
             && pr_has_ability_active(bt, p, wanted));
        int form =''').replace(old,'        GATE(form >= 0 && form != p->form);')
mutations['premature-weather-display']=('ab_attrs',ab[:start_ab]+weather+ab[end_ab:])
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(root/'battle-core/include'),'-I'+str(root/'battle-core/gen/out')]
for name,(file,code) in mutations.items():
 assert code!={'move_effects':effect,'ab_attrs':ab}[file]
 src,obj,exe=(out/(name+suffix) for suffix in ['.c','.o','']);src.write_text(code)
 subprocess.run(['gcc',*flags,'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run(['gcc',*flags,str(root/'battle-core/tests/suppression-test.c'),str(obj),str(root/'battle-core/build/libprcore.a'),'-lm','-o',str(exe)],check=True)
 r=subprocess.run([str(exe),str(a.trace.resolve())],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
 if name=='missing-suppression':
  exe=out/'missing-suppression-turns';subprocess.run(['gcc',*flags,str(root/'battle-core/tests/doublestest.c'),str(obj),str(root/'battle-core/build/libprcore.a'),'-lm','-o',str(exe)],check=True)
  r=subprocess.run([str(exe),str(a.turns.resolve())],capture_output=True,text=True);(out/'missing-suppression-turns.log').write_text(r.stdout+r.stderr);m=re.search(r'^covered \d+: pass \d+, fail (\d+)',r.stdout,re.M);assert r.returncode==1 and m and int(m[1])>0;print('owning-turn failures',m[1],flush=True)

rows=a.trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':['SUPPRESSION 2 16384 777']+rows[1:],'wrong-footer':rows[:-1]+['SUPPRESSION_COMPLETE 1']}
for name,prefix,column,value in [
 ('fractional-case','C ',1,'0.5'),('invalid-mode','C ',2,'3'),('invalid-holder','C ',3,'4'),('invalid-target','C ',4,'-1'),('invalid-acted','C ',5,'2'),
 ('invalid-source-count','G ',1,'4'),('overflow-source','G ',2,'4294967296'),('invalid-weather','W ',1,'255'),('invalid-condition','CAN ',1,'2'),
 ('invalid-ability-active','AC ',1,'2'),('invalid-actor-slot','I ',1,'4'),('invalid-level','I ',6,'256'),('invalid-hp','I ',8,'-1'),
 ('nonfinite-hp','I ',8,'nan'),('invalid-result','RESULT ',1,'2'),('invalid-queue-count','QCOUNT ',1,'129'),('invalid-level-count','QCOUNT ',2,'0'),('invalid-deferred','QCOUNT ',3,'2')]:
 index=next(i for i,row in enumerate(rows) if row.startswith(prefix));parts=rows[index].split();parts[column]=value;line=rows.copy();line[index]=' '.join(parts);bad[name]=line
for name,line in bad.items():
 path=out/(name+'.txt');path.write_text('\n'.join(line)+'\n');r=subprocess.run([str(root/'battle-core/build/suppression-test'),str(path)],capture_output=True,text=True);assert r.returncode==2,(name,r.returncode);print(name,'rejected',flush=True)
print(f'{len(mutations)} compiled mutants caught; {len(bad)} malformed captures rejected')
