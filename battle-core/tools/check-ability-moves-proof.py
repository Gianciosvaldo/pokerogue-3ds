#!/usr/bin/env python3
"""Real callback, helper and owning dispatcher faults against fixed native captures."""
from pathlib import Path
import os,subprocess,tempfile,sys
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/ability-moves-test';subprocess.run([runner,trace],check=True,env=env);subprocess.run([r/'battle-core/build/wide-combat-test',turns],check=True,env=env)
s=(r/'battle-core/src/move_effects.c').read_text();start=s.index('    case PR_AC_AbilityChangeAttr:');end=s.index('    case PR_AC_SuppressAbilitiesAttr:',start);leaf=s[start:end];faults=[]
def fault(name,old,new,count=1):
 assert leaf.count(old)>=count,(name,leaf.count(old));changed=leaf.replace(old,new,count);assert changed!=leaf;faults.append((name,'move_effects.c',s[:start]+changed+s[end:],False))
starts=['AbilityChangeAttr','AbilityCopyAttr','AbilityGiveAttr','SwitchAbilitiesAttr'];gate='if (!effect_can_apply(ef, user, target)) return;'
for j,cls in enumerate(starts):
 a=leaf.index('    case PR_AC_'+cls+':');b=leaf.index('    case PR_AC_'+starts[j+1]+':') if j<3 else len(leaf);part=leaf[a:b];assert part.count(gate)==1
 faults.append((cls+'-missing-gate','move_effects.c',s[:start]+leaf[:a]+part.replace(gate,'/* missing HP gate */')+leaf[b:]+s[end:],False))
fault('extra-user-gate',gate,gate+'\n        if (user->hp<=0) return;',4)
fault('raw-condition-gate',gate,gate+'\n        if (!pr_move_apply_conditions(bt,move,user,target,4)) return;',4)
fault('change-wrong-target','x->selfTarget ? user : target','user')
fault('change-wrong-ability','pr_set_temp_ability(bt, on, x->ability);','pr_set_temp_ability(bt, on, ABILITY_ID_BALL_FETCH);')
fault('missing-first-weather-revert','pr_form_change(bt, on, PR_FCT_SpeciesFormChangeRevertWeatherFormTrigger);','/* missing first weather revert */')
last=leaf.rfind('pr_form_change(bt, on, PR_FCT_SpeciesFormChangeRevertWeatherFormTrigger);');assert last>=0
faults.append(('missing-second-weather-revert','move_effects.c',s[:start]+leaf[:last]+leaf[last:].replace('pr_form_change(bt, on, PR_FCT_SpeciesFormChangeRevertWeatherFormTrigger);','/* missing second weather revert */',1)+s[end:],False))
fault('copy-no-partner','if (P(AbilityCopyAttr, a)->copyToPartner &&','if (false && P(AbilityCopyAttr, a)->copyToPartner &&')
fault('copy-fainted-partner',' && ally->hp)',')')
fault('copy-wrong-source','pr_set_temp_ability(bt, user, target->ability);','pr_set_temp_ability(bt, user, user->ability);')
fault('give-wrong-source','pr_set_temp_ability(bt, target, user->ability);','pr_set_temp_ability(bt, target, target->ability);')
fault('swap-no-snapshot','uint16_t old_ability = user->ability;','uint16_t old_ability = target->ability;')
fault('swap-target-unchanged','pr_set_temp_ability(bt, target, old_ability);','/* omit second setter */')
fault('swap-no-form-revert','pr_weather_form_changes(bt, true, NULL);','/* missing global weather revert */')
fault('wrong-copy-message','pr_arg_pokemon(bt, "targetName", target)','pr_arg_pokemon(bt, "targetName", user)',2)
fault('missing-swap-message','pr_queue_msg(bt, e, pr_msg1("moveTriggers:swappedAbilitiesWithTarget", pr_arg_pokemon(bt, "pokemonName", user)));','/* missing message */')
fault('wrong-affix','pr_arg_pokemon(bt, "pokemonName", on)','pr_arg_pokemon_name(bt, "pokemonName", on)',2)
fault('spurious-rng',gate,'pr_battle_rand_seed_int(bt,100,0);\n        '+gate)
ab=(r/'battle-core/src/ab_attrs.c').read_text()
for name,old,new,own in [('missing-on-lose','pr_apply_ab_attrs(bt, PR_AC_PreLeaveFieldAbAttr, &lose);','(void)0;',False),('missing-on-gain','pr_apply_ab_attrs(bt, PR_AC_PostSummonAbAttr, &gain);','(void)0;',True)]:
 assert ab.count(old)==1;faults.append((name,'ab_attrs.c',ab.replace(old,new,1),own))
p=(r/'battle-core/src/phases.c').read_text();anchor='if (!pr_attr_is(pr_attrs[ai].cls, PR_AC_MoveEffectAttr)) continue;';assert p.count(anchor)==1
faults.append(('missing-owning-callbacks','phases.c',p.replace(anchor,anchor+'\n        if (pr_attrs[ai].cls==PR_AC_AbilityChangeAttr||pr_attrs[ai].cls==PR_AC_AbilityCopyAttr||pr_attrs[ai].cls==PR_AC_AbilityGiveAttr||pr_attrs[ai].cls==PR_AC_SwitchAbilitiesAttr) continue;',1),True))
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='ability-moves-guards-',dir=r/'work') as d:
 d=Path(d)
 for name,file,changed,owning in faults:
  assert changed!=(r/'battle-core/src'/file).read_text(),name;src=d/'fault.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test=r/('battle-core/build/wide-combat-replay.c' if owning else 'battle-core/tests/ability-moves-test.c')
  subprocess.run(['gcc',*flags,test,obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns if owning else trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-400:],result.stderr[-400:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['ABILITY_MOVES_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['ABILITY_MOVES 5 12544 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('mode','C ',2,'6'),('user','C ',3,'4'),('target','C ',4,'4'),('id','I ',5,'4294967296'),('level','I ',6,'2147483648'),('nan','I ',26,'NaN'),('world','WI ',1,'10'),('condition','COND ',1,'2'),('return','RET ',1,'2'),('queue','QCOUNT ',1,'33'),('qlevel','Q ',1,'9999'),('qorder','Q ',2,'9999'),('qkind','Q ',3,'X'),('hex','Q ',4,'xy'),('dynamic','DCOUNT ',1,'17')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,kind,col,value in [('form-recipient','F',5,'4'),('form-target','F',6,'4'),('cached-ability','A',6,'9999'),('animation','C',5,'65536')]:
  ix=next(i for i,row in enumerate(rows) if (row.startswith('Q ') and row.split()[3]==kind) or (kind=='C' and row.startswith('D ')));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  f=d/'bad.txt';f.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,f],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')
