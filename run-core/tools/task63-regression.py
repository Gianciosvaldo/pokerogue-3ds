#!/usr/bin/env python3
"""Durable Task 63 validation driver; established tests and counts are unchanged."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('upstream')
parser.add_argument('baseline')
parser.add_argument('seed', type=int)
parser.add_argument('group', choices=['scalar', 'trainer', 'rival', 'remainder', 'focus', 'moveset'])
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
upstream = Path(args.upstream).resolve()
baseline = Path(args.baseline).resolve()
out = Path(os.environ['TASK63_RESULTS']).resolve()
out.mkdir(parents=True, exist_ok=True)
build = out / 'build'
before_build = out / 'before-build'
records = []

def run(command, name, env=None):
    started = time.monotonic()
    print('$ ' + ' '.join(map(str, command)), flush=True)
    with (out / (name + '.log')).open('w') as log:
        proc = subprocess.run(list(map(str, command)), cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT)
    record = {'name': name, 'command': list(map(str, command)), 'exit': proc.returncode,
              'seconds': round(time.monotonic() - started, 2)}
    if env:
        record['environment'] = {key: env[key] for key in ['BUILD', 'DIFF_FOCUS'] if key in env}
    records.append(record)
    (out / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
    print(json.dumps(record), flush=True)
    return proc.returncode

def require(command, name, env=None):
    code = run(command, name, env)
    if code:
        raise SystemExit(code)

require(['make', '-C', baseline / 'run-core', 'BUILD=' + str(before_build)], 'before-build')
require(['make', '-C', root / 'run-core', 'BUILD=' + str(build)], 'candidate-build')
env = dict(os.environ, BUILD=str(build))
groups = {
    'scalar': [(s, 20000) for s in 'wave shop biome encounter newbattle exp partyexp upkeep trainerspecies luck daily'.split()] + [('battlegen', 10000), ('pokemongen', 8000)],
    'trainer': [('trainerparty', 3000)],
    'rival': [('rival', 3000)],
    'remainder': [('gym_e4', 3000), ('wildenemy', 3000), ('enemyitems', 3000), ('enemybuffs', 2000), ('challenge', 10000)],
    'focus': [('battlegen', 10000), ('pokemongen', 8000)],
    'moveset': [('moveset', 2000)],
}
if args.group == 'focus':
    env['DIFF_FOCUS'] = '1'
for suite, count in groups[args.group]:
    if suite == 'challenge':
        command = ['sh', 'run-core/tools/challengedifftest.sh', upstream, count, args.seed]
        trace = build / 'challenges.txt'
    elif suite == 'moveset':
        command = ['sh', 'run-core/tools/movesetdifftest.sh', upstream, count, args.seed]
        trace = build / f'moveset-{args.seed}.txt'
    else:
        command = ['sh', 'run-core/tools/difftest.sh', upstream, count, args.seed, suite]
        trace = build / (suite + '-cases.txt')
    require(command, suite + '-final', env)
    if not trace.is_file() or not trace.stat().st_size:
        raise SystemExit('Missing nonempty trace: ' + str(trace))
    archived = out / trace.name
    archived.write_bytes(trace.read_bytes())
    if suite != 'moveset':
        executable = before_build / ('challengetest' if suite == 'challenge' else suite + 'test')
        before_exit = run([executable, archived], suite + '-before-replay')
        # Original-main failures are evidence, not successful baseline runs.
        before_text = (out / (suite + '-before-replay.log')).read_text()
        final_text = (out / (suite + '-final.log')).read_text()
        before_coverage = re.findall(r'not covered\s+(\d+)', before_text)
        final_coverage = re.findall(r'not covered\s+(\d+)', final_text)
        if before_coverage != final_coverage:
            raise SystemExit('Coverage changed: ' + suite + ': ' + repr((before_coverage, final_coverage)))
        if before_exit not in (0, 1):
            raise SystemExit('Baseline replay did not execute normally: ' + suite)
    if suite in ('rival', 'gym_e4', 'moveset'):
        source = root / 'run-core/src' / (suite + '.c')
        old, new = {
            'rival': ('out->species[slot]=(uint16_t)species;', 'out->species[slot]=(uint16_t)129;'),
            'gym_e4': ('*out_species = species;', '*out_species = 129;'),
            'moveset': ('out->pp[i]=pr_moves[id].pp + up*(step<1 ? 1 : step);', 'out->pp[i]=1 + pr_moves[id].pp + up*(step<1 ? 1 : step);'),
        }[suite]
        source_text = source.read_text()
        if source_text.count(old) != 1:
            raise SystemExit('Mutation anchor missing or ambiguous: ' + suite)
        mutant_source = out / (suite + '-mutant.c')
        mutant_source.write_text(source_text.replace(old, new))
        mutant = out / (suite + '-mutant')
        require(['gcc', '-Irun-core/include', '-Ibattle-core/include', '-Ibattle-core/gen/out', '-O2', '-std=c99', '-Wall', '-Wextra', '-ffp-contract=off', root / 'run-core/tests' / (suite + 'test.c'), mutant_source, build / 'librun.a', build / 'battle-core/libprcore.a', '-lm', '-o', mutant], suite + '-mutant-build')
        mutation_command = [mutant, archived] + ([count, args.seed] if suite == 'moveset' else [])
        mutation_exit = run(mutation_command, suite + '-mutation')
        mutation_text = (out / (suite + '-mutation.log')).read_text()
        counts = re.findall(r'(?:mismatches|fail)\s+(\d+)', mutation_text)
        counts += re.findall(r'(\d+)\s+mismatches', mutation_text)
        detected = any(int(n) > 0 for n in counts)
        if mutation_exit != 1 or not detected:
            raise SystemExit('Mutation was not caught with nonzero mismatch count and exit 1: ' + suite)
        if suite == 'moveset':
            truncated = out / 'moveset-truncated.txt'
            lines = archived.read_text().splitlines(keepends=True)
            footer = [i for i, line in enumerate(lines) if line.startswith('END')]
            if len(footer) != 1:
                raise SystemExit('Expected one completion footer')
            truncated.write_text(''.join(line for i, line in enumerate(lines) if i != footer[0]))
            code = run([build / 'movesettest', truncated, count, args.seed], 'moveset-missing-footer')
            if code != 2 or 'missing completion footer' not in (out / 'moveset-missing-footer.log').read_text():
                raise SystemExit('Missing-footer trace not rejected')

# Keep traces and evidence; discard rebuildable binaries from the artifact.
import shutil
shutil.rmtree(build)
shutil.rmtree(before_build)
for name in ('rival-mutant', 'gym_e4-mutant', 'moveset-mutant'):
    (out / name).unlink(missing_ok=True)
(out / 'COMPLETE').write_text(f'{args.group} seed={args.seed}: every required candidate command and control completed\n')
