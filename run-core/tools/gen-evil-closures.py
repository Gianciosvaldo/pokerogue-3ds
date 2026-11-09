#!/usr/bin/env python3
"""Writes gen/out/run_evil_closures.c: the partyMemberFuncs of the evil teams' admins and bosses and of the Elite Four and Champions.

Their closures are getRandomPartyMemberFunc([species...], TrainerSlot.TRAINER, ignoreEvolution, postProcess) written out in
src/data/trainers/trainer-config.ts of the pinned upstream, so (unlike the data the vitest dumpers read) they can't be called
to find out what they hold: this reads the source text, balanced-parenthesis by balanced-parenthesis, and fails on anything it
doesn't understand (a nested pool, another slot, a function that isn't getRandomPartyMemberFunc).  The postProcess callbacks
(ability slot, gender, forms, boss segments, balls, friendship, fixed moves and the regenerated moveset) are parsed into a short
list of ops per closure (pr_evil_ops, run by src/evil_team.c at the end of pr_run_enemy_pokemon_finish); a statement or condition
outside the grammar below fails the run instead of being skipped.

usage: tools/gen-evil-closures.py <upstream checkout> [--check]    (--check: fail if the file in the tree is out of date)
"""
import os, re, subprocess, sys

ADMINS = ("PETREL PROTON ARCHER ARIANA TABITHA COURTNEY MATT SHELLY JUPITER MARS SATURN COLRESS ZINZOLIN BRYONY ALIANA CELOSIA MABLE "
          "XEROSIC FABA PLUMERIA OLEANA GIACOMO MELA ATTICUS ORTEGA ERI").split()
BOSSES = ("ROCKET_BOSS_GIOVANNI_1 MAXIE ARCHIE CYRUS GHETSIS LYSANDRE LUSAMINE GUZMA ROSE PENNY ROCKET_BOSS_GIOVANNI_2 MAXIE_2 ARCHIE_2 "
          "CYRUS_2 GHETSIS_2 LYSANDRE_2 LUSAMINE_2 GUZMA_2 ROSE_2 PENNY_2").split()
PIN = "8555c08c823b856cbec4eb99ca84ea52a955836d"


def balanced(s, i):
    depth, quote = 0, None
    while i < len(s):
        c = s[i]
        if quote:
            if c == quote and s[i - 1] != "\\":
                quote = None
        elif c in "\"'`":
            quote = c
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise SystemExit("unbalanced parentheses")


OPS = ["IF", "REGEN", "BALL", "BOSS", "ABILITY", "GENDER", "FORM", "FORM_RAND", "FRIENDSHIP", "MOVE", "MOVE_RAND", "REPLACE_MOVE", "MOVESET", "TERA", "TERA_SPECIES", "JMP"]
BALLS = {"POKEBALL": 0, "GREAT_BALL": 1, "ULTRA_BALL": 2, "ROGUE_BALL": 3, "MASTER_BALL": 4, "LUXURY_BALL": 5}
GENDERS = {"GENDERLESS": "GENDER_GENDERLESS", "MALE": "GENDER_MALE", "FEMALE": "GENDER_FEMALE"}


def braced(s, i):
    """index of the } matching the { at s[i]"""
    depth = 0
    while i < len(s):
        if s[i] == "{":
            depth += 1
        elif s[i] == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise SystemExit("unbalanced braces")


class Post:
    def __init__(self, where):
        self.where, self.ops, self.items = where, [], []

    def fail(self, what):
        raise SystemExit("postProcess of %s: %s" % (self.where, what))

    def emit(self, op, **kw):
        d = dict(op=op, g="PR_EG_NONE", gv="0", a="0", b="0", c="0")
        d.update(kw)
        self.ops.append(d)
        return len(self.ops) - 1

    def cond(self, text):
        t = re.sub(r"\s+", " ", text.strip())
        m = re.fullmatch(r"p\.species\.speciesId === SpeciesId\.([A-Z0-9_]+)", t)
        if m:
            return ("PR_EG_SPECIES_EQ", "SPECIES_ID_" + m.group(1), "PR_EG_SPECIES_NE")
        m = re.fullmatch(r"globalScene\.currentBattle\.waveIndex === ClassicFixedBossWaves\.([A-Z0-9_]+)", t)
        if m:
            return ("PR_EG_WAVE_EQ", "CLASSIC_FIXED_BOSS_WAVES_" + m.group(1), "PR_EG_WAVE_NE")
        m = re.fullmatch(r"!p\.moveset\.some\(move => move\.moveId === MoveId\.([A-Z0-9_]+)\)", t)
        if m:
            return ("PR_EG_LACKS_MOVE", "MOVE_ID_" + m.group(1), None)
        m = re.fullmatch(r"p\.formIndex === (\d+)", t)
        if m:
            return ("PR_EG_FORM_EQ", m.group(1), None)
        m = re.fullmatch(r"p\.species\.speciesId === SpeciesId\.([A-Z0-9_]+) && !p\.moveset\.some\(move => move\.moveId === MoveId\.([A-Z0-9_]+)\)", t)
        if m:
            return ("PR_EG_SPECIES_LACKS", "SPECIES_ID_" + m.group(1), "MOVE_ID_" + m.group(2))
        self.fail("condition outside the grammar: " + t)

    def block(self, s):
        i = 0
        while True:
            m = re.compile(r"\s*").match(s, i)
            i = m.end()
            if i >= len(s):
                return
            if re.match(r"if\b", s[i:]):
                i = self.if_chain(s, i)
            elif re.match(r"switch\b", s[i:]):
                i = self.switch(s, i)
            else:
                j = s.index(";", i)
                self.stmt(s[i:j].strip())
                i = j + 1

    def if_chain(self, s, i):
        """if (c) {..} [else if (c) {..}]* [else {..}] from s[i]; returns the index after it.  Each branch is an IF that skips its body (and the
        JMP that ends it) when its condition fails; a JMP at the end of every branch but the last skips the branches after it"""
        branches = []   # (guard, body) ; guard None = the final else
        while True:
            m = re.compile(r"if\s*\(").match(s, i)
            if m:
                a = m.end() - 1
                b = balanced(s, a)
                guard = self.cond(s[a + 1:b])
                k = s.index("{", b)
                e = braced(s, k)
                branches.append((guard, s[k + 1:e]))
            else:
                m = re.compile(r"\{").match(s, i)
                if not m:
                    self.fail("else outside the grammar")
                e = braced(s, i)
                branches.append((None, s[i + 1:e]))
                return self.emit_chain(branches, e + 1)
            i = e + 1
            m2 = re.compile(r"\s*else\s*").match(s, i)
            if not m2:
                return self.emit_chain(branches, i)
            i = m2.end()

    def emit_chain(self, branches, end):
        jumps = []
        for n, (guard, body) in enumerate(branches):
            if guard is None:
                self.block(body)
                continue
            at = self.emit("IF")
            self.ops[at]["g"], self.ops[at]["gv"] = guard[0], guard[1]
            if guard[0] == "PR_EG_SPECIES_LACKS":
                self.ops[at]["b"] = guard[2]
            before = len(self.ops)
            self.block(body)
            if n + 1 < len(branches):
                jumps.append(self.emit("JMP"))
            self.ops[at]["a"] = str(len(self.ops) - before)
        for j in jumps:
            self.ops[j]["a"] = str(len(self.ops) - j - 1)
        return end

    def guarded(self, kind, val, body):
        at = self.emit("IF", g=None)
        self.ops[at]["g"], self.ops[at]["gv"] = kind, val
        before = len(self.ops)
        self.block(body)
        self.ops[at]["a"] = str(len(self.ops) - before)

    def switch(self, s, i):
        m = re.compile(r"switch\s*\(p\.formIndex\)\s*\{").match(s, i)
        if not m:
            self.fail("switch outside the grammar")
        k = m.end() - 1
        e = braced(s, k)
        body = s[k + 1:e]
        for cm in re.finditer(r"case (\d+):(.*?)break;", body, re.S):
            self.guarded("PR_EG_FORM_EQ", cm.group(1), cm.group(2))
        if re.sub(r"case (\d+):(.*?)break;", "", body, flags=re.S).strip():
            self.fail("switch with something besides case/break")
        return e + 1

    def move(self, expr):
        expr = expr.strip()
        m = re.fullmatch(r"new PokemonMove\(MoveId\.([A-Z0-9_]+)\)", expr)
        if m:
            return ("MOVE", "MOVE_ID_" + m.group(1))
        m = re.fullmatch(r"new PokemonMove\(randSeedItem\(\[([^\]]*)\]\)\)", expr)
        if m:
            names = re.findall(r"MoveId\.([A-Z0-9_]+)", m.group(1))
            if len(names) != len([x for x in m.group(1).split(",") if x.strip()]) or len(names) < 2:
                self.fail("odd randSeedItem list " + expr)
            return ("MOVE_RAND", ["MOVE_ID_" + n for n in names])
        self.fail("move outside the grammar: " + expr)

    def stmt(self, t):
        t = re.sub(r"\s+", " ", t)
        if t == "p.generateAndPopulateMoveset()":
            self.emit("REGEN")
        elif t == "p.generateName()":
            pass   # the display name only; it is derived from species and form wherever it is shown
        elif (m := re.fullmatch(r"p\.pokeball = PokeballType\.([A-Z_]+)", t)):
            self.emit("BALL", a=str(BALLS[m.group(1)]))
        elif (m := re.fullmatch(r"p\.setBoss\(true, (\d+)\)", t)):
            self.emit("BOSS", a=m.group(1))
        elif (m := re.fullmatch(r"p\.friendship = (\d+)", t)):
            self.emit("FRIENDSHIP", a=m.group(1))
        elif (m := re.fullmatch(r"p\.(abilityIndex|formIndex|gender|teraType) = (.*)", t)):
            field, rhs = m.groups()
            op = {"abilityIndex": "ABILITY", "formIndex": "FORM", "gender": "GENDER", "teraType": "TERA"}[field]
            # cond ? x : y on the species (one species, or two joined by ||): two guarded assignments
            t2 = re.fullmatch(r"((?:p\.species\.speciesId === SpeciesId\.[A-Z0-9_]+(?: \|\| )?)+) \? (.+?) : (.+)", rhs)
            if t2:
                names = re.findall(r"SpeciesId\.([A-Z0-9_]+)", t2.group(1))
                if len(names) > 2:
                    self.fail("more than two species in a ternary: " + t)
                for kind, val in ((("PR_EG_SPECIES_EQ", "PR_EG_SPECIES_EQ2")[len(names) - 1], t2.group(2)),
                                  (("PR_EG_SPECIES_NE", "PR_EG_SPECIES_NE2")[len(names) - 1], t2.group(3))):
                    at = self.emit("IF")
                    self.ops[at]["g"], self.ops[at]["gv"], self.ops[at]["a"] = kind, "SPECIES_ID_" + names[0], "1"
                    if len(names) == 2:
                        self.ops[at]["b"] = "SPECIES_ID_" + names[1]
                    self.assign(op, val)
            else:
                self.assign(op, rhs)
        elif (m := re.fullmatch(r"p\.moveset\[(\d)\] = (.*)", t)):
            kind, val = self.move(m.group(2))
            if kind == "MOVE":
                self.emit("MOVE", a=m.group(1), b=val)
            else:
                self.emit("MOVE_RAND", a=m.group(1), b=str(len(val)), c=str(len(self.items)))
                self.items.extend(val)
        elif (m := re.fullmatch(r"replaceInMoveset\(p\.moveset, MoveId\.([A-Z0-9_]+), MoveId\.([A-Z0-9_]+)\)", t)):
            self.emit("REPLACE_MOVE", a="MOVE_ID_" + m.group(1), b="MOVE_ID_" + m.group(2))
        elif (m := re.fullmatch(r"p\.moveset = \[(.*)\]", t)):
            names = re.findall(r"new PokemonMove\(MoveId\.([A-Z0-9_]+)\)", m.group(1))
            if len(names) != 4 or m.group(1).count("new PokemonMove") != 4:
                self.fail("moveset array outside the grammar: " + t)
            self.emit("MOVESET", a=str(len(self.items)))
            self.items.extend("MOVE_ID_" + n for n in names)
        else:
            self.fail("statement outside the grammar: " + t)

    def assign(self, op, rhs):
        rhs = rhs.strip()
        if op == "GENDER":
            m = re.fullmatch(r"Gender\.([A-Z]+)", rhs)
            if not m:
                self.fail("gender " + rhs)
            self.emit("GENDER", a=GENDERS[m.group(1)])
        elif re.fullmatch(r"\d+", rhs):
            self.emit(op, a=rhs)
        elif op == "FORM" and (m := re.fullmatch(r"randSeedInt\((\d+)(?:, (\d+))?\)", rhs)):
            self.emit("FORM_RAND", a=m.group(1), b=m.group(2) or "0")
        elif op == "FORM" and (m := re.fullmatch(r"randSeedIntRange\((\d+), (\d+)\)", rhs)):
            lo, hi = int(m.group(1)), int(m.group(2))   # randSeedInt(hi - lo + 1, lo)
            self.emit("FORM_RAND", a=str(hi - lo + 1), b=str(lo))
        elif op == "TERA" and (m := re.fullmatch(r"PokemonType\.([A-Z]+)", rhs)):
            self.emit("TERA", a="POKEMON_TYPE_" + m.group(1))
        elif op == "TERA" and (m := re.fullmatch(r"p\.species\.type([12])!?", rhs)):
            self.emit("TERA_SPECIES", a=m.group(1))
        else:
            self.fail("%s = %s" % (op, rhs))


def post_ops(where, args):
    """the ops of the postProcess in a getRandomPartyMemberFunc's argument text (none: an empty list)"""
    m = re.search(r"\bp\s*=>\s*", args)
    if not m:
        return None
    body = args[m.end():].strip()
    body = re.sub(r"//[^\n]*", "", body)
    if body.startswith("{"):
        body = body[1:body.rindex("}")]
    else:
        body += ";"
    post = Post(where)
    post.block(body)
    return post


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    up = args[0]
    head = subprocess.check_output(["git", "-C", up, "rev-parse", "HEAD"], text=True).strip()
    if head != PIN:
        raise SystemExit("wrong upstream pin " + head)
    src = open(os.path.join(up, "src/data/trainers/trainer-config.ts")).read()
    starts = [(m.start(), m.group(1)) for m in re.finditer(r"\n  \[TrainerType\.([A-Z0-9_]+)\]: new TrainerConfig\(", src)]
    blocks = {n: src[s:(starts[i + 1][0] if i + 1 < len(starts) else len(src))] for i, (s, n) in enumerate(starts)}
    rows, species, ops, items = [], [], [], []
    # the Elite Four and Champions (initForEliteFour / initForChampion) have the same partyMemberFuncs; the gym leaders (initForGymLeader) have none
    # but are scanned all the same, so one that gains a closure fails the run instead of being skipped
    fixed = [n for _, n in starts if re.search(r"\.initFor(EliteFour|Champion|GymLeader|StatTrainer)\(", blocks[n]) and n not in ADMINS + BOSSES]
    for n in ADMINS + BOSSES + fixed:
        block = re.sub(r"(?m)^(\s*)//[^\n]*|(?<=[;,{(\s])//[^\n]*", r"\1", blocks[n])   # a comment can hold an apostrophe
        seen = set()
        for m in re.finditer(r"\.setPartyMemberFunc\(", block):
            a = m.end() - 1
            body = block[a + 1:balanced(block, a)]
            idx_s, call = body.split(",", 1)
            idx, call = int(idx_s.strip()), call.strip()
            if not call.startswith("getRandomPartyMemberFunc(") or idx in seen:
                raise SystemExit("unexpected closure %s %d" % (n, idx))
            seen.add(idx)
            a2 = call.index("(")
            cargs = call[a2 + 1:balanced(call, a2)]
            if not cargs.lstrip().startswith("["):
                raise SystemExit("pool not a list: %s %d" % (n, idx))
            s0 = cargs.index("[")
            depth = 0
            for j in range(s0, len(cargs)):
                depth += (cargs[j] == "[") - (cargs[j] == "]")
                if depth == 0:
                    e = j
                    break
            pool, rest = cargs[s0 + 1:e], cargs[e + 1:]
            names = re.findall(r"SpeciesId\.([A-Z0-9_]+)", pool)
            if "[" in pool or len(names) != len([x for x in pool.split(",") if x.strip()]):
                raise SystemExit("nested or odd pool: %s %d" % (n, idx))
            slot = re.search(r"TrainerSlot\.([A-Z_]+)", rest)
            if slot and slot.group(1) != "TRAINER":
                raise SystemExit("slot %s: %s %d" % (slot.group(1), n, idx))
            ign = re.match(r"\s*,\s*TrainerSlot\.[A-Z_]+\s*,\s*(true|false)", rest)
            post = post_ops("%s %d" % (n, idx), cargs)
            op_start = len(ops)
            if post:
                for o in post.ops:
                    if o["op"] == "MOVE_RAND":
                        o["c"] = str(int(o["c"]) + len(items))
                    elif o["op"] == "MOVESET":
                        o["a"] = str(int(o["a"]) + len(items))
                    ops.append(o)
                items.extend(post.items)
            rows.append((n, idx, 1 if ign and ign.group(1) == "true" else 0, len(species), len(names), op_start, len(ops) - op_start))
            species.extend(names)
    out = ["/* Generated by tools/gen-evil-closures.py from pagefaultgames/pokerogue %s src/data/trainers/trainer-config.ts: the admins' and bosses' of the" % PIN[:7],
           " * evil teams partyMemberFuncs, getRandomPartyMemberFunc(pool, TrainerSlot.TRAINER, ignoreEvolution, ...). The postProcess callbacks are ops (below). Do not edit. */",
           '#include "pr/run.h"', "",
           "const uint16_t pr_evil_closure_species[%d] = {" % len(species)]
    for i in range(0, len(species), 4):
        out.append("    " + " ".join("SPECIES_ID_%s," % s for s in species[i:i + 4]))
    out += ["};", "const int pr_evil_closure_count = %d;" % len(rows), "const pr_evil_closure pr_evil_closures[%d] = {" % len(rows)]
    out += ["    { TRAINER_TYPE_%s, %d, %d, %d, %d, %d, %d }," % r for r in rows]
    out += ["};", "const pr_evil_op pr_evil_ops[%d] = {" % max(len(ops), 1)]
    out += ["    { PR_EOP_%s, %s, %s, %s, %s, %s }," % (o["op"], o["g"], o["gv"], o["a"], o["b"], o["c"]) for o in ops] or ["    { 0 },"]
    out += ["};", "const uint16_t pr_evil_op_items[%d] = {" % max(len(items), 1)]
    for i in range(0, len(items), 4):
        out.append("    " + " ".join("%s," % x for x in items[i:i + 4]))
    out += ["};", ""]
    text = "\n".join(out)
    dest = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gen", "out", "run_evil_closures.c")
    if "--check" in sys.argv:
        if open(dest).read() != text:
            raise SystemExit("gen/out/run_evil_closures.c is out of date")
        print("up to date: %d closures, %d species entries, %d ops" % (len(rows), len(species), len(ops)))
    else:
        open(dest, "w").write(text)
        print("wrote %s: %d closures, %d species entries, %d ops" % (dest, len(rows), len(species), len(ops)))


main()
