#!/usr/bin/env python3
"""Check history membership, reset, caller and save faults against native observations."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2];bc=root/'battle-core';gc=root/'game-core'
trace=gc/'tests/fixtures/last-resort-history.native.txt'
runner=bc/'build/last-resort-history-test';host=gc/'build/last_resort_history_host_test'
subprocess.run([runner,trace],check=True);subprocess.run([host,trace],check=True)
faults=[]
def fault(file,name,old,new,game=False):
 source=(root/file).read_text();assert source.count(old)==1,(name,source.count(old))
 faults.append((name,source.replace(old,new),game))
fault('battle-core/src/history.c','forget-aged-membership','    if (p->n_move_history >= PR_MAX_MOVE_HISTORY)','    if(p->n_move_history>=PR_MAX_MOVE_HISTORY)memset(p->history_used_moves,0,sizeof p->history_used_moves);\n    if (p->n_move_history >= PR_MAX_MOVE_HISTORY)')
fault('battle-core/src/history.c','count-virtual-moves','tm->move<PR_MOVE_COUNT&&tm->use_mode<MOVE_USE_MODE_INDIRECT','tm->move<PR_MOVE_COUNT')
fault('battle-core/src/history.c','ignore-high-id','tm->move<PR_MOVE_COUNT&&tm->use_mode<MOVE_USE_MODE_INDIRECT','tm->move<256&&tm->use_mode<MOVE_USE_MODE_INDIRECT')
fault('battle-core/src/history.c','only-current-moves','static void remember_used_move(pr_pokemon *p,const pr_turn_move *tm)\n{','static void remember_used_move(pr_pokemon *p,const pr_turn_move *tm)\n{\n    bool in_set=false;for(int i=0;i<PR_N_MOVES(p);i++)if(PR_MOVES(p)[i]==tm->move)in_set=true;if(!in_set)return;')
fault('battle-core/src/phases.c','retain-summary-on-reset','    p->n_move_history = 0;\n    pr_rebuild_history_used_moves(p);','    p->n_move_history = 0;')
fault('game-core/src/gc_save.c','lose-saved-summary','char key[48];snprintf(key,sizeof key,"%s_move_uses",name);\n    s=section_start(w,key);','char key[48];snprintf(key,sizeof key,"old_%s_move_uses",name);\n    s=section_start(w,key);',True)
fault('game-core/src/gc_save.c','invent-complete-legacy','if(!sections){for(int i=0;i<n;i++)pr_rebuild_history_used_moves(&mons[i].b);return true;}','if(!sections){for(int i=0;i<n;i++){pr_rebuild_history_used_moves(&mons[i].b);mons[i].b.history_used_complete=true;}return true;}',True)
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(root/p)for p in ['battle-core/include','battle-core/gen/out','battle-core/src','game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out']]]
with tempfile.TemporaryDirectory(prefix='lr-history-faults-') as temp:
 temp=Path(temp)
 for name,source,game in faults:
  src,obj,binary=temp/(name+'.c'),temp/(name+'.o'),temp/name;src.write_text(source)
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  libs=[gc/'build'/p for p in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]if game else[bc/'build/libprcore.a']
  main=gc/'tests/last_resort_history_host_test.c'if game else bc/'tests/last-resort-history-test.c'
  subprocess.run(['gcc',*flags,main,obj,*libs,'-lm','-o',binary],check=True,capture_output=True)
  r=subprocess.run([binary,trace],capture_output=True)
  assert r.returncode==1,(name,r.returncode,r.stdout,r.stderr)
  print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-record':rows+['JUNK'],'wrong-count':['LRHISTORY 2 431']+rows[1:]}
 for name,prefix,index,new in [('case-id','CASE ',1,'1'),('mode','CASE ',4,'7'),('zero-run','RUN ',1,'0'),('move-range','RUN ',2,'921'),('outcome','RESULT ',1,'2')]:
  changed=rows.copy();i=next(i for i,row in enumerate(rows)if row.startswith(prefix));parts=rows[i].split();parts[index]=new;changed[i]=' '.join(parts);bad[name]=changed
 for name,rows in bad.items():
  path=temp/'bad.txt';path.write_text('\n'.join(rows)+'\n');r=subprocess.run([runner,path],capture_output=True)
  assert r.returncode==2,(name,r.returncode,r.stdout,r.stderr)
  print(name,'rejected',flush=True)
print('Seven compiled history/save faults caught; eight malformed captures rejected')
