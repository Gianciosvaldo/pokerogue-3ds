#!/usr/bin/env python3
"""Compile real behavioral faults; reject incomplete or malformed captures."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('api',type=Path);p.add_argument('turn',type=Path);a=p.parse_args();root=Path(__file__).resolve().parents[1];out=root/'build/weather-guards';out.mkdir(parents=True,exist_ok=True)
if not os.environ.get('TURNTEST_LOCALES'):raise SystemExit('TURNTEST_LOCALES required')
s=(root/'src/phases.c').read_text();inc=['-I'+str(root/'include'),'-I'+str(root/'gen/out'),'-I'+str(root/'build'),'-I'+str(root/'tests')]
faults={
 'unset-ability-does-not-revert':('mon->ability_override != 0 && mon->ability_override != ability','mon->ability_override >= 0 && mon->ability_override != ability'),
 'none-ability-also-reverts':('mon->ability_override != 0 && mon->ability_override != ability','mon->ability_override != ability'),
 'inactive-implicitly-delayed':('if (delayed) pr_phase_push(bt, e, ph);','if (delayed || (!stance && trigger_class == PR_FCT_SpeciesFormChangeActiveTrigger && !is_active_on_field(mon))) pr_phase_push(bt,e,ph);'),
 'all-transitions-delayed':('if (delayed) pr_phase_push(bt, e, ph);','if (true) pr_phase_push(bt,e,ph);'),
 'ignore-explicit-delay':('if (delayed) pr_phase_push(bt, e, ph);','if (false) pr_phase_push(bt,e,ph);'),
 'inactive-revert-needs-ability':('trigger_class == PR_FCT_SpeciesFormChangeActiveTrigger && !is_active_on_field(mon)) target_form = 0;','trigger_class == PR_FCT_SpeciesFormChangeActiveTrigger && !is_active_on_field(mon) && pr_has_ability_active(bt,mon,ability)) target_form = 0;'),
 'weather-ignores-suppression':('&& !mon->ability_suppressed && !suppressed && pr_has_ability_active(bt, mon, ability)','&& pr_has_ability_active(bt, mon, ability)'),
}
for name,(old,new) in faults.items():
 assert s.count(old)==1,(name,s.count(old));src=out/(name+'.c');obj=out/(name+'.o');exe=out/name;src.write_text(s.replace(old,new));subprocess.run(['gcc','-O2','-std=c99','-ffp-contract=off',*inc,'-c',str(src),'-o',str(obj)],check=True);subprocess.run(['gcc','-O2','-std=c99',*inc,str(root/'tests/weather-form-test.c'),str(obj),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True);r=subprocess.run([str(exe),str(a.api.resolve())],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
# Shared getter regression: source reserves have an ally even in singles.
ab=(root/'src/ab_attrs.c').read_text();old='    if (p != first) return first;';assert ab.count(old)==1
src=out/'old-ally-lookup.c';obj=out/'old-ally-lookup.o';exe=out/'old-ally-lookup';src.write_text(ab.replace(old,'    if (!bt->double_battle || p != first) return NULL;'))
subprocess.run(['gcc','-O2','-std=c99','-ffp-contract=off',*inc,'-c',str(src),'-o',str(obj)],check=True)
subprocess.run(['gcc','-O2','-std=c99',*inc,str(root/'tests/ally-lookup-test.c'),str(obj),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
r=subprocess.run([str(exe),str(a.api.parent/'ally.txt')],capture_output=True,text=True);(out/'old-ally-lookup.log').write_text(r.stdout+r.stderr);assert r.returncode==1;print('old-ally-lookup caught',flush=True)
refusals=0
for test,fixture in [('weather-form-test',a.api),('weather-turn-test',a.turn),('ally-lookup-test',a.api.parent/'ally.txt')]:
 rows=fixture.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':[rows[0].replace(' 1 ',' 99 ',1)]+rows[1:]}
 if test=='weather-form-test':
  ci=next(i for i,x in enumerate(rows)if x.startswith('C '));ri=next(i for i,x in enumerate(rows)if x.startswith('I '));wi=next(i for i,x in enumerate(rows)if x.startswith('W '))
  for name,index,col,value in [('fractional-case',ci,1,'0.5'),('bad-kind',ci,3,'3'),('bad-delay',ci,4,'2'),('bad-move',ci,5,'65535'),('bad-passive-flag',ci,8,'2'),('bad-species',ri,1,'1'),('fractional-hp',ri,7,'0.5'),('bad-weather',wi,2,'10')]:
   r=rows.copy();v=r[index].split();v[col]=value;r[index]=' '.join(v);bad[name]=r
 for name,lines in bad.items():
  f=out/(test+'-'+name+'.txt');f.write_text('\n'.join(lines)+'\n');r=subprocess.run([str(root/'build'/test),str(f)],capture_output=True,text=True);assert r.returncode==2,(test,name,r.returncode);refusals+=1
print(f'{len(faults)+1} compiled faults caught; {refusals} malformed fixtures rejected',flush=True)
