#!/usr/bin/env python3
"""First Challenge encounter must supply actual rule context before passive checks."""
from pathlib import Path
import subprocess,tempfile,os,sys
r=Path(__file__).resolve().parents[2];g=r/'game-core';runner=g/'build/sync_nature_host_test';subprocess.run([runner],check=True)
s=(g/'src/gc_encounter.c').read_text();a=s.index('static double scene_double_chance(');z=s.index('void gc_new_battle(',a);part=s[a:z];old='    bt->challenge_flags_known = !run->run.has_challenges || run->run.challenge_flags != 0;';assert part.count(old)==1;s=s[:a]+part.replace(old,'    /* missing first-encounter rule context */',1)+s[z:]
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(r/p) for p in ['battle-core/include','battle-core/gen/out','run-core/include','game-core/include','game-core/src','ai-core/include','reward-core/include','reward-core/gen/out']]]
with tempfile.TemporaryDirectory(prefix='sync-challenge-guard-',dir=r/'work') as tmp:
 d=Path(tmp);src=d/'gc_encounter.c';src.write_text(s);obj=d/'fault.o';exe=d/'fault'
 subprocess.run(['gcc',*flags,'-c',src,'-o',obj],capture_output=True,check=True)
 subprocess.run(['gcc',*flags,g/'tests/sync_nature_host_test.c',obj,g/'build/libgc.a',g/'build/reward-core/libreward.a',g/'build/ai-core/libai.a',g/'build/run-core/librun.a',g/'build/run-core/battle-core/libprcore.a','-Wl,--wrap=pr_run_new_battle','-Wl,--wrap=gc_mon_new_enemy','-lm','-o',exe],capture_output=True,check=True)
 p=subprocess.run([exe],capture_output=True);assert p.returncode==1,(p.returncode,p.stdout[-300:],p.stderr[-300:]);print('missing first-Challenge context caught')
 if len(sys.argv)>1:
  data=Path(sys.argv[1]).read_bytes();a=data.index(b'\nC ')+1;z=data.index(b'\n',a);row=data[a:z].split();row[7]=b'0';bad=d/'bad.txt';bad.write_bytes(data[:a]+b' '.join(row)+data[z:]);e=os.environ.copy();assert e.get('TURNTEST_LOCALES');p=subprocess.run([g/'build/sync_nature_test',bad],env=e,capture_output=True);assert p.returncode==2;print('malformed known-empty Challenge mode rejected')
