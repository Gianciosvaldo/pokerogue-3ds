#!/usr/bin/env python3
"""Run retained actual Protect captures and compiled failure controls."""
from pathlib import Path
import argparse,gzip,subprocess,tempfile
root=Path(__file__).resolve().parents[2];bc=root/'battle-core';gc=root/'game-core'
args=argparse.ArgumentParser();args.add_argument('--run-only',action='store_true');args=args.parse_args()
with tempfile.TemporaryDirectory(prefix='protect-history-')as temporary:
 temp=Path(temporary);trace=temp/'native.txt';trace.write_bytes(gzip.decompress((gc/'tests/fixtures/protect-history.native.txt.gz').read_bytes()))
 runner=bc/'build/protect-history-test';guards=bc/'build/protect-history-guards-test';host=gc/'build/protect_history_host_test'
 for command in [[runner,trace],[guards],[host,trace]]:subprocess.run(command,check=True)
 if args.run_only:raise SystemExit(0)
 faults=[]
 def fault(file,name,old,new,kind='core'):
  source=(root/file).read_text();assert source.count(old)==1,(name,source.count(old));faults.append((name,source.replace(old,new),kind))
 fault('battle-core/src/history.c','truncate-to-tail','if(p->n_move_history<=PR_MAX_MOVE_HISTORY||!complete||p->history_serial<p->n_move_history)','if(true)')
 fault('battle-core/src/history.c','ignore-quick-guard','|| tm->move==MOVE_ID_QUICK_GUARD','|| false')
 fault('battle-core/src/history.c','ignore-wide-guard','|| tm->move==MOVE_ID_WIDE_GUARD','|| false')
 fault('battle-core/src/history.c','ignore-virtual-protections','return tm->result==MOVE_RESULT_SUCCESS &&','return tm->use_mode<MOVE_USE_MODE_INDIRECT && tm->result==MOVE_RESULT_SUCCESS &&')
 fault('battle-core/src/history.c','count-failed-protections','return tm->result==MOVE_RESULT_SUCCESS &&','return true &&')
 fault('battle-core/src/history.c','count-non-protect','return tm->result==MOVE_RESULT_SUCCESS && (pr_move_has_attr(tm->move,PR_AC_ProtectAttr)\n        || tm->move==MOVE_ID_QUICK_GUARD || tm->move==MOVE_ID_WIDE_GUARD);','return tm->result==MOVE_RESULT_SUCCESS;')
 fault('battle-core/src/history.c','wrap-streak-at-16-bits','p->history_protect_count++;','p->history_protect_count=(uint16_t)(p->history_protect_count+1);')
 fault('battle-core/src/history.c','retain-stopped-streak','}else{p->history_protect_count=0;p->history_protect_complete=true;}','}else{/* stale streak */}')
 fault('battle-core/src/history.c','invent-raw-prefix','*complete=p->n_move_history<=PR_MAX_MOVE_HISTORY;','*complete=true;','guard')
 fault('battle-core/src/phases.c','retain-reset-streak','    pr_rebuild_protect_history(p);','    /* stale streak */','guard')
 fault('battle-core/src/hooks.c','skip-infinite-range-draw','    double max=range-1+min;','    if(isinf(range))return INFINITY;\n    double max=range-1+min;')
 fault('battle-core/src/hooks.c','use-global-stream','return floor(pr_rng_real_in_range(&bt->battle_rng,0,max-min+1)+min);','return floor(pr_rng_real_in_range(&bt->rng,0,max-min+1)+min);')
 fault('battle-core/src/closures_conditions.c','force-large-streak-failure','return pr_battle_rand_seed_number(bt,pow(3,(double)times),0)==0;','if(times>19)return 0;return pr_battle_rand_seed_number(bt,pow(3,(double)times),0)==0;')
 fault('battle-core/src/closures_conditions.c','roll-on-empty-streak','    if (times == 0) return 1;','    if (times == 0) return pr_battle_rand_seed_number(bt,3,0)>=0;')
 fault('game-core/src/gc_save.c','omit-saved-streak','    snprintf(key,sizeof key,"%s_protect",name);','    snprintf(key,sizeof key,"old_%s_protect",name);','save')
 fault('game-core/src/gc_save.c','forget-loaded-streak','for(int i=0;i<n;i++){uint8_t complete;memcpy(&complete,&mons[i].b.history_protect_complete,1);if(complete>1)return false;}','for(int i=0;i<n;i++){uint8_t complete;memcpy(&complete,&mons[i].b.history_protect_complete,1);if(complete>1)return false;mons[i].b.history_protect_count=0;}','save')
 fault('game-core/src/gc_save.c','invent-legacy-streak','if(!sections){for(int i=0;i<n;i++)pr_rebuild_protect_history(&mons[i].b);return true;}','if(!sections){for(int i=0;i<n;i++){pr_rebuild_protect_history(&mons[i].b);mons[i].b.history_protect_complete=true;}return true;}','save')
 fault('game-core/src/gc_save.c','accept-nonboolean-streak','if(complete>1)return false;}','if(false)return false;}','save')
 flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(root/p)for p in ['battle-core/include','battle-core/gen/out','battle-core/src','game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out']]]
 for name,source,kind in faults:
  src,obj,binary=temp/(name+'.c'),temp/(name+'.o'),temp/name;src.write_text(source)
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  libs=[gc/'build'/p for p in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]if kind=='save'else[bc/'build/libprcore.a']
  main=gc/'tests/protect_history_host_test.c'if kind=='save'else bc/('tests/protect-history-guards-test.c'if kind=='guard'else'tests/protect-history-test.c')
  subprocess.run(['gcc',*flags,main,obj,*libs,'-lm','-o',binary],check=True,capture_output=True)
  command=[binary]if kind=='guard'else[binary,trace];r=subprocess.run(command,capture_output=True)
  assert r.returncode==1,(name,r.returncode,r.stdout,r.stderr);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-record':rows+['JUNK'],'wrong-count':['PROTECTHISTORY 1 1223 1']+rows[1:]}
 for name,prefix,index,new in [('case-id','CASE ',1,'1'),('mode','CASE ',4,'7'),('owner','OBS ',1,'0'),('rng','OBS ',2,'4'),('state-index','OBS ',3,'32'),('outcome','OBS ',5,'2'),('gate-outcome','GATE ',3,'2')]:
  changed=rows.copy();i=next(i for i,row in enumerate(rows)if row.startswith(prefix));parts=rows[i].split();parts[index]=new;changed[i]=' '.join(parts);bad[name]=changed
 for name,rows in bad.items():
  path=temp/'bad.txt';path.write_text('\n'.join(rows)+'\n');r=subprocess.run([runner,path],capture_output=True)
  assert r.returncode==2,(name,r.returncode,r.stdout,r.stderr);print(name,'rejected',flush=True)
 print('18 compiled Protect/history/RNG/save faults caught; 10 malformed captures rejected')
