#!/usr/bin/env python3
"""Closure helper for hand-porting.
  closures.py show <owner>...     print each closure of an owner (index, hash, captures, source)
  closures.py fill <file.c>...    replace "?<k>" hash placeholders in {"owner", "?k", fn} entries
                                  with the hash of that owner's k-th closure
  closures.py status               how many closures have a C port, listed by owner"""
import json, os, re, sys, hashlib
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(HERE, "gen"))
cl = json.load(open(os.path.join(HERE, "gen/upstream/closures.json")))


def h(c):
    key = c["src"] + ("\n#cap " + json.dumps(c["cap"]) if "cap" in c else "")
    return hashlib.sha1(key.encode()).hexdigest()[:12]


by_owner = {}
for c in cl:
    by_owner.setdefault(c["owner"], []).append(c)

cmd = sys.argv[1] if len(sys.argv) > 1 else "status"
if cmd == "show":
    for o in sys.argv[2:]:
        for k, c in enumerate(by_owner.get(o, [])):
            print(f"== {o} ?{k} {h(c)} cap={c.get('cap')}\n{c['src']}\n")
elif cmd == "fill":
    for path in sys.argv[2:]:
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
            ported |= set(re.findall(r'\{"([a-z]+:\d+)", "([0-9a-f]{12})"', open(os.path.join(HERE, "src", f)).read()))
    have = [c for c in cl if (c["owner"], h(c)) in ported]
    print(f"{len(have)} of {len(cl)} closures have a C port")
