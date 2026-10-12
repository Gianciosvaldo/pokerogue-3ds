#!/usr/bin/env python3
"""Independent reference for custom-core's rematch draw (algorithm 1, docs/CUSTOM-CONTENT-CONTRACTS.md section 2.11).

The C code (src/scheduler.c) and this script are written separately; tests/scheduler_test.c holds the literal values this script
prints, so a change to the C algorithm is caught by the table, not by a second copy of itself.

  python3 custom-core/tools/golden_rematch.py          # prints the values used by tests/scheduler_test.c

Algorithm 1: SplitMix64 (Steele, Lea and Flood, 2014; the constants of the public-domain reference implementation).
  state += 0x9E3779B97F4A7C15 ; z = state ; z = (z ^ z >> 30) * 0xBF58476D1CE4E5B9 ; z = (z ^ z >> 27) * 0x94D049BB133111EB ;
  output = z ^ z >> 31          (all arithmetic modulo 2^64)
The single draw of a run is the first output; the chosen candidate is frozen[output % n].
"""
M = (1 << 64) - 1


def next_(state):
    state = (state + 0x9E3779B97F4A7C15) & M
    z = state
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M
    return state, z ^ (z >> 31)


SEEDS = [0, 1, 2, 12345, 0x0123456789ABCDEF, 0xFFFFFFFFFFFFFFFF]

if __name__ == "__main__":
    for seed in (0, 1234567):
        st, outs = seed, []
        for _ in range(5):
            st, o = next_(st)
            outs.append(o)
        print("outputs of seed", seed, [hex(o) for o in outs], [o for o in outs])
    print("seed, state after one draw, first output, then output % n for n = 1..16")
    for seed in SEEDS:
        st, o = next_(seed)
        print("    {0x%016XULL, 0x%016XULL, 0x%016XULL, {%s}}," % (seed, st, o, ", ".join(str(o % n) for n in range(1, 17))))
