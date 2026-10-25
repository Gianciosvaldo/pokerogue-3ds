#!/usr/bin/env python3
"""Require the native inventory/owner proof to detect capacity regression faults."""
import subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
gc=root/'game-core'
fixture=gc/'tests/fixtures/inventory-capacity.native.txt'
runner=gc/'build/inventory_capacity_host_test'
subprocess.run([runner,fixture],cwd=gc,check=True)
faults=[]
def fault(file,name,old,new):
 s=(gc/'src'/file).read_text();assert s.count(old)==1,(name,s.count(old))
 faults.append((name,s.replace(old,new)))
fault('gc_items.c','battle-truncates-at-96','bt->n_mods[0] < PR_MAX_MODIFIERS','bt->n_mods[0] < 96')
fault('gc_reward.c','reward-truncates-at-256','c->n_items < PR_REWARD_MAX_OWNED','c->n_items < 256')
save=(gc/'src/gc_save.c').read_text()
a=save.index('size_t gc_run_save(');b=save.index('bool gc_run_load(',a)
part=save[a:b];old='writer w = {(uint8_t *)buf, 0, cap, false};';assert part.count(old)==1
faults.append(('old-save-buffer-ceiling',save[:a]+part.replace(old,'writer w = {(uint8_t *)buf, 0, cap<128*1024?cap:128*1024, false};')+save[b:]))
fault('gc_items.c','silent-full-add','if (run->n_items >= PR_MAX_MODIFIERS) {pr_note_unported(&run->bt,PR_FEAT_ENGINE_LIMIT);return;}','if (run->n_items >= PR_MAX_MODIFIERS) return;')
fault('gc_items.c','silent-invalid-count','        pr_note_unported(bt,PR_FEAT_ENGINE_LIMIT);return;','        /* wrong: leave a partial projection */')
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(root/p) for p in ['game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[gc/'build'/p for p in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]
with tempfile.TemporaryDirectory(prefix='inventory-faults-') as temp:
 temp=Path(temp)
 for name,source in faults:
  src,obj,binary=temp/(name+'.c'),temp/(name+'.o'),temp/name
  src.write_text(source)
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,gc/'tests/inventory_capacity_host_test.c',obj,*libs,'-lm','-o',binary],check=True,capture_output=True)
  result=subprocess.run([binary,fixture],cwd=gc,capture_output=True)
  assert result.returncode==1,(name,result.returncode,result.stdout,result.stderr)
  print(name,'caught',flush=True)
print('All five compiled inventory capacity faults detected')
