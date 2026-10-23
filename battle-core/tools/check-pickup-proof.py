#!/usr/bin/env python3
"""Require the native capture to detect compiled faults and malformed input."""
from pathlib import Path
import os, subprocess, sys, tempfile

root = Path(__file__).resolve().parents[2]
trace = Path(sys.argv[1]).resolve()
env = os.environ.copy()
assert env.get('TURNTEST_LOCALES')
runner = root / 'battle-core/build/pickup-test'
subprocess.run([runner, trace], env=env, check=True)
source = (root / 'battle-core/src/pickup.c').read_text()
faults = [
    ('simulation', 'if(simulated||!victory||bt->post_battle_loot<=0)return false;', 'if(!victory||bt->post_battle_loot<=0)return false;'),
    ('victory', 'if(simulated||!victory||bt->post_battle_loot<=0)return false;', 'if(simulated||bt->post_battle_loot<=0)return false;'),
    ('single-item-rng', 'int i=n==1?0:pr_rng_seed_int(&bt->rng,n,0);', 'if(n==1)pr_rng_seed_int(&bt->rng,2,0);int i=n==1?0:pr_rng_seed_int(&bt->rng,n,0);'),
    ('shared-cache', 'if(!bt->pickup_cached_ref&&!select_stock(bt))return false;', 'if(!select_stock(bt))return false;'),
    ('quantity', 'pr_try_transfer_held_item(bt,e,side,index,p,1,false,false)', 'pr_try_transfer_held_item(bt,e,side,index,p,2,false,false)'),
    ('whole-loot-entry', 'if(n>0){if(found<0)', 'if(n>0&&stock->stack_count==0){if(found<0)'),
    ('cache-clear', 'bt->pickup_cached_ref=0;', '/* deliberately retain cache */'),
    ('missing-loot-splice', 'if(found<0)found=n-1;', 'if(found<0)return false;'),
]
flags = ['-O2', '-std=c99', '-ffp-contract=off', '-I'+str(root/'battle-core/include'), '-I'+str(root/'battle-core/gen/out')]
with tempfile.TemporaryDirectory(prefix='pickup-guards-') as tmp:
    tmp = Path(tmp)
    for name, old, new in faults:
        assert source.count(old) == 1, name
        src, obj, binary = tmp/(name+'.c'), tmp/(name+'.o'), tmp/name
        src.write_text(source.replace(old, new))
        subprocess.run(['gcc', *flags, '-c', src, '-o', obj], check=True, capture_output=True)
        subprocess.run(['gcc', *flags, root/'battle-core/tests/pickup-test.c', obj, root/'battle-core/build/libprcore.a', '-lm', '-o', binary], check=True, capture_output=True)
        fault_env = env.copy(); fault_env['PICKUP_FAIL_FAST'] = '1'
        r = subprocess.run([binary, trace], env=fault_env, capture_output=True)
        assert r.returncode == 1, (name, r.returncode, r.stdout, r.stderr)
        print(name, 'caught', flush=True)
    rows = trace.read_text().splitlines()
    malformed = {'missing-footer': rows[:-1], 'extra-tail': rows+['JUNK'], 'wrong-version': ['PICKUP 3 4400']+rows[1:], 'wrong-count': ['PICKUP 2 4399']+rows[1:]}
    for name, prefix, col, value in [
        ('fractional-case','C ',1,'0.5'), ('mode','C ',3,'4'),
        ('unknown-item','M ',2,'UNKNOWN'), ('negative-stock','M ',5,'-1'),
        ('invalid-owner','M ',4,'4'), ('invalid-id','MON ',1,'4294967296'),
        ('nan-actor','A ',2,'nan'), ('invalid-cache','IC ',1,'97'),
    ]:
        i = next(i for i, row in enumerate(rows) if row.startswith(prefix))
        fields = rows[i].split(); fields[col] = value
        bad = rows.copy(); bad[i] = ' '.join(fields); malformed[name] = bad
    for name, content in malformed.items():
        path = tmp/'bad.txt'; path.write_text('\n'.join(content)+'\n')
        r = subprocess.run([runner, path], env=env, capture_output=True)
        assert r.returncode == 2, (name, r.returncode, r.stdout, r.stderr)
        print(name, 'rejected', flush=True)
    print(f'{len(faults)} compiled faults caught; {len(malformed)} malformed captures rejected')
