#!/usr/bin/env python3
"""Compile faults into the real move-effect implementation; never edit expectations."""
from pathlib import Path
import os,subprocess,tempfile,sys
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/stat-rearrangement-test';subprocess.run([runner,trace],check=True,env=env)
s=(r/'battle-core/src/move_effects.c').read_text()
faults=[
 ('swap-base-user','double old_user = pr_get_stat(user, stat, false);','double old_user = pr_get_stat(user, stat, true);'),
 ('swap-base-target','double old_target = pr_get_stat(target, stat, false);','double old_target = pr_get_stat(target, stat, true);'),
 ('swap-permanent','user->summon_stats[stat] = old_target;','user->stats[stat] = old_target;'),
 ('swap-read-after-write','target->summon_stats[stat] = old_user;','target->summon_stats[stat] = pr_get_stat(user,stat,false);'),
 ('swap-nonpositive','if (old_target > 0) user->summon_stats[stat] = old_target;','user->summon_stats[stat] = old_target;'),
 ('shift-read-after-write','user->summon_stats[x->statToSwitchWith] = first;','user->summon_stats[x->statToSwitchWith] = pr_get_stat(user,x->statToSwitch,false);'),
 ('shift-int32','double first = pr_get_stat(user, x->statToSwitch, false);','int32_t first = pr_get_stat(user, x->statToSwitch, false);'),
 ('shift-nonpositive','if (second > 0) user->summon_stats[x->statToSwitch] = second;','user->summon_stats[x->statToSwitch] = second;'),
 ('invert-user','target->stat_stages[s] = -target->stat_stages[s];','user->stat_stages[s] = -user->stat_stages[s];'),
 ('invert-missing-evasion','for (int s = 0; s < 7; s++) target->stat_stages[s] = -target->stat_stages[s];','for (int s = 0; s < 6; s++) target->stat_stages[s] = -target->stat_stages[s];'),
]
for cls in ['SwapStatAttr','ShiftStatAttr','InvertStatsAttr']:
 old='case PR_AC_'+cls+(':' if cls=='InvertStatsAttr' else ': {')+'\n        if (!effect_can_apply(ef, user, target)) return;'
 faults.append((cls+'-gate',old,old.replace('if (!effect_can_apply(ef, user, target)) return;','/* missing gate */')))
faults=[(name,'move_effects.c',old,new) for name,old,new in faults]
faults += [
 ('effective-int32','pokemon.c','return ret > 1 ? ret : 1;','return ret > 1 ? (int32_t)ret : 1;'),
 ('speed-array-int32','engine.c','double spd[PR_DYN_CAP];','int32_t spd[PR_DYN_CAP];'),
 ('speed-key-int32','engine.c','double ks = spd[i];','int32_t ks = spd[i];'),
 ('owning-stat-dispatch','phases.c','for (int k = 0; k < n; k++) pr_apply_move_effect(bt, e, ph, sel[k], user, target);','for (int k = 0; k < n; k++) if (pr_attrs[sel[k]].cls != PR_AC_SwapStatAttr && pr_attrs[sel[k]].cls != PR_AC_ShiftStatAttr && pr_attrs[sel[k]].cls != PR_AC_InvertStatsAttr) pr_apply_move_effect(bt, e, ph, sel[k], user, target);'),
 ('shift-c-cast','pokemon.c','effective_stat_int32(ret)','(int32_t)ret'),
]
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='stat-rearrange-guards-',dir=r/'work') as d:
 d=Path(d)
 for name,file,old,new in faults:
  s=(r/'battle-core/src'/file).read_text()
  assert s.count(old)==(4 if name=='shift-c-cast' else 1),(name,s.count(old));p=d/'fault.c';p.write_text(s.replace(old,new) if name=='shift-c-cast' else s.replace(old,new,1));obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',p,'-o',obj],check=True,capture_output=True)
  owning=file in ['engine.c','phases.c']
  test=r/('battle-core/build/wide-combat-replay.c' if owning else 'battle-core/tests/stat-rearrangement-test.c')
  subprocess.run(['gcc',*flags,test,obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns if owning else trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-500:],result.stderr[-500:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['STAT_REARRANGE_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['STAT_REARRANGE 3 12288 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('mode','C ',2,'3'),('user','C ',3,'4'),('target','C ',4,'4'),('repeats','C ',5,'0'),('id','I ',5,'4294967296'),('level','I ',6,'2147483648'),('nan','I ',26,'NaN'),('queue','QCOUNT ',1,'33')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  p=d/'bad.txt';p.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,p],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed fixtures rejected')
