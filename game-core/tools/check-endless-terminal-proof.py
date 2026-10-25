#!/usr/bin/env python3
"""Compile actual battle-end controller faults and reject damaged native captures."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];g=r/'game-core';trace=Path(sys.argv[1]).resolve();runner=g/'build/endless_terminal_test';host=g/'build/endless_terminal_host_test'
subprocess.run([runner,trace],check=True);subprocess.run([host],check=True)
s=(g/'src/gc_phases.c').read_text();a=s.index('static void battle_end(');z=s.index('/* ---- TrainerVictoryPhase',a);part=s[a:z]
block=part[part.index('    /* Endless graceful end'):part.index('    post_battle_money')]
faults=[]
def fault(name,old,new):
 assert part.count(old)==1,(name,part.count(old));faults.append((name,s[:a]+part.replace(old,new,1)+s[z:]))
fault('missing-terminal-clear',block,'')
fault('late-boundary','run->wave >= 5850','run->wave > 5850')
fault('early-boundary','run->wave >= 5850','run->wave >= 5849')
fault('missing-spliced-mode',' || run->run.mode == GAME_MODES_SPLICED_ENDLESS','')
fault('all-modes-terminal','(run->run.mode == GAME_MODES_ENDLESS || run->run.mode == GAME_MODES_SPLICED_ENDLESS)','true')
fault('stale-future-rewards','        gc_rq_clear(run);','        /* left future phases queued */')
fault('clear-after-abilities',block+'    post_battle_money(run, victory);','    post_battle_money(run, victory);\n'+block)
fault('lost-duplicate-victory','victory |= q->q[l][k].a','victory = q->q[l][k].a')
fault('ignored-nested-victory','l < q->n_levels','l < 1')
fault('wrong-escape-reward-gate','if (!victory && !gc_rq_has(run, RP_SELECT_MODIFIER))','if (!victory && !bt->successful_run && !gc_rq_has(run, RP_SELECT_MODIFIER))')
fault('missing-defeated-enemy-reward','if (bt->enemy_faints > 0)','if (false)')
fault('duplicated-reward','if (!victory && !gc_rq_has(run, RP_SELECT_MODIFIER))','if (!victory)')
fault('lost-callback-victory','post_battle_money(run, victory);','post_battle_money(run, false);')
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(r/p) for p in ['battle-core/include','battle-core/gen/out','battle-core/build','run-core/include','game-core/include','game-core/src','ai-core/include','reward-core/include','reward-core/gen/out']]]
with tempfile.TemporaryDirectory(prefix='terminal-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,changed in faults:
  src=d/'gc_phases.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault';subprocess.run(['gcc',*flags,'-c',src,'-o',obj],capture_output=True,check=True)
  subprocess.run(['gcc',*flags,g/'tests/endless_terminal_test.c',obj,g/'build/libgc.a',g/'build/reward-core/libreward.a',g/'build/ai-core/libai.a',g/'build/run-core/librun.a',g/'build/run-core/battle-core/libprcore.a','-Wl,--wrap=pr_prepare_post_battle_money','-lm','-o',exe],capture_output=True,check=True)
  p=subprocess.run([exe,trace],capture_output=True);assert p.returncode==1,(name,p.returncode,p.stdout[-300:],p.stderr[-300:]);print(name,'caught',flush=True)
 data=trace.read_bytes();bad=d/'bad.txt';count=0
 def reject(name,content):
  global count
  bad.write_bytes(content);p=subprocess.run([runner,bad],capture_output=True);assert p.returncode==2,(name,p.returncode);count+=1;print(name,'rejected',flush=True)
 reject('version',data.replace(b'ENDLESS_TERMINAL 2 ',b'ENDLESS_TERMINAL 3 ',1));reject('count',data.replace(b'2560 ',b'2559 ',1));reject('trailing',data+b'JUNK\n');reject('footer',data[:data.rfind(b'\n',0,-1)+1])
 def rowmut(key,col,value):
  a=data.index(b'\n'+key)+1;z=data.index(b'\n',a);row=data[a:z].split();row[col]=value;return data[:a]+b' '.join(row)+data[z:]
 for col,value in [(1,b'0.5'),(2,b'5'),(3,b'5852'),(4,b'2'),(5,b'2'),(6,b'2'),(7,b'4'),(8,b'2'),(9,b'2'),(2,b'1')]:reject('case-inventory',rowmut(b'C ',col,value))
 reject('unknown-phase',rowmut(b'AP ',3,b'MissingPhase'));reject('bad-prefix-flag',rowmut(b'AP ',4,b'2'))
 print(f'{len(faults)} compiled faults caught; {count} malformed captures rejected',flush=True)
