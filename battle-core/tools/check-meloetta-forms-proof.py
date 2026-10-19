#!/usr/bin/env python3
"""Compiled native-predicate, queue-order, execution and owning-caller faults."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
with turns.open() as f:
 assert sum(line.startswith('TRACE list=') and 'QuietFormChangePhase' in line for line in f)>=100, 'native owning form execution floor'
runner=r/'battle-core/build/meloetta-forms-test';subprocess.run([runner,trace],check=True,env=env);subprocess.run([r/'battle-core/build/wide-combat-test',turns],check=True,env=env)
s=(r/'battle-core/src/phases.c').read_text();a=s.index('static void form_change_with_delay(');b=s.index('\nstatic void form_change(',a);leaf=s[a:b];faults=[]
def fault(name,old,new):
 assert leaf.count(old)==1,(name,leaf.count(old));faults.append((name,s[:a]+leaf.replace(old,new,1)+s[b:],False))
fault('missing-history-predicate','last->move != MOVE_ID_RELIC_SONG','false')
fault('wrong-history-move','last->move != MOVE_ID_RELIC_SONG','last->move != MOVE_ID_TACKLE')
fault('missing-history','if (!last || last->move != MOVE_ID_RELIC_SONG','if (false || (last && last->move != MOVE_ID_RELIC_SONG)')
fault('missing-mono-type-gate','(bt->challenge_flags & (1u << CHALLENGES_SINGLE_TYPE))','false')
fault('wrong-challenge-bit','(1u << CHALLENGES_SINGLE_TYPE)','(1u << CHALLENGES_SINGLE_GENERATION)')
fault('missing-sheer-force-gate','pr_has_ability_active(bt, p, ABILITY_ID_SHEER_FORCE)','false')
fault('primary-only-sheer-force','pr_has_ability_active(bt, p, ABILITY_ID_SHEER_FORCE)','(p->ability==ABILITY_ID_SHEER_FORCE && pr_can_apply_ability(bt,p,p->ability))')
fault('inactive-sheer-force','pr_has_ability_active(bt, p, ABILITY_ID_SHEER_FORCE)','(p->ability==ABILITY_ID_SHEER_FORCE || p->passive==ABILITY_ID_SHEER_FORCE)')
fault('extra-faint-gate','if (!last || last->move != MOVE_ID_RELIC_SONG','if (p->hp<=0 || !last || last->move != MOVE_ID_RELIC_SONG')
fault('wrong-aria-target','!strcmp(from->form_key,"aria") ? "pirouette"','!strcmp(from->form_key,"aria") ? "aria"')
fault('wrong-pirouette-target','!strcmp(from->form_key,"pirouette") ? "aria"','!strcmp(from->form_key,"pirouette") ? "pirouette"')
fault('missing-positive-handler','if (mon->species == SPECIES_ID_MELOETTA && trigger_class == PR_FCT_SpeciesFormChangePostMoveTrigger) {','if (false) {')
fault('wrong-delay-order','if (delayed) pr_phase_push(bt, e, ph);','if (!delayed) pr_phase_push(bt, e, ph);')
fault('eager-form-change','e->pool[ph].pokemon = mon;','mon->form=target_form; e->pool[ph].pokemon = mon;')
for name,old,new,own in [('missing-owning-caller','form_change(bt, user, PR_FCT_SpeciesFormChangePostMoveTrigger);','/* omit post-move scene trigger */',True),('missing-quiet-execution','case PH_QUIET_FORM_CHANGE: quiet_form_change_phase(bt, e, ph); break;','case PH_QUIET_FORM_CHANGE: break;',True)]:
 assert s.count(old)==1,(name,s.count(old));faults.append((name,s.replace(old,new,1),own))
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='meloetta-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,changed,own in faults:
  src=d/'fault.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault';subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test=r/('battle-core/build/wide-combat-replay.c' if own else 'battle-core/tests/meloetta-forms-test.c');subprocess.run(['gcc',*flags,test,obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns if own else trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-500:],result.stderr[-500:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['MELOETTA_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['MELOETTA 2 8192 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('battler','C ',2,'4'),('trigger','C ',3,'3'),('delayed','C ',4,'2'),('ability','C ',6,'99999'),('id','I ',4,'4294967296'),('level','I ',5,'2147483648'),('nan','I ',25,'NaN'),('history','H ',1,'2'),('move','H ',2,'99999'),('mono','H ',5,'2'),('history-result','H ',3,'5'),('history-mode','H ',4,'7'),('queue','Q ',1,'33'),('form','P ',4,'4'),('applied','F ',1,'17'),('text','T ',1,'INVALID')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  f=d/'bad.txt';f.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,f],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')
