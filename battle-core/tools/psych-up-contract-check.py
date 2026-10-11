#!/usr/bin/env python3
"""Additional transport regressions for the PR #194 repair (exit 2 required)."""
from pathlib import Path
import subprocess
import sys
import tempfile

exe, trace = sys.argv[1:]
source = Path(trace).read_text()
lines = source.splitlines(keepends=True)
variants = {}
for prefix in ('TRACE ', 'TEXTS ', 'QUEUE ', 'CMD '):
    i = next(i for i, line in enumerate(lines) if line.startswith(prefix))
    variants['missing-' + prefix.strip()] = ''.join(lines[:i] + lines[i+1:])
    variants['duplicate-' + prefix.strip()] = ''.join(lines[:i] + [lines[i]] + lines[i:])
for key, bad in [('cmd', '0.5'), ('mode', '1x'), ('move', '244x'), ('skip', '2')]:
    i = next(i for i, line in enumerate(lines) if line.startswith('CMD '))
    tokens = lines[i].split()
    tokens = [key + '=' + bad if token.startswith(key + '=') else token for token in tokens]
    variants['invalid-' + key] = ''.join(lines[:i] + [' '.join(tokens) + '\n'] + lines[i+1:])
variants['missing-final-newline'] = source.rstrip('\n')
with tempfile.TemporaryDirectory(prefix='psych-up-contract-') as scratch:
    for name, text in variants.items():
        path = Path(scratch) / name
        path.write_text(text)
        result = subprocess.run([exe, str(path)], capture_output=True)
        print(f'{name}: exit {result.returncode}')
        if result.returncode != 2:
            raise SystemExit(f'{name}: expected parse refusal 2')
