#!/usr/bin/env python3
"""Plant real HP Split faults against unchanged native callback and owning turns."""
from pathlib import Path
import os,subprocess,tempfile,sys
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/hp-split-test';subprocess.run([runner,trace],check=True,env=env);subprocess.run([r/'battle-core/build/wide-combat-test',turns],check=True,env=env)
s=(r/'battle-core/src/move_effects.c').read_text();start=s.index('    case PR_AC_HpSplitAttr:');end=s.index('    case PR_AC_SuppressAbilitiesAttr:',start);leaf=s[start:end];faults=[]
def fault(name,old,new):
 assert leaf.count(old)==1,(name,leaf.count(old));faults.append((name,'move_effects.c',s[:start]+leaf.replace(old,new,1)+s[end:],False))
fault('missing-parent-gate','if (!effect_can_apply(ef, user, target)) return;','/* missing gate */')
fault('extra-user-gate','if (!effect_can_apply(ef, user, target)) return;','if (!effect_can_apply(ef, user, target)||user->hp<=0) return;')
fault('ceil-average','floor(((double)target->hp + user->hp) / 2)','ceil(((double)target->hp + user->hp) / 2)')
fault('recomputed-average','pr_pokemon *p = actors[i];','pr_pokemon *p = actors[i];\n            hp_value=floor(((double)target->hp+user->hp)/2);')
fault('target-first','{user, target}','{target, user}')
fault('unclamped-heal','healing = healing < room ? healing : room;','/* omit maximum HP clamp */')
fault('omit-heal','p->hp += (int32_t)healing;','/* omit healing */')
fault('omit-damage','pr_pokemon_damage(bt, e, p, p->hp - hp_value, true, false, false)','0')
fault('wrong-heal-sign','0, (int32_t)healing, NULL','0, -(int32_t)healing, NULL')
fault('wrong-damage-sign','0, -(int32_t)damage, NULL','0, (int32_t)damage, NULL')
fault('omit-heal-number','if (healing) pr_event_add','if (false) pr_event_add')
fault('omit-damage-number','if (damage) pr_event_add','if (false) pr_event_add')
fault('shield-clamp','p->hp - hp_value, true, false, false','p->hp - hp_value, false, false, false')
fault('damage-animation','pr_pokemon_damage(bt, e, p, p->hp - hp_value, true, false, false)','pr_damage_and_update(bt,e,p,p->hp-hp_value,HIT_RESULT_INDIRECT,false,NULL)')
p=(r/'battle-core/src/phases.c').read_text()
def phase_fault(name,old,new,owning=False):
 assert p.count(old)==1,(name,p.count(old));faults.append((name,'phases.c',p.replace(old,new,1),owning))
phase_fault('missing-faint','pr_queue_faint(bt, e, pr_battler_index(bt, p), prevent_endure, -1);','/* omit faint */')
phase_fault('missing-boss-boosts','boss_segments_cleared(bt, e, p, cleared_boss_segment_index);','(void)0; /* omit boss boosts */')
phase_fault('wrong-cleared-shield','if (ignore_segments) cleared_boss_segment_index = (int)ceil','if (ignore_segments) cleared_boss_segment_index = (int)floor')
phase_fault('missing-owning-effect','if (!pr_attr_is(pr_attrs[ai].cls, PR_AC_MoveEffectAttr)) continue;','if (!pr_attr_is(pr_attrs[ai].cls, PR_AC_MoveEffectAttr)||pr_attrs[ai].cls==PR_AC_HpSplitAttr) continue;',True)
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='hp-split-guards-',dir=r/'work') as d:
 d=Path(d)
 for name,file,changed,owning in faults:
  assert changed!=(r/'battle-core/src'/file).read_text(),name;src=d/'fault.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test=r/('battle-core/build/wide-combat-replay.c' if owning else 'battle-core/tests/hp-split-test.c')
  subprocess.run(['gcc',*flags,test,obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns if owning else trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-400:],result.stderr[-400:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['HP_SPLIT_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['HP_SPLIT 4 8320 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('user','C ',2,'4'),('target','C ',3,'4'),('id','I ',5,'4294967296'),('level','I ',6,'2147483648'),('nan','I ',26,'NaN'),('context','QIN ',2,'2'),('queue','QCOUNT ',1,'33'),('boss','IB ',3,'9'),('return','RET ',1,'2'),('condition','COND ',1,'2'),('events','ECOUNT ',1,'3'),('event-actor','EV ',1,'4'),('dynamic','DCOUNT ',1,'3'),('source','D ',3,'4'),('change-count','D ',5,'8'),('stat','CHANGE ',1,'8')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  f=d/'bad.txt';f.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,f],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')
