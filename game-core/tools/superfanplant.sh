#!/bin/sh
# The Bug-Type Superfan, options 2 and 3: show the comparator (and the host test) can fail. Replays one trace (from tools/superfandiff.sh) through copies of game-core, each with one line of src/gc_superfan.c (or gc_mystery.c) changed, and prints each one's totals. Every copy must be caught: by tests/mystery_superfan_diff_test.c exiting 1, or, for what the oracle does not play (the screen's wording arguments, a rejected pick, the choice through gc_run_choose), by tests/mystery_superfan_test.c failing.
# Not planted (equivalent mutants or not reachable by the oracle): the Tera type left out of the Bug count (nothing out of battle is Terastallized), swapping the common and the rare evolution item generators (the common one never makes an item for the oracle's parties, so the pair is never both on), the stand-in for option 1 (the fight, not ported yet), the unused gc_superfan_pre_rng export, the Golden Bug Net's argument (the item has none), gc_items_to_battle in the gift (the update right after it does the same), the spawn test in the encounter pool (static in gc_mystery.c, the oracle starts the encounter directly), the pick range check at idx == n_party (that slot holds no item), the check of the pick in the execute (the screen's filter and the list length reject the same picks), and option 2's zero-Bug check there (gc_run_choose refuses the option first).
# Usage: tools/superfanplant.sh <trace>   (SUPERFAN_PLANT_FROM=<name> starts at that plant, SUPERFAN_PLANT_ONLY="a b" runs those)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-superfan-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_superfan.c}
  [ "$NAME" != "${SUPERFAN_PLANT_FROM:-}" ] || started=1
  [ -z "${SUPERFAN_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${SUPERFAN_PLANT_ONLY:-}" ] || case " $SUPERFAN_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  case "$FILE" in
    battle-core/*) rm "$W/battle-core"; (cd "$ROOT" && tar cf - --exclude=./battle-core/build battle-core) | tar xf - -C "$W"; TARGET="$W/$FILE"; ORIG="$ROOT/$FILE";;
    *) TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE";;
  esac
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" BATTLE_CORE="$W/battle-core" "$W/build/mystery_superfan_diff_test" "$W/build/mystery_superfan_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_superfan_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_superfan_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}


BUG='/^static bool bug_item/,/^}/'
plant bug-quick "$BUG"'s#!strcmp(it->key, "QUICK_CLAW") || ##'
plant bug-grip "$BUG"'s#!strcmp(it->key, "GRIP_CLAW") || ##'
plant bug-booster-type "$BUG"'s#it->arg == POKEMON_TYPE_BUG#it->arg == POKEMON_TYPE_FIRE#'
plant bug-booster-key "$BUG"'s#!strcmp(it->key, "ATTACK_TYPE_BOOSTER") && ##'
plant count-bug-type 's#if (ty\[k\] == POKEMON_TYPE_BUG) return true;#if (ty[k] == POKEMON_TYPE_BUG \&\& m->b.hp > 0) return true;#'
plant pick-low '/^bool gc_superfan_pick_ok/,/^}/s#idx < 0 ||#idx < 1 ||#'
plant pick-holder 's#run->items\[i\].pokemon_id == id \&\& bug_item(&run->items\[i\])#bug_item(\&run->items[i])#'
plant held-transferable 's#if (it->pokemon_id == id \&\& bug_item(it) \&\& gc_gts_item_transferable(it)) out\[n++\] = i;#if (it->pokemon_id == id \&\& bug_item(it)) out[n++] = i;#'
plant spawn-transferable 's#if (!it->pokemon_id || !bug_item(it) || !gc_gts_item_transferable(it)) continue;#if (!it->pokemon_id || !bug_item(it)) continue;#'
plant spawn-holder 's#for (int k = 0; k < run->n_party; k++) if (run->party\[k\].b.id == it->pokemon_id) return true;#return true;#'
plant spawn-bug 's#return gc_superfan_num_bug_types(run) > 0 || any_transferable_bug_item(run);#return gc_superfan_num_bug_types(run) > 1 || any_transferable_bug_item(run);#'
plant option-two 's#if (option == 1) return gc_superfan_num_bug_types(run) > 0;#if (option == 1) return gc_superfan_num_bug_types(run) > 1;#'
plant option-three 's#return any_transferable_bug_item(run);#return gc_superfan_num_bug_types(run) > 0 || any_transferable_bug_item(run);#'
plant option-fight 's#if (option == 0) return true;#if (option == 0) return gc_superfan_num_bug_types(run) > 0;#'
plant shop-fill 's#c->fill_remaining = false;#c->fill_remaining = true;#'
plant shop-luck 's#c->allow_luck_upgrades = true;#c->allow_luck_upgrades = false;#'
plant generated-null 's#if (arg == -2) return false;#if (arg == -2) (void)0;#'
plant rng-offset '/gc_superfan_pre_rng = pr_run_seed_offset_rng/s#512#511#'
plant six-rng-offset '/run->run.rng = pr_run_seed_offset_rng/s#512#511#'
plant tier-one 's#if (n < 2) {#if (n < 1) {#'
plant tier-two-low 's#} else if (n < 4) {#} else if (n < 3) {#'
plant tier-four-low 's#} else if (n < 6) {#} else if (n < 5) {#'
plant tier-two-top 's#} else if (n < 4) {#} else if (n < 5) {#'
plant tier-four-top 's#} else if (n < 6) {#} else if (n < 7) {#'
plant lure-one 's#{"SUPER_LURE", "GREAT_BALL"}#{"SUPER_LURE", "ULTRA_BALL"}#'
plant lure-two 's#{"QUICK_CLAW", "MAX_LURE", "ULTRA_BALL"}#{"QUICK_CLAW", "SUPER_LURE", "ULTRA_BALL"}#'
plant lure-four 's#{"GRIP_CLAW", "MAX_LURE", "ROGUE_BALL"}#{"GRIP_CLAW", "MAX_LURE", "ULTRA_BALL"}#'
plant six-master 's#generated_option(ctx, "MASTER_BALL", \&opts\[n_opts++\]);#(void)ctx;#'
plant six-mega-owned 's#if (!gc_item_stack(run, "MEGA_BRACELET")) generated_option#if (1) generated_option#'
plant six-dynamax-owned 's#if (!gc_item_stack(run, "DYNAMAX_BAND")) generated_option#if (1) generated_option#'
plant six-special-order 's#static const char \*const keys\[4\] = {"EVOLUTION_ITEM", "RARE_EVOLUTION_ITEM", "FORM_CHANGE_ITEM", "RARE_FORM_CHANGE_ITEM"};#static const char *const keys[4] = {"EVOLUTION_ITEM", "RARE_FORM_CHANGE_ITEM", "FORM_CHANGE_ITEM", "RARE_EVOLUTION_ITEM"};#'
plant exec-remove-last 's#if (--it->stack <= 0) gc_item_remove_at(run, i);#--it->stack;#'
plant six-special-pick 's#opts\[n_opts++\] = special\[pr_rng_seed_int(\&run->run.rng, n_special, 0)\];#opts[n_opts++] = special[0];#'
plant six-special-none 's#if (n_special > 0) opts#if (n_special > 1) opts#'
plant six-rng-restore 's#run->run.rng = saved;#(void)saved;#'
plant exec-index-high 's#|| c >= n)#|| c > n)#'
plant exec-index-low 's#|| c < 0 ||#|| c < -1 ||#'
plant exec-stack 's#if (--it->stack <= 0) gc_item_remove_at(run, i);#if (--it->stack < 0) gc_item_remove_at(run, i);#'
plant exec-arg 's#if (strcmp(it->key, chosen.key) || it->arg != chosen.arg || it->pokemon_id != chosen.pokemon_id) continue;#if (strcmp(it->key, chosen.key) || it->pokemon_id != chosen.pokemon_id) continue;#'
plant exec-holder 's#if (strcmp(it->key, chosen.key) || it->arg != chosen.arg || it->pokemon_id != chosen.pokemon_id) continue;#if (strcmp(it->key, chosen.key) || it->arg != chosen.arg) continue;#'
plant exec-prune 's#^    gc_me_prune_items(run);#    ;#'
plant exec-update 's#^    gc_me_update_modifiers(run);#    ;#'
plant net-tier 's#net.tier = 3;#net.tier = 2;#'
plant reviver 's#{"REVIVER_SEED"}#{"GREAT_BALL"}#'
plant hook-pick 's#if (type == GC_ME_BUG_TYPE_SUPERFAN \&\& option == 2) return gc_superfan_pick_ok(run, idx);#if (type == GC_ME_BUG_TYPE_SUPERFAN \&\& option == 2) return true;#' gc_mystery.c
plant hook-items 's#else if (run->screen.mystery_type == GC_ME_BUG_TYPE_SUPERFAN \&\& option == 2) n = gc_superfan_held_items(run, member, held);#else if (run->screen.mystery_type == GC_ME_BUG_TYPE_SUPERFAN \&\& option == 2) n = 0;#' gc_mystery.c
plant hook-ok 's#for (int o = 0; o < 3; o++) s->option_ok\[o\] = gc_superfan_option_ok(run, o);#for (int o = 0; o < 3; o++) s->option_ok[o] = true;#' gc_mystery.c
plant hook-kind '/case GC_ME_BUG_TYPE_SUPERFAN: {/,/break;/s#s->option_pick\[2\] = GC_PICK_MEMBER_ITEM;#s->option_pick[2] = GC_PICK_MEMBER;#' gc_mystery.c
plant hook-count 's#screen_arg(s, pr_arg_int("numBugTypes", gc_superfan_num_bug_types(run)));#screen_arg(s, pr_arg_int("numBugTypes", 0));#' gc_mystery.c
plant hook-items-text 's#"$t(modifierType:ModifierType.QUICK_CLAW.name)/$t(modifierType:ModifierType.GRIP_CLAW.name)/%s"#"$t(modifierType:ModifierType.QUICK_CLAW.name)/%s"#' gc_mystery.c
plant hook-execute 's#if (!gc_superfan_execute(run, option, b, c, why)) { \*ok = false; return false; }#gc_superfan_execute(run, option, b, c, why);#' gc_mystery.c

echo "$total planted, $fails not caught"
[ "$fails" = 0 ]
