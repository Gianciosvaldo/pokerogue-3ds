#!/usr/bin/env python3
"""Independent copy of the JoJo roll (custom-core/src/jojo.c) for tests/jojo_test.c: prints the expected results of some seeds.

usage: golden_jojo.py       (prints C literals; tests/jojo_test.c holds the printed values)
The table is the one the test uses: character index -> biome (-1 = never dealt), in the order of cc_jojo_chars."""
M = (1 << 64) - 1
SALT = 0x4A4F4A4F424F5A41
BIOMES = [-1, 14, 4, 9, 29, 7, 5, 10, 19, 1, 30, -1, -1, 24, 20, 13, 41]   # kars gyro diavolo kira pucci jolyne joseph jotaro dio koichi mista wamuu esidisi valentine polnareff risotto stroheim
P = 25


def nxt(st):
    st[0] = (st[0] + 0x9E3779B97F4A7C15) & M
    z = st[0]
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M
    return z ^ (z >> 31)


def pick(st, n):
    return nxt(st) % n


def run(seed, biomes, table=BIOMES):
    st = [(seed ^ SALT) & M]
    used, fights, out = set(), 0, []
    for b in biomes:
        cand = [i for i, x in enumerate(table) if x == b and i not in used]
        if not cand:
            out.append(-1)
            continue
        if pick(st, 100) >= P:
            out.append(-1)
            continue
        i = cand[pick(st, len(cand))]
        used.add(i)
        fights += 1
        out.append(i)
    return out, st[0]


if __name__ == "__main__":
    seqs = [[14, 4, 14, 9, 1, 30, 14, 4, 13, 41, 20, 24], [5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5], [14] * 12, [0, 12, 25, 99] * 3]
    for seed in (0, 1, 1234567, 0xDEADBEEFCAFEF00D):
        for k, b in enumerate(seqs):
            r, st = run(seed, b)
            print("seed %#x seq %d -> {%s} state %#x" % (seed, k, ", ".join(map(str, r)), st))
    # a small table with several characters in one biome: indexes 0,1,2 in biome 7, 3 in biome 7 too, 4 elsewhere
    tab = [7, 7, 7, 5]
    for seed in (0, 1, 2, 3, 4, 5, 6, 7):
        r, st = run(seed, [7] * 10 + [5, 5], tab)
        print("small seed %d -> {%s} state %#x" % (seed, ", ".join(map(str, r)), st))
