#!/usr/bin/env python3
"""Actual FaintPhase/consumer captures and fault controls."""
from pathlib import Path
import subprocess,tempfile,gzip,json,copy
root=Path(__file__).resolve().parents[2];bc=root/'battle-core';gc=root/'game-core';ac=root/'ai-core'
raw=gzip.decompress((bc/'tests/fixtures/faint-history.native.json.gz').read_bytes())
with tempfile.TemporaryDirectory(prefix='faint-capture-') as directory:
 temp=Path(directory);capture=temp/'native.json';fixture=temp/'native.inc';capture.write_bytes(raw)
 subprocess.run(['python3',bc/'tools/build-faint-history-proof.py',capture,fixture],check=True)
 assert fixture.read_bytes()==(bc/'tests/faint-history.native.inc').read_bytes()
 original=json.loads(raw)
 for fault in range(12):
  bad=copy.deepcopy(original)
  if fault==0:bad['version']=2
  if fault==1:bad['rows'].pop()
  if fault==2:bad['rows'][1]=bad['rows'][0]
  if fault==3:bad['rows'][0]['side']=2
  if fault==4:bad['rows'][1]['events'].pop()
  if fault==5:bad['rows'][1]['logs'][0].pop()
  if fault==6:bad['rows'][0]['observations'][0]['tera'][0]=1
  if fault==7:bad['rows'][0]['observations'][0]['power'][0]=3
  if fault==8:del bad['maxExpLevel']
  if fault==9:bad['globalRng']='!rnd,invalid'
  if fault==10:bad['rows'][1]['events'][0]['index']=6
  if fault==11:bad['rows'][1]['playerFaints']=2
  capture.write_text(json.dumps(bad));result=subprocess.run(['python3',bc/'tools/build-faint-history-proof.py',capture,fixture],capture_output=True)
  assert result.returncode!=0,fault
 print('12 malformed native faint captures rejected',flush=True)
for binary in [bc/'build/faint-history-test',bc/'build/faint-history-guards-test',gc/'build/faint_history_host_test']:subprocess.run([binary],check=True)
faults=[]
def fault(file,name,old,new,kind='native'):
 source=(root/file).read_text();assert source.count(old)==1,(name,source.count(old));faults.append((name,file,source.replace(old,new),kind))
fault('battle-core/src/history.c','keep-first-eleven','memmove(bt->faint_history[side],bt->faint_history[side]+1,11*sizeof bt->faint_history[side][0]);','/* stale first eleven */')
fault('battle-core/src/history.c','forget-old-membership','    // Raw callers may supply a full tail with no membership summary.','    bt->n_fainted_ids[side]=0;\n    // Raw callers may supply a full tail with no membership summary.')
fault('battle-core/src/history.c','ignore-zero-id','static void remember_fainted_id(pr_battle *bt,int side,uint32_t id)\n{','static void remember_fainted_id(pr_battle *bt,int side,uint32_t id)\n{\n    if(!id)return;','guard')
fault('battle-core/src/history.c','invent-missing-prefix','if(bt->faint_history_total[side]<=(uint32_t)kept||bt->faint_ids_complete[side])return false;','return false;','guard')
fault('battle-core/src/history.c','hide-identity-overflow','bt->faint_ids_complete[side]=false;pr_note_unported(bt,PR_FEAT_ENGINE_LIMIT);return;','bt->faint_ids_complete[side]=false;return;','guard')
for field in ['rng','battle_rng']:fault('battle-core/src/history.c','consume-'+field,'void pr_record_faint(pr_battle *bt,int side,uint32_t id,int turn)\n{','void pr_record_faint(pr_battle *bt,int side,uint32_t id,int turn)\n{\n    pr_rng_frac(&bt->'+field+');')
fault('battle-core/src/phases.c','wrong-player-side','pr_record_faint(bt,0,p->id,bt->turn);','pr_record_faint(bt,1,p->id,bt->turn);')
fault('battle-core/src/phases.c','omit-enemy-log','pr_record_faint(bt,1,p->id,bt->turn);','/* missing enemy faint */')
fault('ai-core/src/ai.c','tera-tail-only','return !fainted&&!unknown;','for(int i=0;i<bt->n_faint_history[1];i++)if(bt->faint_history[1][i].id==p->id)return false;return true;')
fault('game-core/src/gc_battle.c','retain-next-encounter','    memset(bt, 0, sizeof *bt);','    uint32_t previous_faints=bt->faint_history_total[0];\n    memset(bt, 0, sizeof *bt);bt->faint_history_total[0]=previous_faints;','game')
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(root/p)for p in ['battle-core/include','battle-core/gen/out','battle-core/src','battle-core/tests','game-core/include','game-core/src','run-core/include','ai-core/include','ai-core/gen/out','ai-core/src','reward-core/include','reward-core/gen/out']]]
with tempfile.TemporaryDirectory(prefix='faint-history-')as directory:
 temp=Path(directory)
 for name,file,source,kind in faults:
  src,obj,binary=temp/(name+'.c'),temp/(name+'.o'),temp/name;src.write_text(source)
  main=gc/'tests/faint_history_host_test.c'if kind=='game'else bc/('tests/faint-history-guards-test.c'if kind=='guard'else'tests/faint-history-test.c');objects=[]
  if file=='ai-core/src/ai.c':
   main=temp/'main.c';main.write_text((bc/'tests/faint-history-test.c').read_text().replace('#include "../../ai-core/src/ai.c"',source))
  else:subprocess.run(['cc',*flags,'-c',src,'-o',obj],check=True,capture_output=True);objects=[obj]
  libs=[gc/'build'/p for p in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]if kind=='game'else[ac/'build/libai.a',bc/'build/libprcore.a']
  subprocess.run(['cc',*flags,main,*objects,*libs,'-lm','-o',binary],check=True,capture_output=True)
  result=subprocess.run([binary],capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout,result.stderr);print(name,'caught',flush=True)
 print('11 compiled faint history / RNG / AI / encounter controls caught')
