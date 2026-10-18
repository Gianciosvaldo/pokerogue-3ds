#!/usr/bin/env python3
"""Replays a tera-shift-turn fixture with build/turntest and counts the Tera Shift paths among the
covered, exact turns. Exit 1 on any mismatch or a path with no covered turn.
Usage (TURNTEST_LOCALES set): check-tera-shift-turns.py FIXTURE [TURNTEST]"""
from pathlib import Path
import os, re, subprocess, sys

root = Path(__file__).resolve().parents[1]
fixture = Path(sys.argv[1])
turntest = sys.argv[2] if len(sys.argv) > 2 else str(root / 'build/turntest')
if not os.environ.get('TURNTEST_LOCALES'):
    raise SystemExit('TURNTEST_LOCALES required')

# Two producer/harness boundaries outside Tera Shift are normalised field by field, never skipped:
#  - trainer: the producer's trainer picks its next Pokemon with getMatchupScore, whose
#    getAttackTypeEffectiveness reaches isForciblyUngrounded and records Levitate in the player's
#    abilitiesApplied; the C replay receives that pick through EPICK and runs no scoring.
#  - clear: a final-wave VictoryPhase sets BattleType.CLEAR; in the port that is game-core's job.
# The JS field is rewritten to the C value only when it is the first and only difference of that
# line, and the case is replayed again, so any other difference in the same case still fails.
def run(path):
    return subprocess.run([turntest, str(path)], capture_output=True, text=True, env={**os.environ, 'TURNTEST_COVERED': '1'})

def fields(line):
    return dict(t.split('=', 1) for t in line.split()[1:] if '=' in t)

def known(c, js, block):
    fc, fj = fields(c), fields(js)
    diff = {k for k in fc.keys() | fj.keys() if fc.get(k) != fj.get(k)}
    if c.startswith('MON ') and fc.get('b', '')[:1] in ('0', '1', 'p') and diff and diff <= {'aas', 'aaw'} and '\nEPICK ' in block:
        if all(set(fj[k].split(',')) - {'-'} == (set(fc[k].split(',')) - {'-'}) | {'26'} for k in diff):
            return 'trainer'
    if c.startswith('BATTLE ') and diff == {'type'} and fc['type'] == '1' and fj['type'] == '2' and 'VictoryPhase' in block.split('\nRUN\n')[1]:
        return 'clear'
    return None

text = fixture.read_text()
normalised = {'trainer': 0, 'clear': 0}
unknown = []
work = Path(os.environ.get('TMPDIR', '/tmp')) / f'tera-shift-normalised-{os.getpid()}-{fixture.name}'
for _ in range(64):
    work.write_text(text)
    r = run(work)
    fails = re.findall(r'^FAIL (?:CASE )?(\d+)\n(?:  before: .*\n)*  C : (.*)\n  JS: (.*)\n', r.stdout, re.M)
    if not fails:
        break
    changed = False
    for n, c, js in fails:
        start = text.index(f'CASE {n}\n') if text.startswith(f'CASE {n}\n') else text.index(f'\nCASE {n}\n') + 1
        end = text.find('\nCASE ', start)
        end = len(text) if end < 0 else end + 1
        block = text[start:end]
        kind = known(c, js, block)
        before, sep, after = block.partition('\nRUN\n')
        if not kind or after.count(js + '\n') != 1:
            unknown.append(n)
            continue
        normalised[kind] += 1
        text = text[:start] + before + sep + after.replace(js + '\n', c + '\n', 1) + text[end:]
        changed = True
    if not changed:
        break
work.unlink(missing_ok=True)
out = r.stdout
m = re.search(r'^cases (\d+) \(skipped (\d+).*parse mismatches (\d+), stats mismatches (\d+)$', out, re.M)
c = re.search(r'^covered (\d+): pass (\d+), fail (\d+); not covered (\d+)$', out, re.M)
t = re.search(r'^texts of covered turns (\d+): identical (\d+), differ (\d+), a message with no text yet (\d+)$', out, re.M)
if not (m and c and t):
    sys.stdout.write(out + r.stderr)
    raise SystemExit('turntest summary missing')
cases, skipped, parse, stats = map(int, m.groups())
covered, passed, failed, uncovered = map(int, c.groups())
texts, same, differ, notext = map(int, t.groups())
covered_ids = {int(x) for x in re.findall(r'^COVERED (?:CASE )?(\d+)', out, re.M)}
if len(covered_ids) != covered:
    raise SystemExit(f'covered list {len(covered_ids)} != summary {covered}')

TERAPAGOS, TERA_SHIFT, TERA_SHELL = 1024, 307, 308
paths = dict.fromkeys(['Normal -> Terastal on entry', 'player Terapagos takes Tera Shell', 'enemy Terapagos changes form',
                       'Tera Shift shown in Terastal form (no change)', 'Stellar -> Terastal on a Tera lapse', 'Terapagos knocked out'], 0)
def mons(s):
    return {m.group(2): (m.group(1), int(m.group(3)), int(m.group(4)), int(m.group(5))) for m in
            re.finditer(r'^MON b=(\S+) id=(\d+) sp=(\d+) form=(\d+) .*? abil=(\d+) ', s, re.M)}
for block in text.split('\nCASE ')[1:]:
    head, _, rest = block.partition('\n')
    i = int(head.split()[0])
    if i not in covered_ids:
        continue
    before, _, after = rest.partition('\nRUN\n')
    trace = re.search(r'^TRACE list=(.*)$', after, re.M).group(1).split(',')
    state = after.split('\nRNG ', 1)[1] if '\nRNG ' in after else after
    mb, ma = mons(before), mons(state)
    for pid, (b, sp, form, abil) in ma.items():
        if sp != TERAPAGOS or pid not in mb:
            continue
        f0 = mb[pid][2]
        if f0 == 0 and form == 1:
            paths['Normal -> Terastal on entry'] += 1
            if b in ('0',) or b.startswith('p'):
                if abil == TERA_SHELL: paths['player Terapagos takes Tera Shell'] += 1
            else:
                paths['enemy Terapagos changes form'] += 1
        if f0 == 2 and form == 1:
            paths['Stellar -> Terastal on a Tera lapse'] += 1
    for k, ph in enumerate(trace[:-1]):
        if ph.startswith('ShowAbilityPhase') and trace[k + 1] == 'HideAbilityPhase':
            b = ph.split(':')[1] if ':' in ph else ''
            m = next((v for v in mb.values() if v[0] == b and v[1] == TERAPAGOS and v[2] == 1), None)
            if m: paths['Tera Shift shown in Terastal form (no change)'] += 1
    for ph in trace:
        if ph.startswith('FaintPhase:'):
            b = ph.split(':')[1]
            if any(v[0] == b and v[1] == TERAPAGOS for v in mb.values()): paths['Terapagos knocked out'] += 1
print(f'{fixture.name}: cases {cases} (skipped {skipped}), parse {parse}, stats {stats}; covered {covered}: pass {passed}, fail {failed}; '
      f'not covered {uncovered}; texts {texts}: identical {same}, differ {differ}, no text {notext}')
for k, v in paths.items():
    print(f'  {v:5d}  {k}')
print(f'  outside Tera Shift, normalised field by field: {normalised["trainer"]} trainer-pick Levitate records, {normalised["clear"]} final-wave CLEAR battle types'
      + (f'; unclassified failures: {" ".join(sorted(set(unknown)))}' if unknown else ''))
bad = unknown or skipped or parse or stats or failed or differ or notext or texts != covered or same != covered or r.returncode not in (0, 1)
missing = [k for k, v in paths.items() if v == 0]
if missing:
    print('no covered turn for: ' + ', '.join(missing))
if uncovered:
    print('\n'.join(l for l in out.splitlines() if l.startswith('  ') and re.match(r'^\s+\d+\s+\S', l))[:2000], flush=True)
sys.exit(1 if bad or missing else 0)
