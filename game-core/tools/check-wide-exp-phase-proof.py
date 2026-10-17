#!/usr/bin/env python3
"""Compile actual phase/text/app faults against a complete native capture."""
from pathlib import Path
import argparse,os,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);a=p.parse_args();a.trace=a.trace.resolve();r=Path(__file__).resolve().parents[2];env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
exe=r/'game-core/build/wide_exp_phase_diff_test';assert subprocess.run([exe,a.trace],env=env).returncode==0
faults=[
 ('narrow-text','game-core/src/gc_phases.c','pr_arg_int64("exp", (int64_t)exp)','pr_arg_int("exp", (int32_t)exp)'),
 ('narrow-event','game-core/src/gc_phases.c','ev->exp_amount = exp;','ev->exp_amount=(int32_t)exp;'),
 ('missing-before','game-core/src/gc_phases.c','ev->exp_before = before;','ev->exp_before=0;'),
 ('wrong-before','game-core/src/gc_phases.c','ev->exp_before = before;','ev->exp_before=m->exp;'),
 ('missing-floor','game-core/src/gc_phases.c','floor(gc_apply_exp_boosters(run, p->d))','gc_apply_exp_boosters(run, p->d)'),
 ('narrow-level','game-core/src/gc_phases.c','ev->c = last;','ev->c = (uint8_t)last;'),
 ('high-word','battle-core/src/events.c','a.fusion_v = (uint32_t)(bits >> 32);','a.fusion_v=0;'),
 ('booster-order','game-core/src/gc_items.c','for (int i = 0; i < run->n_items; i++) {','for (int i = run->n_items-1; i >= 0; i--) {'),
 ('app-subtract','game/source/run_exp.h','before.exp=pending[i].exp_before;','before.exp=actual->exp-pending[i].exp_amount;'),
 ('app-last','game/source/run_exp.h','    break;','    (void)0;'),
]
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/d) for d in ['game-core/include','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
build=r/'game-core/build';libs=[build/'libgc.a',build/'reward-core/libreward.a',build/'ai-core/libai.a',build/'run-core/librun.a',build/'run-core/battle-core/libprcore.a']
with tempfile.TemporaryDirectory(prefix='wide-exp-guards-',dir=r/'work') as tmp:
 tmp=Path(tmp)
 for name,file,old,new in faults:
  s=(r/file).read_text()
  if name=='booster-order':
   start=s.index('double gc_apply_exp_boosters(');stop=s.index('/* One gc_item',start);before=s[:start];after=s[stop:];s=s[start:stop]
  else:before='';after=''
  assert s.count(old)==1,(name,s.count(old));src=tmp/(name+('.h' if file.endswith('.h') else '.c'));src.write_text(before+s.replace(old,new)+after)
  test=(r/'game-core/tests/wide_exp_phase_diff_test.c').read_text().replace('#include "../src/internal.h"','#include "'+str(r/'game-core/src/internal.h')+'"').replace('#include "../../game/source/run_exp.h"','#include "'+str(src if file.endswith('.h') else r/'game/source/run_exp.h')+'"');test_path=tmp/'test.c';test_path.write_text(test);objects=[]
  if not file.endswith('.h'):
   obj=tmp/'fault.o';subprocess.run(['gcc',*flags,'-I'+str(r/'game-core/src'),'-c',src,'-o',obj],check=True,capture_output=True);objects=[obj]
  binary=tmp/name;subprocess.run(['gcc',*flags,test_path,*objects,*libs,'-lm','-o',binary],check=True,capture_output=True)
  result=subprocess.run([binary,a.trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-400:],result.stderr[-400:]);print(name,'caught',flush=True)
 rows=a.trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'footer':rows[:-1]+['WIDE_EXP_PHASE_COMPLETE 1'],'version':[rows[0].replace(' 3 ',' 4 ',1)]+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('op','C ',2,'2'),('mode','C ',3,'3'),('id','C ',5,'4294967296'),('level','C ',8,'-1'),('amount','C ',10,'-1'),('stack','C ',12,'100'),('order','C ',15,'3'),('queue','L ',1,'2')]:
  index=next(i for i,row in enumerate(rows) if row.startswith(prefix));v=rows[index].split();v[col]=value;changed=rows.copy();changed[index]=' '.join(v);bad[name]=changed
 for prefix in ('T ','L ','RO ','BO '):
  index=next(i for i,row in enumerate(rows) if row.startswith(prefix));bad['missing-'+prefix.strip()]=rows[:index]+rows[index+1:]
 for name,lines in bad.items():
  path=tmp/'bad.txt';path.write_text('\n'.join(lines)+'\n');result=subprocess.run([exe,path],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode,result.stderr[-300:]);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')
