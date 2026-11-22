#!/usr/bin/env python3
"""Planted faults for the save at an encounter's first question (run_screen.c: mystery_screen's save_point()). Same method as check_safari_scan_faults.py (whose
build and run machinery this uses): each fault is a one-line change to the run driver, compiled on its own and linked with the unchanged objects of the host driver
test (game/tests/runtest.c), which must report a BAD line (exit 1) for every one while the unchanged code passes. Usage, from the repository root:
  python3 game/tests/check_me_save_faults.py

The test runs with every upstream encounter forced, one per run (RUNTEST_ME=cycle), saving at every save point and continuing every 4th one from its slot:
with the SWITCH battle style off and on. (The busy guard of save_point, a question that waits for a fade or for events still playing, is never reached in the host
driver: its fake has nothing playing at that moment, so a fault there cannot be caught by it.)"""
import os
import check_safari_scan_faults as base

base.RUNS = os.environ.get('ME_SAVE_FAULT_RUNS', '300')
base.CONFIGS = [
    {'RUNTEST_ME': 'cycle', 'RUNTEST_SYS': '1', 'RUNTEST_SAVE': 'resume'},
    {'RUNTEST_ME': 'cycle', 'RUNTEST_SYS': '1', 'RUNTEST_SAVE': 'resume', 'RUNTEST_ASK_SWITCH': '1'},
]
QUESTION = '  if (!save_point()) return;   /* upstream saves at an encounter\'s start'
base.FAULTS = [
    # the question opens without the run having been written: only the answer's own late write (submit) would, one step too late
    ('question-not-saved', QUESTION, '  if (0) return;   /* upstream saves at an encounter\'s start'),
]

if __name__ == '__main__':
    base.main()
