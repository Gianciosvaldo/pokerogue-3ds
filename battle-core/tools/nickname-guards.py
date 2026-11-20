"""Planted faults and malformed captures for the nickname checks: every fault must be caught (exit 1) by the comparator or the unit test,
every malformed capture refused (exit 2). Usage: nickname-guards.py BATTLE_CORE_DIR OUT_DIR UPSTREAM_LOCALES_EN CAPTURE_TXT
(the capture is build/nickname-message-<seed>.txt from tools/nickname-diff.sh; battle-core must be built, build/libprcore.a)."""
from pathlib import Path
import os
import subprocess
import sys

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
env = os.environ.copy()
env["TURNTEST_LOCALES"] = sys.argv[3]
capture = Path(sys.argv[4]).resolve()
out.mkdir(parents=True, exist_ok=True)

# (name, file, original, replacement, test that must catch it, needs the capture)
faults = [
    ("ignore-nickname", "text.c", "const bool has_nick = nick && nick[0];", "const bool has_nick = 0;", "nickname-message-test.c"),
    ("no-nickname-with-affix", "text.c", "const bool has_nick = nick && nick[0];", "const bool has_nick = nick && nick[0] && !affix;", "nickname-message-test.c"),
    ("fusion-over-nickname", "text.c", "if (!has_nick && fusion_v >> 16)", "if (fusion_v >> 16)", "nickname-message-test.c"),
    ("nickname-in-plain-name", "events.c", "pr_msg_arg a = arg(name, PR_ARG_POKEMON_NAME, pokemon_v(bt, p), NULL);",
     "pr_msg_arg a = arg(name, PR_ARG_POKEMON_NAME, pokemon_v(bt, p), p && p->nickname[0] ? intern_nickname(p->nickname) : NULL);", "nickname-message-test.c"),
    ("nickname-under-illusion", "events.c", "if (p && !p->illusion.active && p->nickname[0])", "if (p && p->nickname[0])", "nickname-message-test.c"),
    ("name-alone-without-nickname", "events.c", "    a.kind = PR_ARG_POKEMON_NAME;\n    return a;", "    a.kind = PR_ARG_POKEMON_NAME;\n    a.s = NULL;\n    return a;", "nickname-message-test.c"),
    ("pointer-into-pokemon", "events.c", "if (p && !p->illusion.active && p->nickname[0]) a.s = intern_nickname(p->nickname);",
     "if (p && !p->illusion.active && p->nickname[0]) a.s = p->nickname;", "nickname-message-test.c"),
    ("truncated-copy", "events.c", "strncpy(slot, text, PR_NICKNAME_BYTES - 1);", "strncpy(slot, text, PR_NICKNAME_BYTES - 9);", "nickname-test.c"),
    ("ring-of-eight", "events.c", "#define NICKNAME_POOL 64", "#define NICKNAME_POOL 8", "nickname-test.c"),
    ("no-copy-sharing", "events.c", "if (!strcmp(pool[i], text)) return pool[i];", "if (0) return pool[i];", "nickname-test.c"),
]
for name, file, a, b, test in faults:
    source = (root / "src" / file).read_text()
    assert a in source, (name, "original text not found")
    mutant = out / f"mutant-{name}.c"
    mutant.write_text(source.replace(a, b, 1))
    # the mutant replaces its own file in the library: compile the rest from the archive
    exe = out / f"mutant-{name}"
    objs = subprocess.run(["ar", "t", str(root / "build/libprcore.a")], capture_output=True, text=True, check=True).stdout.split()
    keep = out / f"libmutant-{name}"
    keep.mkdir(exist_ok=True)
    subprocess.run(["ar", "x", str(root / "build/libprcore.a")], cwd=keep, check=True)
    member = f"{Path(file).stem}.o"
    assert member in objs, member
    mobj = keep / member
    subprocess.run(["gcc", "-O2", "-std=c99", "-I" + str(root / "include"), "-I" + str(root / "gen/out"), "-c", str(mutant), "-o", str(mobj)], check=True, capture_output=True)
    others = [str(keep / o) for o in objs]
    subprocess.run(["gcc", "-O2", "-std=c99", "-I" + str(root / "include"), "-I" + str(root / "gen/out"), str(root / "tests" / test), *others, "-lm", "-o", str(exe)], check=True, capture_output=True)
    args = [str(exe)] + ([str(capture)] if test == "nickname-message-test.c" else [])
    r = subprocess.run(args, env=env, capture_output=True, text=True)
    (out / f"mutant-{name}.log").write_text(r.stdout + r.stderr)
    assert r.returncode == 1, (name, r.returncode, r.stdout[-300:], r.stderr[-300:])
    print(name, "caught", flush=True)

base = capture.read_text()
lines = base.splitlines(True)
first = lines[1]
parts = first.split()
malformed = {
    "truncated": "".join(lines[:-1]),
    "wrong-version": base.replace("NICKNAME 1 ", "NICKNAME 9 ", 1),
    "count": base.replace(lines[-1], lines[-1].replace(lines[-1].split()[1], str(int(lines[-1].split()[1]) - 1)), 1),
    "extra-line": base + "extra\n",
    "bad-hex": base.replace(first, first.replace(" C 0 ", " C 0 ", 1).replace(parts[15], "zz", 1), 1),
    "bad-species": base.replace(first, " ".join(parts[:2] + ["65535"] + parts[3:]) + "\n", 1),
    "bad-wave": base.replace(first, " ".join(parts[:8] + ["7"] + parts[9:]) + "\n", 1),
    "duplicate-index": base.replace(lines[2], first, 1),
    "nickname-too-long": base.replace(first, " ".join(parts[:14] + ["61" * 81] + parts[15:]) + "\n", 1),
}
exe = root / "build/nickname-message-test"
for name, text in malformed.items():
    p = out / f"malformed-{name}.txt"
    p.write_text(text)
    r = subprocess.run([str(exe), str(p)], env=env, capture_output=True, text=True)
    assert r.returncode == 2, (name, r.returncode, r.stdout[-200:], r.stderr[-200:])
    print("malformed", name, "refused", flush=True)
print("ALL nickname guards passed")
