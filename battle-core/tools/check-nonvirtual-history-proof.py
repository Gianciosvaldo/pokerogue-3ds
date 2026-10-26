#!/usr/bin/env python3
"""Compiled counterexamples and strict malformed-transport rejection."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2];bc=root/'battle-core';gc=root/'game-core'
trace=gc/'tests/fixtures/nonvirtual-history.native.txt'
runner=bc/'build/nonvirtual-history-test';guards=bc/'build/nonvirtual-history-guards-test';host=gc/'build/nonvirtual_history_host_test'
for command in [[runner,trace],[guards],[host,trace]]:subprocess.run(command,check=True)
faults=[]
def fault(file,name,old,new,kind='core',also=None):
 source=(root/file).read_text();assert source.count(old)==1,(name,source.count(old));source=source.replace(old,new)
 if also:
  # Recycling and target propagation independently reject stale summon epochs.
  prior,replacement=also;assert source.count(prior)==1;source=source.replace(prior,replacement)
 faults.append((name,source,kind))
fault('battle-core/src/history.c','forget-aged-real-move','    remember_nonvirtual(p,&tm,++p->history_serial);','    /* forgot incoming semantic state */')
fault('battle-core/src/history.c','conflate-query-flags','for(int q=0;q<4;q++)if(nonvirtual_matches(tm,q/2,q%2))','for(int q=0;q<4;q++)if(nonvirtual_matches(tm,false,false))')
fault('battle-core/src/history.c','ignore-follow-ups','(!ignore_follow_up && tm->use_mode==MOVE_USE_MODE_FOLLOW_UP)','(false && tm->use_mode==MOVE_USE_MODE_FOLLOW_UP)')
fault('battle-core/src/history.c','allow-struggle','(!ignore_struggle || tm->move!=MOVE_ID_STRUGGLE)','true')
fault('battle-core/src/history.c','require-success','return tm->move!=MOVE_ID_NONE','return tm->result==MOVE_RESULT_SUCCESS && tm->move!=MOVE_ID_NONE')
fault('battle-core/src/history.c','lose-targets','p->history_last[q]=*tm;','p->history_last[q]=*tm;p->history_last[q].n_targets=0;')
fault('battle-core/src/history.c','lose-result','p->history_last[q]=*tm;','p->history_last[q]=*tm;p->history_last[q].result=MOVE_RESULT_SUCCESS;')
fault('battle-core/src/engine.c','drop-shared-cache-targets','p->history_last_serial[q]==e->hist_alias[k].abs','false')
fault('battle-core/src/engine.c','truncate-shared-history-position','e->hist_alias[retained].abs = p->history_serial;','e->hist_alias[retained].abs = (uint16_t)p->history_serial;')
fault('battle-core/src/engine.c','reuse-old-summon-alias','if(e->hist_alias[k].epoch!=p->history_epoch)continue;','/* old summon leaks */',also=('if (epoch != p->history_epoch) return false;','/* retain stale summon aliases */'))
fault('battle-core/src/history.c','invent-complete-prefix','p->history_last_known=p->n_move_history<=PR_MAX_MOVE_HISTORY?15:0;','p->history_last_known=15;','guard')
fault('battle-core/src/phases.c','retain-reset-cache','    pr_rebuild_nonvirtual_history(p);','    /* stale summon state */','guard')
fault('game-core/src/gc_save.c','omit-saved-cache','    snprintf(key,sizeof key,"%s_last_moves",name);','    snprintf(key,sizeof key,"old_%s_last_moves",name);','save')
fault('game-core/src/gc_save.c','invent-legacy-prefix','if(!sections){for(int i=0;i<n;i++)pr_rebuild_nonvirtual_history(&mons[i].b);return true;}','if(!sections){for(int i=0;i<n;i++){pr_rebuild_nonvirtual_history(&mons[i].b);mons[i].b.history_last_known=15;}return true;}','save')
fault('game-core/src/gc_save.c','accept-invalid-mask','if(p->history_last_known>15||(p->history_last_present&~p->history_last_known))return false;','if(false)return false;','save')
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(root/p)for p in ['battle-core/include','battle-core/gen/out','battle-core/src','game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out']]]
with tempfile.TemporaryDirectory(prefix='nv-history-faults-')as temp:
 temp=Path(temp)
 for name,source,kind in faults:
  src,obj,binary=temp/(name+'.c'),temp/(name+'.o'),temp/name;src.write_text(source)
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  libs=[gc/'build'/p for p in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]if kind=='save'else[bc/'build/libprcore.a']
  main=gc/'tests/nonvirtual_history_host_test.c'if kind=='save'else bc/('tests/nonvirtual-history-guards-test.c'if kind=='guard'else'tests/nonvirtual-history-test.c')
  subprocess.run(['gcc',*flags,main,obj,*libs,'-lm','-o',binary],check=True,capture_output=True)
  command=[binary]if kind=='guard'else[binary,trace]
  r=subprocess.run(command,capture_output=True);assert r.returncode==1,(name,r.returncode,r.stdout,r.stderr)
  print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-record':rows+['JUNK'],'wrong-count':['NVHISTORY 1 971']+rows[1:]}
 for name,prefix,index,new in [('case-id','CASE ',1,'1'),('mode','CASE ',3,'7'),('zero-run','RUN ',1,'0'),('move-range','RUN ',2,'921'),('bad-result','ADD ',3,'5'),('bad-target-count','ADD ',4,'5'),('bad-target','ADD ',5,'4'),('owner-outcome','OWNERS ',1,'2')]:
  changed=rows.copy();i=next(i for i,row in enumerate(rows)if row.startswith(prefix));parts=rows[i].split();parts[index]=new;changed[i]=' '.join(parts);bad[name]=changed
 for name,rows in bad.items():
  path=temp/'bad.txt';path.write_text('\n'.join(rows)+'\n');r=subprocess.run([runner,path],capture_output=True)
  assert r.returncode==2,(name,r.returncode,r.stdout,r.stderr);print(name,'rejected',flush=True)
print('15 compiled query/reset/save/shared-target faults caught; 11 malformed captures rejected')
