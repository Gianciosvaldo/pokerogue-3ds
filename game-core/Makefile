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
SRCS := src/gc_egg_hatch.c src/gc_custom.c src/gc_gacha.c gen/out/gc_egg_pool.c src/gc_classic.c src/gc_multi.c src/gc_wide.c src/gc_orb_add.c src/gc_soothe.c src/gc_lure.c src/gc_temp.c src/gc_moveset.c src/gc_fungames.c src/gc_run.c src/gc_phases.c src/gc_mon.c src/gc_items.c src/gc_encounter.c src/gc_battle.c src/gc_save.c src/gc_stubs.c src/gc_system.c src/gc_mystery.c src/gc_reward.c src/gc_reward_lifecycle.c src/gc_learnset.c src/gc_catch.c gen/out/gc_evolutions.c gen/out/gc_biome_fields.c gen/out/gc_starters.c gen/out/gc_trainers.c gen/out/gc_dex.c gen/out/gc_learnsets.c
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
all: $(BUILD)/mystery_salesman_diff_test
$(BUILD)/mystery_salesman_diff_test: tests/mystery_salesman_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
all: $(BUILD)/mystery_fungames_test
$(BUILD)/mystery_fungames_test: tests/mystery_fungames_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
test: fungames-host-test
fungames-host-test: $(BUILD)/mystery_fungames_test
	$(BUILD)/mystery_fungames_test
.PHONY: fungames-host-test
all: $(BUILD)/mystery_fungames_diff_test
$(BUILD)/mystery_fungames_diff_test: tests/mystery_fungames_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
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

all: $(BUILD)/stance_change_host_test
test: stance-change-host-test
stance-change-host-test: $(BUILD)/stance_change_host_test
	$(BUILD)/stance_change_host_test
$(BUILD)/stance_change_host_test: tests/stance_change_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: stance-change-host-test

all: $(BUILD)/transform_restriction_host_test
test: transform-restriction-host-test
transform-restriction-host-test: $(BUILD)/transform_restriction_host_test
	$(BUILD)/transform_restriction_host_test
$(BUILD)/transform_restriction_host_test: tests/transform_restriction_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: transform-restriction-host-test

all: $(BUILD)/weather_form_host_test
test: weather-form-host-test
weather-form-host-test: $(BUILD)/weather_form_host_test
	$(BUILD)/weather_form_host_test
$(BUILD)/weather_form_host_test: tests/weather_form_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: weather-form-host-test

all: $(BUILD)/pending_heal_host_test
test: pending-heal-host-test
pending-heal-host-test: $(BUILD)/pending_heal_host_test
	$(BUILD)/pending_heal_host_test
$(BUILD)/pending_heal_host_test: tests/pending_heal_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: pending-heal-host-test

all: $(BUILD)/stat_order_host_test
test: stat-order-host-test
stat-order-host-test: $(BUILD)/stat_order_host_test
	$(BUILD)/stat_order_host_test
$(BUILD)/stat_order_host_test: tests/stat_order_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: stat-order-host-test

all: $(BUILD)/fusion_name_host_test
test: fusion-name-host-test
fusion-name-host-test: $(BUILD)/fusion_name_host_test
	$(BUILD)/fusion_name_host_test
$(BUILD)/fusion_name_host_test: tests/fusion_name_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: fusion-name-host-test

all: $(BUILD)/interlude_host_test
test: interlude-host-test
interlude-host-test: $(BUILD)/interlude_host_test
	$(BUILD)/interlude_host_test
$(BUILD)/interlude_host_test: tests/interlude_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: interlude-host-test

all: $(BUILD)/boss_pp_host_test
test: boss-pp-host-test
boss-pp-host-test: $(BUILD)/boss_pp_host_test
	$(BUILD)/boss_pp_host_test
$(BUILD)/boss_pp_host_test: tests/boss_pp_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: boss-pp-host-test

all: $(BUILD)/item_transfer_host_test
test: item-transfer-host-test
item-transfer-host-test: $(BUILD)/item_transfer_host_test
	$(BUILD)/item_transfer_host_test
$(BUILD)/item_transfer_host_test: tests/item_transfer_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: item-transfer-host-test

all: $(BUILD)/helping_hand_host_test
test: helping-hand-host-test
helping-hand-host-test: $(BUILD)/helping_hand_host_test
	$(BUILD)/helping_hand_host_test
$(BUILD)/helping_hand_host_test: tests/helping_hand_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: helping-hand-host-test

all: $(BUILD)/acupressure_host_test
test: acupressure-host-test
acupressure-host-test: $(BUILD)/acupressure_host_test
	$(BUILD)/acupressure_host_test
$(BUILD)/acupressure_host_test: tests/acupressure_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: acupressure-host-test

all: $(BUILD)/suppression_host_test
test: suppression-host-test
suppression-host-test: $(BUILD)/suppression_host_test
	$(BUILD)/suppression_host_test
$(BUILD)/suppression_host_test: tests/suppression_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: suppression-host-test

all: $(BUILD)/wide_level_host_test
test: wide-level-host-test
wide-level-host-test: $(BUILD)/wide_level_host_test
	$(BUILD)/wide_level_host_test
$(BUILD)/wide_level_host_test: tests/wide_level_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: wide-level-host-test

all: $(BUILD)/flame_burst_host_test
test: flame-burst-host-test
flame-burst-host-test: $(BUILD)/flame_burst_host_test
	$(BUILD)/flame_burst_host_test
$(BUILD)/flame_burst_host_test: tests/flame_burst_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: flame-burst-host-test

all: $(BUILD)/partner_cures_host_test
test: partner-cures-host-test
partner-cures-host-test: $(BUILD)/partner_cures_host_test
	$(BUILD)/partner_cures_host_test
$(BUILD)/partner_cures_host_test: tests/partner_cures_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: partner-cures-host-test

all: $(BUILD)/attention_host_test
test: attention-host-test
attention-host-test: $(BUILD)/attention_host_test
	$(BUILD)/attention_host_test
$(BUILD)/attention_host_test: tests/attention_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: attention-host-test

all: $(BUILD)/commander_host_test
test: commander-host-test
commander-host-test: $(BUILD)/commander_host_test
	$(BUILD)/commander_host_test
$(BUILD)/commander_host_test: tests/commander_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: commander-host-test

all: $(BUILD)/wide_progression_diff_test
$(BUILD)/wide_progression_diff_test: tests/wide_progression_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/wide_progression_host_test
test: wide-progression-host-test
wide-progression-host-test: $(BUILD)/wide_progression_host_test
	$(BUILD)/wide_progression_host_test
$(BUILD)/wide_progression_host_test: tests/wide_progression_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: wide-progression-host-test

all: $(BUILD)/move_theft_host_test
test: move-theft-host-test
move-theft-host-test: $(BUILD)/move_theft_host_test
	$(BUILD)/move_theft_host_test
$(BUILD)/move_theft_host_test: tests/move_theft_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: move-theft-host-test

all: $(BUILD)/move_theft_tier_diff_test
$(BUILD)/move_theft_tier_diff_test: tests/move_theft_tier_diff_test.c tests/move_theft_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/move_theft_tier_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

all: $(BUILD)/berry_moves_host_test
test: berry-moves-host-test
berry-moves-host-test: $(BUILD)/berry_moves_host_test
	$(BUILD)/berry_moves_host_test
$(BUILD)/berry_moves_host_test: tests/berry_moves_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: berry-moves-host-test

all: $(BUILD)/wide_exp_phase_diff_test
$(BUILD)/wide_exp_phase_diff_test: tests/wide_exp_phase_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/wide_exp_phase_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

$(BUILD)/wide_exp_phase_diff_test: ../game/source/run_exp.h

test: wide-exp-phase-host-test
wide-exp-phase-host-test: $(BUILD)/wide_exp_phase_diff_test
	$(BUILD)/wide_exp_phase_diff_test --selftest
.PHONY: wide-exp-phase-host-test

all: $(BUILD)/wide_combat_save_host_test
test: wide-combat-save-host-test
wide-combat-save-host-test: $(BUILD)/wide_combat_save_host_test
	$(BUILD)/wide_combat_save_host_test
$(BUILD)/wide_combat_save_host_test: tests/wide_combat_save_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: wide-combat-save-host-test

all: $(BUILD)/zero_to_hero_host_test
test: zero-to-hero-host-test
zero-to-hero-host-test: $(BUILD)/zero_to_hero_host_test
	$(BUILD)/zero_to_hero_host_test
$(BUILD)/zero_to_hero_host_test: tests/zero_to_hero_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: zero-to-hero-host-test

all: $(BUILD)/enemy_zero_to_hero_host_test
test: enemy-zero-to-hero-host-test
enemy-zero-to-hero-host-test: $(BUILD)/enemy_zero_to_hero_host_test
	$(BUILD)/enemy_zero_to_hero_host_test
$(BUILD)/enemy_zero_to_hero_host_test: tests/enemy_zero_to_hero_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: enemy-zero-to-hero-host-test

all: $(BUILD)/illusion_host_test
test: illusion-host-test
illusion-host-test: $(BUILD)/illusion_host_test
	$(BUILD)/illusion_host_test
$(BUILD)/illusion_host_test: tests/illusion_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: illusion-host-test

all: $(BUILD)/aftermath_host_test
test: aftermath-host-test
aftermath-host-test: $(BUILD)/aftermath_host_test
	$(BUILD)/aftermath_host_test
$(BUILD)/aftermath_host_test: tests/aftermath_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: aftermath-host-test

all: $(BUILD)/tera_shift_host_test
test: tera-shift-host-test
tera-shift-host-test: $(BUILD)/tera_shift_host_test
	$(BUILD)/tera_shift_host_test
$(BUILD)/tera_shift_host_test: tests/tera_shift_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: tera-shift-host-test
all: $(BUILD)/meloetta_forms_host_test
test: meloetta-forms-host-test
meloetta-forms-host-test: $(BUILD)/meloetta_forms_host_test
	$(BUILD)/meloetta_forms_host_test
$(BUILD)/meloetta_forms_host_test: tests/meloetta_forms_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: meloetta-forms-host-test

all: $(BUILD)/revival_blessing_host_test
test: revival-blessing-host-test
revival-blessing-host-test: $(BUILD)/revival_blessing_host_test
	$(BUILD)/revival_blessing_host_test
$(BUILD)/revival_blessing_host_test: tests/revival_blessing_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: revival-blessing-host-test

all: $(BUILD)/ball_fetch_host_test
test: ball-fetch-host-test
ball-fetch-host-test: $(BUILD)/ball_fetch_host_test
	$(BUILD)/ball_fetch_host_test
$(BUILD)/ball_fetch_host_test: tests/ball_fetch_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: ball-fetch-host-test

all: $(BUILD)/gulp_missile_host_test
test: gulp-missile-host-test
gulp-missile-host-test: $(BUILD)/gulp_missile_host_test
	$(BUILD)/gulp_missile_host_test
$(BUILD)/gulp_missile_host_test: tests/gulp_missile_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: gulp-missile-host-test

all: $(BUILD)/tera_host_test
test: tera-host-test
tera-host-test: $(BUILD)/tera_host_test
	$(BUILD)/tera_host_test
$(BUILD)/tera_host_test: tests/tera_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: tera-host-test

all: $(BUILD)/manage_items_host_test
test: manage-items-host-test
manage-items-host-test: $(BUILD)/manage_items_host_test
	$(BUILD)/manage_items_host_test
$(BUILD)/manage_items_host_test: tests/manage_items_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: manage-items-host-test
all: $(BUILD)/keldeo_host_test
test: keldeo-host-test
keldeo-host-test: $(BUILD)/keldeo_host_test
	$(BUILD)/keldeo_host_test
$(BUILD)/keldeo_host_test: tests/keldeo_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: keldeo-host-test

all: $(BUILD)/keldeo_learn_test
$(BUILD)/keldeo_learn_test: tests/keldeo_learn_test.c $(BATTLE_CORE)/build/wide-substitute-frame.inc $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) -I$(BATTLE_CORE)/build $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
$(BATTLE_CORE)/build/wide-substitute-frame.inc:
	$(MAKE) -C $(BATTLE_CORE) build/wide-substitute-frame.inc

all: $(BUILD)/double_chance_host_test
$(BUILD)/double_chance_host_test: tests/double_chance_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -Wl,--wrap=pr_run_new_battle -lm -o $@
test: double-chance-host-test
double-chance-host-test: $(BUILD)/double_chance_host_test
	$(BUILD)/double_chance_host_test
.PHONY: double-chance-host-test

all: $(BUILD)/sync_nature_test
$(BUILD)/sync_nature_test: tests/sync_nature_test.c $(BATTLE_CORE)/tests/double-chance-test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(MAKE) -s -C $(BATTLE_CORE) build/wide-substitute-frame.inc
	$(CC) $(CPPFLAGS) -I$(BATTLE_CORE)/build $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

all: $(BUILD)/sync_nature_host_test
$(BUILD)/sync_nature_host_test: tests/sync_nature_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -Wl,--wrap=pr_run_new_battle -Wl,--wrap=gc_mon_new_enemy -lm -o $@
test: sync-nature-host-test
sync-nature-host-test: $(BUILD)/sync_nature_host_test
	$(BUILD)/sync_nature_host_test
.PHONY: sync-nature-host-test

all: $(BUILD)/wide_money_test
$(BUILD)/wide_money_test: tests/wide_money_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(MAKE) -s -C $(BATTLE_CORE) build/wide-substitute-frame.inc
	$(CC) $(CPPFLAGS) -I$(BATTLE_CORE)/build $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

all: $(BUILD)/honey_money_host_test
test: honey-money-host-test
honey-money-host-test: $(BUILD)/honey_money_host_test
	$(BUILD)/honey_money_host_test
$(BUILD)/honey_money_host_test: tests/honey_money_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: honey-money-host-test

all: $(BUILD)/endless_terminal_test
$(BUILD)/endless_terminal_test: tests/endless_terminal_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(MAKE) -s -C $(BATTLE_CORE) build/wide-substitute-frame.inc
	$(CC) $(CPPFLAGS) -I$(BATTLE_CORE)/build $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -Wl,--wrap=pr_prepare_post_battle_money -lm -o $@

all: $(BUILD)/endless_terminal_host_test
test: endless-terminal-host-test
endless-terminal-host-test: $(BUILD)/endless_terminal_host_test
	$(BUILD)/endless_terminal_host_test
$(BUILD)/endless_terminal_host_test: tests/endless_terminal_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: endless-terminal-host-test

all: $(BUILD)/command_stack_host_test
test: command-stack-host-test
command-stack-host-test: $(BUILD)/command_stack_host_test
	$(BUILD)/command_stack_host_test
$(BUILD)/command_stack_host_test: tests/command_stack_host_test.c ../game/include/runtime_limits.h $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -pthread -lm -o $@
.PHONY: command-stack-host-test

$(BUILD)/tm_compat_diff_test: tests/tm_compat_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/grip_claw_host_test
test: grip-claw-host-test
grip-claw-host-test: $(BUILD)/grip_claw_host_test
	$(BUILD)/grip_claw_host_test
$(BUILD)/grip_claw_host_test: tests/grip_claw_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: grip-claw-host-test

all: $(BUILD)/baton_host_test
test: baton-host-test
baton-host-test: $(BUILD)/baton_host_test
	$(BUILD)/baton_host_test
$(BUILD)/baton_host_test: tests/baton_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: baton-host-test

all: $(BUILD)/egg_hatch_diff_test
$(BUILD)/egg_hatch_diff_test: tests/egg_hatch_diff_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/egg_hatch_host_test
test: egg-hatch-host-test
egg-hatch-host-test: $(BUILD)/egg_hatch_host_test
	$(BUILD)/egg_hatch_host_test
$(BUILD)/egg_hatch_host_test: tests/egg_hatch_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: egg-hatch-host-test

test: starter-options-test
starter-options-test: $(BUILD)/starter_options_test
	$(BUILD)/starter_options_test tests/fixtures/starter-options.native.txt
$(BUILD)/starter_options_test: tests/starter_options_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: starter-options-test

test: shared-type-host-test
shared-type-host-test: $(BUILD)/shared_type_host_test
	$(BUILD)/shared_type_host_test
$(BUILD)/shared_type_host_test: tests/shared_type_host_test.c tests/item_transfer_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/shared_type_host_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@
.PHONY: shared-type-host-test

$(BUILD)/species_egg_test: tests/species_egg_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
species-egg-test: $(BUILD)/species_egg_test
	$(BUILD)/species_egg_test tests/fixtures/species-eggs.native.txt
test: species-egg-test
.PHONY: species-egg-test
$(BUILD)/src/gc_gacha.o: gen/out/gc_species_egg_info.inc
