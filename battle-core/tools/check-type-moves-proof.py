#!/usr/bin/env python3
"""Compile real type-change, original-type, history and RNG faults."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/type-moves-test';subprocess.run([runner,trace],check=True,env=env);subprocess.run([r/'battle-core/build/wide-combat-test',turns],check=True,env=env)
s=(r/'battle-core/src/move_effects.c').read_text();starts=['CopyTypeAttr','FirstMoveTypeAttr','ResistLastMoveTypeAttr'];faults=[]
for j,cls in enumerate(starts):
 a=s.index('    case PR_AC_'+cls+':');b=s.index('    case PR_AC_'+(starts[j+1] if j<2 else 'CopyBiomeTypeAttr')+':',a);leaf=s[a:b]
 def fault(name,old,new):
  assert leaf.count(old)==1,(name,leaf.count(old));faults.append((cls+'-'+name,'move_effects.c',s[:a]+leaf.replace(old,new,1)+s[b:],False))
 gate='if (!effect_can_apply(ef, user, target)) return;'
 fault('missing-parent-gate',gate,'/* no HP gate */')
 fault('raw-condition-gate',gate,gate+'\n        if (!pr_move_apply_conditions(bt,move,user,target,4)) return;')
 if j==0:
  fault('extra-user-gate',gate,gate+'\n        if (user->hp<=0) return;')
  fault('wrong-stellar-types','pr_get_types(target, types, PR_TYPES_ORIGINAL_IF_STELLAR)','pr_get_types(target, types, 0)')
  fault('unknown-to-grass','types[i] = POKEMON_TYPE_NORMAL','types[i] = POKEMON_TYPE_GRASS')
  fault('first-type-only','pr_set_summon_types(bt, user, types, n)','pr_set_summon_types(bt, user, types, 1)')
  fault('wrong-recipient','pr_set_summon_types(bt, user, types, n)','pr_set_summon_types(bt, target, types, n)')
  fault('clear-added-type','int8_t types[PR_MAX_TYPES];','user->added_type=0;\n        int8_t types[PR_MAX_TYPES];')
  fault('wrong-copy-name','pr_arg_pokemon(bt, "targetPokemonName", target)','pr_arg_pokemon(bt, "targetPokemonName", user)')
 elif j==1:
  fault('extra-target-gate',gate,gate+'\n        if (target->hp<=0) return;')
  fault('second-slot-type','pr_moves[PR_MOVES(user)[0]].type','pr_moves[PR_MOVES(user)[1]].type')
  fault('caller-type','pr_moves[PR_MOVES(user)[0]].type','pr_moves[move].type')
  fault('missing-setter','pr_set_summon_types(bt, user, &type, 1);','/* omit typing */')
 else:
  fault('extra-target-gate',gate,gate+'\n        if (target->hp<=0) return;')
  fault('user-history','pr_last_move(bt, target, 0)','pr_last_move(bt, user, 0)')
  fault('nonvirtual-history','const pr_turn_move *last = pr_last_move(bt, target, 0);','bool unknown=false;const pr_turn_move *last=pr_last_non_virtual_move(bt,target,false,true,&unknown);')
  fault('effective-move-type','pr_moves[last->move].type','pr_get_move_type(bt,target,last->move)')
  fault('wrong-stellar-exclusions','pr_get_types(user, current, PR_TYPES_ORIGINAL_IF_STELLAR)','pr_get_types(user, current, 0)')
  fault('reverse-enum-order','int type = 0; type < PR_TYPE_COUNT; type++','int type = PR_TYPE_COUNT-1; type >= 0; type--')
  fault('include-current-types','if (!have && pr_type_chart_x2','if (pr_type_chart_x2')
  fault('exclude-immunities','pr_type_chart_x2[attack_type][type] < 2','pr_type_chart_x2[attack_type][type] == 1')
  fault('draw-without-candidates','if (!count) return;','if (!count) {pr_battle_rand_seed_int(bt,1,0);return;}')
  fault('skip-single-choice-rng','choices[pr_battle_rand_seed_int(bt, count, 0)]','choices[count==1?0:pr_battle_rand_seed_int(bt,count,0)]')
  fault('missing-setter','pr_set_summon_types(bt, user, &type, 1);','/* omit typing */')
p=(r/'battle-core/src/phases.c').read_text();anchor='if (!pr_attr_is(pr_attrs[ai].cls, PR_AC_MoveEffectAttr)) continue;';assert p.count(anchor)==1;faults.append(('missing-owning-callbacks','phases.c',p.replace(anchor,anchor+'\n        if (pr_attrs[ai].cls==PR_AC_CopyTypeAttr||pr_attrs[ai].cls==PR_AC_FirstMoveTypeAttr||pr_attrs[ai].cls==PR_AC_ResistLastMoveTypeAttr) continue;',1),True))
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='type-moves-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,file,changed,own in faults:
  src=d/'fault.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault';subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test=r/('battle-core/build/wide-combat-replay.c' if own else 'battle-core/tests/type-moves-test.c')
  subprocess.run(['gcc',*flags,test,obj,r/'battle-core/build/libprcore.a',*([] if own else ['-Wl,--wrap=pr_battle_rand_seed_int']),'-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns if own else trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-400:],result.stderr[-400:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['TYPE_MOVES_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['TYPE_MOVES 4 12672 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('mode','C ',2,'3'),('user','C ',3,'4'),('target','C ',4,'4'),('operations','C ',5,'0'),('id','I ',5,'4294967296'),('level','I ',6,'2147483648'),('nan','I ',26,'NaN'),('context','QIN ',2,'2'),('history','HI ',2,'9'),('history-move','HENTRY ',1,'9999'),('history-target','HENTRY ',5,'4'),('condition','COND ',1,'2'),('return','RET ',1,'2'),('calls','CALLS ',1,'17'),('range','CALL ',1,'0'),('queue','QCOUNT ',1,'4'),('qlevel','Q ',1,'9999'),('text','Q ',3,'xy')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  f=d/'bad.txt';f.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,f],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')
