#!/usr/bin/env python3
"""Mutation check of the starter select's form and gender cycling, the same method as capsule_mutations.py.

Each mutation is one textual change to a source file, applied in place, built into the host test that should notice it, run, and always
restored. KILLED: it builds and the test fails. SURVIVED: it builds and the test still passes. INVALID: the pattern did not match exactly
once, or the build failed (nothing was shown either way). Exit 1 on any SURVIVED or INVALID.

  python3 game/tests/starter_cycle_mutations.py            # every mutation
  python3 game/tests/starter_cycle_mutations.py 3 7        # only these numbers

Every run builds a host test with the address and undefined-behaviour sanitizers from all of game-core: about a minute each.
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GAME_TESTS = os.path.join(ROOT, "game", "tests")
CORE = os.path.join(ROOT, "game-core")

SD = "game/source/starter_data.c"
SC = "game/menus/starter.c"
SH = "game/source/starter_host.c"
GS = "game-core/src/gc_save.c"
GD = "game-core/gen/out/gc_dex.c"

DT = (GAME_TESTS, "starter_data_test", ["build/starter_data_test"])
UT = (GAME_TESTS, "starter_ui_test", ["build/starter_ui_test"])
GT = (CORE, "starter-options-test", ["build/starter_options_test", "tests/fixtures/starter-options.native.txt"])

# (file under the repo, old text, new text, (cwd, make target, command), what the mutation is)
M = [
    # ---- what is stored is used only while it is unlocked (initStarterPrefs :680-715)
    (SD, "(attr & (pref->gender_preference ? DEX_FEMALE : DEX_MALE))) p->female", "1) p->female", DT, "a stored gender is used whether or not that sex was caught"),
    (SD, "  if ((pref->preference_flags & PREF_GENDER) &&", "  if (", DT, "a gender value is used without its flag"),
    (SD, "p->female = pref->gender_preference != 0;", "p->female = true;", UT, "a stored male is read as female"),
    (SD, "((gc_species_starter_forms(species) >> pref->form_preference) & 1) && (attr", "(attr", DT, "a stored form the starter select does not offer is used (Gigantamax Pikachu)"),
    (SD, "((gc_species_starter_forms(species) >> pref->form_preference) & 1) && (attr & (DEX_FORM_0 << pref->form_preference));", "((gc_species_starter_forms(species) >> pref->form_preference) & 1);", DT, "a stored form that is not caught is used"),
    (SD, "const bool form_pref = (pref->preference_flags & PREF_FORM) &&", "const bool form_pref =", DT, "a form value is used without its flag"),
    # ---- when the buttons are offered (updateCanCycle :2846)
    (SD, "return (a & DEX_MALE) && (a & DEX_FEMALE);", "return (a & DEX_MALE) || (a & DEX_FEMALE);", DT, "the gender button is offered with one sex caught"),
    (SD, "return caught_count > 1;", "return caught_count > 0;", DT, "the form button is offered with one form caught"),
    (SD, "    if (!form_selectable(species, f) && keyed) continue;", "    if (!form_selectable(species, f)) continue;", DT, "forms of a species no form change leads from are dropped"),
    # ---- what the buttons do (processCycleButtonsInput :1151)
    (SD, "    if (ss_form_offered(sys, species, next)) break;", "    if (form_selectable(species, next)) break;", DT, "the form button stops on a form that is not caught"),
    (SD, "    if (ss_form_offered(sys, species, next)) break;", "    if (sys->dex[species].caught_attr & (DEX_FORM_0 << next)) break;", DT, "the form button stops on a form that is not offered"),
    (SD, "    next = (next + 1) % n;", "    next = (next + 1) % (n > 1 ? n - 1 : n);", DT, "the form button never reaches the last form"),
    (SD, "    next = (next + 1) % n;", "    next = (next + n - 1) % n;", DT, "the form button goes backwards"),
    (SD, "set_gender(&p, now.sp, !(p.female_set && p.female));", "set_gender(&p, now.sp, true);", DT, "the gender button only ever makes female"),
    (SD, "  if (has_female_form(sp)) {\n    const bool female = form == 1;", "  if (0 && has_female_form(sp)) {\n    const bool female = form == 1;", DT, "choosing Indeedee's form does not choose its sex"),
    (SD, "  if (has_female_form(sp)) {\n    const int form = female ? 1 : 0;", "  if (0 && has_female_form(sp)) {\n    const int form = female ? 1 : 0;", DT, "choosing Indeedee's sex does not choose its form"),
    (SD, 'if (!strcmp(pr_forms[sp->form0 + f].form_key, "female")) return true;', 'if (!strcmp(pr_forms[sp->form0 + f].form_key, "male")) return true;', DT, "the species whose sexes are forms are not recognised"),
    (SD, "(p->form_set ? PREF_FORM : 0)", "0", DT, "the form's flag is not stored"),
    (SD, "e->gender_preference = p->female_set && p->female ? 1 : 0;", "e->gender_preference = 0;", DT, "the chosen sex is not stored"),
    (SD, "  e->form_preference = p->form_set ? p->form : 0;", "  e->form_preference = 0;", DT, "the chosen form is not stored"),
    # ---- the screen and the host
    (SC, "    const uint16_t sp = S.m.focus == SM_GRID ? sm_species(&S.m) : 0;", "    const uint16_t sp = sm_species(&S.m);", UT, "the buttons act with the cursor on the team, Start or Randomize"),
    (SC, '    mui_sfx("ui/select");\n    starter_refresh(); /* the panel', '    starter_refresh(); /* the panel', UT, "a cycle plays no sound"),
    (SC, "    starter_refresh(); /* the panel, its sprite and the icon show the new form */\n", "", UT, "the panel keeps showing the old form"),
    (SC, "    if (!sp || !S.ev.cycle || !S.ev.cycle(S.ev.ctx, sp, b)) return false;", "    if (!sp || !S.ev.cycle) return false;\n    S.ev.cycle(S.ev.ctx, sp, b);", UT, "a button that did nothing is reported as pressed"),
    (SH, "  if(b==MB_CYCLE_FORM ? !ss_can_cycle_form(H.sys,species) : !ss_can_cycle_gender(H.sys,species))return false;\n", "", UT, "the host cycles a species the button is not offered for"),
    (SH, "if(!(b==MB_CYCLE_FORM ? ss_cycle_form(H.sys,species) : ss_cycle_gender(H.sys,species)))", "if(!(b==MB_CYCLE_FORM ? ss_cycle_gender(H.sys,species) : ss_cycle_form(H.sys,species)))", UT, "L cycles the gender and ZL the form"),
    (SH, "  if(!editor_save()) {*p=before;rebuild();starter_message", "  if(!editor_save()) {rebuild();starter_message", UT, "a change that could not be saved is kept"),
    (SH, "  rebuild();\n  return true;\n}\nstatic void configure", "  return true;\n}\nstatic void configure", UT, "the grid keeps the old form after a cycle"),
    (SH, "    if(ss_can_cycle_form(H.sys,H.editing))editor_add(EDIT_FORM,\"Form\");", "    editor_add(EDIT_FORM,\"Form\");", UT, "Customize offers Form for every species"),
    (SH, "  else if(mode==EDIT_FORM) ok=ok && ss_set_form(H.sys,H.editing,value);", "  else if(mode==EDIT_FORM) ok=ok && ss_set_gender(H.sys,H.editing,value!=0);", UT, "the Form list sets the gender"),
    (SH, "ss_set_gender(H.sys,H.editing,value!=0);\n  else if(mode==EDIT_TOP", "ss_set_gender(H.sys,H.editing,value==0);\n  else if(mode==EDIT_TOP", UT, "the Gender list is inverted"),
    # ---- the save
    (GS, "    SF(sv_starter_entry, entry.form_preference), SF(sv_starter_entry, entry.gender_preference),\n", "", GT, "the system save drops the form and gender preferences"),
    (GS, "p->form_preference>56 || p->gender_preference>1) {", "0) {", GT, "a save with an impossible form or gender is loaded"),
    (GS, "if (p->preference_flags>31 ||", "if (p->preference_flags>63 ||", GT, "a save with an unknown preference flag is loaded"),
    # ---- the data dumped from upstream
    (GD, "{25, 172, 50, true, false, 65535ULL, 255ULL},", "{25, 172, 50, true, false, 65535ULL, 511ULL},", DT, "Pikachu's Gigantamax is offered"),
    (GD, "{741, 0, 25, false, false, 1951ULL, 15ULL},", "{741, 0, 25, false, false, 1951ULL, 1ULL},", DT, "Oricorio's other styles are not offered"),
]

# mutations that cannot change what the tests can see, with why
EQUIVALENT = [
    ("a stored form the species does not have is used (the nforms test in defaults)",
     "the offered-forms mask and the caught bit are both zero past the last form, so the other two tests already refuse it"),
    ("updateCanCycle numbers the forms that pass its filter again from 0 (ss_can_cycle_form's j)",
     "no starter species has a form that is not offered before one that is while a form change leads from it, so j == f for all of them"),
    ("the gender button reads the stored female, not the resolved one",
     "with both sexes caught a stored female is always valid, so the two agree"),
]


def run(cmd, cwd, timeout=1800):
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=timeout)
    return p.returncode, p.stdout


def main():
    want = set(int(a) for a in sys.argv[1:])
    results = []
    for i, (rel, old, new, (cwd, target, cmd), what) in enumerate(M, 1):
        if want and i not in want:
            continue
        path = os.path.join(ROOT, rel)
        with open(path) as f:
            src = f.read()
        if src.count(old) != 1:
            results.append((i, "INVALID", what))
            print("%2d %-8s %s (pattern matches %d times)" % (i, "INVALID", what, src.count(old)), flush=True)
            continue
        out = ""
        try:
            with open(path, "w") as f:
                f.write(src.replace(old, new))
            rc, out = run(["make", target], cwd)
            if rc == 0:
                rc, out = run(cmd, cwd)
                status = "KILLED" if rc != 0 else "SURVIVED"
            else:
                status = "INVALID"
                out = out[-600:]
        finally:
            with open(path, "w") as f:
                f.write(src)
            os.utime(path, None)  # newer than the mutant's build, so the next make rebuilds from the real source
        results.append((i, status, what))
        print("%2d %-8s %s" % (i, status, what), flush=True)
        if status == "INVALID":
            print(out)
    for t in sorted(set((m[3][0], m[3][1]) for m in M)):
        run(["make", t[1]], t[0])
    k = sum(1 for r in results if r[1] == "KILLED")
    for what, why in EQUIVALENT:
        print("equivalent, not run: %s -- %s" % (what, why))
    print("mutations: %d run, %d killed, %d survived, %d invalid" % (len(results), k, sum(1 for r in results if r[1] == "SURVIVED"), sum(1 for r in results if r[1] == "INVALID")))
    return 0 if k == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
