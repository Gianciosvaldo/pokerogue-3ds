#!/usr/bin/env python3
"""Planted faults for the Safari Zone's IV Scanner question (run_screen.c: scan_safari_ivs and its answer): each one is a one-line change to the
run driver, compiled on its own and linked with the unchanged objects of the host driver test (game/tests/runtest.c). The test must report a
BAD line (exit 1) for every one, while the unchanged code passes it. Usage, from the repository root:  python3 game/tests/check_safari_scan_faults.py

The test runs twice per build: with a scanner (RUNTEST_SCANNER=1 RUNTEST_SAFARI=1: the question comes once per Pokemon, Yes opens the window with the
Pokemon's own IVs, the run's RNG streams are not touched) and without one (RUNTEST_SAFARI=1 only: a Safari is played and nothing is asked)."""
from pathlib import Path
import concurrent.futures
import os
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / 'game' / 'tests'
RS = 'game/source/run_screen.c'
RUNS = os.environ.get('SAFARI_FAULT_RUNS', '60')
CONFIGS = [{'RUNTEST_SCANNER': '1', 'RUNTEST_SAFARI': '1'}, {'RUNTEST_SAFARI': '1'}]

# (name, original text (exactly once), replacement)
FAULTS = [
    ('never-asked', 'if (G.run.screen.me_stage == GC_ME_STAGE_SAFARI && scan_safari_ivs()) return;', 'if (0) return;'),
    ('asked-without-a-scanner', 'if (gc_item_stack(&G.run, "IV_SCANNER") <= 0) return false;', ''),
    ('asked-every-time', 'if (key == G.safari_scanned) return false;', ''),
    ('asked-again-after-bait', 'const int key = G.run.wave * 8 + remaining;', 'const int key = G.run.wave * 8 + remaining + (int)(G.st.bev & 1);'),
    ('remembered-never', '  G.safari_scanned = key;\n', ''),
    ('window-of-the-wrong-stat', 'G.scan_stats[s] = p->ivs[s];', 'G.scan_stats[s] = p->ivs[(s + 1) % 6];'),
    ('window-of-the-wild-slot', 'const pr_pokemon* p = G.scan_safari ? (G.run.n_enemy > 0 ? &G.run.enemy[0].b : NULL) : G.run.bt.field[G.scan_target];',
     'const pr_pokemon* p = G.run.bt.field[G.scan_target];'),
    ('scan-flag-kept', '  G.scan_safari = false;\n  G.n_ev = G.ev_pos = G.n_txt = 0;\n  pump(NULL);', '  G.n_ev = G.ev_pos = G.n_txt = 0;\n  pump(NULL);'),
    ('wrong-slot', 'G.scan_target = 2;   /* the enemy', 'G.scan_target = 0;   /* the enemy'),
    ('declined-skips-the-actions', 'if (!yes) { scan_done(NULL); return; }', 'if (!yes) return;'),
    ('remaining-not-read', 'if (!strcmp(s->me_args[i].name, "remainingCount")) remaining = (int)s->me_args[i].v;', ''),
]


def sh(args, cwd, **kw):
    return subprocess.run(args, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace', **kw)


def compile_command():
    """The cc line `make` would run for build/runtest: its flags, its sources and its link options."""
    out = sh(['make', '-n', '-B', 'build/runtest'], TESTS).stdout
    lines = [l for l in out.split('\n') if l.startswith('cc ') or l.startswith('gcc ')]
    if not lines:
        sys.exit('could not read the compile command from make -n:\n' + out[-2000:])
    toks = shlex.split(lines[-1])
    srcs = [t for t in toks[1:] if t.endswith('.c')]
    link = [t for t in toks[1:] if t.startswith('-Wl,') or t == '-lm']
    flags = [t for t in toks[1:] if t not in srcs and t not in link and t != '-o' and t != 'build/runtest']
    return toks[0], flags, srcs, link


def main():
    cc, flags, srcs, link = compile_command()
    me = [s for s in srcs if s.endswith('source/run_screen.c')]
    if len(me) != 1:
        sys.exit('run_screen.c is not in the runtest sources')
    others = [s for s in srcs if s != me[0]]
    original = (ROOT / RS).read_text()
    with tempfile.TemporaryDirectory(prefix='safari-scan-faults-') as temp:
        temp = Path(temp)

        def obj(args):
            i, src, extra = args
            o = temp / ('o%d.o' % i) if extra is None else temp / ('f%d.o' % i)
            r = sh([cc, *flags, '-w', *(extra or []), '-c', src, '-o', str(o)], TESTS)
            if r.returncode:
                raise RuntimeError('compile failed: ' + src + '\n' + r.stdout[-3000:])
            return str(o)

        print('compiling %d unchanged sources...' % len(others), flush=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
            objs = list(pool.map(obj, [(i, s, None) for i, s in enumerate(others)]))

        def build(text, tag):
            src = temp / ('run_screen_%s.c' % tag)
            src.write_text(text)
            o = temp / ('rs_%s.o' % tag)
            r = sh([cc, *flags, '-w', '-I' + str(ROOT / 'game' / 'source'), '-c', str(src), '-o', str(o)], TESTS)
            if r.returncode:
                return None, r.stdout
            exe = temp / ('runtest_%s' % tag)
            r = sh([cc, str(o), *objs, *link, '-o', str(exe)], TESTS)
            return (exe, None) if r.returncode == 0 else (None, r.stdout)

        def play(exe):
            """True when every configuration passes."""
            bad = []
            for cfg in CONFIGS:
                r = sh([str(exe), RUNS, '0'], TESTS, env={**os.environ, **cfg})
                if r.returncode:
                    bad.append((cfg, r.stdout))
            return bad

        exe, err = build(original, 'base')
        if not exe:
            sys.exit('the unchanged driver does not build:\n' + err[-3000:])
        bad = play(exe)
        if bad:
            sys.exit('the unchanged driver fails its own test (%s):\n%s' % (bad[0][0], bad[0][1][-3000:]))
        print('unchanged: passes %d runs in %d configurations' % (int(RUNS), len(CONFIGS)), flush=True)

        caught_all = True
        for n, (name, old, new) in enumerate(FAULTS):
            if original.count(old) != 1:
                print('%s: the original text occurs %d times' % (name, original.count(old)))
                caught_all = False
                continue
            exe, err = build(original.replace(old, new), 'f%d' % n)
            if not exe:
                print('%s: does not build (%s)' % (name, err.strip().split('\n')[-1][:160]))
                caught_all = False
                continue
            bad = play(exe)
            if bad:
                first = [l for l in bad[0][1].split('\n') if l.startswith('BAD') or 'BAD' in l][:1]
                print('%s: caught (%s)%s' % (name, ' '.join(sorted(bad[0][0])), ' ' + first[0][:110] if first else ''), flush=True)
            else:
                print('%s: NOT CAUGHT' % name, flush=True)
                caught_all = False
        print('all %d faults caught' % len(FAULTS) if caught_all else 'some faults were not caught')
        sys.exit(0 if caught_all else 1)


if __name__ == '__main__':
    main()
