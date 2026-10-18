#!/usr/bin/env python3
"""Compile native-pre-damage callback/caller faults; keep source records fixed."""
from pathlib import Path
import os,subprocess,tempfile,sys
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/spectral-thief-test';subprocess.run([runner,trace],check=True,env=env);subprocess.run([r/'battle-core/build/wide-combat-test',turns],check=True,env=env)
s=(r/'battle-core/src/move_effects.c').read_text();start=s.index('void pr_apply_pre_damage_stat_changes(');end=s.index('\nvoid pr_apply_move_effect(',start);leaf=s[start:end]
queue='pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, user), stats, 1, available, user);'
faults=[]
def leaf_fault(name,old,new):
 assert leaf.count(old)==1,(name,leaf.count(old));faults.append((name,'move_effects.c',s[:start]+leaf.replace(old,new,1)+s[end:],False))
leaf_fault('user-hp-gate','    const pr_move *mv =','    if (user->hp <= 0) return;\n    const pr_move *mv =')
leaf_fault('target-hp-gate','    const pr_move *mv =','    if (target->hp <= 0) return;\n    const pr_move *mv =')
leaf_fault('zero-target-stages','if (target_value > 0)','if (target_value >= 0)')
leaf_fault('uncapped-theft','int available = target_value < 6 - user_value ? target_value : 6 - user_value;','int available = target_value;')
leaf_fault('wrong-budget','int available = target_value < 6 - user_value ? target_value : 6 - user_value;','int available = target_value < 6 + user_value ? target_value : 6 + user_value;')
leaf_fault('clear-entire-target','(int8_t)(target_value - available)','0')
leaf_fault('immediate-user-gain',queue,queue+'\n                user->stat_stages[stat - 1] += available;')
leaf_fault('omit-zero-phases','int32_t stats[] = {stat};','if (!available) continue;\n                int32_t stats[] = {stat};')
leaf_fault('wrong-source',queue,queue.replace('available, user','available, target'))
leaf_fault('wrong-recipient',queue,queue.replace('pr_battler_index(bt, user)','pr_battler_index(bt, target)'))
leaf_fault('reverse-stat-order','for (int stat = STAT_ATK; stat <= STAT_EVA; stat++)','for (int stat = STAT_EVA; stat >= STAT_ATK; stat--)')
leaf_fault('missing-evasion','stat <= STAT_EVA','stat < STAT_EVA')
leaf_fault('missing-gain-queue',queue,'/* missing gains */')
leaf_fault('missing-message','pr_queue_msg(bt, e, pr_msg2("moveTriggers:stealPositiveStats",\n            pr_arg_pokemon(bt, "pokemonName", user), pr_arg_pokemon(bt, "targetName", target)));','/* omitted message */')
leaf_fault('wrong-message-target','pr_arg_pokemon(bt, "targetName", target)','pr_arg_pokemon(bt, "targetName", user)')
leaf_fault('wrong-affix','pr_arg_pokemon(bt, "pokemonName", user)','pr_arg_pokemon_name(bt, "pokemonName", user)')
leaf_fault('spurious-rng','    const pr_move *mv =','    pr_battle_rand_seed_int(bt,100,0);\n    const pr_move *mv =')
caller=(r/'battle-core/src/phases.c').read_text();call='    pr_apply_pre_damage_stat_changes(bt, e, user, target, move);';damage='    pr_damage_result d = pr_get_attack_damage_ex(bt, target, user, move, crit, false, eff);';assert caller.count(call)==caller.count(damage)==1
faults += [('missing-owning-call','phases.c',caller.replace(call,'    /* omitted pre-damage callbacks */'),True),('theft-after-damage','phases.c',caller.replace(call+'\n'+damage,damage+'\n'+call),True)]
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='spectral-thief-guards-',dir=r/'work') as d:
 d=Path(d)
 for name,file,changed,owning in faults:
  assert changed!=(r/'battle-core/src'/file).read_text(),name;src=d/'fault.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test=r/('battle-core/build/wide-combat-replay.c' if owning else 'battle-core/tests/spectral-thief-test.c')
  subprocess.run(['gcc',*flags,test,obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns if owning else trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-400:],result.stderr[-400:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['SPECTRAL_THIEF_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['SPECTRAL_THIEF 2 8192 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('user','C ',2,'4'),('target','C ',3,'4'),('id','I ',5,'4294967296'),('level','I ',6,'2147483648'),('nan','I ',26,'NaN'),('queue','QCOUNT ',1,'33'),('dynamic','DCOUNT ',1,'8'),('source','D ',3,'4'),('stat','D ',6,'8'),('change-count','D ',5,'2'),('stages','D ',7,'7'),('self','D ',4,'2')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  p=d/'bad.txt';p.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,p],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')
