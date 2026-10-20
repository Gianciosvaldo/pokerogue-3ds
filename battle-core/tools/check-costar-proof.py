#!/usr/bin/env python3
"""Compiled Costar source regressions and strict full-state transport guards."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/costar-test';subprocess.run([runner,trace],check=True,env=env)
source=(r/'battle-core/src/ab_attrs.c').read_text();faults=[]
a=source.index('    case PR_AC_PostSummonCopyAllyStatsAbAttr: {');b=source.index('\n    case PR_AC_',a+1);body=source[a:b]
def fault(name,old,new):
 assert body.count(old)==1,(name,body.count(old));faults.append((name,source[:a]+body.replace(old,new,1)+source[b:]))
gate='GATE(bt->double_battle && ally && ally->hp > 0 && ally->on_field && !ally->switch_out_status);'
fault('missing-double-gate',gate,'GATE(ally && ally->hp > 0 && ally->on_field && !ally->switch_out_status);')
fault('fainted-ally',gate,'GATE(bt->double_battle && ally && ally->on_field && !ally->switch_out_status);')
fault('inactive-ally',gate,'GATE(bt->double_battle && ally && ally->hp > 0);')
fault('extra-user-field-gate',gate,'GATE(p->on_field && !p->switch_out_status && bt->double_battle && ally && ally->hp > 0 && ally->on_field && !ally->switch_out_status);')
fault('missing-simulation-gate','if (pp->simulated) return 1;','/* omitted simulation check */')
fault('no-stage-copy','memcpy(user->stat_stages, ally->stat_stages, sizeof user->stat_stages);','/* omitted stage copy */')
fault('positive-stages-only','memcpy(user->stat_stages, ally->stat_stages, sizeof user->stat_stages);','for(int j=0;j<7;j++)if(ally->stat_stages[j]>0)user->stat_stages[j]=ally->stat_stages[j];')
fault('zero-instead-of-copied-stages','memcpy(user->stat_stages, ally->stat_stages, sizeof user->stat_stages);','memset(user->stat_stages,0,sizeof user->stat_stages);')
fault('omitted-Dragon-Cheer','if (cheer) {','if (false) {')
fault('new-Cheer-instead-of-first-class','pr_find_battler_tag_class(user, PR_TAGCLASS_CRIT_BOOST)','pr_get_battler_tag(user, BATTLER_TAG_TYPE_DRAGON_CHEER)')
fault('wrong-copied-Cheer-stage','first->crit_stages = cheer->crit_stages;','first->crit_stages = 2;')
fault('wrong-new-Cheer-source-move','user, BATTLER_TAG_TYPE_DRAGON_CHEER, 0, 0, -1','user, BATTLER_TAG_TYPE_DRAGON_CHEER, 0, MOVE_ID_DRAGON_CHEER, -1')
fault('wrong-new-Focus-Energy-source-move','user, BATTLER_TAG_TYPE_CRIT_BOOST, 0, 0, -1','user, BATTLER_TAG_TYPE_CRIT_BOOST, 0, MOVE_ID_FOCUS_ENERGY, -1')
fault('omitted-Focus-Energy','if (pr_get_battler_tag(ally, BATTLER_TAG_TYPE_CRIT_BOOST))','if (false)')
fault('omitted-Laser-Focus','if (pr_get_battler_tag(ally, BATTLER_TAG_TYPE_ALWAYS_CRIT))','if (false)')
fault('discarded-existing-user-tags','memcpy(user->stat_stages, ally->stat_stages, sizeof user->stat_stages);','memcpy(user->stat_stages, ally->stat_stages, sizeof user->stat_stages);user->n_tags=0;')
boost='''        if (pr_get_battler_tag(ally, BATTLER_TAG_TYPE_CRIT_BOOST))
            pr_add_tag(bt, c->e, user, BATTLER_TAG_TYPE_CRIT_BOOST, 0, 0, -1);
'''
assert body.count(boost)==1;changed=body.replace(boost,'').replace('        const pr_battler_tag *cheer =',boost+'        const pr_battler_tag *cheer =',1);faults.append(('wrong-tag-copy-order',source[:a]+changed+source[b:]))
old='    case PR_AC_PostSummonAddBattlerTagAbAttr: return P(PostSummonAddBattlerTagAbAttr, a)->activateOnGain;';assert source.count(old)==1;faults.append(('disabled-ability-gain',source.replace(old,'    case PR_AC_PostSummonCopyAllyStatsAbAttr:return false;\n'+old,1)))
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(r/'battle-core'/p) for p in ['include','gen/out','build']]]
with tempfile.TemporaryDirectory(prefix='costar-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,changed in faults:
  src=d/'ab_attrs.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/tests/costar-test.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  p=subprocess.run([exe,trace],env=env,capture_output=True);assert p.returncode==1,(name,p.returncode,p.stdout[-500:],p.stderr[-500:]);print(name,'caught',flush=True)
 data=trace.read_bytes();first,rest=data.split(b'\n',1);bad=d/'bad.txt';count=0
 def reject(name,changed):
  global count
  bad.write_bytes(changed);p=subprocess.run([runner,bad],env=env,capture_output=True);assert p.returncode==2,(name,p.returncode);count+=1;print(name,'rejected',flush=True)
 reject('version',first.replace(b'COSTAR 1 ',b'COSTAR 2 ')+b'\n'+rest)
 reject('small',b'COSTAR 1 65535 777\n'+rest)
 reject('footer',data.rsplit(b'COSTAR_COMPLETE ',1)[0]+b'COSTAR_COMPLETE 1\n')
 reject('trailing',data+b'JUNK\n')
 for name,prefix,col,value in [('fractional-case',b'C ',1,b'0.5'),('actor',b'C ',2,b'8'),('simulation',b'C ',3,b'2'),('gain',b'C ',4,b'2'),('single',b'C ',5,b'2'),('tag-count',b'IT ',2,b'4'),('tag-kind',b'TAG ',1,b'UNKNOWN'),('tag-fractional-turn',b'TAG ',2,b'1.5'),('tag-source',b'TAG ',4,b'0'),('tag-NaN-crit',b'TAG ',5,b'NaN')]:
  a=data.index(b'\n'+prefix)+1;b=data.index(b'\n',a);row=data[a:b].split();row[col]=value;reject(name,data[:a]+b' '.join(row)+data[b:])
 print(f'{len(faults)} compiled production faults caught; {count} malformed captures rejected')
