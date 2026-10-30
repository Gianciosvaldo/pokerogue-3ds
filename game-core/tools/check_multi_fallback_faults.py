#!/usr/bin/env python3
"""Prove fallback, legacy migration and strike-observation failures are detected."""
from pathlib import Path
import argparse,os,subprocess,tempfile
parser=argparse.ArgumentParser();parser.add_argument('--build',type=Path);args=parser.parse_args()
root=Path(__file__).resolve().parents[2];os.chdir(root)
build=(args.build or root/'game-core/build').resolve()
folder=Path(tempfile.mkdtemp(prefix='multi-fallback-faults-'))
flags=['-Igame-core/include','-Igame-core/src','-Irun-core/include','-Iai-core/include','-Ireward-core/include','-Ireward-core/gen/out','-Ibattle-core/include','-Ibattle-core/gen/out','-O1','-std=c99','-ffp-contract=off']
libs=[str(build/p) for p in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]
faults=[('fallback','game-core/src/gc_stubs.c','const int status = gc_multi_add(run, party);','const int status = PR_REWARD_HELD_UNSUPPORTED;'),('migration-omitted','game-core/src/gc_save.c','if(multi_storage.bad)repair_legacy_multi_lenses(run);','if(multi_storage.bad){}'),('migration-unconditional','game-core/src/gc_save.c','if(multi_storage.bad)repair_legacy_multi_lenses(run);','if(true)repair_legacy_multi_lenses(run);'),('migration-opaque','game-core/src/gc_save.c','(b->arg!=-1&&b->arg!=0)||b->detached','b->detached'),('migration-uncapped','game-core/src/gc_save.c','a->stack=2;','a->stack=(int16_t)total;'),('full-later-hit','battle-core/src/modifiers.c','else if (src->hit_count - src->hits_left != stacks + 1) *mult *= 0.25;','else if (src->hit_count - src->hits_left != stacks + 1) *mult *= 1;')]
with (folder/'faults.log').open('w') as log:
 for name,path,old,new in faults:
  source=Path(path).read_text();assert source.count(old)==1,(name,source.count(old));f=folder/'fault.c';f.write_text(source.replace(old,new));obj=folder/'fault.o';exe=folder/'fault'
  subprocess.run(['cc',*flags,'-c',str(f),'-o',str(obj)],stdout=log,stderr=log,check=True)
  macro='-DMULTI_FALLBACK_LEGACY="'+str(root/'game-core/tests/fixtures/multi-fallback-legacy.save')+'"'
  subprocess.run(['cc',*flags,macro,'game-core/tests/multi_acquire_host_test.c',str(obj),*libs,'-lm','-o',str(exe)],stdout=log,stderr=log,check=True)
  r=subprocess.run([str(exe)],stdout=log,stderr=log)
  if r.returncode==0:raise RuntimeError('missed fault '+name)
  log.write('CAUGHT '+name+'\n');log.flush();exe.unlink()
 print('Six compiled Multi Lens fallback, migration and damage faults rejected; logs: '+str(folder/'faults.log'))
