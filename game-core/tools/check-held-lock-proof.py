#!/usr/bin/env python3
"""Retain constructor and legacy-load faults at actual production boundaries."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2];gc=root/'game-core';bc=root/'battle-core';fixture=gc/'tests/fixtures/held-lock.native.txt';runner=gc/'build/held_lock_host_test'
subprocess.run([runner,fixture],check=True)
libs=[gc/'build/libgc.a',gc/'build/reward-core/libreward.a',gc/'build/ai-core/libai.a',gc/'build/run-core/librun.a',gc/'build/run-core/battle-core/libprcore.a']
incs=[gc/'include',gc/'src',root/'run-core/include',root/'ai-core/include',root/'reward-core/include',root/'reward-core/gen/out',bc/'include',bc/'gen/out']
faults=[]
def fault(file,name,old,new):
 s=(gc/file).read_text();assert s.count(old)==1,(name,s.count(old));faults.append((name,s.replace(old,new)))
fault('src/gc_items.c','new-slot-unlocked','it->non_transferable = gc_item_key_locked(key);','it->non_transferable = false;')
fault('src/gc_save.c','legacy-load-unlocked','lock != 0 || gc_item_key_locked(items[i].key)','lock != 0')
fault('src/gc_items.c','bridge-unlocked','!it->non_transferable && !gc_item_key_locked(it->key)','!it->non_transferable')
fault('src/gc_items.c','transfer-ui-unlocked','|| gc_item_key_locked(src.key) || src.stack <= 0','|| src.stack <= 0')
fault('src/gc_items.c','discard-ui-unlocked','|| gc_item_key_locked(it->key) || it->stack <= 0','|| it->stack <= 0')
fault('src/gc_items.c','miss-macho-brace','|| !strcmp(key, "MYSTERY_ENCOUNTER_MACHO_BRACE")','|| false')
with tempfile.TemporaryDirectory(prefix='held-lock-proof-') as d:
 d=Path(d)
 for name,s in faults:
  src=d/(name+'.c');obj=d/(name+'.o');exe=d/name;src.write_text(s)
  subprocess.run(['cc','-O1','-std=c99',*[f'-I{x}' for x in incs],'-c',str(src),'-o',str(obj)],check=True)
  subprocess.run(['cc','-O1','-std=c99',*[f'-I{x}' for x in incs],str(gc/'tests/held_lock_host_test.c'),str(obj),*map(str,libs),'-lm','-o',str(exe)],check=True)
  p=subprocess.run([exe,fixture],capture_output=True,text=True);assert p.returncode==1,(name,p.returncode,p.stdout,p.stderr);print('caught compiled fault:',name,flush=True)
 text=fixture.read_text()
 malformed=[text.rsplit('END',1)[0],text+'EXTRA\n',text.replace('C 0 ','C 1 ',1),text.replace('HELDLOCK 1 2632','HELDLOCK 1 2631',1),text.replace('C 0 FORM_CHANGE_ITEM 0 0 0 1 1 0 0','C 0 FORM_CHANGE_ITEM 0 4 0 1 1 0 0',1),text.replace('C 0 FORM_CHANGE_ITEM 0 0 0 1 1 0 0','C 0 FORM_CHANGE_ITEM 0 0 2 1 1 0 0',1),text.replace('C 0 FORM_CHANGE_ITEM 0 0 0 1 1 0 0','C 0 FORM_CHANGE_ITEM 0 0 0 2 1 0 0',1),text.replace('MYSTERY_ENCOUNTER_SHUCKLE_JUICE 10','MYSTERY_ENCOUNTER_SHUCKLE_JUICE 11',1)]
 for i,bad in enumerate(malformed):
  assert bad!=text
  path=d/f'bad-{i}.txt';path.write_text(bad);p=subprocess.run([runner,path],capture_output=True);assert p.returncode!=0,('malformed accepted',i)
 print(f'Held locks: {len(faults)} compiled faults, {len(malformed)} malformed inputs rejected')
