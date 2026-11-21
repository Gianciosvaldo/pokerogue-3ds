"""Planted faults for game-core's starter nickname test: each one is a one-line change to a source file, built and linked ahead of the library, and
tests/starter_nickname_host_test.c must fail on every one (exit 1 with a 'starter nickname FAIL' line), while the unchanged code passes it.
Usage: python3 game-core/tools/starter-nickname-guards.py [OUT_DIR]   (from the repository root, after `make -C game-core build/libgc.a`)."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
out = Path(sys.argv[1] if len(sys.argv) > 1 else root / "build" / "starter-nickname-guards").resolve()
out.mkdir(parents=True, exist_ok=True)
libs = [root / "build/libgc.a", root / "build/reward-core/libreward.a", root / "build/ai-core/libai.a", root / "build/run-core/librun.a",
        root / "build/run-core/battle-core/libprcore.a"]
flags = ["-I" + str(root / p) for p in ("include", "../run-core/include", "../ai-core/include", "../reward-core/include", "../reward-core/gen/out",
                                      "../battle-core/include", "../battle-core/gen/out")]
flags += ["-O2", "-g", "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter", "-ffp-contract=off"]

# (name, file, original, replacement)
# Not listed, because the test cannot tell them from the code by a failed check: the clamp of the stored count (nick_count, and the same in the saver:
# without it the test's own corrupted count crashes it, a crash and not a 'FAIL' line), the byte limit in the setter (the 20-unit limit already keeps a
# text under 61 bytes) and the terminator check when loading (a text with no terminator in its 61 bytes is never valid); they only keep a read of
# damaged memory or data inside the buffer.
faults = [
    ("lower-bound-skips-the-match", "gc_system.c", "if (sys->starter_nicknames[mid].species < species) lo = mid + 1; else hi = mid;",
     "if (sys->starter_nicknames[mid].species <= species) lo = mid + 1; else hi = mid;"),
    ("getter-ignores-the-species", "gc_system.c", "return at < n && sys->starter_nicknames[at].species == species ? sys->starter_nicknames[at].text : \"\";",
     "return at < n ? sys->starter_nicknames[at].text : \"\";"),
    ("set-takes-a-non-starter", "gc_system.c", "if (!sys || gc_starter_cost(species) < 0) return GC_STARTER_NICK_NOT_A_STARTER;",
     "if (!sys) return GC_STARTER_NICK_NOT_A_STARTER;"),
    ("set-takes-bad-text", "gc_system.c", "if (!gc_nickname_valid(text) || (text && strlen(text) >= GC_STARTER_NICKNAME_BYTES)) return GC_STARTER_NICK_BAD_TEXT;",
     "if (text && strlen(text) >= GC_STARTER_NICKNAME_BYTES) return GC_STARTER_NICK_BAD_TEXT;"),
    ("set-stores-empty-text", "gc_system.c", "if (!text || !text[0]) {", "if (!text) {"),
    ("set-appends-a-duplicate", "gc_system.c", "const bool have = at < n && e[at].species == species;", "const bool have = false;"),
    ("set-leaves-a-tail", "gc_system.c", "memset(&e[at], 0, sizeof e[at]);", "(void)0;"),
    ("insert-makes-no-room", "gc_system.c", "memmove(&e[at + 1], &e[at], (size_t)(n - at) * sizeof e[0]);", "(void)0;"),
    ("insert-keeps-the-count", "gc_system.c", "sys->n_starter_nicknames = n + 1;", "(void)0;"),
    ("full-table-takes-one-more", "gc_system.c", "if (n >= GC_STARTER_NICKNAMES) return GC_STARTER_NICK_FULL;",
     "if (n > GC_STARTER_NICKNAMES) return GC_STARTER_NICK_FULL;"),
    ("clear-leaves-a-gap", "gc_system.c", "memmove(&e[at], &e[at + 1], (size_t)(n - at - 1) * sizeof e[0]);", "(void)0;"),
    ("clear-leaves-the-slot", "gc_system.c", "memset(&e[n - 1], 0, sizeof e[0]);", "(void)0;"),
    ("clear-keeps-the-count", "gc_system.c", "sys->n_starter_nicknames = n - 1;", "(void)0;"),
    ("section-always-written", "gc_save.c", "if (sys->n_starter_nicknames > 0) {", "if (1) {"),
    ("section-never-written", "gc_save.c", "if (sys->n_starter_nicknames > 0) {", "if (0) {"),
    ("last-nickname-not-saved", "gc_save.c", "sys->starter_nicknames, sizeof sys->starter_nicknames[0], n);", "sys->starter_nicknames, sizeof sys->starter_nicknames[0], n - 1);"),
    ("section-not-loaded", "gc_save.c", "r = section(&body, \"starter_nicknames\");", "r = section(&body, \"starter_nicknamez\");"),
    ("load-keeps-empty-text", "gc_save.c", "nick[i].text[0] &&\n", "\n"),
    ("bad-nickname-refuses-the-profile", "gc_save.c", "if (!kept) dropped++;", "if (!kept) { free(nick); *why = \"x\"; return false; }"),
]

test = root / "tests/starter_nickname_host_test.c"
exe = out / "baseline"
cc = ["cc", *flags, str(test), *map(str, libs), "-lm", "-o", str(exe)]
subprocess.run(cc, check=True)
base = subprocess.run([str(exe)], capture_output=True, text=True, errors="replace")
if base.returncode != 0:
    sys.exit("the unchanged code fails its own test:\n" + base.stderr)
print("baseline:", base.stdout.strip())

missed = []
for name, file, a, b in faults:
    source = (root / "src" / file).read_text()
    if source.count(a) != 1:
        sys.exit(f"{name}: the original text is not in src/{file} exactly once")
    mutant = out / f"mutant-{name}.c"
    mutant.write_text(source.replace(a, b, 1))
    obj = out / f"mutant-{name}.o"
    # `#include "internal.h"` and the other local headers are found through -Isrc; the mutant's object is linked ahead of the library, so the
    # library's copy of the same file is not pulled in
    subprocess.run(["cc", *flags, "-w", "-I" + str(root / "src"), "-c", str(mutant), "-o", str(obj)], check=True)
    exe = out / f"mutant-{name}"
    subprocess.run(["cc", *flags, str(test), str(obj), *map(str, libs), "-lm", "-o", str(exe)], check=True)
    r = subprocess.run([str(exe)], capture_output=True, text=True, errors="replace")
    caught = r.returncode != 0 and "starter nickname FAIL" in r.stderr
    print(("caught " if caught else "MISSED ") + name + ("" if caught else f"  (exit {r.returncode})"))
    if not caught:
        missed.append(name)
if missed:
    sys.exit("planted faults not caught: " + ", ".join(missed))
print(f"all {len(faults)} planted faults caught")
