#!/usr/bin/env python3
"""Retain every supported Endless wave; reject uncovered results and weakened captures."""
from pathlib import Path
import argparse
import copy
import gzip
import hashlib
import json
import math
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / 'run-core'
PIN = '8555c08c823b856cbec4eb99ca84ea52a955836d'
LOCALES = '270ed0a2938b2c3e2e34e7de940e84aecadf548b'
ASSETS = '87426a79611f9d212c4dc8af58e2834a05b93725'
PRODUCER = CORE / 'tests/endless-wave-sweep.producer.test.ts'
FIXTURE = CORE / 'tests/fixtures/endless-wave-sweep.native.json.gz'
SOURCE_FILES = ['src/game-mode.ts', 'src/battle.ts', 'src/battle-scene.ts', 'src/phases/battle-end-phase.ts']
KEYS = {'version', 'upstream', 'locales', 'assets', 'seed', 'producer_sha256', 'source_files', 'rows'}

def sha(data):
    return hashlib.sha256(data).hexdigest()

def unique(pairs):
    out = {}
    for key, value in pairs:
        assert key not in out, ('duplicate JSON key', key)
        out[key] = value
    return out

def validate(data):
    assert isinstance(data, dict) and set(data) == KEYS
    assert type(data['version']) is int and data['version'] == 1
    assert data['upstream'] == PIN and data['locales'] == LOCALES and data['assets'] == ASSETS
    assert type(data['seed']) is int and data['seed'] == 777
    assert data['producer_sha256'] == sha(PRODUCER.read_bytes())
    assert isinstance(data['source_files'], dict) and set(data['source_files']) == set(SOURCE_FILES)
    assert all(isinstance(v, str) and re.fullmatch('[0-9a-f]{64}', v) for v in data['source_files'].values())
    rows = data['rows']
    assert isinstance(rows, list) and len(rows) == 11700
    floats = {26, 27, 28, 31, 32, 33, 35, 36, 37, 40, 41, 42, 50}
    flags = {3, 9, 10, 11, 12, 13, 14, 15, 16, 38, 44, 47}
    for i, row in enumerate(rows):
        assert isinstance(row, str) and '\n' not in row
        fields = row.split()
        assert len(fields) == 51 and fields[0] == 'W', ('row width', i)
        assert fields[1:3] == [str(1 + i % 2), str(1 + i // 2)], ('missing/reordered wave', i)
        assert re.fullmatch('[A-Za-z0-9]{24}', fields[6])
        for k, field in enumerate(fields):
            if k in (0, 6):
                continue
            assert math.isfinite(float(field)), ('nonfinite observation', i, k)
            if k not in floats:
                assert re.fullmatch('-?[0-9]+', field), ('noninteger field', i, k)
            if k in flags:
                assert field in ('0', '1')
        assert int(fields[22]) in (0, 1 + i // 2)
        assert all(int(fields[k]) > 0 for k in (23, 24, 29, 43))
        assert 0 <= int(fields[49]) <= 3
    assert max(int(row.split()[29]) for row in rows) > 65535
    assert max(int(row.split()[23]) for row in rows) > 65535
    return ('\n'.join(rows) + '\n').encode()

def command(args):
    p = subprocess.run(list(map(str, args)), cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if p.returncode:
        raise RuntimeError(str(args) + '\n' + p.stdout[-4000:])
    return p.stdout

def pack(upstream, raw, output):
    def git(path, *args):
        return command(['git', '-C', path, *args]).strip()
    assert git(upstream, 'rev-parse', 'HEAD') == PIN
    assert git(upstream / 'locales', 'rev-parse', 'HEAD') == LOCALES
    assert git(upstream, 'ls-tree', 'HEAD', 'assets').split()[2] == ASSETS
    assert not git(upstream, 'diff', '--name-only', '--', 'src', 'test/framework', 'test/helpers')
    assert not git(upstream / 'locales', 'diff', '--name-only')
    data = dict(version=1, upstream=PIN, locales=LOCALES, assets=ASSETS, seed=777,
                producer_sha256=sha(PRODUCER.read_bytes()),
                source_files={path: sha((upstream / path).read_bytes()) for path in SOURCE_FILES},
                rows=raw.read_text().splitlines())
    validate(data)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(gzip.compress((json.dumps(data, separators=(',', ':')) + '\n').encode(), mtime=0))
    print('Packed 11,700 native rows with source and producer provenance')

def replay(exe, raw):
    p = subprocess.run([str(exe), str(raw)], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if p.returncode:
        raise RuntimeError(p.stdout[-4000:])
    assert re.search(r'^cases 11700$', p.stdout, re.M), p.stdout
    counts = re.findall(r'pass\s+(\d+)\s+fail\s+(\d+)\s+not covered\s+(\d+)', p.stdout)
    assert counts == [('175500', '0', '0')] + [('11700', '0', '0')] * 6, p.stdout
    return p.stdout

def check():
    data = json.loads(gzip.decompress(FIXTURE.read_bytes()), object_pairs_hook=unique)
    raw = validate(data)
    command(['make', '-C', CORE, 'build/wavetest'])
    with tempfile.TemporaryDirectory(prefix='endless-wave-sweep-') as td:
        temp = Path(td)
        cases = temp / 'native.txt'
        cases.write_bytes(raw)
        print(replay(CORE / 'build/wavetest', cases), end='', flush=True)
        def nonfinite(d):
            fields = d['rows'][0].split()
            fields[29] = 'NaN'
            d['rows'][0] = ' '.join(fields)
        edits = [
            ('missing wave', lambda d: d['rows'].pop()),
            ('duplicate wave', lambda d: d['rows'].__setitem__(1, d['rows'][0])),
            ('missing column', lambda d: d['rows'].__setitem__(0, ' '.join(d['rows'][0].split()[:-1]))),
            ('nonfinite observation', nonfinite),
            ('wrong pin', lambda d: d.__setitem__('upstream', '0' * 40)),
            ('boolean version', lambda d: d.__setitem__('version', True)),
            ('wrong producer', lambda d: d.__setitem__('producer_sha256', '0' * 64)),
            ('missing source', lambda d: d['source_files'].pop(SOURCE_FILES[0])),
        ]
        for label, edit in edits:
            changed = copy.deepcopy(data)
            edit(changed)
            try:
                validate(changed)
            except (AssertionError, ValueError):
                print('Rejected malformed capture:', label, flush=True)
            else:
                raise RuntimeError('Accepted malformed capture: ' + label)
        for column, label in [(23, 'wide cap'), (29, 'wide level'), (30, 'level RNG'), (43, 'seeded battle level')]:
            changed = copy.deepcopy(data)
            fields = changed['rows'][-1].split()
            fields[column] = str(int(fields[column]) + 1)
            changed['rows'][-1] = ' '.join(fields)
            cases.write_bytes(validate(changed))
            try:
                replay(CORE / 'build/wavetest', cases)
            except (AssertionError, RuntimeError):
                print('Rejected forged observation:', label, flush=True)
            else:
                raise RuntimeError('Accepted forged observation: ' + label)
        cases.write_bytes(raw)
        source = (CORE / 'src/wave.c').read_text()
        faults = [
            ('return ceil(base / 2) * 2 + 2;', 'return (uint16_t)(int)(ceil(base / 2) * 2 + 2);', 'narrow cap to 16 bits'),
            ('rand += pr_rng_seed_float(rng);', 'rand += 0;', 'omit ordinary level RNG draw'),
            ('return (int)(ret + offset);', 'return (uint16_t)(int)(ret + offset);', 'narrow boss level'),
            ('return pr_run_seed_offset_rng(run, wave << 3, &run->wave_seed);', 'return pr_run_seed_offset_rng(run, wave << 2, &run->wave_seed);', 'wrong battle seed offset'),
            ('offset = js_round(pr_rng_real_in_range(rng, -1, 1) * floor(lw / 10));', 'offset = 0;', 'omit boss RNG draw'),
            ('int pr_run_get_level_for_wave(pr_run *run, pr_rng *rng, int wave)\n{', 'int pr_run_get_level_for_wave(pr_run *run, pr_rng *rng, int wave)\n{\n run->unported |= 1u;', 'report comparison as uncovered'),
        ]
        cc = os.environ.get('CC', 'cc')
        flags = ['-O2', '-std=c99', '-ffp-contract=off', '-I' + str(CORE / 'include'),
                 '-I' + str(ROOT / 'battle-core/include'), '-I' + str(ROOT / 'battle-core/gen/out')]
        # The same unmodified object/link path must pass before any fault.
        for i, change in enumerate([None, *faults]):
            unit = source
            if change:
                old, new, label = change
                assert source.count(old) == 1, label
                unit = source.replace(old, new)
            path, obj, exe = temp / 'wave.c', temp / 'wave.o', temp / 'wave-test'
            path.write_text(unit)
            command([cc, *flags, '-c', path, '-o', obj])
            command([cc, *flags, CORE / 'tests/wavetest.c', obj, CORE / 'build/librun.a',
                     CORE / 'build/battle-core/libprcore.a', '-lm', '-o', exe])
            try:
                replay(exe, cases)
            except (AssertionError, RuntimeError):
                if not change:
                    raise
                print('Caught compiled fault:', label, flush=True)
            else:
                assert not change, ('undetected compiled fault', label)
        # Preserve the exploratory 16-bit ordinary-level probe. These valid waves
        # never reach that boundary on the ordinary branch, so it must survive;
        # do not count it as detected or claim that branch exceeds 65,535 here.
        ordinary = source.replace('return (int)(level > 1 ? level : 1);',
                                  'return (uint16_t)(int)(level > 1 ? level : 1);')
        assert ordinary != source
        path.write_text(ordinary)
        command([cc, *flags, '-c', path, '-o', obj])
        command([cc, *flags, CORE / 'tests/wavetest.c', obj, CORE / 'build/librun.a',
                 CORE / 'build/battle-core/libprcore.a', '-lm', '-o', exe])
        replay(exe, cases)
        print('Scope probe: ordinary-level 16-bit narrowing survives; no crossing on that branch is claimed')
        print('Endless wave sweep: 6 compiled faults, 8 malformed captures and 4 forged observations rejected')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pack', type=Path, help='clean pinned upstream checkout')
    parser.add_argument('--raw', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.pack:
        assert args.raw and args.output
        pack(args.pack.resolve(), args.raw, args.output)
    else:
        check()
