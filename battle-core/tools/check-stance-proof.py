#!/usr/bin/env python3
"""Compiled source-trigger faults and malformed whole-fixture checks."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('api',type=Path);p.add_argument('turn',type=Path);a=p.parse_args();root=Path(__file__).resolve().parents[1];out=root/'build/stance-guards';out.mkdir(parents=True,exist_ok=True)
if not os.environ.get('TURNTEST_LOCALES'):raise SystemExit('TURNTEST_LOCALES required')
faults={
 'premoves-use-current-phase':('int move = bt->commands[b].move;','int move = bt->engine->pool[bt->engine->current].move.id;','stance-change-test',a.api),
 'leave-requires-ability':('mon->form == 1 && !is_active_on_field(mon)','mon->form == 1 && !is_active_on_field(mon) && pr_has_ability_active(bt,mon,ABILITY_ID_STANCE_CHANGE)','stance-change-test',a.api),
 'leave-ignores-field':('mon->form == 1 && !is_active_on_field(mon)','mon->form == 1 && !is_active(mon)','stance-change-test',a.api),
 'no-stance-change-queue':('if (target_form >= 0 && target_form != mon->form) {','if (!stance && target_form >= 0 && target_form != mon->form) {','stance-change-test',a.api),
 'missing-delay-selection':('if (delayed || (!stance && trigger_class','if (false || (!stance && trigger_class','stance-change-test',a.api),
 'missing-kings-shield':('move == MOVE_ID_KINGS_SHIELD) target_form = 0;','move == MOVE_ID_SPLASH) target_form = 0;','stance-change-test',a.api),
 'faint-revert-is-immediate':('pr_queue_msg(bt, e, pr_msg_p(bt, "battle:fainted", p));\n    form_change_with_delay(bt, p, PR_FCT_SpeciesFormChangeActiveTrigger, true);','pr_queue_msg(bt, e, pr_msg_p(bt, "battle:fainted", p));\n    form_change_with_delay(bt, p, PR_FCT_SpeciesFormChangeActiveTrigger, false);','stance-turn-test',a.turn),
}
s=(root/'src/phases.c').read_text();inc=['-I'+str(root/'include'),'-I'+str(root/'gen/out'),'-I'+str(root/'build'),'-I'+str(root/'tests')]
for name,(old,new,test,fixture) in faults.items():
 assert s.count(old)==1,(name,s.count(old));src=out/(name+'.c');obj=out/(name+'.o');exe=out/name;src.write_text(s.replace(old,new));subprocess.run(['gcc','-O2','-std=c99','-ffp-contract=off',*inc,'-c',str(src),'-o',str(obj)],check=True);subprocess.run(['gcc','-O2','-std=c99',*inc,str(root/'tests'/(test+'.c')),str(obj),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True);r=subprocess.run([str(exe),str(fixture.resolve())],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
refusals=0
for test,fixture in [('stance-change-test',a.api),('stance-turn-test',a.turn)]:
 rows=fixture.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':[rows[0].replace(' 1 ',' 99 ',1).replace(' 2 ',' 99 ',1)]+rows[1:]}
 if test=='stance-change-test':
  ci=next(i for i,x in enumerate(rows)if x.startswith('C '));ri=next(i for i,x in enumerate(rows)if x.startswith('I '))
  for name,index,col,value in [('fractional-case',ci,1,'0.5'),('bad-kind',ci,3,'2'),('bad-delay',ci,4,'2'),('bad-move',ci,5,'65535'),('bad-passive-flag',ci,8,'2'),('bad-species',ri,1,'1'),('fractional-hp',ri,7,'0.5')]:
   r=rows.copy();v=r[index].split();v[col]=value;r[index]=' '.join(v);bad[name]=r
 for name,lines in bad.items():
  f=out/(test+'-'+name+'.txt');f.write_text('\n'.join(lines)+'\n');r=subprocess.run([str(root/'build'/test),str(f)],capture_output=True,text=True);assert r.returncode==2,(test,name,r.returncode);refusals+=1
print(f'{len(faults)} compiled faults caught; {refusals} malformed fixtures rejected',flush=True)
