#!/usr/bin/env python3
"""Compile real storage-owner faults against the retained native turn and raw guards."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / 'game'
TESTS = APP / 'tests'
CC = os.environ.get('CC', 'cc')
INCLUDES = ['game/tests', 'game/include', 'game/gen', 'game/menus',
            'game-core/include', 'game-core/src', 'game-core/gen/out',
            'battle-core/include', 'battle-core/gen/out', 'ai-core/include',
            'ai-core/gen/out', 'run-core/include', 'run-core/gen/out',
            'reward-core/include', 'reward-core/gen/out', 'assets-pipeline/app-src/include']
FLAGS = ['-O1', '-std=gnu11', '-ffp-contract=off',
         *['-I' + str(ROOT / p) for p in INCLUDES],
         '-DEVENT_STORE_LEGACY_SAVE="' + str(ROOT / 'game-core/tests/fixtures/held-form.pr399.save') + '"']
OWNERS = ['game-core/src/gc_run.c', 'game-core/src/gc_battle.c',
          'game/source/battle_event_store.c', 'game/source/battle_events.c']
FAULTS = [
    (OWNERS[0], 'n > capacity-first && run->battle_event_reserve && !run->battle_events_overflow', 'false', 'omit reserve'),
    (OWNERS[0], 'return run->battle_event_storage ? run->battle_event_storage : run->battle_events;', 'return run->battle_events;', 'read inactive inline buffer'),
    (OWNERS[0], '&& storage && reserved >= first+n', '&& storage', 'accept undersized capacity'),
    (OWNERS[0], 'n = capacity-first; run->battle_events_overflow = true;', 'n = capacity-first;', 'hide storage overflow'),
    (OWNERS[0], 'memcpy(storage+first, events, sizeof(pr_event) * (size_t)n);', 'if(!run->battle_event_storage)memcpy(storage+first, events, sizeof(pr_event) * (size_t)n);', 'omit external append'),
    (OWNERS[2], 'if(!was_arena&&used)', 'if(false)', 'lose inline prefix'),
    (OWNERS[2], 'if(!was_arena&&used)', 'if(used)', 'read freed prefix after relocation'),
    (OWNERS[2], 'gc_run_set_battle_event_storage(run,reserve_events,NULL);', '(void)run;', 'omit app storage binding'),
    (OWNERS[1], 'gc_ev_battle_chunk(run,snapshot,1);', 'if(run->n_battle_events<GC_MAX_BATTLE_EVENTS)gc_ev_battle_chunk(run,snapshot,1);', 'truncate charge cleanup at inline limit'),
]

def command(args):
    p = subprocess.run(list(map(str, args)), cwd=ROOT, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True)
    if p.returncode:
        raise RuntimeError(str(args) + '\n' + p.stdout[-5000:])
    return p.stdout

def execute(exe):
    return subprocess.run([str(exe)], cwd=ROOT, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True)

def main():
    command(['make', '-C', TESTS, 'build/ptx_core.a'])
    with tempfile.TemporaryDirectory(prefix='controller-event-store-') as td:
        temp = Path(td)
        objects = {}
        sanitize = os.environ.get('EVENT_STORE_SANITIZE') == '1'
        flags = FLAGS + (['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all'] if sanitize else [])
        for i, file in enumerate(OWNERS):
            obj = temp / f'normal-{i}.o'
            command([CC, *flags, '-c', ROOT / file, '-o', obj])
            objects[file] = obj
        def link(exe, replacement=None):
            units = [replacement[1] if replacement and file == replacement[0] else objects[file] for file in OWNERS]
            command([CC, *flags, TESTS / 'event_store_test.c', *units,
                     TESTS / 'build/ptx_core.a', '-Wl,--wrap=pr_phase_run',
                     '-Wl,--wrap=realloc', '-lm', '-o', exe])
        exe = temp / 'normal'
        link(exe)
        baseline = execute(exe)
        if baseline.returncode:
            raise RuntimeError('Unmodified runner failed in fault-runner directory:\n' + baseline.stdout[-5000:])
        print(baseline.stdout, end='', flush=True)
        if sanitize:
            print('Scoped ASan/UBSan/LSan storage owners passed; supporting archive is unsanitized.')
            return
        for i, (file, old, new, label) in enumerate(FAULTS):
            source = (ROOT / file).read_text()
            assert source.count(old) == 1, (label, source.count(old))
            mutated = temp / Path(file).name
            mutated.write_text(source.replace(old, new))
            obj = temp / 'fault.o'
            command([CC, *flags, '-c', mutated, '-o', obj])
            exe = temp / 'fault'
            link(exe, (file, obj))
            result = execute(exe)
            if not result.returncode:
                raise RuntimeError('Undetected compiled fault: ' + label)
            print('Caught compiled fault:', label, flush=True)
        print(f'Controller event storage: {len(FAULTS)} compiled faults rejected')
        # Compile real run_screen readers/bindings, without editing the checkout.
        recipe = command(['make', '-s', '-n', '-C', TESTS, '-W', '../source/run_screen.c', 'build/runtest'])
        compile_line = next(line for line in recipe.splitlines() if ' -o build/runtest' in line)
        args = shlex.split(compile_line)
        # The archive was rebuilt above from these same core sources/headers.
        # Keep every app unit in the real recipe and avoid recompiling unchanged
        # core units for each driver fault.
        core_prefixes = ('../../game-core/', '../../run-core/', '../../reward-core/', '../../battle-core/', '../../ai-core/')
        args = [arg for arg in args if not (arg.startswith(core_prefixes) and arg.endswith('.c'))]
        args.insert(args.index('-lm'), str(TESTS / 'build/ptx_core.a'))
        env = dict(os.environ, RUNTEST_EVENT_STORE='1', RUNTEST_SAVE='resume', RUNTEST_FAILFAST='1')
        def driver_run(exe):
            return subprocess.run([str(exe), '300', '0'], cwd=TESTS, env=env,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        base_exe = temp / 'driver-baseline'
        base_build = [str(base_exe) if arg == 'build/runtest' else arg for arg in args]
        p = subprocess.run(base_build, cwd=TESTS, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if p.returncode:
            raise RuntimeError('Unmodified driver did not compile:\n' + p.stdout[-5000:])
        base = driver_run(base_exe)
        if base.returncode:
            raise RuntimeError('Unmodified real driver failed:\n' + base.stdout[-5000:])
        source = (APP / 'source/run_screen.c').read_text()
        reader = 'gc_run_battle_event_data(&G.run)'
        assert source.count(reader) == 4
        changes = [
            (source.replace(reader, 'G.run.battle_events'), 'all app readers use inactive inline storage'),
            (source.replace('  bes_bind(&G.run);\n  gc_starter team[6];', '  gc_starter team[6];'), 'omit new-run binding'),
            (source.replace('  bes_bind(&G.run);\n  G.run.sys = cfg->sys;', '  G.run.sys = cfg->sys;'), 'omit restored-run binding'),
        ]
        for mutated, label in changes:
            assert mutated != source, label
            path = temp / 'run_screen.c'
            path.write_text(mutated)
            exe = temp / 'driver-fault'
            build = [str(path) if arg == '../source/run_screen.c' else str(exe) if arg == 'build/runtest' else arg for arg in args]
            build.append('-I' + str(APP / 'source'))
            p = subprocess.run(build, cwd=TESTS, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            if p.returncode:
                raise RuntimeError('Driver fault did not compile: ' + label + '\n' + p.stdout[-5000:])
            result = driver_run(exe)
            if not result.returncode:
                raise RuntimeError('Undetected driver fault: ' + label)
            print('Caught compiled driver fault:', label, flush=True)
        print('Real app event storage: 3 compiled driver faults rejected')

if __name__ == '__main__':
    main()
