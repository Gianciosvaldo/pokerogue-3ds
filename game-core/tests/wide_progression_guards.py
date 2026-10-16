#!/usr/bin/env python3
"""Compile real runtime faults and reject malformed native captures. Never edits production files."""
import pathlib, subprocess, sys, tempfile
root = pathlib.Path(__file__).resolve().parents[2]
trace = pathlib.Path(sys.argv[1]).resolve()
runner = root / 'game-core/build/wide_progression_diff_test'
data = trace.read_text()
assert subprocess.run([runner, trace], check=False).returncode == 0
cc = ['gcc', '-O2', '-g', '-std=c99', '-ffp-contract=off']
for d in ['game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']:
    cc += ['-I'+str(root/d)]
libs = [root/'game-core/build'/p for p in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]
faults = [
 ('exp200','game-core/src/gc_mon.c','m->b.level < INT32_MAX &&','m->b.level < 200 &&'),
 ('candy65535','reward-core/src/candy.c','p->level += level_count;','p->level += level_count; if(p->level>65535)p->level=65535;'),
 ('oldlevel16','reward-core/src/candy.c','const int32_t old_level = p->level;','const uint16_t old_level = p->level;'),
 ('bridge255','game-core/src/gc_reward.c','b->level = r->level;','b->level = r->level > 255 ? 255 : r->level;'),
 ('bridgeExp','game-core/src/gc_reward.c','m->exp = r->exp;','m->exp = gc_level_total_exp(255, gc_species(b->species)->growth_rate);'),
 ('friendship','reward-core/src/candy.c','int friendship = p->friendship + 6;','int friendship = p->friendship + 5;'),
]
# The request comparator directly observes the wide old/new fields. Host tests
# additionally check reward-core's request metadata before the game bridge.
with tempfile.TemporaryDirectory(prefix='wide-progression-',dir=root/'work') as tmp:
    tmp=pathlib.Path(tmp)
    for name, file, before, after in faults:
        text=(root/file).read_text();assert text.count(before)==1,(name,text.count(before))
        source=tmp/(name+'.c');source.write_text(text.replace(before,after))
        obj=tmp/(name+'.o');binary=tmp/name
        subprocess.run(cc+['-c',str(source),'-o',str(obj)],check=True,capture_output=True)
        # Reward-level-event fields are not consumed by the game bridge; use
        # the host's direct metadata assertions for that particular fault.
        test='wide_progression_host_test.c' if name=='oldlevel16' else 'wide_progression_diff_test.c'
        subprocess.run(cc+[str(root/'game-core/tests'/test),str(obj),*map(str,libs),'-lm','-o',str(binary)],check=True,capture_output=True)
        result=subprocess.run([binary]+([] if name=='oldlevel16' else [trace]),capture_output=True)
        assert result.returncode>0,(name,result.stdout,result.stderr)
        print('caught runtime fault',name,flush=True)
    lines=data.splitlines(keepends=True)
    malformed=[data.rsplit('WIDE_PROGRESSION_COMPLETE',1)[0],data+'E\n',data.replace('WIDE_PROGRESSION 1 ','WIDE_PROGRESSION 2 ',1),data.replace('C 0 0 ','C 1 0 ',1),data.replace('R 1 ','R NaN ',1),data.replace(lines[1], ' '.join(lines[1].split()[:5]+['7']+lines[1].split()[6:])+'\n', 1)]
    for marker in ['P ','A ','RESULT ','LEVELS ','L ','RO ','BO ','E\n']:
        j=next(i for i,l in enumerate(lines) if l.startswith(marker));malformed.append(''.join(lines[:j]+lines[j+1:]))
    for i,payload in enumerate(malformed):
        source=tmp/'bad.txt';source.write_text(payload)
        result=subprocess.run([runner,source],capture_output=True)
        assert result.returncode>0,('malformed',i)
    print(f'{len(faults)} compiled faults caught; {len(malformed)} malformed captures rejected')
