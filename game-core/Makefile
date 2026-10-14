# Host build of game-core + its PC text driver. It links run-core's, ai-core's and battle-core's
# libraries and never changes them; the 3DS build compiles the same sources.
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
BATTLE_CORE ?= ../battle-core
RUN_CORE ?= ../run-core
AI_CORE ?= ../ai-core
REWARD_CORE ?= ../reward-core
CPPFLAGS += -Iinclude -I$(RUN_CORE)/include -I$(AI_CORE)/include -I$(REWARD_CORE)/include -I$(REWARD_CORE)/gen/out -I$(BATTLE_CORE)/include -I$(BATTLE_CORE)/gen/out
BUILD ?= build
RUN_LIB := $(BUILD)/run-core/librun.a
CORE_LIB := $(BUILD)/run-core/battle-core/libprcore.a
AI_LIB := $(BUILD)/ai-core/libai.a
REWARD_LIB := $(BUILD)/reward-core/libreward.a
SRCS := src/gc_custom.c src/gc_gacha.c gen/out/gc_egg_pool.c src/gc_classic.c src/gc_multi.c src/gc_wide.c src/gc_orb_add.c src/gc_soothe.c src/gc_lure.c src/gc_temp.c src/gc_moveset.c src/gc_run.c src/gc_phases.c src/gc_mon.c src/gc_items.c src/gc_encounter.c src/gc_battle.c src/gc_save.c src/gc_stubs.c src/gc_system.c src/gc_mystery.c src/gc_reward.c src/gc_reward_lifecycle.c src/gc_learnset.c src/gc_catch.c gen/out/gc_evolutions.c gen/out/gc_biome_fields.c gen/out/gc_starters.c gen/out/gc_trainers.c gen/out/gc_dex.c gen/out/gc_learnsets.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/transform_host_test
test: transform-host-test
transform-host-test: $(BUILD)/transform_host_test
	$(BUILD)/transform_host_test
$(BUILD)/transform_host_test: tests/transform_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: transform-host-test

test: temp-host-test temp-raw-guards
temp-raw-guards: $(BUILD)/temp_diff_test
	$(BUILD)/temp_diff_test --selftest
temp-host-test: $(BUILD)/temp_caller_test
	$(BUILD)/temp_caller_test --selftest

test: wide-acquire-host wide-raw-guards
wide-acquire-host: $(BUILD)/wide_acquire_host_test
	$(BUILD)/wide_acquire_host_test
wide-raw-guards: $(BUILD)/wide_raw_test
	$(BUILD)/wide_raw_test --selftest
all: $(BUILD)/wide_acquire_host_test
$(BUILD)/wide_acquire_host_test: tests/wide_acquire_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/wide_raw_test
$(BUILD)/wide_raw_test: tests/wide_raw_test.c tests/wide-raw-bridge.inc $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/wide_raw_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

all: $(BUILD)/wide_acquire_diff_test
$(BUILD)/wide_acquire_diff_test: tests/wide_acquire_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/temp_caller_test
$(BUILD)/temp_caller_test: tests/temp_caller_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/temp_diff_test
$(BUILD)/temp_diff_test: tests/temp_diff_test.c tests/temp-replay-bridge.inc $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/temp_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

all: $(BUILD)/multi_diff_test $(BUILD)/multi_host_test
$(BUILD)/multi_diff_test: tests/multi_diff_test.c $(BATTLE_CORE)/tests/multi-lens-replay.inc $(BATTLE_CORE)/tests/multilens-guards.inc $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/multi_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -Wl,--wrap=pr_apply_multi_hit_count_modifiers -Wl,--wrap=pr_apply_multi_hit_damage_modifiers -lm -o $@
$(BUILD)/multi_host_test: tests/multi_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -Wl,--wrap=pr_apply_multi_hit_count_modifiers -lm -o $@
test: multi-host-test
multi-host-test: $(BUILD)/multi_host_test
	$(BUILD)/multi_host_test

all: $(BUILD)/golden_diff_test $(BUILD)/golden_host_test
$(BUILD)/golden_diff_test: tests/golden_diff_test.c $(BATTLE_CORE)/tests/golden-punch-replay.inc $(BATTLE_CORE)/tests/golden-guards.inc $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/golden_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
$(BUILD)/golden_host_test: tests/golden_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
test: golden-host-test
golden-host-test: $(BUILD)/golden_host_test
	$(BUILD)/golden_host_test

all: $(BUILD)/lens_diff_test $(BUILD)/lens_host_test
$(BUILD)/lens_diff_test: tests/lens_diff_test.c $(BATTLE_CORE)/tests/wide-lens-replay.inc $(BATTLE_CORE)/tests/widelens-guards.inc $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/lens_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
$(BUILD)/lens_host_test: tests/lens_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
test: lens-host-test
lens-host-test: $(BUILD)/lens_host_test
	$(BUILD)/lens_host_test

all: $(BUILD)/seed_diff_test $(BUILD)/seed_host_test
$(BUILD)/seed_diff_test: tests/seed_diff_test.c $(BATTLE_CORE)/tests/turntest.c $(BATTLE_CORE)/tests/reviverseed-guards.inc $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/seed_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
$(BUILD)/seed_host_test: tests/seed_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
test: seed-host-test
seed-host-test: $(BUILD)/seed_host_test
	$(BUILD)/seed_host_test

all: $(BUILD)/money_diff_test $(BUILD)/money_host_test
$(BUILD)/money_diff_test: tests/money_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/money_host_test: tests/money_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/payday_diff_test
$(BUILD)/payday_diff_test: tests/payday_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/biome_fields_diff_test
$(BUILD)/biome_fields_diff_test: tests/biome_fields_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/flee_diff_test
$(BUILD)/flee_diff_test: tests/flee_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/mystery_actions_diff_test
$(BUILD)/mystery_actions_diff_test: tests/mystery_actions_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/mystery_trip_diff_test
$(BUILD)/mystery_trip_diff_test: tests/mystery_trip_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/mystery_lost_diff_test
$(BUILD)/mystery_lost_diff_test: tests/mystery_lost_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/mystery_timer_diff_test
$(BUILD)/mystery_timer_diff_test: tests/mystery_timer_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/mystery_offer_diff_test
$(BUILD)/mystery_offer_diff_test: tests/mystery_offer_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/mystery_shady_diff_test
$(BUILD)/mystery_shady_diff_test: tests/mystery_shady_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/item_evolution_test
$(BUILD)/item_evolution_test: tests/item_evolution_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
test: money-host-test
money-host-test: $(BUILD)/money_host_test
	$(BUILD)/money_host_test

# replays the upstream trace of tools/trainervictorydiff.sh; run by hand, it needs the trace
all: $(BUILD)/trainer_victory_diff_test
$(BUILD)/trainer_victory_diff_test: tests/trainer_victory_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

# replays the upstream trace of tools/voucherapplydiff.sh; run by hand, it needs the trace
all: $(BUILD)/voucher_apply_diff_test
$(BUILD)/voucher_apply_diff_test: tests/voucher_apply_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/orb_diff_test $(BUILD)/orb_host_test $(BUILD)/mint_diff_test $(BUILD)/mint_host_test

all: $(BUILD)/vitamin_diff_test $(BUILD)/vitamin_host_test
$(BUILD)/vitamin_diff_test: tests/vitamin_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/vitamin_host_test: tests/vitamin_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
test: vitamin-host-test
vitamin-host-test: $(BUILD)/vitamin_host_test
	$(BUILD)/vitamin_host_test

$(BUILD)/orb_diff_test: tests/orb_diff_test.c $(BATTLE_CORE)/tests/status-orb-replay.inc $(BATTLE_CORE)/tests/statusorb-guards.inc $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/orb_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

$(BUILD)/orb_host_test: tests/orb_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

test: orb-host-test
orb-host-test: $(BUILD)/orb_host_test
	$(BUILD)/orb_host_test

$(BUILD)/mint_diff_test: tests/mint_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/mint_host_test: tests/mint_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

test: mint-host-test
mint-host-test: $(BUILD)/mint_host_test
	$(BUILD)/mint_host_test

all: $(BUILD)/enemy_moveset_test $(BUILD)/reward_lifecycle_host_test $(BUILD)/reward_lifecycle_test $(BUILD)/resume_test $(BUILD)/itembridge_test $(BUILD)/libgc.a $(BUILD)/gcplay $(BUILD)/savetest $(BUILD)/system_save_test $(BUILD)/dextest $(BUILD)/dexupdatetest $(BUILD)/level_evo_test $(BUILD)/biome_test $(BUILD)/voucher_egg_test $(BUILD)/mystery_test $(BUILD)/mystery_diff_test $(BUILD)/reward_test $(BUILD)/reward_diff_test $(BUILD)/switch_prompt_test $(BUILD)/turn_tail_test $(BUILD)/custom_hooks_test $(BUILD)/boss_segments_test $(BUILD)/catch_test $(BUILD)/catch_diff_test $(BUILD)/reward_apply_test $(BUILD)/reward_apply_diff_test $(BUILD)/flee_test $(BUILD)/lapse_test $(BUILD)/starter_unlock_test

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# run-core builds battle-core's library into its own build folder
$(RUN_LIB) $(CORE_LIB): FORCE
	$(MAKE) -s -C $(RUN_CORE) BUILD=$(abspath $(BUILD))/run-core $(abspath $(RUN_LIB)) $(abspath $(CORE_LIB))

# reward-core's library alone (the run and the tables come from run-core's and battle-core's)
$(REWARD_LIB): FORCE
	$(MAKE) -s -C $(REWARD_CORE) BUILD=$(abspath $(BUILD))/reward-core $(abspath $(REWARD_LIB))

# ai-core's library alone (its battle-core copy isn't needed: game-core links run-core's)
$(AI_LIB): FORCE
	$(MAKE) -s -C $(AI_CORE) BUILD=$(abspath $(BUILD))/ai-core $(abspath $(AI_LIB))

$(BUILD)/libgc.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/gcplay: tools/gcplay.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/savetest: tools/savetest.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/system_save_test: tests/system_save_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/dextest: tests/dextest.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/dexupdatetest: tests/dexupdatetest.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/level_evo_test: tests/level_evo_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/biome_test: tests/biome_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/voucher_egg_test: tests/voucher_egg_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/mystery_test: tests/mystery_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/mystery_diff_test: tests/mystery_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/reward_test: tests/reward_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/reward_diff_test: tests/reward_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/switch_prompt_test: tests/switch_prompt_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/turn_tail_test: tests/turn_tail_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/custom_hooks_test: tests/custom_hooks_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/boss_segments_test: tests/boss_segments_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/catch_test: tests/catch_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

# replays the upstream trace of tools/catchdiff.sh; run by hand, it needs the trace
$(BUILD)/catch_diff_test: tests/catch_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/reward_lifecycle_host_test: tests/reward_lifecycle_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/reward_lifecycle_test: tests/reward_lifecycle_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/reward_apply_test: tests/reward_apply_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/reward_apply_diff_test: tests/reward_apply_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/itembridge_test: tests/itembridge_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/flee_test: tests/flee_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/lapse_test: tests/lapse_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/starter_unlock_test: tests/starter_unlock_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/resume_test: tests/resume_test.c tools/gcplay.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/resume_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

$(BUILD)/enemy_moveset_test: tests/enemy_moveset_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

# the save checks, level/evo tests, biome tests, voucher/egg tests, then 200 soak runs twice: with a save/load round trip at every save point, the runs'
# digest must come out the same
test: all
	$(BUILD)/enemy_moveset_test
	$(BUILD)/reward_lifecycle_host_test
	$(BUILD)/resume_test
	$(BUILD)/itembridge_test
	$(BUILD)/savetest
	$(BUILD)/system_save_test
	$(BUILD)/level_evo_test
	$(BUILD)/biome_test
	$(BUILD)/voucher_egg_test
	$(BUILD)/mystery_test
	$(BUILD)/mystery_diff_test
	$(BUILD)/reward_test
	$(BUILD)/switch_prompt_test
	$(BUILD)/turn_tail_test
	$(BUILD)/custom_hooks_test
	$(BUILD)/boss_segments_test
	$(BUILD)/catch_test
	$(BUILD)/flee_test
	$(BUILD)/lapse_test
	$(BUILD)/reward_apply_test
	$(BUILD)/starter_unlock_test
	@a=$$($(BUILD)/gcplay --auto 200 --jobs 8 | grep digest); b=$$($(BUILD)/gcplay --auto 200 --jobs 8 --save-test | grep digest); \
	echo "without saves: $$a"; echo "with saves:    $$b"; [ "$$a" = "$$b" ]

clean:
	rm -rf $(BUILD)

FORCE:
.PHONY: multi-host-test all clean test FORCE mint-host-test orb-host-test vitamin-host-test money-host-test seed-host-test lens-host-test golden-host-test
-include $(OBJS:.o=.d)

# Separate canonical Multi Lens caller fixtures; established checks stay intact.
all: $(BUILD)/multi_acquire_host_test $(BUILD)/multi_acquire_raw_test $(BUILD)/multi_acquire_diff_test
test: multi-acquire-host multi-acquire-raw-guards
multi-acquire-host: $(BUILD)/multi_acquire_host_test
	$(BUILD)/multi_acquire_host_test
multi-acquire-raw-guards: $(BUILD)/multi_acquire_raw_test
	$(BUILD)/multi_acquire_raw_test --selftest
$(BUILD)/multi_acquire_host_test: tests/multi_acquire_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/multi_acquire_diff_test: tests/multi_acquire_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/multi_acquire_raw_test: tests/multi_acquire_raw_test.c tests/multi-acquire-raw-bridge.inc $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/multi_acquire_raw_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

all: $(BUILD)/classic_win_diff_test
$(BUILD)/classic_win_diff_test: tests/classic_win_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/classic_win_host_test
test: classic-win-host
classic-win-host: $(BUILD)/classic_win_host_test
	$(BUILD)/classic_win_host_test
$(BUILD)/classic_win_host_test: tests/classic_win_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/gcstress
$(BUILD)/gcstress: tools/gcstress.c tools/endgame-diagnostic.h $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/gcstress.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

test: endgame-diagnostic-test
endgame-diagnostic-test: $(BUILD)/endgame_diagnostic_test $(BUILD)/gcstress
	$(BUILD)/endgame_diagnostic_test
	python3 tests/endgame_diagnostic_checks.py $(BUILD)/gcstress
$(BUILD)/endgame_diagnostic_test: tests/endgame_diagnostic_test.c tools/gcstress.c tools/endgame-diagnostic.h $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/endgame_diagnostic_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

all: $(BUILD)/egg_gacha_diff_test
$(BUILD)/egg_gacha_diff_test: tests/egg_gacha_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

test: egg-gacha-host
all: $(BUILD)/egg_gacha_host_test
egg-gacha-host: $(BUILD)/egg_gacha_host_test
	$(BUILD)/egg_gacha_host_test
$(BUILD)/egg_gacha_host_test: tests/egg_gacha_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/orb_acquisition_diff_test $(BUILD)/orb_acquisition_host_test
test: orb-acquisition-host
orb-acquisition-host: $(BUILD)/orb_acquisition_host_test
	$(BUILD)/orb_acquisition_host_test
$(BUILD)/orb_acquisition_diff_test: tests/orb_acquisition_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/orb_acquisition_host_test: tests/orb_acquisition_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/soothe_acquisition_diff_test $(BUILD)/soothe_acquisition_host_test
test: soothe-acquisition-host
soothe-acquisition-host: $(BUILD)/soothe_acquisition_host_test
	$(BUILD)/soothe_acquisition_host_test
$(BUILD)/soothe_acquisition_diff_test: tests/soothe_acquisition_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/soothe_acquisition_host_test: tests/soothe_acquisition_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/soothe_battle_test
$(BUILD)/soothe_battle_test: tests/soothe_battle_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/soothe_battle_host_test
$(BUILD)/soothe_battle_host_test: tests/soothe_battle_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
test: soothe-battle-host
soothe-battle-host: $(BUILD)/soothe_battle_host_test
	$(BUILD)/soothe_battle_host_test

all: $(BUILD)/lure_acquisition_test
$(BUILD)/lure_acquisition_test: tests/lure_acquisition_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/lure_acquisition_host_test
$(BUILD)/lure_acquisition_host_test: tests/lure_acquisition_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
test: lure-acquisition-host
lure-acquisition-host: $(BUILD)/lure_acquisition_host_test
	$(BUILD)/lure_acquisition_host_test

all: $(BUILD)/lure_lapse_test
$(BUILD)/lure_lapse_test: tests/lure_lapse_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/lure_lapse_host_test
$(BUILD)/lure_lapse_host_test: tests/lure_lapse_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
test: lure-lapse-host
lure-lapse-host: $(BUILD)/lure_lapse_host_test
	$(BUILD)/lure_lapse_host_test

all: $(BUILD)/lure_encounter_test
$(BUILD)/lure_encounter_test: tests/lure_encounter_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -Wl,--wrap=pr_run_new_battle -Wl,--wrap=pr_rng_seed_int -lm -o $@

all: $(BUILD)/lure_encounter_host_test
$(BUILD)/lure_encounter_host_test: tests/lure_encounter_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -Wl,--wrap=pr_run_new_battle -lm -o $@
test: lure-encounter-host
lure-encounter-host: $(BUILD)/lure_encounter_host_test
	$(BUILD)/lure_encounter_host_test

all: $(BUILD)/berry_history_host_test
test: berry-history-host-test
berry-history-host-test: $(BUILD)/berry_history_host_test
	$(BUILD)/berry_history_host_test
$(BUILD)/berry_history_host_test: tests/berry_history_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: berry-history-host-test

all: $(BUILD)/receiver_save_host_test
test: receiver-save-host-test
receiver-save-host-test: $(BUILD)/receiver_save_host_test
	$(BUILD)/receiver_save_host_test
$(BUILD)/receiver_save_host_test: tests/receiver_save_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: receiver-save-host-test

all: $(BUILD)/ability_lookup_host_test
test: ability-lookup-host-test
ability-lookup-host-test: $(BUILD)/ability_lookup_host_test
	$(BUILD)/ability_lookup_host_test
$(BUILD)/ability_lookup_host_test: tests/ability_lookup_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: ability-lookup-host-test

all: $(BUILD)/form_lifecycle_host_test
test: form-lifecycle-host-test
form-lifecycle-host-test: $(BUILD)/form_lifecycle_host_test
	$(BUILD)/form_lifecycle_host_test
$(BUILD)/form_lifecycle_host_test: tests/form_lifecycle_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: form-lifecycle-host-test

all: $(BUILD)/battle_bond_host_test
test: battle-bond-host-test
battle-bond-host-test: $(BUILD)/battle_bond_host_test
	$(BUILD)/battle_bond_host_test
$(BUILD)/battle_bond_host_test: tests/battle_bond_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: battle-bond-host-test

all: $(BUILD)/copying_host_test
test: copying-host-test
copying-host-test: $(BUILD)/copying_host_test
	$(BUILD)/copying_host_test
$(BUILD)/copying_host_test: tests/copying_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: copying-host-test

all: $(BUILD)/native_form_host_test
test: native-form-host-test
native-form-host-test: $(BUILD)/native_form_host_test
	$(BUILD)/native_form_host_test
$(BUILD)/native_form_host_test: tests/native_form_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: native-form-host-test

all: $(BUILD)/arena_cleanup_host_test
test: arena-cleanup-host-test
arena-cleanup-host-test: $(BUILD)/arena_cleanup_host_test
	$(BUILD)/arena_cleanup_host_test
$(BUILD)/arena_cleanup_host_test: tests/arena_cleanup_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: arena-cleanup-host-test
