#!/usr/bin/env python3
"""Retained actual all-history callers, strict guards and compiled controls."""
from pathlib import Path
import subprocess,tempfile,gzip,json,copy
root=Path(__file__).resolve().parents[2];bc=root/'battle-core';gc=root/'game-core'
# Rebuild from the lossless native capture and reject incomplete transports.
raw=gzip.decompress((bc/'tests/fixtures/remaining-history.native.json.gz').read_bytes())
with tempfile.TemporaryDirectory(prefix='remaining-capture-') as directory:
 temp=Path(directory);capture=temp/'native.json';fixture=temp/'native.inc';capture.write_bytes(raw)
 subprocess.run(['python3',bc/'tools/build-remaining-history-proof.py',capture,fixture],check=True)
 assert fixture.read_bytes()==(bc/'tests/remaining-history.native.inc').read_bytes()
 original=json.loads(raw)
 for fault in range(8):
  bad=copy.deepcopy(original)
  if fault==0:bad['version']=2
  if fault==1:bad['rows'].pop()
  if fault==2:bad['rows'][1]=bad['rows'][0]
  if fault==3:bad['rows'][0]['mode']=7
  if fault==4:bad['rows'][0]['stages'].pop()
  if fault==5:bad['rows'][0]['stages'][0]['accuracy'][0]=2
  if fault==6:bad['rows'][0]['stages'][0]['conversion']=1
  if fault==7:del bad['rows'][0]['length']
  capture.write_text(json.dumps(bad));result=subprocess.run(['python3',bc/'tools/build-remaining-history-proof.py',capture,fixture],capture_output=True)
  assert result.returncode!=0,fault
 print('8 malformed native captures rejected',flush=True)
for binary in [bc/'build/remaining-history-test',bc/'build/remaining-history-guards-test',gc/'build/remaining_history_host_test']:subprocess.run([binary],check=True)
faults=[]
def fault(file,name,old,new,kind='native',also=None):
 source=(root/file).read_text();assert source.count(old)==1,(name,source.count(old));source=source.replace(old,new)
 if also:
  # Recycling and target propagation independently reject stale summon epochs.
  prior,replacement=also;assert source.count(prior)==1;source=source.replace(prior,replacement)
 faults.append((name,file,source,kind))
fault('battle-core/src/history.c','ignore-virtual','if(tm->move!=MOVE_ID_NONE){','if(tm->move!=MOVE_ID_NONE&&tm->use_mode<MOVE_USE_MODE_INDIRECT){')
fault('battle-core/src/history.c','ignore-failed-aim','if(tm->move==MOVE_ID_LOCK_ON||tm->move==MOVE_ID_MIND_READER){','if(tm->result==MOVE_RESULT_SUCCESS&&(tm->move==MOVE_ID_LOCK_ON||tm->move==MOVE_ID_MIND_READER)){')
fault('battle-core/src/history.c','omit-mind-reader','if(tm->move==MOVE_ID_LOCK_ON||tm->move==MOVE_ID_MIND_READER){','if(tm->move==MOVE_ID_LOCK_ON){')
fault('battle-core/src/history.c','retain-first-aim','if(tm->move==MOVE_ID_LOCK_ON||tm->move==MOVE_ID_MIND_READER){','if(!(p->history_all_present&2)&&(tm->move==MOVE_ID_LOCK_ON||tm->move==MOVE_ID_MIND_READER)){')
fault('battle-core/src/history.c','invent-legacy-absence','p->history_all_known=p->n_move_history<=PR_MAX_MOVE_HISTORY?3:0;','p->history_all_known=3;','guard')
fault('battle-core/src/engine.c','lose-shared-targets','if((p->history_all_present&2)&&p->history_aim_serial==e->hist_alias[k].abs){','if(false){')
fault('battle-core/src/engine.c','bind-old-summon-array','if(e->hist_alias[k].epoch!=p->history_epoch)continue;','/* stale array remains bound */','guard',also=('if (epoch != p->history_epoch) return false;','/* retain stale summon aliases */'))
fault('game-core/src/gc_save.c','omit-save-extension','    snprintf(key,sizeof key,"%s_all_moves",name);','    snprintf(key,sizeof key,"old_%s_all_moves",name);','save')
fault('game-core/src/gc_save.c','invent-legacy-prefix','if(!sections){for(int i=0;i<n;i++)pr_rebuild_all_history(&mons[i].b);return true;}','if(!sections){for(int i=0;i<n;i++){pr_rebuild_all_history(&mons[i].b);mons[i].b.history_all_known=3;}return true;}','save')
fault('game-core/src/gc_save.c','admit-invalid-mask','if(p->history_all_known>3||','if(false||','save')
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(root/p)for p in ['battle-core/include','battle-core/gen/out','battle-core/src','battle-core/tests','game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out']]]
with tempfile.TemporaryDirectory(prefix='remaining-history-') as directory:
 temp=Path(directory)
 for name,file,source,kind in faults:
  src,obj,binary=temp/(name+'.c'),temp/(name+'.o'),temp/name;src.write_text(source)
  main=gc/'tests/remaining_history_host_test.c' if kind=='save' else bc/('tests/remaining-history-guards-test.c' if kind=='guard' else 'tests/remaining-history-test.c')
  objects=[]
  if file=='battle-core/src/phases.c' and kind!='guard':
   main=temp/'main.c';main.write_text((bc/'tests/remaining-history-test.c').read_text().replace('#include "../src/phases.c"',source))
  else:
   subprocess.run(['cc',*flags,'-c',src,'-o',obj],check=True,capture_output=True);objects=[obj]
  libs=[gc/'build'/p for p in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]if kind=='save'else[bc/'build/libprcore.a']
  subprocess.run(['cc',*flags,main,*objects,*libs,'-lm','-o',binary],check=True,capture_output=True)
  result=subprocess.run([binary],capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout,result.stderr);print(name,'caught',flush=True)
 print('10 compiled all-history/alias/reset/save faults caught')
