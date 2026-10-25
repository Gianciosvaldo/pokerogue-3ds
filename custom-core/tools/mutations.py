#!/usr/bin/env python3
"""Mutation check for custom-core: the unit tests must be able to fail.

Each mutation is a single textual change to one source or header file. For every mutation this script copies
include/, src/ and tests/ to a scratch directory, applies the change, builds model_test, reducer_test, layout_test, codec_test, commit_test, scheduler_test and jojo_test
from that copy and runs them. A mutation is KILLED when a test fails (a nonzero exit, a sanitizer abort or a timeout) and SURVIVES
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
SOURCES = ["src/status.c", "src/history.c", "src/nemesis.c", "src/schedule.c", "src/ids.c", "src/meta.c", "src/reducer.c",
           "src/codec.c", "src/commit.c", "src/scheduler.c", "src/jojo.c", "src/jojo_table.c"]
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
for tr, ref in [("BOSS", "BOSS"), ("FUSED", "FUSED"), ("TERA", "TERA"),
                ("TRAINER_OWNED", "TRAINER_OWNED"), ("MYSTERY", "MYSTERY"), ("FIXED_SCRIPTED", "FIXED_SCRIPTED"),
                ("REMATCH", "REMATCH")]:
    drop(N, f"if (n->traits & CC_TR_{tr}) return CC_REFUSE_{ref};")
mut(N, "/* Doubles is provenance: an attributed enemy can return alone in a rematch. */",
    "if (n->traits & CC_TR_DOUBLES) return CC_REFUSE_DOUBLES;", "reject attributed doubles killer")
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
mut(D, "(uint32_t)(uid >> 32) == c->install_nonce && ", "")
mut(D, "(uint32_t)uid >= 1 && ", "")
mut(D, "(uint32_t)uid < c->next_run_seq", "(uint32_t)uid <= c->next_run_seq")
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


# ---------------------------------------------------------------- history.c: the attribution of an observation (C02)
for old, new in [
    ("if (sk & ~(uint16_t)CC_EK_SOURCE_ALL) return CC_E_UNKNOWN_BITS;", ";"),
    ("(sk & (CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES)) && !(sk & CC_EK_SOURCE_SIDE)", "(sk & (CC_EK_SOURCE_SPECIES)) && !(sk & CC_EK_SOURCE_SIDE)"),
    ("(sk & (CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES)) && !(sk & CC_EK_SOURCE_SIDE)", "(sk & (CC_EK_SOURCE_ID)) && !(sk & CC_EK_SOURCE_SIDE)"),
    ("if ((sk & (CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES)) && !(sk & CC_EK_SOURCE_SIDE)) return CC_E_CONSISTENCY;", ";"),
    ("if (!(sk & CC_EK_SOURCE_SIDE) && o->src_side) return CC_E_NONCANONICAL;", ";"),
    ("if (!(sk & CC_EK_SOURCE_ID) && o->src_id) return CC_E_NONCANONICAL;", ";"),
    ("if (!(sk & CC_EK_SOURCE_SPECIES) && (o->src_species || o->src_form)) return CC_E_NONCANONICAL;", ";"),
    ("(o->src_species || o->src_form)) return CC_E_NONCANONICAL;", "(o->src_species)) return CC_E_NONCANONICAL;"),
    ("if (!(sk & CC_EK_MOVE) && o->src_move) return CC_E_NONCANONICAL;", ";"),
    ("if ((sk & CC_EK_SOURCE_SIDE) && o->src_side > CC_SIDE_ENEMY) return CC_E_RANGE;", ";"),
    ("o->src_side > CC_SIDE_ENEMY) return CC_E_RANGE;", "o->src_side > CC_SIDE_ENEMY + 1) return CC_E_RANGE;"),
    ("if ((sk & CC_EK_SOURCE_ID) && o->src_id == 0) return CC_E_RANGE;", ";"),
    ("(o->src_species < 1 || o->src_species > CC_SPECIES_ID_MAX)) return CC_E_RANGE;", "(o->src_species > CC_SPECIES_ID_MAX)) return CC_E_RANGE;"),
    ("(o->src_species < 1 || o->src_species > CC_SPECIES_ID_MAX)) return CC_E_RANGE;", "(o->src_species < 1 || o->src_species > CC_SPECIES_ID_MAX + 1)) return CC_E_RANGE;"),
    ("(o->src_species < 1 || o->src_species > CC_SPECIES_ID_MAX)) return CC_E_RANGE;", "(o->src_species < 1 || o->src_species >= CC_SPECIES_ID_MAX)) return CC_E_RANGE;"),
    ("(o->src_move < 1 || o->src_move > CC_MOVE_ID_MAX)) return CC_E_RANGE;", "(o->src_move > CC_MOVE_ID_MAX)) return CC_E_RANGE;"),
    ("(o->src_move < 1 || o->src_move > CC_MOVE_ID_MAX)) return CC_E_RANGE;", "(o->src_move < 1 || o->src_move > CC_MOVE_ID_MAX + 1)) return CC_E_RANGE;"),
    ("if (sk && o->kind != CC_OBS_DAMAGE && o->kind != CC_OBS_FAINT && !(o->kind == CC_OBS_HP && o->value > 0))", "if (0 && sk && o->kind != CC_OBS_DAMAGE && o->kind != CC_OBS_FAINT && !(o->kind == CC_OBS_HP && o->value > 0))"),
    ("if (sk && o->kind != CC_OBS_DAMAGE &&", "if (sk && o->kind != CC_OBS_HP &&"),
    ("o->kind != CC_OBS_DAMAGE && o->kind != CC_OBS_FAINT &&", "o->kind != CC_OBS_FAINT &&"),
    ("o->kind != CC_OBS_DAMAGE && o->kind != CC_OBS_FAINT &&", "o->kind != CC_OBS_DAMAGE &&"),
    ("!(o->kind == CC_OBS_HP && o->value > 0))", "!(o->kind == CC_OBS_HP && o->value >= 0))"),
    ("!(o->kind == CC_OBS_HP && o->value > 0))", "!(o->kind == CC_OBS_HP))"),
    ("!(o->kind == CC_OBS_HP && o->value > 0))", "!(o->kind == CC_OBS_HP && o->value < 0))"),
]:
    mut(H, old, new)
mut(L, "#define CC_MAX_BATCH_OBS 384 ", "#define CC_MAX_BATCH_OBS 383 ")
mut(L, "#define CC_MAX_BATCH_OBS 384 ", "#define CC_MAX_BATCH_OBS 385 ")
mut("include/cc/types.h", "CC_EK_SOURCE_ALL    = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE",
    "CC_EK_SOURCE_ALL    = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES")
mut("include/cc/types.h", "CC_EK_SOURCE_ALL    = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE",
    "CC_EK_SOURCE_ALL    = CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_MOVE")

# ---------------------------------------------------------------- reducer.c (C02)
R = "src/reducer.c"
# batch digest
for stmt in ["*h ^= v;", "*h *= FNV_PRIME;", "fnv8(h, (uint8_t)v);", "fnv8(h, (uint8_t)(v >> 8));", "fnv16(h, (uint16_t)v);",
             "fnv16(h, (uint16_t)(v >> 16));", "fnv32(&h, b->battle_seq);", "fnv16(&h, b->wave);", "fnv16(&h, b->turn);",
             "fnv8(&h, b->flags);", "fnv8(&h, o->kind);", "fnv8(&h, (uint8_t)o->battler);", "fnv8(&h, (uint8_t)o->target);",
             "fnv8(&h, o->battler_form);", "fnv8(&h, o->target_form);", "fnv16(&h, o->battler_species);",
             "fnv16(&h, o->target_species);", "fnv32(&h, o->battler_id);", "fnv32(&h, o->target_id);", "fnv16(&h, o->id);",
             "fnv32(&h, (uint32_t)o->value);", "fnv32(&h, (uint32_t)o->hp_after);", "fnv32(&h, (uint32_t)o->hp_max_after);",
             "fnv8(&h, o->status_after);", "fnv16(&h, o->src_known);", "fnv8(&h, o->src_side);", "fnv8(&h, o->src_form);",
             "fnv16(&h, o->src_species);", "fnv16(&h, o->src_move);", "fnv32(&h, o->src_id);"]:
    drop(R, stmt)
mut(R, "for (unsigned i = 0; i < b->n_obs; i++) {\n        const cc_obs *o = &b->obs[i];",
    "for (unsigned i = 0; i + 1 < b->n_obs; i++) {\n        const cc_obs *o = &b->obs[i];", "digest skips the last observation")
# measuring HP movement
drop(R, "if (o->hp_after < 0) return false;")
mut(R, "prev->species != o->battler_species || prev->form != o->battler_form", "prev->form != o->battler_form")
mut(R, "prev->species != o->battler_species || prev->form != o->battler_form", "prev->species != o->battler_species")
mut(R, "*delta = o->hp_after - prev->hp;", "*delta = prev->hp - o->hp_after;")
# one observation -> one event
drop(R, "if (o->kind == CC_OBS_MOVE_ANIM) return false;")
drop(R, "if (o->kind == CC_OBS_HP && o->value == 0) return false;")
mut(R, "o->kind == CC_OBS_HP && o->value == 0", "o->kind == CC_OBS_HP && o->value == 1")
mut(R, "o->kind == CC_OBS_HP && o->value == 0", "o->kind == CC_OBS_HP && o->value <= 0")
mut(R, "e->target_side = p >= 2 ? CC_SIDE_ENEMY : CC_SIDE_PLAYER;", "e->target_side = p >= 3 ? CC_SIDE_ENEMY : CC_SIDE_PLAYER;")
mut(R, "e->target_side = p >= 2 ? CC_SIDE_ENEMY : CC_SIDE_PLAYER;", "e->target_side = p >= 1 ? CC_SIDE_ENEMY : CC_SIDE_PLAYER;")
mut(R, "e->target_side = p >= 2 ? CC_SIDE_ENEMY : CC_SIDE_PLAYER;", "e->target_side = p >= 2 ? CC_SIDE_PLAYER : CC_SIDE_ENEMY;")
mut(R, "e->target_species = o->battler_species;", "e->target_species = o->target_species;")
mut(R, "e->target_form = o->battler_form;", "e->target_form = o->target_form;")
mut(R, "if (o->battler_id) {", "if (o->target_id) {")
mut(R, "e->target_id = o->battler_id;", "e->target_id = o->target_id;")
drop(R, "e->known |= CC_EK_TARGET_ID;")
drop(R, "e->known |= o->src_known;")
for stmt in ["e->source_side = o->src_side;", "e->source_form = o->src_form;", "e->source_species = o->src_species;",
             "e->move_id = o->src_move;", "e->source_id = o->src_id;"]:
    drop(R, stmt)
mut(R, "e->source_side = o->src_side;", "e->source_side = o->src_form;")
mut(R, "e->move_id = o->src_move;", "e->move_id = o->src_species;")
mut(R, "e->kind = CC_EV_DAMAGE;", "e->kind = CC_EV_HP_LOSS;")
mut(R, "e->known |= CC_EK_RESULT | CC_EK_SHOWN;", "e->known |= CC_EK_SHOWN;")
mut(R, "e->known |= CC_EK_RESULT | CC_EK_SHOWN;", "e->known |= CC_EK_RESULT;")
drop(R, "e->result = (uint8_t)o->id;")
mut(R, "e->shown = o->value;\n        if (measured) {\n            if (delta > 0) {",
    "e->shown = 0;\n        if (measured) {\n            if (delta > 0) {")
mut(R, "if (delta > 0) {", "if (delta >= 0) {")
mut(R, "if (delta > 0) {", "if (delta < 0) {")
mut(R, "(*conflicts)++;                                 /* HP rose", ";                                 /* HP rose")
mut(R, "                e->known |= CC_EK_HP_DELTA;\n                e->hp_delta = delta;\n            }\n        }\n        break;\n    case CC_OBS_HP:",
    "                e->known |= CC_EK_HP_DELTA;\n                e->hp_delta = 0;\n            }\n        }\n        break;\n    case CC_OBS_HP:")
mut(R, "                e->known |= CC_EK_HP_DELTA;\n                e->hp_delta = delta;\n            }\n        }\n        break;\n    case CC_OBS_HP:",
    "                e->hp_delta = delta;\n            }\n        }\n        break;\n    case CC_OBS_HP:", "damage: hp_delta stored without its known bit")
mut(R, "if (o->value > 0) {\n            e->kind = CC_EV_HEAL;", "if (o->value > 1) {\n            e->kind = CC_EV_HEAL;")
mut(R, "e->kind = CC_EV_HEAL;", "e->kind = CC_EV_DAMAGE;")
mut(R, "e->known |= CC_EK_SHOWN;\n            e->shown = o->value;", "e->shown = o->value;", "heal: shown stored without its known bit")
mut(R, "e->shown = o->value;\n            if (measured) {\n                if (delta < 0) {",
    "e->shown = 0;\n            if (measured) {\n                if (delta < 0) {")
mut(R, "if (delta < 0) {", "if (delta <= 0) {")
mut(R, "if (delta < 0) {", "if (delta > 0) {")
mut(R, "(*conflicts)++;\n                } else {", ";\n                } else {")
mut(R, "                    e->known |= CC_EK_HP_DELTA;\n                    e->hp_delta = delta;", "                    e->known |= CC_EK_HP_DELTA;\n                    e->hp_delta = -delta;")
mut(R, "e->kind = CC_EV_HP_LOSS;\n            e->known |= CC_EK_HP_DELTA;", "e->kind = CC_EV_DAMAGE;\n            e->known |= CC_EK_HP_DELTA;")
mut(R, "e->kind = CC_EV_HP_LOSS;\n            e->known |= CC_EK_HP_DELTA;", "e->kind = CC_EV_HP_LOSS;\n            e->known |= CC_EK_SHOWN;")
mut(R, "e->hp_delta = o->value;", "e->hp_delta = -o->value;")
mut(R, "e->kind = CC_EV_FAINT;", "e->kind = CC_EV_SWITCH_OUT;")
mut(R, "e->kind = CC_EV_SWITCH_OUT;", "e->kind = CC_EV_SWITCH_IN;")
mut(R, "e->kind = CC_EV_SWITCH_IN;", "e->kind = CC_EV_FAINT;")
mut(R, "if (o->hp_after >= 0) {\n        e->known |= CC_EK_HP_AFTER;", "if (o->hp_after > 0) {\n        e->known |= CC_EK_HP_AFTER;")
mut(R, "    if (o->hp_after >= 0) {\n        e->known |= CC_EK_HP_AFTER;\n        e->hp_after = o->hp_after;", "    if (o->hp_after >= 0) {\n        e->known |= CC_EK_HP_AFTER;\n        e->hp_after = 0;")
mut(R, "    if (o->hp_after >= 0) {\n        e->known |= CC_EK_HP_AFTER;\n        e->hp_after = o->hp_after;", "    if (o->hp_after >= 0) {\n        e->hp_after = o->hp_after;")
# the tracker
mut(R, "if (o->kind == CC_OBS_FAINT || o->kind == CC_OBS_SWITCH_OUT) {", "if (o->kind == CC_OBS_SWITCH_OUT) {")
mut(R, "if (o->kind == CC_OBS_FAINT || o->kind == CC_OBS_SWITCH_OUT) {", "if (o->kind == CC_OBS_FAINT) {")
drop(R, "memset(&trk[p], 0, sizeof trk[p]);                      /* the occupant is gone */")
mut(R, "} else if (o->hp_after >= 0) {\n        trk[p].species", "} else if (o->hp_after > 0) {\n        trk[p].species")
drop(R, "trk[p].species = o->battler_species;")
drop(R, "trk[p].form = o->battler_form;")
drop(R, "trk[p].hp = o->hp_after;")
drop(R, "memset(&trk[p], 0, sizeof trk[p]);                      /* HP now unknown */")
# plan: argument and state checks
mut(R, "if (!r || !b) return CC_E_NULL;", "if (!r) return CC_E_NULL;")
mut(R, "if (!r || !b) return CC_E_NULL;", "if (!b) return CC_E_NULL;")
drop(R, "if (b->n_obs > 0 && !b->obs) return CC_E_NULL;")
drop(R, "if (b->flags & ~(uint8_t)CC_BF_ALL) return CC_E_UNKNOWN_BITS;")
mut(R, "if (b->n_obs > CC_MAX_BATCH_OBS) return CC_E_COUNT;", "if (b->n_obs >= CC_MAX_BATCH_OBS) return CC_E_COUNT;")
mut(R, "if (b->n_obs > CC_MAX_BATCH_OBS) return CC_E_COUNT;", "if (b->n_obs > CC_MAX_BATCH_OBS + 1) return CC_E_COUNT;")
mut(R, "b->run_uid == 0 || b->batch_id == 0 ||", "b->batch_id == 0 ||")
mut(R, "b->batch_id == 0 || b->battle_seq < 1 ||", "b->battle_seq < 1 ||")
mut(R, "b->battle_seq < 1 || b->wave < 1) return CC_E_RANGE;", "b->wave < 1) return CC_E_RANGE;")
mut(R, "b->battle_seq < 1 || b->wave < 1) return CC_E_RANGE;", "b->battle_seq < 1) return CC_E_RANGE;")
mut(R, "if (r->run_uid == 0 || r->closed) return CC_E_LIFECYCLE;", "if (r->closed) return CC_E_LIFECYCLE;")
mut(R, "if (r->run_uid == 0 || r->closed) return CC_E_LIFECYCLE;", "if (r->run_uid == 0) return CC_E_LIFECYCLE;")
drop(R, "if (b->run_uid != r->run_uid) return CC_E_CONSISTENCY;")
mut(R, "if (h->n_turns > CC_HISTORY_TURNS) return CC_E_CONSISTENCY;", "if (h->n_turns >= CC_HISTORY_TURNS) return CC_E_CONSISTENCY;")
drop(R, "if (h->n_turns > CC_HISTORY_TURNS) return CC_E_CONSISTENCY;")
mut(R, "if (last && last->n_events > CC_MAX_EVENTS_PER_TURN) return CC_E_CONSISTENCY;", "if (last && last->n_events >= CC_MAX_EVENTS_PER_TURN) return CC_E_CONSISTENCY;")
drop(R, "if (last && last->n_events > CC_MAX_EVENTS_PER_TURN) return CC_E_CONSISTENCY;")
drop(R, "if (r->last_batch_id != 0 && !last) return CC_E_CONSISTENCY;")
# plan: batch identity
mut(R, "if (b->batch_id < r->last_batch_id) return CC_E_ORDER;", "if (b->batch_id <= r->last_batch_id) return CC_E_ORDER;")
drop(R, "if (b->batch_id < r->last_batch_id) return CC_E_ORDER;")
mut(R, "if (b->batch_id == r->last_batch_id) {", "if (b->batch_id >= r->last_batch_id) {")
mut(R, "if (b->batch_id == r->last_batch_id) {", "if (b->batch_id != r->last_batch_id) {")
drop(R, "if (pl->digest != r->last_digest) return CC_E_DUPLICATE;")
mut(R, "if (pl->digest != r->last_digest) return CC_E_DUPLICATE;", "if (pl->digest == r->last_digest) return CC_E_DUPLICATE;")
drop(R, "pl->replay = true;")
mut(R, "pl->gap = r->last_batch_id != 0 && b->batch_id - r->last_batch_id > 1;", "pl->gap = r->last_batch_id != 0 && b->batch_id - r->last_batch_id > 2;")
mut(R, "pl->gap = r->last_batch_id != 0 && b->batch_id - r->last_batch_id > 1;", "pl->gap = r->last_batch_id != 0 && b->batch_id - r->last_batch_id >= 1;")
mut(R, "pl->gap = r->last_batch_id != 0 && b->batch_id - r->last_batch_id > 1;", "pl->gap = b->batch_id - r->last_batch_id > 1;")
# plan: the turn key
mut(R, "pl->fresh_tracker = pl->gap || (b->flags & CC_BF_SOURCE_LOSS) != 0;", "pl->fresh_tracker = (b->flags & CC_BF_SOURCE_LOSS) != 0;")
mut(R, "pl->fresh_tracker = pl->gap || (b->flags & CC_BF_SOURCE_LOSS) != 0;", "pl->fresh_tracker = pl->gap;")
mut(R, "if (b->battle_seq < last->battle_seq) return CC_E_ORDER;", "if (b->battle_seq <= last->battle_seq) return CC_E_ORDER;")
drop(R, "if (b->battle_seq < last->battle_seq) return CC_E_ORDER;")
mut(R, "if (b->battle_seq == last->battle_seq) {", "if (b->battle_seq != last->battle_seq) {")
drop(R, "if (b->wave != last->wave) return CC_E_CONSISTENCY;")
mut(R, "if (b->turn < last->turn) return CC_E_ORDER;", "if (b->turn <= last->turn) return CC_E_ORDER;")
drop(R, "if (b->turn < last->turn) return CC_E_ORDER;")
mut(R, "pl->new_turn = b->turn != last->turn;", "pl->new_turn = b->turn == last->turn;")
mut(R, "pl->new_turn = b->turn != last->turn;", "pl->new_turn = true;")
mut(R, "pl->new_turn = b->turn != last->turn;", "pl->new_turn = false;")
mut(R, "if (b->wave < last->wave) return CC_E_ORDER;", "if (b->wave <= last->wave) return CC_E_ORDER;")
drop(R, "if (b->wave < last->wave) return CC_E_ORDER;")
drop(R, "pl->fresh_tracker = true;                           /* another battle: other Pokemon */")
drop(R, "if (!pl->new_turn && (b->flags & CC_BF_BEGAN_MID)) return CC_E_CONSISTENCY;")
mut(R, "if (!pl->new_turn && (b->flags & CC_BF_BEGAN_MID)) return CC_E_CONSISTENCY;", "if ((b->flags & CC_BF_BEGAN_MID)) return CC_E_CONSISTENCY;")
# plan: the simulation
mut(R, "    if (pl->fresh_tracker) memset(trk, 0, sizeof trk);\n    else memcpy(trk, r->pos, sizeof trk);\n    cc_event ev;\n    for (",
    "    memset(trk, 0, sizeof trk);\n    cc_event ev;\n    for (", "plan: tracker always cleared")
mut(R, "    if (pl->fresh_tracker) memset(trk, 0, sizeof trk);\n    else memcpy(trk, r->pos, sizeof trk);\n    cc_event ev;\n    for (",
    "    memcpy(trk, r->pos, sizeof trk);\n    cc_event ev;\n    for (", "plan: tracker never cleared")
mut(R, "cc_status s = cc_obs_validate(&b->obs[i]);", "cc_status s = CC_OK;")
mut(R, "            pl->n_events++;", "            pl->n_events += 2;")
drop(R, "pl->ignored++;")
mut(R, "*bad_obs = (int32_t)i;", "*bad_obs = (int32_t)i + 1;")
mut(R, "if ((uint64_t)r->next_seq + pl->n_events > UINT32_MAX) return CC_E_OVERFLOW;", "if ((uint64_t)r->next_seq + pl->n_events >= UINT32_MAX) return CC_E_OVERFLOW;")
mut(R, "if ((uint64_t)r->next_seq + pl->n_events > UINT32_MAX) return CC_E_OVERFLOW;", "if ((uint32_t)r->next_seq + pl->n_events > UINT32_MAX) return CC_E_OVERFLOW;")
drop(R, "if ((uint64_t)r->next_seq + pl->n_events > UINT32_MAX) return CC_E_OVERFLOW;")
equiv(R, "s = cc_event_validate(&ev);                         /* cannot fail for a valid observation; a guard, not a rule */",
      "(void)cc_event_validate(&ev);",
      "a guard on a proved property: every event built from a valid observation passes cc_event_validate (tests check each event shape)")
# apply: turns
mut(R, "if (pl->gap) h->turns[h->n_turns - 1].flags |= CC_TF_SOURCE_LOSS;", "if (pl->gap) h->turns[h->n_turns - 1].flags |= CC_TF_BEGAN_MID;")
drop(R, "if (pl->gap) h->turns[h->n_turns - 1].flags |= CC_TF_SOURCE_LOSS;")
mut(R, "if (h->n_turns == CC_HISTORY_TURNS) {", "if (h->n_turns > CC_HISTORY_TURNS) {")
mut(R, "if (h->n_turns == CC_HISTORY_TURNS) {", "if (h->n_turns == CC_HISTORY_TURNS - 1) {")
mut(R, "memmove(&h->turns[0], &h->turns[1], (CC_HISTORY_TURNS - 1) * sizeof h->turns[0]);", "memmove(&h->turns[0], &h->turns[2], (CC_HISTORY_TURNS - 2) * sizeof h->turns[0]);")
mut(R, "memmove(&h->turns[0], &h->turns[1], (CC_HISTORY_TURNS - 1) * sizeof h->turns[0]);", "memmove(&h->turns[0], &h->turns[1], (CC_HISTORY_TURNS - 2) * sizeof h->turns[0]);")
drop(R, "res->turn_evicted = 1;")
drop(R, "h->n_turns++;")
drop(R, "memset(t, 0, sizeof *t);")
drop(R, "t->battle_seq = b->battle_seq;")
drop(R, "t->wave = b->wave;")
drop(R, "t->turn = b->turn;")
mut(R, "if (b->flags & CC_BF_BEGAN_MID) t->flags |= CC_TF_BEGAN_MID;", "if (b->flags & CC_BF_BEGAN_MID) t->flags |= CC_TF_SOURCE_LOSS;")
drop(R, "if (b->flags & CC_BF_BEGAN_MID) t->flags |= CC_TF_BEGAN_MID;")
mut(R, "if (pl->gap || (b->flags & CC_BF_SOURCE_LOSS)) t->flags |= CC_TF_SOURCE_LOSS;", "if ((b->flags & CC_BF_SOURCE_LOSS)) t->flags |= CC_TF_SOURCE_LOSS;")
mut(R, "if (pl->gap || (b->flags & CC_BF_SOURCE_LOSS)) t->flags |= CC_TF_SOURCE_LOSS;", "if (pl->gap) t->flags |= CC_TF_SOURCE_LOSS;")
mut(R, "if (pl->gap || (b->flags & CC_BF_SOURCE_LOSS)) t->flags |= CC_TF_SOURCE_LOSS;", "if (pl->gap || (b->flags & CC_BF_SOURCE_LOSS)) t->flags |= CC_TF_BEGAN_MID;")
# apply: eviction inside a turn
mut(R, "const unsigned total = existing + pl->n_events;", "const unsigned total = pl->n_events;")
mut(R, "const unsigned drop = total > CC_MAX_EVENTS_PER_TURN ? total - CC_MAX_EVENTS_PER_TURN : 0;", "const unsigned drop = total > CC_MAX_EVENTS_PER_TURN + 1 ? total - CC_MAX_EVENTS_PER_TURN : 0;")
mut(R, "const unsigned drop = total > CC_MAX_EVENTS_PER_TURN ? total - CC_MAX_EVENTS_PER_TURN : 0;", "const unsigned drop = total > CC_MAX_EVENTS_PER_TURN ? total - CC_MAX_EVENTS_PER_TURN + 1 : 0;")
mut(R, "const unsigned drop = total > CC_MAX_EVENTS_PER_TURN ? total - CC_MAX_EVENTS_PER_TURN : 0;", "const unsigned drop = total > CC_MAX_EVENTS_PER_TURN ? total - CC_MAX_EVENTS_PER_TURN - 1 : 0;")
equiv(R, "const unsigned drop = total > CC_MAX_EVENTS_PER_TURN ? total - CC_MAX_EVENTS_PER_TURN : 0;",
      "const unsigned drop = total >= CC_MAX_EVENTS_PER_TURN ? total - CC_MAX_EVENTS_PER_TURN : 0;",
      "at total == 64 both give a drop of 0")
mut(R, "const unsigned drop_old = drop < existing ? drop : existing;", "const unsigned drop_old = drop > existing ? drop : existing;")
mut(R, "const unsigned drop_old = drop < existing ? drop : existing;", "const unsigned drop_old = drop;")
equiv(R, "const unsigned drop_old = drop < existing ? drop : existing;", "const unsigned drop_old = drop <= existing ? drop : existing;",
      "at drop == existing both give the same value")
mut(R, "const unsigned drop_new = drop - drop_old;", "const unsigned drop_new = drop;")
equiv(R, "    if (drop_old) {\n        memmove(", "    if (1) {\n        memmove(",
      "with drop_old == 0 the move and the clear are no-ops; the guard only saves a copy of the turn onto itself")
mut(R, "memmove(&t->events[0], &t->events[drop_old], (existing - drop_old) * sizeof t->events[0]);", "memmove(&t->events[0], &t->events[drop_old + 1], (existing - drop_old) * sizeof t->events[0]);")
mut(R, "memmove(&t->events[0], &t->events[drop_old], (existing - drop_old) * sizeof t->events[0]);", "memmove(&t->events[0], &t->events[drop_old], (existing - drop_old - 1) * sizeof t->events[0]);")
mut(R, "memmove(&t->events[0], &t->events[drop_old], (existing - drop_old) * sizeof t->events[0]);", "memmove(&t->events[0], &t->events[0], (existing - drop_old) * sizeof t->events[0]);")
drop(R, "t->n_events = (uint8_t)(existing - drop_old);")
mut(R, "t->n_events = (uint8_t)(existing - drop_old);", "t->n_events = (uint8_t)(existing - drop_old + 1);")
# apply: the batch's own events
drop(R, "if (!make_event(trk, &b->obs[i], &ev, &conflicts)) continue;")
mut(R, "if (!make_event(trk, &b->obs[i], &ev, &conflicts)) continue;", "if (make_event(trk, &b->obs[i], &ev, &conflicts)) continue;")
mut(R, "ev.seq = r->next_seq++;", "ev.seq = r->next_seq;")
mut(R, "ev.seq = r->next_seq++;", "ev.seq = ++r->next_seq;")
mut(R, "if (produced++ < drop_new) continue;", "if (produced++ <= drop_new) continue;")
mut(R, "if (produced++ < drop_new) continue;", "if (produced < drop_new) continue;")
drop(R, "if (produced++ < drop_new) continue;")
drop(R, "memcpy(&t->events[t->n_events], &ev, sizeof ev);")
drop(R, "t->n_events++;")
mut(R, "if (drop) {\n        const uint32_t d", "if (drop > 1) {\n        const uint32_t d")
mut(R, "const uint32_t d = (uint32_t)t->dropped + drop;", "const uint32_t d = (uint32_t)t->dropped + drop - 1;")
mut(R, "const uint32_t d = (uint32_t)t->dropped + drop;", "const uint32_t d = drop;")
mut(R, "t->dropped = d > 0xFFFFu ? 0xFFFFu : (uint16_t)d;", "t->dropped = (uint16_t)d;")
mut(R, "t->dropped = d > 0xFFFFu ? 0xFFFFu : (uint16_t)d;", "t->dropped = d > 0xFFFFu ? 0xFFFEu : (uint16_t)d;")
equiv(R, "t->dropped = d > 0xFFFFu ? 0xFFFFu : (uint16_t)d;", "t->dropped = d >= 0xFFFFu ? 0xFFFFu : (uint16_t)d;", "at d == 0xFFFF both give 0xFFFF")
drop(R, "t->flags |= CC_TF_EVICTED;")
drop(R, "memcpy(r->pos, trk, sizeof trk);")
drop(R, "r->last_batch_id = b->batch_id;")
drop(R, "r->last_digest = pl->digest;")
for stmt in ["res->new_turn = pl->new_turn;", "res->gap = pl->gap;", "res->events_added = (uint16_t)pl->n_events;",
             "res->ignored = (uint16_t)pl->ignored;", "res->evicted = (uint16_t)drop;", "res->sign_conflicts = (uint16_t)pl->sign_conflicts;"]:
    drop(R, stmt)
# API
mut(R, "r->next_seq = 1;", "r->next_seq = 0;")
drop(R, "memset(r, 0, sizeof *r);")
drop(R, "if (uid == 0) return CC_E_RANGE;")
drop(R, "if (uid == r->run_uid) return CC_E_DUPLICATE;")
mut(R, "    cc_reducer_init(r);\n    r->run_uid = uid;", "    r->run_uid = uid;", "begin_run no longer resets")
drop(R, "r->run_uid = uid;")
mut(R, "if (resumed) r->hist.flags |= CC_HF_BEGAN_MID;", "if (!resumed) r->hist.flags |= CC_HF_BEGAN_MID;")
drop(R, "if (resumed) r->hist.flags |= CC_HF_BEGAN_MID;")
drop(R, "if (r->run_uid == 0) return CC_E_LIFECYCLE;")
drop(R, "r->closed = 1;")
mut(R, "if (pl.replay) res.duplicate = 1;\n        else apply_batch(r, b, &pl, &res);", "if (pl.replay) res.duplicate = 1;\n        apply_batch(r, b, &pl, &res);", "a replay is applied again")
mut(R, "if (pl.replay) res.duplicate = 1;\n        else apply_batch(r, b, &pl, &res);", "if (pl.replay) ;\n        else apply_batch(r, b, &pl, &res);", "a replay is not reported")
mut(R, "int32_t bad_obs = -1;", "int32_t bad_obs = 0;")
mut(R, "res.bad_obs = bad_obs;", "res.bad_obs = 0;")
drop(R, "if (out) *out = res;")
# state check
mut(R, "cc_status s = cc_history_validate(&r->hist);\n    if (s != CC_OK) return s;", "cc_status s = cc_history_validate(&r->hist);\n    (void)s;")
drop(R, "if (r->closed > 1) return CC_E_RANGE;")
mut(R, "if (r->closed > 1) return CC_E_RANGE;", "if (r->closed >= 1) return CC_E_RANGE;")
drop(R, "if (r->next_seq < 1) return CC_E_RANGE;")
mut(R, "if (r->next_seq < 1) return CC_E_RANGE;", "if (r->next_seq < 2) return CC_E_RANGE;")
mut(R, "if (sn->form || sn->hp) return CC_E_NONCANONICAL;", "if (sn->hp) return CC_E_NONCANONICAL;")
mut(R, "if (sn->form || sn->hp) return CC_E_NONCANONICAL;", "if (sn->form) return CC_E_NONCANONICAL;")
drop(R, "any_pos = true;")
drop(R, "if (sn->species > CC_SPECIES_ID_MAX) return CC_E_RANGE;")
mut(R, "if (sn->species > CC_SPECIES_ID_MAX) return CC_E_RANGE;", "if (sn->species >= CC_SPECIES_ID_MAX) return CC_E_RANGE;")
drop(R, "if (sn->hp < 0) return CC_E_RANGE;")
mut(R, "if (sn->hp < 0) return CC_E_RANGE;", "if (sn->hp <= 0) return CC_E_RANGE;")
mut(R, "if (i >= h->n_turns) {\n            if (!all_zero", "if (i > h->n_turns) {\n            if (!all_zero")
drop(R, "if (!all_zero(t, sizeof *t)) return CC_E_NONCANONICAL;")
mut(R, "(CC_MAX_EVENTS_PER_TURN - t->n_events) * sizeof t->events[0])) {", "(CC_MAX_EVENTS_PER_TURN - t->n_events - 1) * sizeof t->events[0])) {")
mut(R, "!all_zero(&t->events[t->n_events], (CC_MAX", "!all_zero(&t->events[t->n_events + 1], (CC_MAX")
mut(R, "sizeof t->events[0])) {\n            return CC_E_NONCANONICAL;", "sizeof t->events[0])) {\n            ;")
full = "if (r->closed || r->last_batch_id || r->last_digest || r->next_seq != 1 || h->n_turns || h->flags || any_pos)"
for new in ["if (r->last_batch_id || r->last_digest || r->next_seq != 1 || h->n_turns || h->flags || any_pos)",
            "if (r->closed || r->last_digest || r->next_seq != 1 || h->n_turns || h->flags || any_pos)",
            "if (r->closed || r->last_batch_id || r->next_seq != 1 || h->n_turns || h->flags || any_pos)",
            "if (r->closed || r->last_batch_id || r->last_digest || h->n_turns || h->flags || any_pos)",
            "if (r->closed || r->last_batch_id || r->last_digest || r->next_seq != 1 || h->flags || any_pos)",
            "if (r->closed || r->last_batch_id || r->last_digest || r->next_seq != 1 || h->n_turns || any_pos)",
            "if (r->closed || r->last_batch_id || r->last_digest || r->next_seq != 1 || h->n_turns || h->flags)"]:
    mut(R, full, new)
full = "if (r->last_digest || r->next_seq != 1 || h->n_turns || any_pos) return CC_E_CONSISTENCY;"
for new in ["if (r->next_seq != 1 || h->n_turns || any_pos) return CC_E_CONSISTENCY;",
            "if (r->last_digest || h->n_turns || any_pos) return CC_E_CONSISTENCY;",
            "if (r->last_digest || r->next_seq != 1 || any_pos) return CC_E_CONSISTENCY;",
            "if (r->last_digest || r->next_seq != 1 || h->n_turns) return CC_E_CONSISTENCY;"]:
    mut(R, full, new)
drop(R, "if (h->n_turns == 0) return CC_E_CONSISTENCY;")
drop(R, "if (i > 0 && t->wave < h->turns[i - 1].wave) return CC_E_ORDER;")
mut(R, "if (i > 0 && t->wave < h->turns[i - 1].wave) return CC_E_ORDER;", "if (i > 0 && t->wave <= h->turns[i - 1].wave) return CC_E_ORDER;")
drop(R, "if (i > 0 && t->battle_seq == h->turns[i - 1].battle_seq && t->wave != h->turns[i - 1].wave) return CC_E_CONSISTENCY;")
mut(R, "if (i > 0 && t->battle_seq == h->turns[i - 1].battle_seq && t->wave != h->turns[i - 1].wave)", "if (i > 0 && t->wave != h->turns[i - 1].wave)")
drop(R, "have_last = true;")
mut(R, "newest = t->events[t->n_events - 1].seq;", "newest = t->events[0].seq;")
drop(R, "if (have_last && newest >= r->next_seq) return CC_E_CONSISTENCY;")
mut(R, "if (have_last && newest >= r->next_seq) return CC_E_CONSISTENCY;", "if (have_last && newest > r->next_seq) return CC_E_CONSISTENCY;")
# per-turn totals
mut(R, "return v < 0 ? (uint64_t)(-(int64_t)v) : (uint64_t)v;", "return v < 0 ? (uint64_t)(-v) : (uint64_t)v;")
mut(R, "return v < 0 ? (uint64_t)(-(int64_t)v) : (uint64_t)v;", "return (uint64_t)v;")
drop(R, "a->n++;")
mut(R, "if (e->known & CC_EK_SHOWN) {", "if (1) {")
drop(R, "a->n_shown++;")
drop(R, "a->shown += (uint64_t)e->shown;")
mut(R, "a->shown += (uint64_t)e->shown;", "a->shown += (uint64_t)e->shown + 1;")
mut(R, "if (e->known & CC_EK_HP_DELTA) {", "if (1) {")
drop(R, "a->n_measured++;")
drop(R, "a->measured += magnitude(e->hp_delta);")
mut(R, "if (!t || !out) return CC_E_NULL;", "if (!t) return CC_E_NULL;")
equiv(R, "if (!t || !out) return CC_E_NULL;", "if (!out) return CC_E_NULL;", "cc_turn_validate refuses a NULL turn with the same status")
mut(R, "cc_status s = cc_turn_validate(t);\n    if (s != CC_OK) return s;", "cc_status s = cc_turn_validate(t);\n    (void)s;")
mut(R, "sm.exact = t->flags == 0;", "sm.exact = 1;")
mut(R, "sm.exact = t->flags == 0;", "sm.exact = t->flags == 0 && t->dropped == 0 && t->n_events < CC_MAX_EVENTS_PER_TURN;")
mut(R, "sm.exact = t->flags == 0;", "sm.exact = (t->flags & CC_TF_EVICTED) == 0;")
drop(R, "sm.flags = t->flags;")
drop(R, "sm.dropped = t->dropped;")
drop(R, "sm.n_events = t->n_events;")
mut(R, "const unsigned side = e->target_side;", "const unsigned side = e->source_side;")
mut(R, "case CC_EV_DAMAGE: add_amounts(&sm.damage[side], e); break;", "case CC_EV_DAMAGE: add_amounts(&sm.heal[side], e); break;")
mut(R, "case CC_EV_HEAL: add_amounts(&sm.heal[side], e); break;", "case CC_EV_HEAL: add_amounts(&sm.damage[side], e); break;")
mut(R, "case CC_EV_HP_LOSS: add_amounts(&sm.hp_loss[side], e); break;", "case CC_EV_HP_LOSS: add_amounts(&sm.damage[side], e); break;")
mut(R, "case CC_EV_FAINT: sm.n_faint[side]++; break;", "case CC_EV_FAINT: sm.n_switch_out[side]++; break;")
mut(R, "case CC_EV_SWITCH_OUT: sm.n_switch_out[side]++; break;", "case CC_EV_SWITCH_OUT: sm.n_switch_in[side]++; break;")
mut(R, "case CC_EV_SWITCH_IN: sm.n_switch_in[side]++; break;", "case CC_EV_SWITCH_IN: sm.n_switch_out[side]++; break;")
mut(R, "default: sm.n_status[side]++; break;", "default: break;")
drop(R, "memcpy(out, &sm, sizeof sm);")


# ---------------------------------------------------------------- codec.c (C04)
K = "src/codec.c"
mut(K, "if (want > cap || want > CC_META_BUDGET_BYTES) return CC_E_COUNT;", "if (want >= cap || want > CC_META_BUDGET_BYTES) return CC_E_COUNT;")
mut(K, "if (want > cap || want > CC_META_BUDGET_BYTES) return CC_E_COUNT;", "if (want > CC_META_BUDGET_BYTES) return CC_E_COUNT;",
    "encode no longer checks the caller's buffer size")
equiv(K, "if (want > cap || want > CC_META_BUDGET_BYTES) return CC_E_COUNT;", "if (want > cap) return CC_E_COUNT;",
      "the budget check cannot fire: the largest valid blob is 18,378 bytes, far below 64 KiB (layout_test asserts it)")
mut(K, "w8(w, (uint8_t)(v >> 8));", "w8(w, (uint8_t)(v >> 7));", "w16 byte order")
mut(K, "w16(w, (uint16_t)(v >> 16));", "w16(w, (uint16_t)(v >> 15));", "w32 byte order")
mut(K, "w32(w, (uint32_t)(v >> 32));", "w32(w, (uint32_t)(v >> 31));", "w64 byte order")
mut(K, "w16(&w, 0);\n    w32(&w, (uint32_t)(want", "w16(&w, 1);\n    w32(&w, (uint32_t)(want", "reserved written as 1")
mut(K, "(uint32_t)(want - CC_WIRE_ENVELOPE_HEADER)", "(uint32_t)(want - CC_WIRE_ENVELOPE_HEADER + 1)", "body_len one too long")
mut(K, "w32(&w, m->counters.next_record_id);", "w32(&w, m->counters.next_run_seq);", "counter order")
mut(K, "w32(&w, m->counters.install_nonce);", "w32(&w, 0);", "nonce not written")
mut(K, "w64(&w, m->counters.next_txn_id);", "w64(&w, m->counters.next_txn_id & 0xFFFFFFFFu);", "txn counter truncated")
mut(K, "w8(&w, e->outcome);", ";", "ledger outcome not written")
mut(K, "w32(&w, e->record_id);", "w32(&w, 0);", "ledger record id not written")
mut(K, "w64(&w, e->run_uid);", "w64(&w, e->txn_id);", "ledger run uid replaced")
mut(K, "w8(&w, m->roster.count);", "w8(&w, (uint8_t)(m->roster.count - 1));", "roster count off by one")
mut(K, "w8(&w, m->has_report);", "w8(&w, 1);", "has_report always 1")
mut(K, "for (unsigned i = 0; i < m->history.n_turns; i++) w_turn(&w, &m->history.turns[i]);", "for (unsigned i = 0; i + 1 < m->history.n_turns; i++) w_turn(&w, &m->history.turns[i]);", "last turn not written")
mut(K, "for (unsigned i = 0; i < t->n_events; i++) w_event(w, &t->events[i]);", "for (unsigned i = 0; i + 1 < t->n_events; i++) w_event(w, &t->events[i]);", "last event not written")
mut(K, "    w32(w, e->seq);\n    w8(w, e->kind);", "    w32(w, e->seq);\n    w8(w, e->target_side);", "event kind replaced")
mut(K, "wi32(w, e->hp_delta);", "wi32(w, e->hp_after);", "event hp_delta replaced")
mut(K, "w32(w, e->source_id);", "w32(w, e->target_id);", "event source id replaced")
mut(K, "w16(w, e->move_id);", "w16(w, e->source_species);", "event move replaced")
mut(K, "w16(w, e->known);", "w16(w, e->known & 0xFF);", "event known truncated")
mut(K, "w16(w, t->dropped);", "w16(w, 0);", "turn dropped count not written")
mut(K, "w8(w, t->flags);\n    w8(w, t->n_events);", "w8(w, 0);\n    w8(w, t->n_events);", "turn flags not written")
mut(K, "w64(w, t->txn_id);", "w64(w, 0);", "terminal txn not written")
mut(K, "w_actor(w, &t->finish_actor);\n    w_actor(w, &t->fallen_actor);", "w_actor(w, &t->fallen_actor);\n    w_actor(w, &t->finish_actor);", "terminal actors swapped")
mut(K, "w8(w, a->level);", "w8(w, a->side);", "actor level replaced")
mut(K, "w8(w, n->life);", "w8(w, n->traits);", "record life replaced")
mut(K, "wi8(w, n->gender);", "w8(w, 0);", "record gender not written")
mut(K, "w8(w, n->shiny);", "w8(w, 0);", "record shiny not written")
mut(K, "for (int i = 0; i < 6; i++) w8(w, n->ivs[i]);", "for (int i = 0; i < 5; i++) w8(w, n->ivs[i]);", "last IV not written")
mut(K, "for (int k = 0; k < CC_ITEM_KEY_BYTES; k++) w8(w, (uint8_t)n->items[i].key[k]);", "for (int k = 0; k < CC_ITEM_KEY_BYTES - 1; k++) w8(w, (uint8_t)n->items[i].key[k]);", "item key one byte short")
mut(K, "wi16(w, n->items[i].arg);", "w16(w, 0);", "item arg not written")
mut(K, "w16(w, n->items[i].stack);", "w16(w, 1);", "item stack not written")
mut(K, "w64(w, n->sched_run_uid);", "w64(w, 0);", "sched run uid not written")
mut(K, "w16(w, n->challenge_wave);", "w16(w, 0);", "challenge wave not written")
mut(K, "w64(w, n->reward_txn);", "w64(w, 0);", "reward txn not written")
mut(K, "w16(w, n->fallen_species);", "w16(w, n->finish_move);", "fallen species replaced")
equiv(K, "if (w.pos != want) return CC_E_CONSISTENCY;", ";",
      "a defensive check for a bug in this file: it fires only if the writer and cc_wire_meta_size disagree, and every size the tests produce agrees")
# decode
mut(K, "return (int32_t)r32(r); }", "return (int32_t)(r32(r) & 0x7FFFFFFFu); }", "negative values lost on read")
mut(K, "return (int16_t)r16(r); }", "return (int16_t)(r16(r) & 0x7FFFu); }", "negative 16-bit values lost on read")
mut(K, "(uint16_t)(lo | ((uint16_t)r8(r) << 8))", "(uint16_t)(lo | ((uint16_t)r8(r) << 7))", "r16 byte order")
mut(K, "return lo | ((uint32_t)r16(r) << 16);", "return lo | ((uint32_t)r16(r) << 15);", "r32 byte order")
mut(K, "return lo | ((uint64_t)r32(r) << 32);", "return lo | ((uint64_t)r32(r) << 31);", "r64 byte order")
mut(K, "if (r->bad || r->pos >= r->len) {", "if (r->bad || r->pos > r->len) {", "reader allows one byte past the end")
mut(K, "if (magic != CC_META_MAGIC) return CC_E_FORMAT;", ";", "magic not checked")
mut(K, "if (schema != CC_SCHEMA_VERSION) return CC_E_VERSION;", "if (schema > CC_SCHEMA_VERSION) return CC_E_VERSION;", "older schema accepted")
mut(K, "if (schema != CC_SCHEMA_VERSION) return CC_E_VERSION;", "if (schema < CC_SCHEMA_VERSION) return CC_E_VERSION;", "newer schema accepted")
mut(K, "if (reserved != 0) return CC_E_FORMAT;", ";", "reserved not checked")
mut(K, "if (body_len != len - CC_WIRE_ENVELOPE_HEADER) return CC_E_FORMAT;", ";", "body_len not checked")
equiv(K, "if (len < CC_WIRE_ENVELOPE_HEADER) return CC_E_FORMAT;", "if (len < CC_WIRE_ENVELOPE_HEADER - 1) return CC_E_FORMAT;",
      "a blob of 11 bytes still fails: the header length check then compares body_len with an underflowed length and returns the same CC_E_FORMAT (the header read itself is bounded by the reader)")
equiv(K, "if (len > CC_WIRE_META_MAX) return CC_E_FORMAT;", ";",
      "a blob over the maximum cannot be a valid one: body_len equals its length, the body parse then leaves bytes over and the trailing-byte check refuses it with the same status")
mut(K, "if (n_ledger > CC_LEDGER_ENTRIES) return CC_E_COUNT;", "if (n_ledger >= CC_LEDGER_ENTRIES) return CC_E_COUNT;", "")
mut(K, "if (n_ledger > CC_LEDGER_ENTRIES) return CC_E_COUNT;", ";", "ledger count not limited")
mut(K, "if (n_records > CC_MAX_NEMESES) return CC_E_COUNT;", ";", "record count not limited")
equiv(K, "if (out->has_report > 1) return CC_E_RANGE;", ";",
      "cc_meta_validate refuses has_report > 1 with the same status (CC_E_RANGE) right after the body is read")
mut(K, "if (n_turns > CC_HISTORY_TURNS) return CC_E_COUNT;", ";", "turn count not limited")
mut(K, "if (n_events > CC_MAX_EVENTS_PER_TURN) return CC_E_COUNT;", ";", "event count not limited")
mut(K, "    if (r->bad) return CC_E_FORMAT;                      /* a count promised more bytes than the body has */\n", "", "truncation at the end not noticed")
mut(K, "if (r->pos != r->len) return CC_E_FORMAT;", ";", "trailing bytes accepted")
mut(K, "    out->schema = CC_SCHEMA_VERSION;\n", "", "decoded schema not set")
mut(K, "if (s == CC_OK) s = cc_meta_validate(out);", ";", "decoded state not validated")
mut(K, "if (s != CC_OK) memset(out, 0, sizeof *out);", ";", "a refused blob leaves a half-read state")
mut(K, "    if (!out) return CC_E_NULL;\n    memset(out, 0, sizeof *out);\n", "    if (!out) return CC_E_NULL;\n", "output not cleared first")

# ---------------------------------------------------------------- commit.c (C04)
T = "src/commit.c"
mut(T, "        if (cc_life_overwritable(r->rec[i].life)) return (int)i;", "        if (!cc_life_overwritable(r->rec[i].life)) return (int)i;", "overwrite picks a protected record")
mut(T, "    for (unsigned i = 0; i < r->count; i++)\n        if (cc_life_overwritable", "    for (unsigned i = r->count; i-- > 0;)\n        if (cc_life_overwritable", "overwrite picks the newest, not the oldest")
mut(T, "r->rec[i].source_run_uid == n->source_run_uid && r->rec[i].source_pokemon_id == n->source_pokemon_id", "r->rec[i].source_run_uid == n->source_run_uid || r->rec[i].source_pokemon_id == n->source_pokemon_id", "")
mut(T, "r->rec[i].source_run_uid == n->source_run_uid && r->rec[i].source_pokemon_id == n->source_pokemon_id", "r->rec[i].source_pokemon_id == n->source_pokemon_id", "")
mut(T, "    memset(res, 0, sizeof *res);\n", "", "result not cleared")
mut(T, "    cc_status s = cc_meta_validate(m);\n    if (s != CC_OK) return s;\n    s = cc_terminal_validate", "    cc_status s = CC_OK;\n    s = cc_terminal_validate", "stored state not validated")
mut(T, "    s = cc_terminal_validate(term, false);\n    if (s != CC_OK) return s;\n", "", "terminal not validated")
mut(T, "    s = cc_history_validate(hist);\n    if (s != CC_OK) return s;\n", "", "history not validated")
mut(T, "if (!cc_run_uid_issued(&m->counters, term->run_uid)) return CC_E_CONSISTENCY;", ";", "run uid not checked against the counters")
mut(T, "        res->replayed = true;", "        res->replayed = false;", "")
mut(T, "res->refusal = prior.outcome == CC_TXO_INSERTED ? CC_SUPPORT_OK : -1;", "res->refusal = prior.outcome == CC_TXO_INSERTED ? -1 : CC_SUPPORT_OK;", "")
mut(T, "        res->record_id = prior.record_id;", "        res->record_id = 0;", "")
mut(T, "        res->txn_id = prior.txn_id;", "        res->txn_id = 0;", "")
mut(T, "if (cand->record_id != 0) return CC_E_RANGE;", ";", "candidate may carry its own record id")
mut(T, "rec.record_id = counters.next_record_id;", "rec.record_id = 0;", "")
mut(T, "if (rec.life != CC_LIFE_STORED) return CC_E_LIFECYCLE;", ";", "a scheduled candidate accepted")
mut(T, "refusal = cc_insertion_check(term, &rec);", "refusal = CC_SUPPORT_OK;", "the insertion gate is skipped")
mut(T, "if (roster_has_source(&m->roster, &rec)) return CC_E_DUPLICATE;", ";", "same enemy of the same run inserted twice")
mut(T, "            if (m->roster.count == CC_MAX_NEMESES) {\n                slot = overwrite_slot", "            if (m->roster.count >= CC_MAX_NEMESES - 1) {\n                slot = overwrite_slot", "")
mut(T, "if (slot < 0) refusal = CC_REFUSE_ROSTER_FULL;", "if (slot < 0) refusal = CC_REFUSE_BOSS;", "")
mut(T, "if (slot < 0) refusal = CC_REFUSE_ROSTER_FULL;", ";", "a full protected roster still inserts")
mut(T, "const bool insert = refusal == CC_SUPPORT_OK;", "const bool insert = true;", "")
mut(T, "    if (insert) {\n        s = cc_alloc_record_id", "    if (1) {\n        s = cc_alloc_record_id", "a record id is spent without inserting")
mut(T, "(uint8_t)(insert ? CC_TXO_INSERTED : CC_TXO_REPORT_ONLY)", "(uint8_t)(insert ? CC_TXO_REPORT_ONLY : CC_TXO_INSERTED)", "")
mut(T, "    m->counters = counters;\n", "", "counters not advanced")
mut(T, "    m->ledger = ledger;\n", "", "ledger not appended")
mut(T, "    m->has_report = 1;\n", "", "has_report not set")
mut(T, "    m->report = *term;\n", "    memset(&m->report, 0, sizeof m->report);\n", "report not stored")
mut(T, "    m->report.txn_id = txn_id;\n", "", "report keeps txn 0")
mut(T, "    m->history = *hist;\n", "", "history not stored")
mut(T, "            res->overwritten = m->roster.rec[slot].record_id;\n", "", "overwritten id not reported")
mut(T, "for (int i = slot + 1; i < CC_MAX_NEMESES; i++)", "for (int i = slot; i < CC_MAX_NEMESES; i++)", "")
mut(T, "            m->roster.count--;\n", "", "roster not shortened before the append")
mut(T, "m->roster.rec[m->roster.count++] = rec;", "m->roster.rec[m->roster.count] = rec;", "inserted record not counted")
mut(T, "    res->outcome = entry.outcome;\n", "", "")
mut(T, "    res->refusal = refusal;\n", "", "")
mut(T, "    res->record_id = record_id;\n    return CC_OK;", "    return CC_OK;", "")



# ---------------------------------------------------------------- scheduler.c (C05)
Q = "src/scheduler.c"
# the stream and the one integer selection
mut(Q, "*state += 0x9E3779B97F4A7C15ull;", "*state += 0x9E3779B97F4A7C16ull;", "stream increment")
mut(Q, "z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;", "z = (z ^ (z >> 29)) * 0xBF58476D1CE4E5B9ull;", "stream shift 30")
mut(Q, "z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;", "z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5BAull;", "stream multiplier 1")
mut(Q, "z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;", "z = (z ^ (z >> 26)) * 0x94D049BB133111EBull;", "stream shift 27")
mut(Q, "z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;", "z = (z ^ (z >> 27)) * 0x94D049BB133111ECull;", "stream multiplier 2")
mut(Q, "return z ^ (z >> 31);", "return z ^ (z >> 30);", "stream final shift")
mut(Q, "return z ^ (z >> 31);", "return z;", "stream final xorshift removed")
mut(Q, "(unsigned)(r % n)", "(unsigned)(r % (n > 1 ? n - 1 : n))", "selection: modulus one short")
mut(Q, "(unsigned)(r % n)", "(unsigned)((r >> 1) % n)", "selection: shifted output")
mut(Q, "(unsigned)(r % n)", "(unsigned)(r & (n - 1))", "selection: mask instead of modulus")
mut(Q, "(unsigned)(r % n)", "(unsigned)((r >> 32) % n)", "selection: high half only")
mut(Q, "(unsigned)(r % n)", "(unsigned)(n - 1 - r % n)", "selection: reversed")
mut(Q, "    const uint64_t r = cc_rng_v1_next(state);\n    return", "    const uint64_t r = cc_rng_v1_next(state);\n    cc_rng_v1_next(state);\n    return", "pick draws twice")
mut(Q, "        const unsigned pick = cc_rng_v1_pick(&state, ns.n_frozen);", "        const unsigned pick = cc_rng_v1_pick(&state, ns.n_frozen);\n        cc_rng_v1_next(&state);", "start draws a second time")
mut(Q, "        uint64_t state = seed;", "        uint64_t state = seed ^ 1;", "start does not seed the stream with the seed")
mut(Q, "        ns.rng_seed = seed;\n", "", "seed not recorded")
mut(Q, "        ns.rng_state = state;\n", "        ns.rng_state = seed;\n", "state after the draw not stored")
mut(Q, "        ns.chosen_id = ns.frozen_ids[pick];", "        ns.chosen_id = ns.frozen_ids[0];", "first candidate always chosen")
mut(Q, "        ns.chosen_id = ns.frozen_ids[pick];", "        ns.chosen_id = ns.frozen_ids[ns.n_frozen - 1];", "last candidate always chosen")
mut(Q, "        ns.chosen_id = ns.frozen_ids[pick];", "        ns.chosen_id = ns.frozen_ids[(pick + 1) % ns.n_frozen];", "pick shifted by one")
# the candidate and record-count rules
mut(Q, "if (n->life != CC_LIFE_STORED || cc_nemesis_support(n) != CC_SUPPORT_OK) continue;", "if (cc_nemesis_support(n) != CC_SUPPORT_OK) continue;", "candidates: life not required")
mut(Q, "if (n->life != CC_LIFE_STORED || cc_nemesis_support(n) != CC_SUPPORT_OK) continue;", "if (n->life != CC_LIFE_STORED && n->life != CC_LIFE_DEFEATED || cc_nemesis_support(n) != CC_SUPPORT_OK) continue;", "candidates: defeated records allowed")
mut(Q, "if (n->life != CC_LIFE_STORED || cc_nemesis_support(n) != CC_SUPPORT_OK) continue;", "if (n->life != CC_LIFE_STORED && n->life != CC_LIFE_REWARD_CLAIMED || cc_nemesis_support(n) != CC_SUPPORT_OK) continue;", "candidates: claimed records allowed")
mut(Q, "        if (representable && !representable[i]) continue;\n", "", "candidates: representable mask ignored")
mut(Q, "if (representable && !representable[i]) continue;", "if (representable && representable[i]) continue;", "candidates: representable mask inverted")
mut(Q, "if (representable && !representable[i]) continue;", "if (representable && !representable[0]) continue;", "candidates: mask read at slot 0")
mut(Q, "    for (unsigned i = 0; i < r->count; i++) {\n        const cc_nemesis *n = &r->rec[i];", "    for (unsigned i = 0; i + 1 < r->count; i++) {\n        const cc_nemesis *n = &r->rec[i];", "candidates: last record never counted")
mut(Q, "    for (unsigned i = 0; i < r->count; i++) {\n        const cc_nemesis *n = &r->rec[i];", "    for (unsigned i = 1; i < r->count; i++) {\n        const cc_nemesis *n = &r->rec[i];", "candidates: first record never counted")
mut(Q, "    for (unsigned i = 0; i < r->count; i++) {\n        const cc_nemesis *n = &r->rec[i];", "    for (unsigned i = 0; i < r->count && ns.n_frozen < 8; i++) {\n        const cc_nemesis *n = &r->rec[i];", "candidates: at most eight counted")
mut(Q, "ns.frozen_ids[ns.n_frozen++] = n->record_id;", "ns.frozen_ids[ns.n_frozen] = n->record_id;", "candidate count never grows")
mut(Q, "if (ns.n_frozen == 0) {", "if (ns.n_frozen <= 1) {", "no candidate: one record is not enough")
mut(Q, "if (ns.n_frozen == 0) {", "if (ns.n_frozen == 0 || r->count < 2) {", "no candidate: a single stored record is not enough")
mut(Q, "if (ns.n_frozen == 0) {", "if (ns.n_frozen == 1) {", "no candidate: exactly one record")
mut(Q, "        ns.state = CC_SCHED_NO_CANDIDATE;\n", "        ns.state = CC_SCHED_EXHAUSTED;\n", "no candidate: wrong state")
mut(Q, "        ns.rng_algo = CC_RNG_ALGO_V1;\n", "", "algorithm not recorded")
mut(Q, "        ns.n_windows = CC_NUM_WINDOWS;\n", "        ns.n_windows = CC_NUM_WINDOWS - 1;\n", "one window fewer")
mut(Q, "        for (int i = 0; i < CC_NUM_WINDOWS; i++) ns.windows[i].wave = cc_window_waves[i];", "        for (int i = 0; i < CC_NUM_WINDOWS; i++) ns.windows[i].wave = cc_window_waves[0];", "every window at the first wave")
mut(Q, "    ns.run_uid = run_uid;\n", "", "run uid not recorded")
drop(Q, "if (is_held(&r->rec[i])) return CC_E_CONSISTENCY;")
mut(Q, "        r->rec[chosen_slot] = rec;\n", "", "chosen record not scheduled")
mut(Q, "cc_nemesis_transition(&rec, CC_LIFE_SCHEDULED, run_uid, 0, 0)", "cc_nemesis_transition(&rec, CC_LIFE_SCHEDULED, run_uid + 1, 0, 0)", "scheduled by another run")
mut(Q, "    *s = ns;\n    return CC_OK;\n}\n\n/* `out` already holds", "    return CC_OK;\n}\n\n/* `out` already holds", "schedule not stored")
# encounter classification
mut(Q, "case CC_ENC_FINAL: r = CC_SKIP_FINAL; break;", "case CC_ENC_FINAL: r = CC_SKIP_BOSS; break;", "classify: final reported as boss")
mut(Q, "case CC_ENC_BOSS: r = CC_SKIP_BOSS; break;", "case CC_ENC_BOSS: r = CC_SKIP_FINAL; break;", "classify: boss reported as final")
mut(Q, "case CC_ENC_TRAINER: r = CC_SKIP_TRAINER; break;", "case CC_ENC_TRAINER: r = CC_SKIP_NONE; break;", "classify: trainer allowed")
mut(Q, "case CC_ENC_MYSTERY: r = CC_SKIP_MYSTERY; break;", "case CC_ENC_MYSTERY: r = CC_SKIP_NONE; break;", "classify: mystery allowed")
mut(Q, "case CC_ENC_FIXED_OR_SCRIPTED: r = CC_SKIP_FIXED_OR_SCRIPTED; break;", "case CC_ENC_FIXED_OR_SCRIPTED: r = CC_SKIP_NONE; break;", "classify: scripted allowed")
mut(Q, "case CC_ENC_FIXED_OR_SCRIPTED: r = CC_SKIP_FIXED_OR_SCRIPTED; break;", "case CC_ENC_FIXED_OR_SCRIPTED: r = CC_SKIP_TRAINER; break;", "classify: scripted reported as trainer")
mut(Q, "case CC_ENC_UNSUPPORTED: r = CC_SKIP_UNSUPPORTED; break;", "case CC_ENC_UNSUPPORTED: r = CC_SKIP_NONE; break;", "classify: unsupported allowed")
mut(Q, "default: r = enc->flags ? CC_SKIP_UNSUPPORTED : CC_SKIP_NONE; break;", "default: r = CC_SKIP_NONE; break;", "classify: fusion, Tera and doubles allowed")
mut(Q, "default: r = enc->flags ? CC_SKIP_UNSUPPORTED : CC_SKIP_NONE; break;", "default: r = (enc->flags & ~CC_ENF_DOUBLES) ? CC_SKIP_UNSUPPORTED : CC_SKIP_NONE; break;", "classify: doubles allowed")
mut(Q, "default: r = enc->flags ? CC_SKIP_UNSUPPORTED : CC_SKIP_NONE; break;", "default: r = (enc->flags & ~CC_ENF_TERA) ? CC_SKIP_UNSUPPORTED : CC_SKIP_NONE; break;", "classify: Tera allowed")
mut(Q, "default: r = enc->flags ? CC_SKIP_UNSUPPORTED : CC_SKIP_NONE; break;", "default: r = (enc->flags & ~CC_ENF_FUSED) ? CC_SKIP_UNSUPPORTED : CC_SKIP_NONE; break;", "classify: fusion allowed")
mut(Q, "if (enc->kind >= CC_ENC_COUNT_ || (enc->flags & ~CC_ENF_ALL)) return CC_E_RANGE;", "if (enc->kind >= CC_ENC_COUNT_) return CC_E_RANGE;", "classify: unknown flag accepted")
mut(Q, "if (enc->kind >= CC_ENC_COUNT_ || (enc->flags & ~CC_ENF_ALL)) return CC_E_RANGE;", "if ((enc->flags & ~CC_ENF_ALL)) return CC_E_RANGE;", "classify: unknown kind accepted")
mut(Q, "    *reason = r;\n    return CC_OK;", "    return CC_OK;", "classify: reason not written")
# windows
mut(Q, "    if (j < 0) {\n        p.why = CC_WHY_NOT_A_WINDOW;", "    if (j < 0 && s->state == 99) {\n        p.why = CC_WHY_NOT_A_WINDOW;", "any wave is a window")
mut(Q, "    p.wave = enc->wave;\n", "", "proposal wave not set")
mut(Q, "    p.opportunity = (uint8_t)(j + 1);\n    if (s->state", "    if (s->state", "window answer without opportunity")
mut(Q, "    p.opportunity = (uint8_t)(j + 1);\n    if (s->state", "    p.opportunity = (uint8_t)j;\n    if (s->state", "opportunity numbered from 0")
mut(Q, "    if (s->state == CC_SCHED_NO_CANDIDATE) {\n        p.why = CC_WHY_NO_CANDIDATE;", "    if (s->state == CC_SCHED_NO_CANDIDATE) {\n        p.why = CC_WHY_SKIPPED;", "no-candidate reported as skipped")
mut(Q, "        p.skip = w->reason;\n", "", "replayed skip loses its reason")
mut(Q, "    case CC_WIN_OFFERED:\n        fill_offer(&p, CC_PROP_OFFER, s->chosen_id);", "    case CC_WIN_OFFERED:\n        fill_offer(&p, CC_PROP_RESUME, s->chosen_id);", "replayed offer reported as resume")
mut(Q, "    case CC_WIN_ACCEPTED:\n        fill_offer(&p, CC_PROP_RESUME, s->chosen_id);", "    case CC_WIN_ACCEPTED:\n        fill_offer(&p, CC_PROP_OFFER, s->chosen_id);", "accepted window reported as a fresh offer")
mut(Q, "    if (s->state == CC_SCHED_DECLINED) {\n        p.why = CC_WHY_DECLINED;", "    if (s->state == CC_SCHED_DECLINED && j == 0) {\n        p.why = CC_WHY_DECLINED;", "decline consumes only the first window")
mut(Q, "    if (s->state == CC_SCHED_ACCEPTED) {\n        p.why = CC_WHY_RESOLVED;", "    if (s->state == CC_SCHED_ACCEPTED) {\n        p.why = CC_WHY_DECLINED;", "resolved reported as declined")
mut(Q, "if (s->state != CC_SCHED_ACTIVE || j != first_in_state(s, CC_WIN_PENDING)) return CC_E_ORDER;", "if (s->state != CC_SCHED_ACTIVE) return CC_E_ORDER;", "windows may be reported out of order")
mut(Q, "if (s->state != CC_SCHED_ACTIVE || j != first_in_state(s, CC_WIN_PENDING)) return CC_E_ORDER;", "if (j != first_in_state(s, CC_WIN_PENDING)) return CC_E_ORDER;", "window order check misses a non-active schedule")
mut(Q, "        ns.windows[j].reason = reason;\n", "", "skip reason not stored")
mut(Q, "        ns.windows[j].state = CC_WIN_SKIPPED;\n", "        ns.windows[j].state = CC_WIN_DECLINED;\n", "skipped window stored as declined")
mut(Q, "if (j == CC_NUM_WINDOWS - 1) {", "if (j == CC_NUM_WINDOWS - 2) {", "exhausted one window early")
mut(Q, "if (j == CC_NUM_WINDOWS - 1) {", "if (j >= 0) {", "exhausted at every skip")
mut(Q, "if (j == CC_NUM_WINDOWS - 1) {", "if (j == CC_NUM_WINDOWS) {", "never exhausted")
mut(Q, "            ns.state = CC_SCHED_EXHAUSTED;\n", "", "exhausted state not set")
mut(Q, "            st = cc_nemesis_transition(&rec, CC_LIFE_STORED, 0, 0, 0);\n            if (st != CC_OK) return st;\n        }", "        }", "record not released at exhaustion")
mut(Q, "        if (slot >= 0) r->rec[slot] = rec;\n", "", "released record not stored")
mut(Q, "        ns.windows[j].state = CC_WIN_OFFERED;\n", "        ns.windows[j].state = CC_WIN_ACCEPTED;\n", "offered window stored as accepted")
mut(Q, "        ns.state = CC_SCHED_OFFERED;\n", "        ns.state = CC_SCHED_ACCEPTED;\n", "offered schedule stored as accepted")
mut(Q, "    out->kind = kind;\n", "", "offer kind missing")
mut(Q, "    out->level_rule = CC_LEVEL_ORDINARY_WAVE;\n", "", "offer level rule missing")
mut(Q, "    out->reward_vouchers = (uint8_t)CC_REMATCH_REWARD_VOUCHERS;\n", "    out->reward_vouchers = 2;\n", "offer allowance of two vouchers")
mut(Q, "    out->reward_vouchers = (uint8_t)CC_REMATCH_REWARD_VOUCHERS;\n", "", "offer allowance missing")
mut(Q, "    out->record_id = id;\n", "", "offer record missing")
# the answer
mut(Q, "    if (s->state == CC_SCHED_ACCEPTED && accept) {", "    if (s->state == CC_SCHED_ACCEPTED) {", "accept replay also swallows a decline")
mut(Q, "    if (s->state == CC_SCHED_DECLINED && !accept) {", "    if (s->state == CC_SCHED_DECLINED) {", "decline replay also swallows an accept")
mut(Q, "        if (replayed) *replayed = true;\n        return CC_OK;\n    }\n    if (s->state == CC_SCHED_DECLINED", "        return CC_OK;\n    }\n    if (s->state == CC_SCHED_DECLINED", "accept replay not flagged")
mut(Q, "    if (s->state != CC_SCHED_OFFERED) return CC_E_LIFECYCLE;\n    const int j", "    const int j", "answer without an offer")
mut(Q, "cc_nemesis_transition(&rec, CC_LIFE_CHALLENGED, s->run_uid, s->windows[j].wave, 0)", "cc_nemesis_transition(&rec, CC_LIFE_CHALLENGED, s->run_uid, 0, 0)", "challenge without its wave")
mut(Q, "cc_nemesis_transition(&rec, CC_LIFE_CHALLENGED, s->run_uid, s->windows[j].wave, 0)", "cc_nemesis_transition(&rec, CC_LIFE_CHALLENGED, s->run_uid, cc_window_waves[0], 0)", "challenge at the first window's wave")
mut(Q, "        ns.state = CC_SCHED_ACCEPTED;\n        ns.windows[j].state = CC_WIN_ACCEPTED;", "        ns.state = CC_SCHED_ACCEPTED;", "accepted window not marked")
mut(Q, "        ns.state = CC_SCHED_DECLINED;\n        ns.windows[j].state = CC_WIN_DECLINED;", "        ns.state = CC_SCHED_ACTIVE;\n        ns.windows[j].state = CC_WIN_PENDING;", "decline does not consume the schedule")
mut(Q, "        ns.state = CC_SCHED_DECLINED;\n        ns.windows[j].state = CC_WIN_DECLINED;", "        ns.state = CC_SCHED_DECLINED;\n        ns.windows[j].state = CC_WIN_SKIPPED;", "declined window marked skipped")
mut(Q, "        st = cc_nemesis_transition(&rec, CC_LIFE_STORED, 0, 0, 0);\n        ns.state = CC_SCHED_DECLINED;", "        ns.state = CC_SCHED_DECLINED;", "decline keeps the record scheduled")
mut(Q, "    r->rec[slot] = rec;\n    *s = ns;\n    return CC_OK;\n}\n\ncc_status cc_rematch_result", "    *s = ns;\n    return CC_OK;\n}\n\ncc_status cc_rematch_result", "answer does not store the record")
mut(Q, "    r->rec[slot] = rec;\n    *s = ns;\n    return CC_OK;\n}\n\ncc_status cc_rematch_result", "    r->rec[slot] = rec;\n    return CC_OK;\n}\n\ncc_status cc_rematch_result", "answer does not store the schedule")
# the result
mut(Q, "    if (s->state != CC_SCHED_ACCEPTED) return CC_E_LIFECYCLE;\n    const int slot = find_record(r, s->chosen_id);\n    if (slot < 0) return CC_E_CONSISTENCY;", "    const int slot = find_record(r, s->chosen_id);\n    if (slot < 0) return CC_E_CONSISTENCY;", "result without an accepted rematch")
mut(Q, "if (rec.life == CC_LIFE_CHALLENGED && rec.sched_run_uid == s->run_uid) {", "if (rec.life == CC_LIFE_CHALLENGED) {", "result ignores the run that holds the record")
mut(Q, "won ? CC_LIFE_DEFEATED : CC_LIFE_STORED", "won ? CC_LIFE_STORED : CC_LIFE_DEFEATED", "win and loss swapped")
mut(Q, "won ? CC_LIFE_DEFEATED : CC_LIFE_STORED", "CC_LIFE_DEFEATED", "a loss counts as a win")
mut(Q, "won ? CC_LIFE_DEFEATED : CC_LIFE_STORED", "CC_LIFE_STORED", "a win counts as a loss")
mut(Q, "        r->rec[slot] = rec;\n        return CC_OK;\n    }\n    /* the outcome", "        return CC_OK;\n    }\n    /* the outcome", "result not stored")
mut(Q, "(rec.life == CC_LIFE_DEFEATED || rec.life == CC_LIFE_REWARD_CLAIMED) && rec.sched_run_uid == s->run_uid", "rec.life == CC_LIFE_DEFEATED && rec.sched_run_uid == s->run_uid", "claimed record is no longer a won replay")
mut(Q, "(rec.life == CC_LIFE_DEFEATED || rec.life == CC_LIFE_REWARD_CLAIMED) && rec.sched_run_uid == s->run_uid", "(rec.life == CC_LIFE_DEFEATED || rec.life == CC_LIFE_REWARD_CLAIMED)", "won replay ignores the run")
mut(Q, "const bool was_lost = rec.life == CC_LIFE_STORED;", "const bool was_lost = rec.life != CC_LIFE_CHALLENGED;", "lost replay matches anything")
mut(Q, "    if ((won && was_won) || (!won && was_lost)) {\n        if (replayed) *replayed = true;", "    if ((won && was_won) || (!won && was_lost)) {", "result replay not flagged")
mut(Q, "if ((won && was_won) || (!won && was_lost)) {", "if (was_won || was_lost) {", "result replay accepts the other outcome")
# the view
mut(Q, "    v.n_candidates = s->n_frozen;\n", "", "view hides the candidate count")
mut(Q, "    v.record_id = s->chosen_id;\n", "", "view hides the record")
mut(Q, "        v.next_opportunity = (uint8_t)(next + 1);\n", "", "view hides the next opportunity")
mut(Q, "        v.next_wave = s->windows[next].wave;\n", "", "view hides the next wave")
mut(Q, "        next = first_in_state(s, CC_WIN_OFFERED);\n", "        next = -1;\n", "view hides the offered window")
mut(Q, "    v.state = s->state;\n", "", "view hides the state")
# release
mut(Q, "if (uid == 0 && !except) return CC_E_RANGE;", "if (uid == 0) return CC_E_RANGE;", "release-except refuses uid 0")
mut(Q, "if (uid == 0 && !except) return CC_E_RANGE;", "", "release accepts uid 0")
mut(Q, "if ((r->rec[i].sched_run_uid == uid) == except) continue;", "if ((r->rec[i].sched_run_uid == uid) != except) continue;", "release polarity inverted")
mut(Q, "if ((r->rec[i].sched_run_uid == uid) == except) continue;", "", "release ignores the run")
mut(Q, "        if (!is_held(&r->rec[i])) continue;\n", "", "release also touches records that are not held")
mut(Q, "        r->rec[i] = rec;                                           /* a valid", "        /* a valid", "release does not store")
mut(Q, "        if (released) *released = 1;\n", "", "release count missing")
mut(Q, "    if (released) *released = 0;\n    if (!r) return CC_E_NULL;", "    if (!r) return CC_E_NULL;", "release count not reset")
# entitlement and claim
mut(Q, "    cc_status st = cc_nemesis_validate(n);\n    if (st != CC_OK) return st;\n    cc_reward_entitlement e;", "    cc_reward_entitlement e;", "entitlement of an invalid record")
mut(Q, "        e.state = CC_RW_OWED;\n        e.vouchers = (uint8_t)CC_REMATCH_REWARD_VOUCHERS;", "        e.state = CC_RW_OWED;", "owed without vouchers")
mut(Q, "        e.state = CC_RW_OWED;\n        e.vouchers = (uint8_t)CC_REMATCH_REWARD_VOUCHERS;", "        e.state = CC_RW_OWED;\n        e.vouchers = 2;", "owed two vouchers")
mut(Q, "    } else if (n->life == CC_LIFE_REWARD_CLAIMED) {\n        e.state = CC_RW_CLAIMED;", "    } else if (n->life == CC_LIFE_REWARD_CLAIMED) {\n        e.state = CC_RW_OWED;", "claimed record still owed")
mut(Q, "    if (n->life == CC_LIFE_DEFEATED) {\n        e.state", "    if (n->life == CC_LIFE_DEFEATED || n->life == CC_LIFE_CHALLENGED) {\n        e.state", "challenged record already owed")
mut(Q, "    if (n->life == CC_LIFE_DEFEATED) {\n        e.state", "    if (n->life == CC_LIFE_DEFEATED || n->life == CC_LIFE_STORED) {\n        e.state", "stored record owed")
mut(Q, "    if (slot < 0) return CC_E_RANGE;\n    const cc_nemesis *n = &m->roster", "    const cc_nemesis *n = &m->roster", "claim of an unknown record")
mut(Q, "if (have_prior && prior.txn_id != n->reward_txn) return CC_E_CONSISTENCY;", "", "claimed record with a foreign ledger entry accepted")
mut(Q, "        res->replayed = true;\n        res->txn_id = n->reward_txn;", "        res->txn_id = n->reward_txn;", "claim replay not flagged")
mut(Q, "        res->replayed = true;\n        res->txn_id = n->reward_txn;", "        res->replayed = true;", "claim replay without its transaction")
mut(Q, "    if (n->life != CC_LIFE_DEFEATED) return CC_E_LIFECYCLE;", "    if (n->life != CC_LIFE_DEFEATED && n->life != CC_LIFE_STORED) return CC_E_LIFECYCLE;", "claim of a stored record")
mut(Q, "    if (have_prior) return CC_E_CONSISTENCY;", "", "ledger/record disagreement accepted")
mut(Q, "    const cc_ledger_entry entry = {txn_id, n->sched_run_uid, record_id, CC_TXN_REWARD_CLAIM, CC_TXO_CLAIMED};", "    const cc_ledger_entry entry = {txn_id, n->sched_run_uid, record_id, CC_TXN_REWARD_CLAIM, CC_TXO_NONE};", "ledger entry with another outcome")
mut(Q, "    st = cc_ledger_append(&ledger, &entry);\n    if (st != CC_OK) return st;\n", "", "no ledger entry")
mut(Q, "cc_nemesis_transition(&rec, CC_LIFE_REWARD_CLAIMED, 0, 0, txn_id)", "cc_nemesis_transition(&rec, CC_LIFE_REWARD_CLAIMED, 0, 0, txn_id + 1)", "record keeps another transaction")
mut(Q, "    m->counters = counters;\n", "", "transaction counter not advanced")
mut(Q, "    m->ledger = ledger;\n", "", "ledger not stored")
mut(Q, "    m->roster.rec[slot] = rec;\n    res->txn_id = txn_id;", "    res->txn_id = txn_id;", "record not marked claimed")
mut(Q, "    m->roster.rec[slot] = rec;\n    res->txn_id = txn_id;", "    m->roster.rec[slot] = rec;", "claim result without its transaction")
mut(Q, "    memset(res, 0, sizeof *res);\n    cc_status st = cc_meta_validate(m);", "    cc_status st = cc_meta_validate(m);", "claim result not cleared")
# the agreement checks on every entry point
mut(Q, "    if (i < 0) return CC_E_CONSISTENCY;\n    const cc_nemesis *n = &r->rec[i];", "    if (i < 0) return CC_OK;\n    const cc_nemesis *n = &r->rec[i];", "agreement: missing chosen record tolerated")
mut(Q, "return n->life == CC_LIFE_SCHEDULED && n->sched_run_uid == s->run_uid ? CC_OK : CC_E_CONSISTENCY;", "return n->life == CC_LIFE_SCHEDULED ? CC_OK : CC_E_CONSISTENCY;", "agreement: other run's hold tolerated")
mut(Q, "return n->life == CC_LIFE_SCHEDULED && n->sched_run_uid == s->run_uid ? CC_OK : CC_E_CONSISTENCY;", "return CC_OK;", "agreement not checked")
mut(Q, "    if (s->state != CC_SCHED_ACTIVE && s->state != CC_SCHED_OFFERED) return CC_OK;", "    if (s->state != CC_SCHED_ACTIVE) return CC_OK;", "agreement: offered schedule not checked")
mut(Q, "    return check_agreement(s, r);\n}", "    return CC_OK;\n}", "inputs: agreement dropped")
mut(Q, "    st = cc_roster_validate(r);\n    if (st != CC_OK) return st;\n    return check_agreement", "    return check_agreement", "inputs: roster not validated")
mut(Q, "    cc_status st = cc_schedule_validate(s);\n    if (st != CC_OK) return st;\n    st = cc_roster_validate(r);", "    cc_status st = CC_OK;\n    st = cc_roster_validate(r);", "inputs: schedule not validated")


equiv(Q, "if (n->life != CC_LIFE_STORED || cc_nemesis_support(n) != CC_SUPPORT_OK) continue;", "if (n->life != CC_LIFE_STORED) continue;",
      "cc_roster_validate (called first) already rejects any record cc_nemesis_support refuses, so no valid roster holds an unsupported record; the check names the rule")
equiv(Q, "            if (s->windows[i].state == CC_WIN_PENDING) v.n_pending++;", "            if (s->windows[i].state != CC_WIN_SKIPPED) v.n_pending++;",
      "in the ACTIVE state a valid schedule has only SKIPPED and PENDING windows, so the two conditions count the same windows")

equiv(Q, "    if (run_uid == 0) return CC_E_RANGE;\n    cc_status st = cc_roster_validate(r);", "    cc_status st = cc_roster_validate(r);",
      "cc_schedule_validate (no candidate) and cc_nemesis_transition (a candidate) both refuse run uid 0 with the same status, CC_E_RANGE, before anything is stored")
equiv(Q, "    if (n->life != CC_LIFE_DEFEATED) return CC_E_LIFECYCLE;", "    if (n->life != CC_LIFE_DEFEATED && n->life != CC_LIFE_CHALLENGED) return CC_E_LIFECYCLE;",
      "cc_nemesis_transition has no CHALLENGED -> REWARD_CLAIMED edge and refuses with the same status, CC_E_LIFECYCLE, on the staged copy (nothing is committed)")

# ---------------------------------------------------------------- jojo.c and jojo_table.c (the JoJo trainers)
J, JH, JT = "src/jojo.c", "include/cc/jojo.h", "src/jojo_table.c"
mut(JH, "#define CC_JOJO_P_PERCENT 25u", "#define CC_JOJO_P_PERCENT 26u")
mut(JH, "#define CC_JOJO_P_PERCENT 25u", "#define CC_JOJO_P_PERCENT 24u")
mut(J, "0x4A4F4A4F424F5A41ull", "0x4A4F4A4F424F5A40ull", "the salt changes")
mut(J, "    s->rng_state = seed ^ CC_JOJO_SALT;", "    s->rng_state = seed;", "the stream is the plain seed")
mut(J, "    s->used = 0;\n    s->fights = 0;\n}", "    s->fights = 0;\n}", "start leaves used as it was")
mut(J, "if (biome < 0) return CC_JOJO_NONE;", "", "a negative biome deals a Pillar Man")
mut(J, "if (biome < 0) return CC_JOJO_NONE;", "if (biome < 0 || s->fights >= 2) return CC_JOJO_NONE;", "a cap of two creeps back")
mut(J, "if (chars[i].biome == biome && !(s->used >> i & 1u)) n++;", "if (chars[i].biome == biome) n++;", "a dealt character counts as a candidate")
mut(J, "if (cc_rng_v1_pick(&s->rng_state, 100) >= CC_JOJO_P_PERCENT) return CC_JOJO_NONE;", "if (cc_rng_v1_pick(&s->rng_state, 100) > CC_JOJO_P_PERCENT) return CC_JOJO_NONE;", "chance off by one")
mut(J, "    if (!n) return CC_JOJO_NONE;\n", "    if (!n && 0) return CC_JOJO_NONE;\n", "draws with nobody to deal")
mut(J, "unsigned pick = cc_rng_v1_pick(&s->rng_state, n);", "unsigned pick = 0;", "no pick draw")
mut(J, "if (pick-- == 0) {", "if (--pick == 0) {", "the pick is off by one")
mut(J, "            s->used |= 1u << i;\n", "", "the dealt character is not marked")
mut(J, "            s->fights++;\n", "", "the fight is not counted")
mut(J, "if (chars[i].biome != biome || (s->used >> i & 1u)) continue;", "if (chars[i].biome != biome) continue;", "the pick walks over dealt characters")
mut(J, "if (count < 32 && (s->used >> count) != 0) return CC_E_RANGE;", "", "validate: unknown bits")
mut(J, "if (popcount32(s->used) != s->fights) return CC_E_CONSISTENCY;", "if (popcount32(s->used) != s->fights || s->fights > 2) return CC_E_CONSISTENCY;", "validate: a cap of two creeps back")
mut(J, "if (popcount32(s->used) != s->fights) return CC_E_CONSISTENCY;", "", "validate: count versus mask")
mut(J, "for (; v; v &= v - 1) n++;", "for (; v; v >>= 1) n += 0;", "popcount")
mut(J, "if (!s || !chars || count > 32 || cc_jojo_validate_in(s, count) != CC_OK) return CC_JOJO_NONE;", "if (!s || !chars || count > 32) return CC_JOJO_NONE;", "roll: an invalid state is used")
mut(J, "while (*a && *a == *b) { a++; b++; }\n        if (*a == *b) return (int)i;", "while (*a && *a == *b) { a++; b++; }\n        if (!*a) return (int)i;", "find: a longer id matches its prefix")
mut(JT, '{"kira", "Yoshikage Kira", "My name is Yoshikage Kira. I\'m 33 years old.", 9 /* Lake */', '{"kira", "Yoshikage Kira", "My name is Yoshikage Kira. I\'m 33 years old.", 0 /* Town */', "Kira back in Town (no trainers there)")
mut(JT, '"Do you believe in \'gravity\'?", 29 /* Temple */, "speedBoost"', '"Do you believe in \'gravity\'?", 29 /* Temple */, NULL', "Pucci loses the passive")
mut(JT, '"I shall become the ultimate lifeform!", -1,', '"I shall become the ultimate lifeform!", 25,', "Kars in a biome pool")
mut(JT, '"Nyo-ho!", 14 /* Desert */, NULL, 6,', '"Nyo-ho!", 14 /* Desert */, NULL, 5,', "Gyro loses a member")


def build_and_run(workdir: Path):
    """Returns (stage, detail): stage is 'ok' when all tests pass, else where it failed."""
    exes = {"model_test": "tests/model_test.c", "reducer_test": "tests/reducer_test.c", "layout_test": "tests/layout_test.c",
            "codec_test": "tests/codec_test.c", "commit_test": "tests/commit_test.c",
            "scheduler_test": "tests/scheduler_test.c", "jojo_test": "tests/jojo_test.c"}
    objs = []
    for src in SOURCES:
        obj = src.replace("/", "_")[:-2] + ".o"
        try:
            r = subprocess.run(["cc", *CFLAGS, "-Iinclude", "-c", src, "-o", obj], cwd=workdir, capture_output=True,
                               text=True, timeout=BUILD_TIMEOUT)
        except subprocess.TimeoutExpired:
            return "build-error", f"{src}: compiler timed out"
        if r.returncode != 0:
            return "build-error", f"{src}: {r.stderr.strip()[:300]}"
        objs.append(obj)
    for name, src in exes.items():
        cmd = ["cc", *CFLAGS, "-Iinclude", src, *objs, "-o", name]
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
