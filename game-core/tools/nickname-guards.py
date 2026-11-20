"""Planted faults for game-core's nickname test: each one is a one-line change to a source file, built and linked ahead of the library, and
tests/nickname_host_test.c must fail on every one (exit 1 with a 'nickname FAIL' line), while the unchanged code passes it.
Usage: python3 game-core/tools/nickname-guards.py [OUT_DIR]   (from the repository root, after `make -C game-core build/libgc.a`)."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
out = Path(sys.argv[1] if len(sys.argv) > 1 else root / "build" / "nickname-guards").resolve()
out.mkdir(parents=True, exist_ok=True)
libs = [root / "build/libgc.a", root / "build/reward-core/libreward.a", root / "build/ai-core/libai.a", root / "build/run-core/librun.a",
        root / "build/run-core/battle-core/libprcore.a"]
flags = ["-I" + str(root / p) for p in ("include", "../run-core/include", "../ai-core/include", "../reward-core/include", "../reward-core/gen/out",
                                      "../battle-core/include", "../battle-core/gen/out")]
flags += ["-O2", "-g", "-std=c99", "-Wall", "-Wextra", "-Wno-unused-parameter", "-ffp-contract=off"]

# (name, file, original, replacement)
# Not listed, because no input can tell them apart from the code: the byte limit in gc_nickname_valid and the terminator check in check_nicknames are
# implied by the 20-unit limit (81 bytes of text are always more than 20 units); they only keep a read of damaged data inside the buffer.
faults = [
    ("starter-nickname-dropped", "gc_mon.c", "if (gc_nickname_valid(s->nickname)) memcpy(p->nickname, s->nickname, strlen(s->nickname));", ""),
    ("emoji-counts-one", "gc_mon.c", "units += cp >= 0x10000 ? 2 : 1;", "units += 1;"),
    ("c1-controls-allowed", "gc_mon.c", "(cp >= 0x80 && cp < 0xA0)) return false;", "0) return false;"),
    ("overlong-allowed", "gc_mon.c", "(n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) || ", ""),
    ("surrogates-allowed", "gc_mon.c", "(cp >= 0xD800 && cp <= 0xDFFF)) return false;", "0) return false;"),
    ("continuation-unchecked", "gc_mon.c", "if ((p[i] & 0xC0) != 0x80) return false;", ""),
    ("set-leaves-tail", "gc_mon.c", "memset(dst, 0, PR_NICKNAME_BYTES);", ""),
    ("set-takes-bad-text", "gc_mon.c", "idx >= 6 || !gc_nickname_valid(text)) return false;", "idx >= 6) return false;"),
    ("set-takes-bad-slot", "gc_mon.c", "if (!run || idx < 0 || idx >= run->n_party || idx >= 6", "if (!run || idx < 0 || idx >= 6"),
    ("starter-list-takes-bad-nickname", "gc_run.c", "if (!gc_nickname_valid(starters[i].nickname)) { *why = \"the starter's nickname is not valid\"; return false; }", ""),
    ("section-always-written", "gc_save.c", "if(any_nickname(run->party,run->n_party)){", "if(1){"),
    ("party-nicknames-not-saved", "gc_save.c", "if(any_nickname(run->party,run->n_party)){", "if(0){"),
    ("party-nicknames-not-checked", "gc_save.c", "check_nicknames(run->party,run->n_party,&dropped);", ""),
    ("dropped-not-counted", "gc_save.c", "memset(p->nickname,0,sizeof p->nickname);(*dropped)++;", "memset(p->nickname,0,sizeof p->nickname);"),
    ("chip-nicknames-not-saved", "gc_save.c", "if(any_nickname(in->chips.mon,in->chips.n)){", "if(0){"),
    ("chip-nicknames-not-checked", "gc_save.c", "check_nicknames(chip_mons,d.n,&dropped);", ""),
    ("evolved-name-is-the-nickname", "gc_phases.c", "pr_arg_pokemon_plain_name(&run->bt, \"evolvedPokemonName\", &m->b)", "pr_arg_pokemon_name(&run->bt, \"evolvedPokemonName\", &m->b)"),
]

test = root / "tests/nickname_host_test.c"
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
    caught = r.returncode != 0 and "nickname FAIL" in r.stderr
    print(("caught " if caught else "MISSED ") + name + ("" if caught else f"  (exit {r.returncode})"))
    if not caught:
        missed.append(name)
if missed:
    sys.exit("planted faults not caught: " + ", ".join(missed))
print(f"all {len(faults)} planted faults caught")
