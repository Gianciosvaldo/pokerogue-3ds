#!/usr/bin/env python3
"""Compare two closures.txt manifests (before/after an upstream bump) by owner and
source hash. Every changed, added or removed closure is a piece of hand-written C
that must be re-checked against upstream. Ids can shift between dumps, so matching
is by owner + position within that owner."""
import sys
from collections import defaultdict


def load(p):
    per = defaultdict(list)
    try:
        for line in open(p):
            if line.startswith("#") or not line.strip():
                continue
            _id, owner, h = line.split()
            per[owner].append(h)
    except FileNotFoundError:
        pass
    return per


a, b = load(sys.argv[1]), load(sys.argv[2])
changed = [o for o in sorted(set(a) | set(b)) if a.get(o) != b.get(o)]
if not a:
    print("no previous manifest; nothing to compare")
elif not changed:
    print("closures unchanged")
else:
    print(f"{len(changed)} owners have changed closures (re-check their C ports):")
    for o in changed:
        print(f"  {o}: {len(a.get(o, []))} -> {len(b.get(o, []))} closures")
