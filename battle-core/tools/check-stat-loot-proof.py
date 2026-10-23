#!/usr/bin/env python3
"""Native comparison must reject clone/matching faults and save class loss."""
from pathlib import Path
import os, subprocess, sys, tempfile

root=Path(__file__).resolve().parents[2]
trace,save_trace=map(lambda p:Path(p).resolve(),sys.argv[1:3])
env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=root/'battle-core/build/stat-loot-test'
host=root/'game-core/build/stat_loot_host_test'
subprocess.run([runner,trace],env=env,check=True)
subprocess.run([host,save_trace],check=True)
source=(root/'battle-core/src/item_transfer.c').read_text()
cut=source.index('bool pr_try_transfer_held_item(')
clone='if(copy.cls==PR_MOD_EVOLUTION_STAT_BOOSTER)copy.cls=PR_MOD_STAT_BOOSTER;'
faults=[
 ('preflight-clone',source[:cut].replace(clone,'/* omit clone */')+source[cut:]),
 ('apply-clone',source[:cut]+source[cut:].replace(clone,'/* omit clone */')),
 ('original-match',source.replace('same_type(m,&bt->mods[from_side][index])','same_type(m,&copy)')),
 ('stat-mask-match',source.replace('||m->stats_mask!=other->stats_mask','')),
 ('stat-cap',source.replace('return m->n_species<=4?1:-1;','return m->n_species<=4?2:-1;')),
]
mods=(root/'battle-core/src/modifiers.c').read_text()
old='default:\n            *value *= m->mult;'
assert mods.count(old)==1
faults.append(('base-clone-effective',mods.replace(old,'default:\n            if(pr_pokemon_species(p)->has_evolutions)*value *= m->mult;')))
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(root/'battle-core/include'),'-I'+str(root/'battle-core/gen/out')]
with tempfile.TemporaryDirectory(prefix='stat-loot-guards-') as tmp:
 tmp=Path(tmp)
 for name,changed in faults:
  assert changed!=source,name
  src,obj,binary=tmp/(name+'.c'),tmp/(name+'.o'),tmp/name
  src.write_text(changed)
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,root/'battle-core/tests/stat-loot-test.c',obj,root/'battle-core/build/libprcore.a','-lm','-o',binary],check=True,capture_output=True)
  fault_env=env.copy();fault_env['PICKUP_FAIL_FAST']='1'
  result=subprocess.run([binary,trace],env=fault_env,capture_output=True)
  assert result.returncode==1,(name,result.returncode,result.stdout,result.stderr)
  print(name,'caught',flush=True)
 gc_source=(root/'game-core/src/gc_save.c').read_text()
 host_flags=flags+['-I'+str(root/p) for p in ['game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out']]
 libs=[root/'game-core/build'/p for p in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]
 for name,old,new in [
  ('save-class','sv[i].eviolite_base = items[i].eviolite_base;','sv[i].eviolite_base = false;'),
  ('load-class','items[i].eviolite_base=base_class!=0;','items[i].eviolite_base=false;'),
 ]:
  assert gc_source.count(old)==1,name
  src,obj,binary=tmp/(name+'.c'),tmp/(name+'.o'),tmp/name
  src.write_text(gc_source.replace(old,new))
  subprocess.run(['gcc',*host_flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*host_flags,root/'game-core/tests/stat_loot_host_test.c',obj,*libs,'-lm','-o',binary],check=True,capture_output=True)
  result=subprocess.run([binary,save_trace],capture_output=True)
  assert result.returncode==1,(name,result.returncode,result.stdout,result.stderr)
  print(name,'caught',flush=True)
 rows=trace.read_text().splitlines()
 for name,content in [('missing-footer',rows[:-1]),('extra-tail',rows+['JUNK']),('bad-count',['STATLOOT 2 959']+rows[1:])]:
  path=tmp/'bad.txt';path.write_text('\n'.join(content)+'\n')
  result=subprocess.run([runner,path],env=env,capture_output=True)
  assert result.returncode==2,(name,result.returncode,result.stdout,result.stderr)
  print(name,'rejected',flush=True)
 print('8 compiled faults caught; 3 malformed captures rejected')
