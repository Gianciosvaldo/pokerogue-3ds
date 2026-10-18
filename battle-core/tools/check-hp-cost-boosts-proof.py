#!/usr/bin/env python3
"""Detect compiled cost, gate, HP-cap, queue and owning-message faults."""
from pathlib import Path
import os,subprocess,tempfile,sys
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/hp-cost-boosts-test';subprocess.run([runner,trace],check=True,env=env);subprocess.run([r/'battle-core/build/wide-combat-test',turns],check=True,env=env)
s=(r/'battle-core/src/move_effects.c').read_text();start=s.index('    case PR_AC_CutHpStatStageBoostAttr:');end=s.index('    case PR_AC_StatStageChangeAttr:',start);leaf=s[start:end];faults=[]
def fault(name,old,new):
 assert leaf.count(old)==1,(name,leaf.count(old));faults.append((name,'move_effects.c',s[:start]+leaf.replace(old,new,1)+s[end:],False))
cost='pr_damage_and_update(bt, e, user, cost, HIT_RESULT_INDIRECT, false, NULL);'
boost='stat_stage_change(bt, e, ef, a->cls, user, target, move, x->stats.v, x->stats.n, x->stages);'
fault('omit-cost',cost,'/* missing cost */')
fault('parent-gate-before-cost','double cost =','if (!effect_can_apply(ef,user,target)) return;\n        double cost =')
fault('raw-move-condition','double cost =','if (!pr_move_apply_conditions(bt,move,user,target,4)) return;\n        double cost =')
fault('wrong-cost-recipient',cost,cost.replace('e, user,','e, target,'))
fault('boost-before-cost',cost+'\n        '+boost,boost+'\n        '+cost)
fault('wrong-divisor','/ x->cutRatio','/ 2.0')
fault('round-up','to_dmg_value(pr_get_stat(user, STAT_HP, true) / x->cutRatio)','ceil(pr_get_stat(user, STAT_HP, true) / x->cutRatio)')
fault('omit-minimum-one','to_dmg_value(pr_get_stat(user, STAT_HP, true) / x->cutRatio)','floor(pr_get_stat(user, STAT_HP, true) / x->cutRatio)')
fault('temporary-hp-cost','pr_get_stat(user, STAT_HP, true)','pr_get_stat(user, STAT_HP, false)')
fault('wrong-result',cost,cost.replace('HIT_RESULT_INDIRECT','HIT_RESULT_EFFECTIVE'))
fault('ignore-faint',cost,cost.replace('INDIRECT, false','INDIRECT, true'))
fault('omit-boost',boost,'/* missing boost */')
fault('wrong-stages','x->stats.n, x->stages','x->stats.n, 1')
fault('spurious-rng','double cost =','pr_battle_rand_seed_int(bt,100,0);\n        double cost =')
a=s.index('static bool stat_stage_change(');b=s.index('/* FaintPhase',a);helper=s[a:b]
for name,old,new,own in [('missing-post-cost-hp-gate','if (!effect_can_apply(ef, user, target)) return false;','/* missing gate */',False),('wrong-boost-recipient','ef->self_target ? user : target','target',False),('wrong-boost-source','stats, n, stages, user)','stats, n, stages, target)',False),('missing-custom-message','e->pool[ph].ssc_message = true;','e->pool[ph].ssc_message = false;',False)]:
 assert helper.count(old)==1,(name,helper.count(old));faults.append((name,'move_effects.c',s[:a]+helper.replace(old,new,1)+s[b:],own))
p=(r/'battle-core/src/phases.c').read_text()
for name,old,new,own in [('temporary-boss-hp','static int32_t max_hp(const pr_pokemon *p) { return pr_get_stat(p, STAT_HP, true); }','static int32_t max_hp(const pr_pokemon *p) { return pr_get_stat(p, STAT_HP, false); }',False),('missing-faint','pr_queue_faint(bt, e, pr_battler_index(bt, p), prevent_endure, -1);','/* no faint */',False),('missing-animation-amount','if (anim >= 0) PH(anim)->anim_amount = (int32_t)damage;','if (anim >= 0) PH(anim)->anim_amount = 0;',False),('missing-owning-callback','if (!pr_attr_is(pr_attrs[ai].cls, PR_AC_MoveEffectAttr)) continue;','if (!pr_attr_is(pr_attrs[ai].cls, PR_AC_MoveEffectAttr)||pr_attrs[ai].cls==PR_AC_CutHpStatStageBoostAttr) continue;',True),('wrong-custom-message','"moveTriggers:cutOwnHpAndMaximizedStat"','"battle:statRose"',True),('wrong-message-stat','pr_arg_stat("statName", STAT_ATK)','pr_arg_stat("statName", STAT_DEF)',True)]:
 assert p.count(old)==1,(name,p.count(old));faults.append((name,'phases.c',p.replace(old,new,1),own))
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='hp-cost-guards-',dir=r/'work') as d:
 d=Path(d)
 for name,file,changed,owning in faults:
  assert changed!=(r/'battle-core/src'/file).read_text(),name;src=d/'fault.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test=r/('battle-core/build/wide-combat-replay.c' if owning else 'battle-core/tests/hp-cost-boosts-test.c')
  subprocess.run(['gcc',*flags,test,obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns if owning else trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-400:],result.stderr[-400:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['HP_COST_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['HP_COST 4 12928 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('mode','C ',2,'3'),('user','C ',3,'4'),('target','C ',4,'4'),('id','I ',5,'4294967296'),('level','I ',6,'2147483648'),('nan','I ',26,'NaN'),('context','QIN ',2,'2'),('queue','QCOUNT ',1,'33'),('boss','IB ',3,'9'),('tag','IT ',3,'INVALID'),('return','RET ',1,'2'),('condition','COND ',1,'2'),('dynamic','DCOUNT ',1,'5'),('source','D ',3,'4'),('change-count','D ',5,'8'),('stat','CHANGE ',1,'8'),('stage','CHANGE ',2,'13')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  f=d/'bad.txt';f.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,f],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')
