#!/usr/bin/env python3
"""Mutation check for custom-core: the unit tests must be able to fail.

Each mutation is a single textual change to one source or header file. For every mutation this script copies
include/, src/ and tests/ to a scratch directory, applies the change, builds model_test and layout_test from that copy
and runs them. A mutation is KILLED when a test fails (a nonzero exit, a sanitizer abort or a timeout) and SURVIVES
when both tests still pass. The script exits nonzero if

  * the unmodified copy does not build and pass (nothing is learned from mutating a broken tree),
  * a mutation does not match exactly once in its file (the table is stale), or does not compile
    (a compile error is not evidence that a test catches anything), or
  * any mutation survives.

Usage: tools/mutations.py [-j N] [-k SUBSTRING] [--list]      (run from custom-core/ or anywhere)
"""
import argparse
import concurrent.futures
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ENUMS = ROOT.parent / "battle-core" / "gen" / "out" / "pr_enums.h"
CFLAGS = ["-O0", "-g", "-w", "-std=c99", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]
SOURCES = ["src/status.c", "src/history.c", "src/nemesis.c", "src/schedule.c", "src/ids.c", "src/meta.c"]
BUILD_TIMEOUT = 120
RUN_TIMEOUT = 30

MUTATIONS = []  # (file, old, new, label)


def mut(file, old, new, label=None):
    MUTATIONS.append((file, old, new, label or f"{old.strip()[:60]} -> {new.strip()[:40]}"))


def drop(file, stmt):
    """Remove one checking statement."""
    mut(file, stmt, ";", f"removed: {stmt[:70]}")


EQUIVALENT = []  # (file, old, new, reason): mutations that cannot change behavior; checked to still apply, never run


def equiv(file, old, new, reason):
    EQUIVALENT.append((file, old, new, reason))


# ---------------------------------------------------------------- limits and wire constants
L, W = "include/cc/limits.h", "include/cc/wire.h"
for old, new in [
    ("#define CC_SCHEMA_VERSION 1u", "#define CC_SCHEMA_VERSION 2u"),
    ("#define CC_META_MAGIC 0x4D435250u", "#define CC_META_MAGIC 0x4D435251u"),
    ("#define CC_MAX_NEMESES 16 ", "#define CC_MAX_NEMESES 15 "),
    ("#define CC_MAX_NEMESES 16 ", "#define CC_MAX_NEMESES 17 "),
    ("#define CC_HISTORY_TURNS 5 ", "#define CC_HISTORY_TURNS 4 "),
    ("#define CC_HISTORY_TURNS 5 ", "#define CC_HISTORY_TURNS 6 "),
    ("#define CC_MAX_EVENTS_PER_TURN 64 ", "#define CC_MAX_EVENTS_PER_TURN 63 "),
    ("#define CC_MAX_EVENTS_PER_TURN 64 ", "#define CC_MAX_EVENTS_PER_TURN 65 "),
    ("#define CC_LEDGER_ENTRIES 8 ", "#define CC_LEDGER_ENTRIES 7 "),
    ("#define CC_LEDGER_ENTRIES 8 ", "#define CC_LEDGER_ENTRIES 9 "),
    ("#define CC_MAX_MOVES 4\n", "#define CC_MAX_MOVES 5\n"),
    ("#define CC_MAX_LOADOUT_ITEMS 8 ", "#define CC_MAX_LOADOUT_ITEMS 7 "),
    ("#define CC_ITEM_KEY_BYTES 33 ", "#define CC_ITEM_KEY_BYTES 32 "),
    ("#define CC_SPECIES_ID_MAX 8901 ", "#define CC_SPECIES_ID_MAX 8902 "),
    ("#define CC_SPECIES_ID_MAX 8901 ", "#define CC_SPECIES_ID_MAX 8900 "),
    ("#define CC_MOVE_ID_MAX 920 ", "#define CC_MOVE_ID_MAX 921 "),
    ("#define CC_MOVE_ID_MAX 920 ", "#define CC_MOVE_ID_MAX 919 "),
    ("#define CC_ABILITY_ID_MAX 319 ", "#define CC_ABILITY_ID_MAX 320 "),
    ("#define CC_ABILITY_ID_MAX 319 ", "#define CC_ABILITY_ID_MAX 318 "),
    ("#define CC_BIOME_ID_MAX 50 ", "#define CC_BIOME_ID_MAX 51 "),
    ("#define CC_BIOME_ID_MAX 50 ", "#define CC_BIOME_ID_MAX 49 "),
    ("#define CC_NATURE_MAX 24 ", "#define CC_NATURE_MAX 25 "),
    ("#define CC_STATUS_MAX 7 ", "#define CC_STATUS_MAX 8 "),
    ("#define CC_HIT_RESULT_MAX 15 ", "#define CC_HIT_RESULT_MAX 16 "),
    ("#define CC_BATTLER_MAX 3 ", "#define CC_BATTLER_MAX 4 "),
    ("(64u * 1024u)", "(63u * 1024u)"),
    ("(128u * 1024u)", "(1u * 1024u)"),
]:
    mut(L, old, new)
for name, was, now in [
    ("ACTOR", "9u", "8u"), ("EVENT", "38u", "37u"), ("EVENT", "38u", "39u"), ("TURN_HEADER", "12u", "11u"),
    ("HISTORY_HEADER", "2u", "3u"), ("TERMINAL", "41u", "40u"), ("ITEM", "37u", "36u"), ("NEMESIS", "369u", "368u"),
    ("NEMESIS", "369u", "370u"), ("COUNTERS", "20u", "21u"), ("LEDGER_ENTRY", "22u", "21u"),
    ("ENVELOPE_HEADER", "12u", "13u"),
]:
    mut(W, f"#define CC_WIRE_{name} {was}", f"#define CC_WIRE_{name} {now}")
mut(W, "(1u + CC_LEDGER_ENTRIES * CC_WIRE_LEDGER_ENTRY)", "(CC_LEDGER_ENTRIES * CC_WIRE_LEDGER_ENTRY)",
    "META_MAX forgets the ledger count byte")
mut(W, "(1u + CC_MAX_NEMESES * CC_WIRE_NEMESIS) + 1u + CC_WIRE_TERMINAL", "(1u + CC_MAX_NEMESES * CC_WIRE_NEMESIS) + CC_WIRE_TERMINAL",
    "META_MAX forgets the has_report byte")

# ---------------------------------------------------------------- status.c
S = "src/status.c"
mut(S, "{27, 77, 127, 177}", "{27, 77, 127, 178}")
mut(S, "{27, 77, 127, 177}", "{26, 77, 127, 177}")
mut(S, '"out of order"', '"duplicate"')
mut(S, '"boss enemy"', '"fused enemy"')

# ---------------------------------------------------------------- history.c
H = "src/history.c"
# observation
mut(H, "o->battler > CC_BATTLER_MAX) return CC_E_RANGE;", "o->battler > CC_BATTLER_MAX + 1) return CC_E_RANGE;")
mut(H, "o->target < -1 || o->target > CC_BATTLER_MAX", "o->target < -2 || o->target > CC_BATTLER_MAX")
mut(H, "o->battler_species == 0 || ", "")
drop(H, "if (o->target_species > CC_SPECIES_ID_MAX) return CC_E_RANGE;")
mut(H, "if (o->target_species || o->target_form || o->target_id) return CC_E_NONCANONICAL;",
    "if (o->target_species || o->target_form) return CC_E_NONCANONICAL;")
mut(H, "} else if (o->target_species == 0) {", "} else if (0) {")
mut(H, "o->hp_after < -1 || o->hp_max_after < -1", "o->hp_after < -2 || o->hp_max_after < -1")
drop(H, "if ((o->hp_after == -1) != (o->hp_max_after == -1)) return CC_E_CONSISTENCY;")
drop(H, "if (o->hp_max_after == 0) return CC_E_RANGE;")
mut(H, "if (o->hp_after > o->hp_max_after) return CC_E_CONSISTENCY;", "if (o->hp_after >= o->hp_max_after) return CC_E_CONSISTENCY;")
mut(H, "if (o->status_after > CC_STATUS_MAX) return CC_E_RANGE;", "if (o->status_after > CC_STATUS_MAX + 1) return CC_E_RANGE;")
mut(H, "if (o->id > CC_HIT_RESULT_MAX) return CC_E_RANGE;", "if (o->id > CC_HIT_RESULT_MAX + 1) return CC_E_RANGE;")
mut(H, "if (o->value < 0) return CC_E_RANGE;", "if (o->value < -1) return CC_E_RANGE;")
mut(H, "case CC_OBS_HP:\n        if (o->id) return CC_E_NONCANONICAL;", "case CC_OBS_HP:\n        ;")
mut(H, "o->id < 1 || o->id > CC_MOVE_ID_MAX", "o->id < 1 || o->id > CC_MOVE_ID_MAX + 1")
drop(H, "if (o->value) return CC_E_NONCANONICAL;")
mut(H, "if (o->value < 0 || o->value > 5) return CC_E_RANGE;", "if (o->value < 0 || o->value > 6) return CC_E_RANGE;")
mut(H, "if (o->id || o->value) return CC_E_NONCANONICAL;", "if (o->id) return CC_E_NONCANONICAL;")
# actor
drop(H, "if (a->id == 0) return CC_E_RANGE;")
mut(H, "a->species < 1 || a->species > CC_SPECIES_ID_MAX", "a->species < 1 || a->species > CC_SPECIES_ID_MAX + 1")
drop(H, "if (a->side > CC_SIDE_ENEMY) return CC_E_RANGE;")
mut(H, "if (a->level < 1) return CC_E_RANGE;", "if (a->level < 0) return CC_E_RANGE;")
# event
mut(H, "e->kind >= CC_EV_KIND_COUNT_) return CC_E_RANGE;", "e->kind > CC_EV_KIND_COUNT_) return CC_E_RANGE;")
drop(H, "if (e->known & ~(uint16_t)CC_EK_ALL) return CC_E_UNKNOWN_BITS;")
drop(H, "if (e->target_side > CC_SIDE_ENEMY) return CC_E_RANGE;")
mut(H, "e->target_species < 1 || e->target_species > CC_SPECIES_ID_MAX) return CC_E_RANGE;",
    "e->target_species < 1 || e->target_species > CC_SPECIES_ID_MAX + 1) return CC_E_RANGE;")
drop(H, "if (k & ~kind_allowed[e->kind]) return CC_E_CONSISTENCY;")
drop(H, "if (kind_need_any[e->kind] && !(k & kind_need_any[e->kind])) return CC_E_CONSISTENCY;")
drop(H, "if ((k & (CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES)) && !(k & CC_EK_SOURCE_SIDE)) return CC_E_CONSISTENCY;")
for stmt in [
    "if (!(k & CC_EK_TARGET_ID) && e->target_id) return CC_E_NONCANONICAL;",
    "if (!(k & CC_EK_SOURCE_SIDE) && e->source_side) return CC_E_NONCANONICAL;",
    "if (!(k & CC_EK_SOURCE_ID) && e->source_id) return CC_E_NONCANONICAL;",
    "if (!(k & CC_EK_SOURCE_SPECIES) && (e->source_species || e->source_form)) return CC_E_NONCANONICAL;",
    "if (!(k & CC_EK_MOVE) && e->move_id) return CC_E_NONCANONICAL;",
    "if (!(k & CC_EK_RESULT) && e->result) return CC_E_NONCANONICAL;",
    "if (!(k & CC_EK_SHOWN) && e->shown) return CC_E_NONCANONICAL;",
    "if (!(k & CC_EK_HP_DELTA) && e->hp_delta) return CC_E_NONCANONICAL;",
    "if (!(k & CC_EK_HP_AFTER) && e->hp_after) return CC_E_NONCANONICAL;",
    "if ((k & CC_EK_TARGET_ID) && e->target_id == 0) return CC_E_RANGE;",
    "if ((k & CC_EK_SOURCE_SIDE) && e->source_side > CC_SIDE_ENEMY) return CC_E_RANGE;",
    "if ((k & CC_EK_SOURCE_ID) && e->source_id == 0) return CC_E_RANGE;",
    "if ((k & CC_EK_HP_AFTER) && e->hp_after < 0) return CC_E_RANGE;",
]:
    drop(H, stmt)
mut(H, "(e->source_species || e->source_form)) return CC_E_NONCANONICAL;", "(e->source_species)) return CC_E_NONCANONICAL;")
mut(H, "e->source_species < 1 || e->source_species > CC_SPECIES_ID_MAX))", "e->source_species < 1 || e->source_species > CC_SPECIES_ID_MAX + 1))")
mut(H, "(e->move_id < 1 || e->move_id > CC_MOVE_ID_MAX)", "(e->move_id < 1 || e->move_id > CC_MOVE_ID_MAX + 1)")
mut(H, "e->result > CC_HIT_RESULT_MAX) return CC_E_RANGE;", "e->result > CC_HIT_RESULT_MAX + 1) return CC_E_RANGE;")
mut(H, "e->shown < 1 || e->shown > CC_STATUS_MAX", "e->shown < 0 || e->shown > CC_STATUS_MAX")
mut(H, "e->shown < 1 || e->shown > CC_STATUS_MAX", "e->shown < 1 || e->shown > CC_STATUS_MAX + 1")
mut(H, "} else if (e->shown < 0) {", "} else if (e->shown < -1) {")
mut(H, "e->kind == CC_EV_HEAL ? e->hp_delta < 0 : e->hp_delta > 0", "e->kind == CC_EV_HEAL ? e->hp_delta <= 0 : e->hp_delta > 0")
mut(H, "e->kind == CC_EV_HEAL ? e->hp_delta < 0 : e->hp_delta > 0", "e->kind == CC_EV_HEAL ? e->hp_delta < 0 : e->hp_delta >= 0")
mut(H, "e->kind == CC_EV_HEAL ? e->hp_delta < 0 : e->hp_delta > 0", "e->kind == CC_EV_DAMAGE ? e->hp_delta < 0 : e->hp_delta > 0")
# turn
drop(H, "if (t->flags & ~(uint8_t)CC_TF_ALL) return CC_E_UNKNOWN_BITS;")
mut(H, "if (t->battle_seq < 1 || t->wave < 1) return CC_E_RANGE;", "if (t->battle_seq < 0 || t->wave < 1) return CC_E_RANGE;")
mut(H, "if (t->battle_seq < 1 || t->wave < 1) return CC_E_RANGE;", "if (t->battle_seq < 1 || t->wave < 0) return CC_E_RANGE;")
mut(H, "if (t->n_events > CC_MAX_EVENTS_PER_TURN) return CC_E_COUNT;", "if (t->n_events >= CC_MAX_EVENTS_PER_TURN) return CC_E_COUNT;")
drop(H, "if (((t->flags & CC_TF_EVICTED) != 0) != (t->dropped != 0)) return CC_E_CONSISTENCY;")
mut(H, "if ((t->flags & CC_TF_EVICTED) && t->n_events != CC_MAX_EVENTS_PER_TURN) return CC_E_CONSISTENCY;",
    "if ((t->flags & CC_TF_EVICTED) && t->n_events > CC_MAX_EVENTS_PER_TURN) return CC_E_CONSISTENCY;")
drop(H, "if (t->events[i].seq == t->events[i - 1].seq) return CC_E_DUPLICATE;")
mut(H, "if (t->events[i].seq < t->events[i - 1].seq) return CC_E_ORDER;", "if (t->events[i].seq > t->events[i - 1].seq) return CC_E_ORDER;")
# history
mut(H, "if (h->n_turns > CC_HISTORY_TURNS) return CC_E_COUNT;", "if (h->n_turns >= CC_HISTORY_TURNS) return CC_E_COUNT;")
drop(H, "if (h->flags & ~(uint8_t)CC_HF_ALL) return CC_E_UNKNOWN_BITS;")
drop(H, "if (t->battle_seq == p->battle_seq && t->turn == p->turn) return CC_E_DUPLICATE;")
mut(H, "(t->battle_seq == p->battle_seq && t->turn < p->turn)", "(t->battle_seq == p->battle_seq && t->turn > p->turn)")
mut(H, "if (t->battle_seq < p->battle_seq ||", "if (t->battle_seq > p->battle_seq ||")
mut(H, "            have_seq = true;\n", "            have_seq = false;\n")
drop(H, "if (t->events[j].seq == last_seq) return CC_E_DUPLICATE;")
mut(H, "if (t->events[j].seq < last_seq) return CC_E_ORDER;", "if (t->events[j].seq > last_seq) return CC_E_ORDER;")
# terminal
drop(H, "if (t->run_uid == 0) return CC_E_RANGE;")
mut(H, "committed ? t->txn_id == 0 : t->txn_id != 0", "committed ? t->txn_id != 0 : t->txn_id == 0")
mut(H, "t->outcome >= CC_OUT_COUNT_) return CC_E_RANGE;", "t->outcome > CC_OUT_COUNT_) return CC_E_RANGE;")
drop(H, "if (t->known & ~(uint8_t)CC_TK_ALL) return CC_E_UNKNOWN_BITS;")
mut(H, "if (t->biome > CC_BIOME_ID_MAX) return CC_E_RANGE;", "if (t->biome > CC_BIOME_ID_MAX + 1) return CC_E_RANGE;")
mut(H, "if (t->wave < 1) return CC_E_RANGE;", "if (t->wave < 0) return CC_E_RANGE;")
drop(H, "if (t->outcome != CC_OUT_DEFEAT && t->known) return CC_E_CONSISTENCY;")
mut(H, "t->finish_move < 1 || t->finish_move > CC_MOVE_ID_MAX", "t->finish_move < 1 || t->finish_move > CC_MOVE_ID_MAX + 1")
mut(H, "} else if (t->finish_move) {", "} else if (0) {")
mut(H, "} else if (!actor_is_zero(&t->finish_actor)) {", "} else if (0) {")
mut(H, "} else if (!actor_is_zero(&t->fallen_actor)) {", "} else if (0) {")
mut(H, "!a->form && !a->side && !a->level", "!a->form && !a->side")

# ---------------------------------------------------------------- nemesis.c
N = "src/nemesis.c"
mut(N, "if (cc_window_waves[i] == wave) return true;", "if (cc_window_waves[i] != wave) return true;")
mut(N, "return !it->arg && !it->stack;", "return !it->stack;")
drop(N, "if (it->key[i]) return false;")
mut(N, "if (len == 0 || len >= CC_ITEM_KEY_BYTES) return CC_E_RANGE;", "if (len >= CC_ITEM_KEY_BYTES) return CC_E_RANGE;")
mut(N, "if (len == 0 || len >= CC_ITEM_KEY_BYTES) return CC_E_RANGE;", "if (len == 0 || len > CC_ITEM_KEY_BYTES) return CC_E_RANGE;")
mut(N, "(c >= 'A' && c <= 'Z')", "(c >= 'A' && c <= 'Y')")
mut(N, "(c >= '0' && c <= '9')", "(c >= '0' && c <= '8')")
mut(N, "c == '_'", "c == '-'")
drop(N, "if (it->key[i]) return CC_E_NONCANONICAL;")
mut(N, "if (it->arg < -1) return CC_E_RANGE;", "if (it->arg < -2) return CC_E_RANGE;")
mut(N, "if (it->stack < 1) return CC_E_RANGE;", "if (it->stack < 0) return CC_E_RANGE;")
# lifecycle bookkeeping fields
mut(N, "if (n->sched_run_uid || n->challenge_wave || n->reward_txn) return CC_E_CONSISTENCY;",
    "if (n->sched_run_uid || n->challenge_wave) return CC_E_CONSISTENCY;")
mut(N, "if (!n->sched_run_uid || n->challenge_wave || n->reward_txn) return CC_E_CONSISTENCY;",
    "if (!n->sched_run_uid || n->challenge_wave) return CC_E_CONSISTENCY;")
mut(N, "if (!n->sched_run_uid || n->challenge_wave || n->reward_txn) return CC_E_CONSISTENCY;",
    "if (n->challenge_wave || n->reward_txn) return CC_E_CONSISTENCY;")
mut(N, "!is_window_wave(n->challenge_wave) || n->reward_txn) return CC_E_CONSISTENCY;",
    "!is_window_wave(n->challenge_wave)) return CC_E_CONSISTENCY;")
mut(N, "if (!n->sched_run_uid || !is_window_wave(n->challenge_wave) || n->reward_txn)",
    "if (!is_window_wave(n->challenge_wave) || n->reward_txn)")
mut(N, "!is_window_wave(n->challenge_wave) || !n->reward_txn) return CC_E_CONSISTENCY;",
    "!is_window_wave(n->challenge_wave)) return CC_E_CONSISTENCY;")
# nemesis fields
drop(N, "if (n->record_id == 0) return CC_E_RANGE;")
equiv(N, "n->life >= CC_LIFE_COUNT_) return CC_E_RANGE;", "n->life > CC_LIFE_COUNT_) return CC_E_RANGE;",
      "life == CC_LIFE_COUNT_ is still refused, with the same status, by the default case of lifecycle_fields")
drop(N, "if (n->known & ~(uint16_t)CC_NK_ALL) return CC_E_UNKNOWN_BITS;")
mut(N, "n->source_run_uid == 0 || ", "")
mut(N, "n->source_pokemon_id == 0 || ", "")
mut(N, "n->source_wave < 1) return CC_E_RANGE;", "n->source_wave < 0) return CC_E_RANGE;")
mut(N, "if (n->source_biome > CC_BIOME_ID_MAX) return CC_E_RANGE;", "if (n->source_biome > CC_BIOME_ID_MAX + 1) return CC_E_RANGE;")
mut(N, "n->species < 1 || n->species > CC_SPECIES_ID_MAX", "n->species < 1 || n->species > CC_SPECIES_ID_MAX + 1")
mut(N, "n->gender < -1 || n->gender > 1", "n->gender < -2 || n->gender > 1")
mut(N, "n->gender < -1 || n->gender > 1", "n->gender < -1 || n->gender > 2")
mut(N, "if (n->shiny > 1) return CC_E_RANGE;", "if (n->shiny > 2) return CC_E_RANGE;")
mut(N, "if (n->orig_level < 1) return CC_E_RANGE;", "if (n->orig_level < 0) return CC_E_RANGE;")
mut(N, "if (n->nature > CC_NATURE_MAX) return CC_E_RANGE;", "if (n->nature > CC_NATURE_MAX + 1) return CC_E_RANGE;")
mut(N, "} else if (n->nature) {", "} else if (0) {")
mut(N, "n->ability < 1 || n->ability > CC_ABILITY_ID_MAX", "n->ability < 1 || n->ability > CC_ABILITY_ID_MAX + 1")
mut(N, "} else if (n->ability) {", "} else if (0) {")
mut(N, "if (n->passive > CC_ABILITY_ID_MAX) return CC_E_RANGE;", "if (n->passive > CC_ABILITY_ID_MAX + 1) return CC_E_RANGE;")
mut(N, "} else if (n->passive) {", "} else if (0) {")
mut(N, "if (n->ivs[i] > 31) return CC_E_RANGE;", "if (n->ivs[i] > 32) return CC_E_RANGE;")
mut(N, "} else if (n->ivs[i]) {", "} else if (0) {")
mut(N, "if (n->n_moves > CC_MAX_MOVES) return CC_E_COUNT;", "if (n->n_moves >= CC_MAX_MOVES) return CC_E_COUNT;")
drop(N, "if (n->n_moves < 1) return CC_E_RANGE;")
mut(N, "n->moves[i] < 1 || n->moves[i] > CC_MOVE_ID_MAX", "n->moves[i] < 1 || n->moves[i] > CC_MOVE_ID_MAX + 1")
drop(N, "if (n->moves[j] == n->moves[i]) return CC_E_DUPLICATE;")
mut(N, "} else if (n->moves[i]) {", "} else if (0) {")
mut(N, "} else if (n->n_moves) {", "} else if (0) {")
drop(N, "if (n->moves[i]) return CC_E_NONCANONICAL;")
mut(N, "if (n->n_items > CC_MAX_LOADOUT_ITEMS) return CC_E_COUNT;", "if (n->n_items >= CC_MAX_LOADOUT_ITEMS) return CC_E_COUNT;")
mut(N, "} else if (!item_is_zero(&n->items[i])) {", "} else if (0) {")
drop(N, "if (n->n_items) return CC_E_NONCANONICAL;")
drop(N, "if (!item_is_zero(&n->items[i])) return CC_E_NONCANONICAL;")
mut(N, "(k & CC_NK_LOADOUT_TRUNCATED) && n->n_items != CC_MAX_LOADOUT_ITEMS)", "(k & CC_NK_LOADOUT_TRUNCATED) && n->n_items > CC_MAX_LOADOUT_ITEMS)")
mut(N, "(k & CC_NK_LOADOUT_TRUNCATED) && n->n_items != CC_MAX_LOADOUT_ITEMS)", "(k & CC_NK_LOADOUT_TRUNCATED) && 0)")
mut(N, "n->finish_move < 1 || n->finish_move > CC_MOVE_ID_MAX", "n->finish_move < 1 || n->finish_move > CC_MOVE_ID_MAX + 1")
mut(N, "} else if (n->finish_move) {", "} else if (0) {")
mut(N, "n->fallen_species < 1 || n->fallen_species > CC_SPECIES_ID_MAX", "n->fallen_species < 1 || n->fallen_species > CC_SPECIES_ID_MAX + 1")
mut(N, "} else if (n->fallen_species || n->fallen_form) {", "} else if (n->fallen_species) {")
mut(N, "} else if (n->fallen_species || n->fallen_form) {", "} else if (0) {")
# support and the insertion gate
for tr, ref in [("BOSS", "BOSS"), ("FUSED", "FUSED"), ("TERA", "TERA"), ("DOUBLES", "DOUBLES"),
                ("TRAINER_OWNED", "TRAINER_OWNED"), ("MYSTERY", "MYSTERY"), ("FIXED_SCRIPTED", "FIXED_SCRIPTED"),
                ("REMATCH", "REMATCH")]:
    drop(N, f"if (n->traits & CC_TR_{tr}) return CC_REFUSE_{ref};")
drop(N, "if (!(n->known & CC_NK_FINISHER_VERIFIED)) return CC_REFUSE_NOT_ATTRIBUTED;")
drop(N, "if (!(n->known & CC_NK_ABILITY)) return CC_REFUSE_ABILITY_UNKNOWN;")
drop(N, "if (!(n->known & CC_NK_MOVES)) return CC_REFUSE_MOVES_UNKNOWN;")
mut(N, "if (!n) return CC_REFUSE_NOT_ATTRIBUTED;", "if (!n) return CC_SUPPORT_OK;")
drop(N, "if (term->outcome != CC_OUT_DEFEAT) return CC_REFUSE_NOT_DEFEAT;")
drop(N, "if (!(term->known & CC_TK_FINISH_ACTOR)) return CC_REFUSE_NOT_ATTRIBUTED;")
mut(N, "f->side != CC_SIDE_ENEMY || ", "")
mut(N, "f->id != cand->source_pokemon_id || ", "")
mut(N, "f->species != cand->species ||", "0 ||")
mut(N, "term->run_uid != cand->source_run_uid || ", "")
mut(N, "term->wave != cand->source_wave || ", "")
mut(N, "term->biome != cand->source_biome)", "0)")
mut(N, "return cc_nemesis_support(cand);", "return CC_SUPPORT_OK;")
mut(N, "cc_nemesis_support(n) == CC_SUPPORT_OK ? CC_OK : CC_E_CONSISTENCY", "CC_OK")
# roster
mut(N, "if (r->count > CC_MAX_NEMESES) return CC_E_COUNT;", "if (r->count >= CC_MAX_NEMESES) return CC_E_COUNT;")
mut(N, "cc_status s = cc_nemesis_validate_stored(n);", "cc_status s = cc_nemesis_validate(n);")
drop(N, "if (n->record_id == r->rec[i - 1].record_id) return CC_E_DUPLICATE;")
mut(N, "if (n->record_id < r->rec[i - 1].record_id) return CC_E_ORDER;", "if (n->record_id > r->rec[i - 1].record_id) return CC_E_ORDER;")
mut(N, "r->rec[j].source_run_uid == n->source_run_uid && r->rec[j].source_pokemon_id == n->source_pokemon_id",
    "r->rec[j].source_run_uid == n->source_run_uid")
mut(N, "r->rec[j].source_run_uid == n->source_run_uid && r->rec[j].source_pokemon_id == n->source_pokemon_id",
    "r->rec[j].source_pokemon_id == n->source_pokemon_id")
mut(N, "if (n->life == CC_LIFE_SCHEDULED || n->life == CC_LIFE_CHALLENGED) active++;", "if (n->life == CC_LIFE_SCHEDULED) active++;")
mut(N, "if (n->life == CC_LIFE_SCHEDULED || n->life == CC_LIFE_CHALLENGED) active++;", "if (n->life == CC_LIFE_CHALLENGED) active++;")
mut(N, "return active <= 1 ? CC_OK : CC_E_CONSISTENCY;", "return active <= 2 ? CC_OK : CC_E_CONSISTENCY;")
# lifecycle table
mut(N, "[CC_LIFE_STORED] = {[CC_LIFE_SCHEDULED] = true},", "[CC_LIFE_STORED] = {[CC_LIFE_SCHEDULED] = true, [CC_LIFE_CHALLENGED] = true},")
mut(N, "[CC_LIFE_STORED] = {[CC_LIFE_SCHEDULED] = true},", "[CC_LIFE_STORED] = {0},")
mut(N, "[CC_LIFE_SCHEDULED] = {[CC_LIFE_STORED] = true, [CC_LIFE_CHALLENGED] = true},",
    "[CC_LIFE_SCHEDULED] = {[CC_LIFE_STORED] = true},")
mut(N, "[CC_LIFE_SCHEDULED] = {[CC_LIFE_STORED] = true, [CC_LIFE_CHALLENGED] = true},",
    "[CC_LIFE_SCHEDULED] = {[CC_LIFE_CHALLENGED] = true},")
mut(N, "[CC_LIFE_SCHEDULED] = {[CC_LIFE_STORED] = true, [CC_LIFE_CHALLENGED] = true},",
    "[CC_LIFE_SCHEDULED] = {[CC_LIFE_STORED] = true, [CC_LIFE_CHALLENGED] = true, [CC_LIFE_DEFEATED] = true},")
mut(N, "[CC_LIFE_CHALLENGED] = {[CC_LIFE_STORED] = true, [CC_LIFE_DEFEATED] = true},", "[CC_LIFE_CHALLENGED] = {[CC_LIFE_DEFEATED] = true},")
mut(N, "[CC_LIFE_CHALLENGED] = {[CC_LIFE_STORED] = true, [CC_LIFE_DEFEATED] = true},", "[CC_LIFE_CHALLENGED] = {[CC_LIFE_STORED] = true},")
mut(N, "[CC_LIFE_CHALLENGED] = {[CC_LIFE_STORED] = true, [CC_LIFE_DEFEATED] = true},",
    "[CC_LIFE_CHALLENGED] = {[CC_LIFE_STORED] = true, [CC_LIFE_DEFEATED] = true, [CC_LIFE_REWARD_CLAIMED] = true},")
mut(N, "[CC_LIFE_DEFEATED] = {[CC_LIFE_REWARD_CLAIMED] = true},", "[CC_LIFE_DEFEATED] = {[CC_LIFE_REWARD_CLAIMED] = true, [CC_LIFE_STORED] = true},")
mut(N, "[CC_LIFE_DEFEATED] = {[CC_LIFE_REWARD_CLAIMED] = true},", "[CC_LIFE_DEFEATED] = {0},")
mut(N, "if (from >= CC_LIFE_COUNT_ || to >= CC_LIFE_COUNT_) return false;", "if (from >= CC_LIFE_COUNT_) return false;")
mut(N, "if (from >= CC_LIFE_COUNT_ || to >= CC_LIFE_COUNT_) return false;", "if (to >= CC_LIFE_COUNT_) return false;")
mut(N, "return life == CC_LIFE_STORED || life == CC_LIFE_REWARD_CLAIMED;", "return life == CC_LIFE_STORED;")
mut(N, "return life == CC_LIFE_STORED || life == CC_LIFE_REWARD_CLAIMED;", "return life == CC_LIFE_REWARD_CLAIMED;")
mut(N, "return life == CC_LIFE_STORED || life == CC_LIFE_REWARD_CLAIMED;",
    "return life == CC_LIFE_STORED || life == CC_LIFE_REWARD_CLAIMED || life == CC_LIFE_DEFEATED;")
mut(N, "return life == CC_LIFE_STORED || life == CC_LIFE_REWARD_CLAIMED;",
    "return life == CC_LIFE_STORED || life == CC_LIFE_REWARD_CLAIMED || life == CC_LIFE_SCHEDULED;")
# transition
mut(N, "cc_status s = cc_nemesis_validate(n);\n    if (s != CC_OK) return s;\n    if (!cc_life_can_transition",
    "cc_status s = CC_OK;\n    if (!cc_life_can_transition", "transition no longer validates its input")
drop(N, "if (!cc_life_can_transition(n->life, to)) return CC_E_LIFECYCLE;")
drop(N, "if (run_uid == 0) return CC_E_RANGE;")
mut(N, "c.sched_run_uid = run_uid;", "c.sched_run_uid = 1;")
drop(N, "if (!is_window_wave(wave)) return CC_E_RANGE;")
drop(N, "if (run_uid != n->sched_run_uid) return CC_E_CONSISTENCY;")
mut(N, "c.challenge_wave = wave;", "c.challenge_wave = cc_window_waves[0];")
mut(N, "        c.sched_run_uid = 0;\n", "        ;\n")
mut(N, "        c.challenge_wave = 0;\n", "        ;\n")
drop(N, "if (reward_txn == 0) return CC_E_RANGE;")
mut(N, "c.reward_txn = reward_txn;", "c.reward_txn = 1;")
mut(N, "    c.life = to;\n", "    ;\n")
equiv(N, "    s = cc_nemesis_validate(&c);\n    if (s != CC_OK) return s;\n    *n = c;", "    *n = c;",
      "every allowed transition builds a valid copy after the argument checks above it; the re-validation guards later edits of the table")
mut(N, "    *n = c;\n    return CC_OK;", "    return CC_OK;", "transition never commits")
equiv(N, "    default:\n        return CC_E_LIFECYCLE;\n    }\n    c.life = to;", "    default:\n        break;\n    }\n    c.life = to;",
      "unreachable: cc_life_can_transition has already refused every target outside the five lifecycle states")

# ---------------------------------------------------------------- schedule.c
C = "src/schedule.c"
mut(C, "s->state >= CC_SCHED_COUNT_) return CC_E_RANGE;", "s->state > CC_SCHED_COUNT_) return CC_E_RANGE;")
drop(C, "if (s->run_uid == 0) return CC_E_RANGE;")
mut(C, "if (s->n_frozen > CC_MAX_FROZEN) return CC_E_COUNT;", "if (s->n_frozen >= CC_MAX_FROZEN) return CC_E_COUNT;")
drop(C, "if (s->frozen_ids[i] == 0) return CC_E_RANGE;")
drop(C, "if (s->frozen_ids[i] == s->frozen_ids[i - 1]) return CC_E_DUPLICATE;")
mut(C, "if (s->frozen_ids[i] < s->frozen_ids[i - 1]) return CC_E_ORDER;", "if (s->frozen_ids[i] > s->frozen_ids[i - 1]) return CC_E_ORDER;")
for term in ["s->n_frozen || ", "s->chosen_id || ", "s->n_windows || ", "s->rng_algo != CC_RNG_ALGO_NONE || ", "s->rng_seed ||\n            s->rng_state"]:
    mut(C, term, "" if not term.startswith("s->rng_seed") else "s->rng_seed", f"NO_CANDIDATE no longer checks: {term.strip()[:30]}")
mut(C, "s->rng_seed ||\n            s->rng_state", "s->rng_state", "NO_CANDIDATE no longer checks rng_seed")
mut(C, "if (s->windows[i].wave || s->windows[i].state || s->windows[i].reason) return CC_E_NONCANONICAL;",
    "if (s->windows[i].wave || s->windows[i].state) return CC_E_NONCANONICAL;")
mut(C, "if (s->windows[i].wave || s->windows[i].state || s->windows[i].reason) return CC_E_NONCANONICAL;",
    "if (s->windows[i].state || s->windows[i].reason) return CC_E_NONCANONICAL;")
mut(C, "if (s->rng_algo != CC_RNG_ALGO_V1 || s->n_windows != CC_NUM_WINDOWS) return CC_E_CONSISTENCY;",
    "if (s->n_windows != CC_NUM_WINDOWS) return CC_E_CONSISTENCY;")
mut(C, "if (s->rng_algo != CC_RNG_ALGO_V1 || s->n_windows != CC_NUM_WINDOWS) return CC_E_CONSISTENCY;",
    "if (s->rng_algo != CC_RNG_ALGO_V1) return CC_E_CONSISTENCY;")
drop(C, "if (!chosen_frozen) return CC_E_CONSISTENCY;")
drop(C, "if (w->wave != cc_window_waves[i]) return CC_E_RANGE;")
mut(C, "if (w->state >= CC_WIN_COUNT_) return CC_E_RANGE;", "if (w->state > CC_WIN_COUNT_) return CC_E_RANGE;")
mut(C, "if (w->reason >= CC_SKIP_COUNT_) return CC_E_RANGE;", "if (w->reason > CC_SKIP_COUNT_) return CC_E_RANGE;")
drop(C, "if ((w->state == CC_WIN_SKIPPED) != (w->reason != CC_SKIP_NONE)) return CC_E_CONSISTENCY;")
mut(C, "return i == CC_NUM_WINDOWS ? CC_OK : CC_E_CONSISTENCY;", "return CC_OK;")
mut(C, "case CC_SCHED_ACTIVE: expect = CC_WIN_PENDING; break;", "case CC_SCHED_ACTIVE: expect = CC_WIN_OFFERED; break;")
mut(C, "case CC_SCHED_OFFERED: expect = CC_WIN_OFFERED; break;", "case CC_SCHED_OFFERED: expect = CC_WIN_DECLINED; break;")
mut(C, "case CC_SCHED_DECLINED: expect = CC_WIN_DECLINED; break;", "case CC_SCHED_DECLINED: expect = CC_WIN_ACCEPTED; break;")
mut(C, "case CC_SCHED_ACCEPTED: expect = CC_WIN_ACCEPTED; break;", "case CC_SCHED_ACCEPTED: expect = CC_WIN_OFFERED; break;")
mut(C, "if (i == CC_NUM_WINDOWS || s->windows[i].state != expect) return CC_E_CONSISTENCY;", "if (s->windows[i].state != expect) return CC_E_CONSISTENCY;")
mut(C, "if (i == CC_NUM_WINDOWS || s->windows[i].state != expect) return CC_E_CONSISTENCY;", "if (i == CC_NUM_WINDOWS) return CC_E_CONSISTENCY;")
mut(C, "(expect == CC_WIN_PENDING ? i : i + 1)", "(expect == CC_WIN_PENDING ? i : i + 2)")
drop(C, "if (s->windows[j].state != CC_WIN_PENDING) return CC_E_CONSISTENCY;")

# ---------------------------------------------------------------- ids.c
D = "src/ids.c"
mut(D, "if (c->next_record_id < 1 || c->next_run_seq < 1 || c->next_txn_id < 1) return CC_E_RANGE;",
    "if (c->next_run_seq < 1 || c->next_txn_id < 1) return CC_E_RANGE;")
mut(D, "if (c->next_record_id < 1 || c->next_run_seq < 1 || c->next_txn_id < 1) return CC_E_RANGE;",
    "if (c->next_record_id < 1 || c->next_txn_id < 1) return CC_E_RANGE;")
mut(D, "if (c->next_record_id < 1 || c->next_run_seq < 1 || c->next_txn_id < 1) return CC_E_RANGE;",
    "if (c->next_record_id < 1 || c->next_run_seq < 1) return CC_E_RANGE;")
drop(D, "if (c->next_record_id == UINT32_MAX) return CC_E_OVERFLOW;")
drop(D, "if (c->next_txn_id == UINT64_MAX) return CC_E_OVERFLOW;")
drop(D, "if (c->next_run_seq == UINT32_MAX) return CC_E_OVERFLOW;")
mut(D, "*out = c->next_record_id++;", "*out = ++c->next_record_id;")
mut(D, "*out = c->next_txn_id++;", "*out = ++c->next_txn_id;")
mut(D, "*out = c->next_txn_id++;", "*out = c->next_txn_id;")
mut(D, "    c->next_run_seq++;\n", "")
mut(D, "((cc_run_uid)c->install_nonce << 32)", "((cc_run_uid)c->install_nonce << 31)")
mut(D, "| c->next_run_seq;", "| (c->next_run_seq + 1);")
mut(D, "if (!c || !out) return CC_E_NULL;\n    cc_status s = cc_counters_validate(c);\n    if (s != CC_OK) return s;\n    if (c->next_record_id",
    "if (!c || !out) return CC_E_NULL;\n    cc_status s = CC_OK;\n    if (c->next_record_id", "allocator no longer validates counters")
# ledger
mut(D, "if (e->txn_id == 0 || e->run_uid == 0) return CC_E_RANGE;", "if (e->run_uid == 0) return CC_E_RANGE;")
mut(D, "if (e->txn_id == 0 || e->run_uid == 0) return CC_E_RANGE;", "if (e->txn_id == 0) return CC_E_RANGE;")
mut(D, "if (e->outcome == CC_TXO_INSERTED) return e->record_id ? CC_OK : CC_E_RANGE;", "if (e->outcome == CC_TXO_INSERTED) return CC_OK;")
mut(D, "return e->record_id ? CC_E_NONCANONICAL : CC_OK;", "return CC_OK;")
mut(D, "if (e->outcome != CC_TXO_CLAIMED) return CC_E_RANGE;", "if (e->outcome == CC_TXO_NONE) return CC_E_RANGE;")
mut(D, "        return e->record_id ? CC_OK : CC_E_RANGE;\n    default:", "        return CC_OK;\n    default:")
mut(D, "    default:\n        return CC_E_RANGE;\n    }\n}", "    default:\n        return CC_OK;\n    }\n}")
mut(D, "a->kind == CC_TXN_TERMINAL ? a->run_uid == b->run_uid : a->record_id == b->record_id",
    "a->kind != CC_TXN_TERMINAL ? a->run_uid == b->run_uid : a->record_id == b->record_id")
mut(D, "a->kind == CC_TXN_TERMINAL ? a->run_uid == b->run_uid : a->record_id == b->record_id",
    "a->kind == CC_TXN_TERMINAL ? a->run_uid == b->run_uid && a->record_id == b->record_id : a->record_id == b->record_id")
mut(D, "    if (a->kind != b->kind) return false;\n", "")
mut(D, "if (l->n > CC_LEDGER_ENTRIES) return CC_E_COUNT;", "if (l->n >= CC_LEDGER_ENTRIES) return CC_E_COUNT;")
drop(D, "if (l->e[i].txn_id == l->e[i - 1].txn_id) return CC_E_DUPLICATE;")
mut(D, "if (l->e[i].txn_id < l->e[i - 1].txn_id) return CC_E_ORDER;", "if (l->e[i].txn_id > l->e[i - 1].txn_id) return CC_E_ORDER;")
drop(D, "if (same_key(&l->e[j], &l->e[i])) return CC_E_DUPLICATE;")
mut(D, "if (kind == CC_TXN_TERMINAL ? e->run_uid == run_uid : e->record_id == record_id) {",
    "if (kind != CC_TXN_TERMINAL ? e->run_uid == run_uid : e->record_id == record_id) {")
drop(D, "if (e->kind != kind) continue;")
mut(D, "if (out) *out = *e;", "if (out) *out = l->e[0];")
mut(D, "if (l->n > 0 && e->txn_id <= l->e[l->n - 1].txn_id) return CC_E_ORDER;", "if (l->n > 0 && e->txn_id < l->e[l->n - 1].txn_id) return CC_E_ORDER;")
drop(D, "if (l->n > 0 && e->txn_id <= l->e[l->n - 1].txn_id) return CC_E_ORDER;")
drop(D, "if (same_key(&l->e[i], e)) return CC_E_DUPLICATE;")
mut(D, "for (unsigned i = 1; i < CC_LEDGER_ENTRIES; i++) l->e[i - 1] = l->e[i];", "for (unsigned i = 2; i < CC_LEDGER_ENTRIES; i++) l->e[i - 1] = l->e[i];")
mut(D, "        l->n--;\n", "")
mut(D, "    s = entry_validate(e);\n    if (s != CC_OK) return s;\n", "")
mut(D, "    cc_status s = cc_ledger_validate(l);\n    if (s != CC_OK) return s;\n    s = entry_validate(e);",
    "    cc_status s = CC_OK;\n    s = entry_validate(e);", "append no longer validates the ledger")

# ---------------------------------------------------------------- meta.c
M = "src/meta.c"
mut(M, "m->counters.next_record_id = 1;", "m->counters.next_record_id = 0;")
mut(M, "m->counters.next_run_seq = 1;", "m->counters.next_run_seq = 0;")
mut(M, "m->counters.next_txn_id = 1;", "m->counters.next_txn_id = 0;")
mut(M, "m->counters.install_nonce = install_nonce;", "m->counters.install_nonce = 0;")
mut(M, "m->schema = CC_SCHEMA_VERSION;", "m->schema = 0;")
mut(M, "(uint32_t)(uid >> 32) == c->install_nonce && ", "")
mut(M, "(uint32_t)uid >= 1 && ", "")
mut(M, "(uint32_t)uid < c->next_run_seq", "(uint32_t)uid <= c->next_run_seq")
mut(M, "if (m->schema != CC_SCHEMA_VERSION) return CC_E_VERSION;", "if (m->schema > CC_SCHEMA_VERSION) return CC_E_VERSION;")
mut(M, "if (m->schema != CC_SCHEMA_VERSION) return CC_E_VERSION;", "if (m->schema < CC_SCHEMA_VERSION) return CC_E_VERSION;")
mut(M, "    s = cc_ledger_validate(&m->ledger);\n    if (s != CC_OK) return s;\n", "")
mut(M, "    s = cc_roster_validate(&m->roster);\n    if (s != CC_OK) return s;\n", "")
mut(M, "if (n->record_id >= c->next_record_id) return CC_E_CONSISTENCY;", "if (n->record_id > c->next_record_id) return CC_E_CONSISTENCY;")
drop(M, "if (!uid_issued(c, n->source_run_uid)) return CC_E_CONSISTENCY;")
drop(M, "if (n->sched_run_uid && !uid_issued(c, n->sched_run_uid)) return CC_E_CONSISTENCY;")
mut(M, "if (n->reward_txn >= c->next_txn_id) return CC_E_CONSISTENCY;", "if (n->reward_txn > c->next_txn_id) return CC_E_CONSISTENCY;")
mut(M, "if (e->txn_id >= c->next_txn_id) return CC_E_CONSISTENCY;", "if (e->txn_id > c->next_txn_id) return CC_E_CONSISTENCY;")
mut(M, "if (e->record_id >= c->next_record_id) return CC_E_CONSISTENCY;", "if (e->record_id > c->next_record_id) return CC_E_CONSISTENCY;")
drop(M, "if (!uid_issued(c, e->run_uid)) return CC_E_CONSISTENCY;")
mut(M, "if (m->has_report > 1) return CC_E_RANGE;", "if (m->has_report > 2) return CC_E_RANGE;")
mut(M, "s = cc_terminal_validate(&m->report, true);", "s = cc_terminal_validate(&m->report, false);")
mut(M, "if (m->report.txn_id >= c->next_txn_id || !uid_issued(c, m->report.run_uid)) return CC_E_CONSISTENCY;",
    "if (m->report.txn_id > c->next_txn_id || !uid_issued(c, m->report.run_uid)) return CC_E_CONSISTENCY;")
mut(M, "if (m->report.txn_id >= c->next_txn_id || !uid_issued(c, m->report.run_uid)) return CC_E_CONSISTENCY;",
    "if (m->report.txn_id >= c->next_txn_id) return CC_E_CONSISTENCY;")
mut(M, "return cc_history_validate(&m->history);", "return CC_OK;")
for old, new in [("t->run_uid || ", ""), ("t->txn_id || ", ""), ("t->outcome || ", ""), ("t->known || ", ""),
                 ("t->biome || ", ""), ("t->wave || ", ""), ("t->finish_move ||\n        ", ""),
                 ("t->finish_actor.id || ", ""), ("t->fallen_actor.id || ", ""), ("m->history.n_turns || ", ""),
                 ("|| m->history.flags", "")]:
    mut(M, old, new, f"no-report canonical check dropped: {old.strip()[:30]}")
# checked arithmetic
mut(M, "if (a > SIZE_MAX - b) return CC_E_OVERFLOW;", "if (a >= SIZE_MAX - b) return CC_E_OVERFLOW;")
mut(M, "if (a > SIZE_MAX - b) return CC_E_OVERFLOW;", "if (a > SIZE_MAX - b + 1) return CC_E_OVERFLOW;")
drop(M, "if (a > SIZE_MAX - b) return CC_E_OVERFLOW;")
mut(M, "if (b != 0 && a > SIZE_MAX / b) return CC_E_OVERFLOW;", "if (b != 0 && a >= SIZE_MAX / b) return CC_E_OVERFLOW;")
mut(M, "if (b != 0 && a > SIZE_MAX / b) return CC_E_OVERFLOW;", "if (b != 0 && a > SIZE_MAX / b + 1) return CC_E_OVERFLOW;")
mut(M, "if (b != 0 && a > SIZE_MAX / b) return CC_E_OVERFLOW;", "if (a > SIZE_MAX / b) return CC_E_OVERFLOW;")
mut(M, "if (b != 0 && a > SIZE_MAX / b) return CC_E_OVERFLOW;", "if (b != 0 && a > SIZE_MAX / 2) return CC_E_OVERFLOW;")
equiv(M, "return cc_size_add(*total, part, total);", "*total += part; return CC_OK;",
      "every count is capped by a compile-time limit first, so no sum nears SIZE_MAX; cc_size_add itself is tested at the boundary")
equiv(M, "    cc_status s = cc_size_mul(count, unit, &part);\n    if (s != CC_OK) return s;\n", "    cc_status s = CC_OK;\n    part = count * unit;\n",
      "every count is capped by a compile-time limit first, so no product nears SIZE_MAX; cc_size_mul itself is tested at the boundary")
mut(M, "if (n_records > CC_MAX_NEMESES || n_ledger > CC_LEDGER_ENTRIES || n_turns > CC_HISTORY_TURNS) return CC_E_COUNT;",
    "if (n_ledger > CC_LEDGER_ENTRIES || n_turns > CC_HISTORY_TURNS) return CC_E_COUNT;")
mut(M, "if (n_records > CC_MAX_NEMESES || n_ledger > CC_LEDGER_ENTRIES || n_turns > CC_HISTORY_TURNS) return CC_E_COUNT;",
    "if (n_records > CC_MAX_NEMESES || n_turns > CC_HISTORY_TURNS) return CC_E_COUNT;")
mut(M, "if (n_records > CC_MAX_NEMESES || n_ledger > CC_LEDGER_ENTRIES || n_turns > CC_HISTORY_TURNS) return CC_E_COUNT;",
    "if (n_records > CC_MAX_NEMESES || n_ledger > CC_LEDGER_ENTRIES) return CC_E_COUNT;")
drop(M, "if (!has_report && n_turns) return CC_E_CONSISTENCY;")
drop(M, "if (n_turns && !events_per_turn) return CC_E_NULL;")
mut(M, "size_t total = CC_WIRE_ENVELOPE_HEADER;", "size_t total = 0;")
mut(M, "add_scaled(&total, 1, CC_WIRE_COUNTERS);", "add_scaled(&total, 0, CC_WIRE_COUNTERS);")
mut(M, "add_scaled(&total, 1, 1);                       /* n_ledger */", "add_scaled(&total, 0, 1);")
mut(M, "add_scaled(&total, n_ledger, CC_WIRE_LEDGER_ENTRY)", "add_scaled(&total, n_ledger + 1, CC_WIRE_LEDGER_ENTRY)")
mut(M, "add_scaled(&total, 1, 1);                       /* n_records */", "add_scaled(&total, 0, 1);")
mut(M, "add_scaled(&total, n_records, CC_WIRE_NEMESIS)", "add_scaled(&total, n_records, CC_WIRE_NEMESIS + 1)")
mut(M, "add_scaled(&total, 1, 1);                       /* has_report */", "add_scaled(&total, 0, 1);")
mut(M, "s = add_scaled(&total, 1, CC_WIRE_TERMINAL);", "s = add_scaled(&total, 0, CC_WIRE_TERMINAL);")
mut(M, "if (s == CC_OK) s = add_scaled(&total, 1, CC_WIRE_HISTORY_HEADER);", "if (s == CC_OK) s = add_scaled(&total, 0, CC_WIRE_HISTORY_HEADER);")
mut(M, "if (events_per_turn[i] > CC_MAX_EVENTS_PER_TURN) return CC_E_COUNT;", "if (events_per_turn[i] >= CC_MAX_EVENTS_PER_TURN) return CC_E_COUNT;")
drop(M, "if (events_per_turn[i] > CC_MAX_EVENTS_PER_TURN) return CC_E_COUNT;")
mut(M, "s = add_scaled(&total, 1, CC_WIRE_TURN_HEADER);", "s = add_scaled(&total, 0, CC_WIRE_TURN_HEADER);")
mut(M, "s = add_scaled(&total, events_per_turn[i], CC_WIRE_EVENT);", "s = add_scaled(&total, events_per_turn[i] + 1, CC_WIRE_EVENT);")
equiv(M, "    if (s != CC_OK) return s;\n    *out = total;", "    *out = total;",
      "s is only non-OK when add_scaled overflowed, which the count limits make unreachable (see add_scaled)")
mut(M, "    cc_status s = cc_meta_validate(m);\n    if (s != CC_OK) return s;\n    unsigned events",
    "    cc_status s = CC_OK;\n    unsigned events", "cc_wire_meta_size no longer validates")
equiv(M, "const unsigned n_turns = m->has_report ? m->history.n_turns : 0;", "const unsigned n_turns = m->history.n_turns;",
      "cc_meta_validate has already required an empty history when there is no report")
mut(M, "events[i] = m->history.turns[i].n_events;", "events[i] = CC_MAX_EVENTS_PER_TURN;")


def build_and_run(workdir: Path):
    """Returns (stage, detail): stage is 'ok' when both tests pass, else where it failed."""
    exes = {"model_test": "tests/model_test.c", "layout_test": "tests/layout_test.c"}
    for name, src in exes.items():
        cmd = ["cc", *CFLAGS, "-Iinclude", src, *SOURCES, "-o", name]
        try:
            r = subprocess.run(cmd, cwd=workdir, capture_output=True, text=True, timeout=BUILD_TIMEOUT)
        except subprocess.TimeoutExpired:
            return "build-error", f"{name}: compiler timed out"
        if r.returncode != 0:
            if name == "layout_test" and "cc_assert_" in r.stderr:
                return "killed", f"{name}: compile-time assertion"
            return "build-error", f"{name}: {r.stderr.strip()[:300]}"
    for name in exes:
        args = [f"./{name}"] + ([str(ENUMS)] if name == "layout_test" else [])
        try:
            r = subprocess.run(args, cwd=workdir, capture_output=True, text=True, timeout=RUN_TIMEOUT)
        except subprocess.TimeoutExpired:
            return "killed", f"{name}: timed out"
        if r.returncode != 0:
            first = next((l for l in (r.stdout + r.stderr).splitlines() if l.startswith("FAIL") or "ERROR" in l or "runtime error" in l), "")
            return "killed", f"{name}: {first.strip()[:140] or 'exit ' + str(r.returncode)}"
    return "ok", ""


def run_one(index, spec):
    file, old, new, label = spec
    work = Path(tempfile.mkdtemp(prefix="ccmut-"))
    try:
        for d in ("include", "src", "tests"):
            shutil.copytree(ROOT / d, work / d)
        if index is not None:
            text = (work / file).read_text()
            if text.count(old) != 1:
                return index, "bad-pattern", f"{file}: pattern matched {text.count(old)} times"
            (work / file).write_text(text.replace(old, new, 1))
        stage, detail = build_and_run(work)
        return index, stage, detail
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-j", type=int, default=os.cpu_count() or 2)
    ap.add_argument("-k", default="", help="only mutations whose file or label contains this text")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--equivalent", action="store_true", help="print the documented behavior-neutral mutations and why")
    args = ap.parse_args()

    if args.equivalent:
        for file, old, new, reason in EQUIVALENT:
            print(f"{file}: {old.strip()[:70]!r}\n    {reason}")
        return 0
    selected = [(i, m) for i, m in enumerate(MUTATIONS) if args.k in m[0] or args.k in m[3]]
    if args.list:
        for i, m in selected:
            print(f"{i:3d} {m[0]}: {m[3]}")
        return 0
    if not ENUMS.exists():
        print(f"missing {ENUMS}")
        return 2

    stale = 0
    for file, old, new, reason in EQUIVALENT:
        n = (ROOT / file).read_text().count(old)
        if n != 1:
            print(f"STALE equivalent-mutation entry ({file}, matched {n} times): {old.strip()[:60]}")
            stale += 1
    if stale:
        return 2

    _, stage, detail = run_one(None, ("", "", "", ""))
    if stage != "ok":
        print(f"BASELINE FAILED ({stage}): {detail}")
        return 2
    print(f"baseline: builds and passes; running {len(selected)} mutations on {args.j} workers")

    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.j) as pool:
        futures = [pool.submit(run_one, i, m) for i, m in selected]
        for f in concurrent.futures.as_completed(futures):
            i, stage, detail = f.result()
            results[i] = (stage, detail)

    killed = survived = invalid = 0
    for i, m in selected:
        stage, detail = results[i]
        if stage == "killed":
            killed += 1
        elif stage == "ok":
            survived += 1
            print(f"SURVIVED  [{i}] {m[0]}: {m[3]}")
        else:
            invalid += 1
            print(f"INVALID   [{i}] {m[0]}: {m[3]}\n          {stage}: {detail}")
    print(f"mutations: {len(selected)} run, {killed} killed, {survived} survived, {invalid} invalid; "
          f"{len(EQUIVALENT)} more are documented as behavior-neutral and not run")
    return 0 if survived == 0 and invalid == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
