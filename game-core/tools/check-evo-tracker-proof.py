#!/usr/bin/env python3
"""Compile actual owner faults and require the committed native matrix to reject them."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2];gc=root/'game-core';bc=root/'battle-core'
fixture=gc/'tests/fixtures/evo-tracker.native.txt';runner=gc/'build/evo_tracker_host_test'
subprocess.run([runner,fixture],check=True)
libs=[gc/'build/libgc.a',gc/'build/reward-core/libreward.a',gc/'build/ai-core/libai.a',gc/'build/run-core/librun.a',gc/'build/run-core/battle-core/libprcore.a']
incs=[gc/'include',gc/'src',root/'run-core/include',root/'ai-core/include',root/'reward-core/include',root/'reward-core/gen/out',bc/'include',bc/'gen/out']
faults=[]
def fault(file,name,old,new):
 s=(root/file).read_text();assert s.count(old)==1,(name,s.count(old));faults.append((name,s.replace(old,new)))
fault('reward-core/src/money_item.c','miss-fusion','ctx->party[j].species == SPECIES_ID_GIMMIGHOUL || ctx->party[j].fusion_species == SPECIES_ID_GIMMIGHOUL','ctx->party[j].species == SPECIES_ID_GIMMIGHOUL')
fault('reward-core/src/money_item.c','one-coin-always','(uint8_t)fmin(floor(multiplier), 3)','1')
fault('game-core/src/gc_mon.c','use-stack-count-as-treasure','treasure += (!strcmp','treasure += it->stack * ((!strcmp')
# Close the additional parentheses in this deliberately compiled arithmetic fault.
n,s=faults[-1];faults[-1]=(n,s.replace('|| !strcmp(it->key, "AMULET_COIN") || !strcmp(it->key, "GOLDEN_POKEBALL");','|| !strcmp(it->key, "AMULET_COIN") || !strcmp(it->key, "GOLDEN_POKEBALL"));'))
fault('game-core/src/gc_mon.c','official-instead-of-pinned-threshold','+ treasure >= c->v[0]','+ treasure >= 999')
fault('game-core/src/gc_mon.c','leave-tracker-after-evolution','if (pre_species == SPECIES_ID_GIMMIGHOUL) {','if (false && pre_species == SPECIES_ID_GIMMIGHOUL) {')
fault('game-core/src/gc_reward.c','overfill-instead-of-fallback','if (found >= 0 && run->items[found].stack + result.tracker_stacks[k] > 999)','if (false && found >= 0 && run->items[found].stack + result.tracker_stacks[k] > 999)')
fault('game-core/src/gc_items.c','transferable-tracker','m->species[0] = SPECIES_ID_GIMMIGHOUL; m->is_transferable = false;','m->species[0] = SPECIES_ID_GIMMIGHOUL; m->is_transferable = true;')
with tempfile.TemporaryDirectory(prefix='evo-tracker-proof-') as d:
 d=Path(d)
 for name,source in faults:
  src=d/(name+'.c');obj=d/(name+'.o');exe=d/name;src.write_text(source)
  subprocess.run(['cc','-O1','-std=c99',*[f'-I{x}' for x in incs],'-c',str(src),'-o',str(obj)],check=True)
  subprocess.run(['cc','-O1','-std=c99',*[f'-I{x}' for x in incs],str(gc/'tests/evo_tracker_host_test.c'),str(obj),*map(str,libs),'-lm','-o',str(exe)],check=True)
  p=subprocess.run([exe,fixture],capture_output=True,text=True);assert p.returncode==1,(name,p.returncode,p.stdout,p.stderr);print('caught compiled fault:',name,flush=True)
 text=fixture.read_text()
 malformed=[text.rsplit('END',1)[0],text+'EXTRA\n',text.replace('C 0 ','C 1 ',1),text.replace('EVOTRACKER 1 576','EVOTRACKER 1 575',1),text.replace('C 0 0 ','C 0 3 ',1),text.replace('I EVOLUTION_TRACKER_GIMMIGHOUL 0 1 0 1','I EVOLUTION_TRACKER_GIMMIGHOUL 0 0 0 1',1),text.replace('A 0\n','A 25\n',1),text.replace('M 151 -1 0','M 0 -1 0',1)]
 for i,bad in enumerate(malformed):
  assert bad!=text
  path=d/f'bad-{i}.txt';path.write_text(bad);p=subprocess.run([runner,path],capture_output=True);assert p.returncode!=0,('malformed accepted',i)
 print(f'EvoTracker controls: {len(faults)} compiled faults, {len(malformed)} malformed inputs rejected')
