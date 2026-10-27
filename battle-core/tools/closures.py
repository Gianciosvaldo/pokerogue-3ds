#!/usr/bin/env python3
"""Closure helper for hand-porting.
  closures.py show <owner>...     print each closure of an owner (index, hash, captures, source)
  closures.py fill <file.c>...    replace "?<k>" hash placeholders in {"owner", "?k", fn} entries
                                  with the hash of that owner's k-th closure
  closures.py status             generic registry/builtin mappings (not semantic coverage)
  --dump-dir DIR                use an existing native dump instead of gen/upstream"""
import argparse, json, os, re, sys
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(HERE, "gen"))
from closure_identity import closure_hash as h

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--dump-dir', default=os.path.join(HERE, 'gen/upstream'))
parser.add_argument('command', choices=['show', 'fill', 'status'], nargs='?', default='status')
parser.add_argument('items', nargs='*')
args = parser.parse_args()
cl = json.load(open(os.path.join(args.dump_dir, 'closures.json')))


by_owner = {}
for c in cl:
    by_owner.setdefault(c["owner"], []).append(c)

cmd = args.command
if cmd == "show":
    for o in args.items:
        for k, c in enumerate(by_owner.get(o, [])):
            print(f"== {o} ?{k} {h(c)} cap={c.get('cap')} vals={c.get('vals')}\n{c['src']}\n")
elif cmd == "fill":
    for path in args.items:
        s = open(path).read()
        def rep(m):
            o, k = m.group(1), int(m.group(2))
            return f'{{"{o}", "{h(by_owner[o][k])}"'
        s2 = re.sub(r'\{"([a-z]+:\d+)", "\?(\d+)"', rep, s)
        open(path, "w").write(s2)
        print(path, "filled", len(re.findall(r'"\?\d+"', s)))
else:
    ported = set()
    for f in os.listdir(os.path.join(HERE, "src")):
        if f.startswith("closures_"):
            rows = re.findall(r'\{\s*(NULL|"[a-z]+:\d+")\s*,\s*"([0-9a-f]{12})"', open(os.path.join(HERE, "src", f)).read())
            ported |= {(None if owner == 'NULL' else owner.strip('"'), digest) for owner, digest in rows}
    source = open(os.path.join(HERE, 'src/closures.c')).read()
    const_true = re.search(r'#define CONST_TRUE_HASH "([0-9a-f]{12})"', source).group(1)
    have = [c for c in cl if h(c) == const_true or (c['owner'], h(c)) in ported or (None, h(c)) in ported]
    print(f'{len(have)} of {len(cl)} closures map through the generic registry/builtin; '
          'class-specific callers require separate review (not semantic coverage)')
