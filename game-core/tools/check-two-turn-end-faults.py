from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2];
import os
os.chdir(root)
 src=(root/'game-core/src/gc_battle.c').read_text();out=root/'work/faults';out.mkdir(parents=True,exist_ok=True)
faults={
'no-cleanup':('if(cancel_encounter_charges(run))charge_cleanup_snapshot(run);','if(false)charge_cleanup_snapshot(run);'),
'clear-all-queues':('if(!unfinished_charge_move(p->move_queue[q].move))','if(false)'),
'only-first-slot':('i<run->n_party;i++){','i<1;i++){'),
'omit-dive':('t->type==BATTLER_TAG_TYPE_UNDERWATER','false'),
'no-reveal':('if(cancel_encounter_charges(run))charge_cleanup_snapshot(run);','(void)cancel_encounter_charges(run);'),
'refund-pp':('p->n_move_queue=(uint8_t)keep;','p->n_move_queue=(uint8_t)keep;p->pp_used[0]=0;'),
'clear-history':('p->n_move_queue=(uint8_t)keep;','p->n_move_queue=(uint8_t)keep;pr_reset_summon_data(p);')}
incs=['-Igame-core/include','-Irun-core/include','-Iai-core/include','-Ireward-core/include','-Ireward-core/gen/out','-Ibattle-core/include','-Ibattle-core/gen/out','-Igame-core/src']
libs=['game-core/build/libgc.a','game-core/build/reward-core/libreward.a','game-core/build/ai-core/libai.a','game-core/build/run-core/librun.a','game-core/build/run-core/battle-core/libprcore.a']
for name,(a,b) in faults.items():
 # Restrict replacements to the newly added policy, never unrelated source loops.
 start=src.index('static bool cancel_encounter_charges');end=src.index('    for (int i = 0; i < run->n_party; i++) pr_reset_turn_data',start)
 part=src[start:end];assert part.count(a)==1,(name,part.count(a))
 path=out/(name+'.c');path.write_text(src[:start]+part.replace(a,b)+src[end:])
 obj=out/(name+'.o');exe=out/name
 subprocess.run(['cc',*incs,'-O1','-std=c99','-ffp-contract=off','-c',str(path),'-o',str(obj)],check=True)
 subprocess.run(['cc',*incs,'-O1','game-core/tests/two_turn_end_host_test.c',str(obj),*libs,'-Wl,--wrap=gc_battle_end_cleanup','-lm','-o',str(exe)],check=True)
 p=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 (out/(name+'.log')).write_bytes(p.stdout);assert p.returncode==1,(name,p.returncode)
 print(name,'rejected:',p.stdout.decode().strip(),flush=True);exe.unlink()
print('7 compiled faults rejected')
