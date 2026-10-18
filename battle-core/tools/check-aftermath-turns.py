#!/usr/bin/env python3
"""Replays an aftermath-turn fixture with the unchanged build/turntest and counts the Aftermath and
Innards Out paths among the covered, exact turns. Exit 1 on any mismatch or a path with no covered turn.
Usage (TURNTEST_LOCALES set): check-aftermath-turns.py FIXTURE [TURNTEST]"""
from pathlib import Path
import os, re, subprocess, sys

root = Path(__file__).resolve().parents[1]
fixture = Path(sys.argv[1])
turntest = sys.argv[2] if len(sys.argv) > 2 else str(root / 'build/turntest')
if not os.environ.get('TURNTEST_LOCALES'):
    raise SystemExit('TURNTEST_LOCALES required')

# Two producer/harness boundaries outside Aftermath and Innards Out are normalised field by field, never skipped:
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
work = Path(os.environ.get('TMPDIR', '/tmp')) / f'aftermath-normalised-{os.getpid()}-{fixture.name}'
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

AFTERMATH, INNARDS_OUT, DAMP, MAGIC_GUARD, LONG_REACH = 106, 215, 6, 98, 203
CONTACT = {33, 98, 369, 34}                       # Tackle, Quick Attack, U-turn, Body Slam
paths = dict.fromkeys(['Aftermath recoil', 'Innards Out recoil', 'recoil knocks the attacker out', 'holder out to a contact move, no recoil (Magic Guard)',
                       'holder out to a contact move, no recoil (Long Reach)', 'holder out with Damp on the field, no recoil',
                       'holder out to a non-contact move', 'holder out with no recoil text'], 0)
def mons(s):
    out = {}
    for m in re.finditer(r'^MON b=(\S+) id=(\d+) .*? abil=(\d+) babil=\d+ passive=(\d) cpass=(-?\d+) .*? hp=(\d+) ', s, re.M):
        b, i, abil, passive, cpass, hp = m.groups()
        ab = {int(abil)} | ({int(cpass)} if passive == '1' else set())
        out[i] = (b, ab, int(hp))
    return out
for block in text.split('\nCASE ')[1:]:
    head, _, rest = block.partition('\n')
    i = int(head.split()[0])
    if i not in covered_ids:
        continue
    before, _, after = rest.partition('\nRUN\n')
    trace = re.search(r'^TRACE list=(.*)$', after, re.M).group(1).split(',')
    texts_line = re.search(r'^TEXTS (.*)$', after, re.M).group(1)
    mb, ma = mons(before), mons(after.split('\nRNG ')[1] if '\nRNG ' in after else after)
    am = texts_line.count('Aftermath\\nhurt its attacker')
    io = texts_line.count('Innards Out\\nhurt its attacker')
    paths['Aftermath recoil'] += am
    paths['Innards Out recoil'] += io
    if (am or io) and trace.count('FaintPhase:0') + trace.count('FaintPhase:2') >= 2:
        paths['recoil knocks the attacker out'] += 1
    field_ab = set().union(*(ab for b, ab, hp in mb.values() if b in ('0', '2')))
    cmds = {m.group(1): int(m.group(2)) for m in re.finditer(r'^CMD b=(\d) cmd=0 move=(\d+)', before, re.M)}
    for pid, (b, ab, hp) in mb.items():
        if b not in ('0', '2') or hp == 0 or not ab & {AFTERMATH, INNARDS_OUT} or pid not in ma or ma[pid][2] != 0:
            continue
        foe = '2' if b == '0' else '0'
        foe_ab = next((a for bb, a, _ in mb.values() if bb == foe), set())
        mv = cmds.get(foe)
        if am or io:
            continue
        paths['holder out with no recoil text'] += 1
        if mv in CONTACT and MAGIC_GUARD in foe_ab: paths['holder out to a contact move, no recoil (Magic Guard)'] += 1
        elif mv in CONTACT and LONG_REACH in foe_ab: paths['holder out to a contact move, no recoil (Long Reach)'] += 1
        elif DAMP in field_ab: paths['holder out with Damp on the field, no recoil'] += 1
        elif mv is not None and mv not in CONTACT: paths['holder out to a non-contact move'] += 1
print(f'{fixture.name}: cases {cases} (skipped {skipped}), parse {parse}, stats {stats}; covered {covered}: pass {passed}, fail {failed}; '
      f'not covered {uncovered}; texts {texts}: identical {same}, differ {differ}, no text {notext}')
for k, v in paths.items():
    print(f'  {v:5d}  {k}')
print(f'  outside Aftermath, normalised field by field: {normalised["trainer"]} trainer-pick Levitate records, {normalised["clear"]} final-wave CLEAR battle types'
      + (f'; unclassified failures: {" ".join(sorted(set(unknown)))}' if unknown else ''))
bad = unknown or skipped or parse or stats or failed or differ or notext or texts != covered or same != covered or r.returncode not in (0, 1)
missing = [k for k, v in paths.items() if v == 0]
if missing:
    print('no covered turn for: ' + ', '.join(missing))
if uncovered:
    print('\n'.join(l for l in out.splitlines() if l.startswith('  ') and re.match(r'^\s+\d+\s+\S', l))[:2000], flush=True)
sys.exit(1 if bad or missing else 0)
