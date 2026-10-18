#!/usr/bin/env python3
"""Plant actual waiting, queue, override and combined-effect faults."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/pledge-test';subprocess.run([runner,trace],check=True,env=env);subprocess.run([r/'battle-core/build/wide-combat-test',turns],check=True,env=env)
p=(r/'battle-core/src/phases.c').read_text();a=p.index('static bool await_combined_pledge(');b=p.index('static void move_effect_phase(',a);leaf=p[a:b];faults=[]
def fault(name,old,new):
 assert leaf.count(old)==1,(name,leaf.count(old));faults.append((name,'phases.c',p[:a]+leaf.replace(old,new,1)+p[b:],False))
fault('omit-user-flag','user->combining_pledge = (uint16_t)move;','/* omit user */')
fault('omit-partner-flag','next->pokemon->combining_pledge = (uint16_t)move;','/* omit partner */')
fault('wrong-combined-move','next->pokemon->combining_pledge = (uint16_t)move;','next->pokemon->combining_pledge = next->move.id;')
fault('extra-hp-gate','    if (user->combining_pledge)', '    if (user->hp<=0) return false;\n    if (user->combining_pledge)')
fault('ignore-existing-combination','if (user->combining_pledge)', 'if (false && user->combining_pledge)')
fault('wrong-side','user->is_player ? 1 : 0','user->is_player ? 0 : 1')
fault('combine-same-move','next->move.id == move || ','')
fault('combine-non-pledge',' || !pr_move_has_attr(next->move.id, PR_AC_AwaitCombinedPledgeAttr)','')
fault('missing-combined-message','pr_queue_msg(bt, e, pr_msg0("moveTriggers:combiningPledge"));','/* omit message */')
fault('wrong-waiting-name','pr_arg_pokemon(bt, "allyPokemonName", next->pokemon)','pr_arg_pokemon(bt, "allyPokemonName", user)')
fault('force-selected-owner','pr_force_move_next(e, pr_get_ally(bt, user));','pr_force_move_next(e, next->pokemon);')
fault('force-last','pr_force_move_next(e, pr_get_ally(bt, user));','pr_force_move_last(e, pr_get_ally(bt, user));')
fault('spurious-rng','    if (user->combining_pledge)', '    pr_battle_rand_seed_int(bt,100,0);\n    if (user->combining_pledge)')
for name,old,new in [('ignore-overridden','if (await_combined_pledge(bt, e, user, move)) overridden = true;','await_combined_pledge(bt,e,user,move);'),('missing-owning-callback','if (await_combined_pledge(bt, e, user, move)) overridden = true;','/* omit callback */')]:
 assert p.count(old)==1;faults.append((name,'phases.c',p.replace(old,new,1),True))
m=(r/'battle-core/src/move_attrs.c').read_text()
for name,old,new in [('combined-power','*power *= 150.0 / 80','*power *= 80.0 / 150'),('combined-type','user->combining_pledge == MOVE_ID_WATER_PLEDGE) *type = POKEMON_TYPE_WATER','user->combining_pledge == MOVE_ID_WATER_PLEDGE) *type = POKEMON_TYPE_FIRE'),('combined-stab','*stab = 1.5;','*stab = 1.0;')]:
 assert m.count(old)==1,(name,m.count(old));faults.append((name,'move_attrs.c',m.replace(old,new,1),True))
m=(r/'battle-core/src/move_effects.c').read_text();old='if (user->combining_pledge != P(AddPledgeEffectAttr, a)->requiredPledge) return;';assert m.count(old)==1;faults.append(('missing-field-effects','move_effects.c',m.replace(old,'return; /* no combined field */',1),True))
e=(r/'battle-core/src/engine.c').read_text();start=e.index('void pr_force_move_next(');end=e.index('/* forceMoveLast',start);part=e[start:end];assert part.count('return;')==1;faults.append(('force-all-duplicate-owners','engine.c',e[:start]+part.replace('return;','continue;')+e[end:],False))
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/tests','battle-core/build']]]
with tempfile.TemporaryDirectory(prefix='pledge-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,file,changed,own in faults:
  src=d/'fault.c';src.write_text(changed);exe=d/'fault';objects=[]
  if own or file!='phases.c':
   obj=d/'fault.o';subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True);objects=[obj]
  if own:test=r/'battle-core/build/wide-combat-replay.c'
  else:
   test=d/'test.c';source=(r/'battle-core/tests/pledge-test.c').read_text().replace('#include "../src/phases.c"','#include "'+str(src if file=='phases.c' else r/'battle-core/src/phases.c')+'"');test.write_text(source)
  subprocess.run(['gcc',*flags,test,*objects,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns if own else trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-400:],result.stderr[-400:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['PLEDGE_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['PLEDGE 3 12544 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('user','C ',2,'4'),('move','C ',3,'0'),('queue-count','C ',4,'9'),('operations','C ',5,'0'),('swap','C ',6,'4'),('species','MON ',1,'0'),('hp','MON ',3,'-1'),('flag','MON ',5,'9999'),('owner','MOVE ',1,'4'),('timing','MOVE ',2,'3'),('return','OP ',2,'2'),('override','OP ',3,'2'),('levels','LEVELS ',1,'9999'),('length','L ',2,'9999'),('dyn-count','DYNAMIC ',1,'9'),('dyn-owner','D ',1,'4'),('after-flag','AFTER ',3,'9999')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  f=d/'bad.txt';f.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,f],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')
