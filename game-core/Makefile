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
SRCS := src/gc_run.c src/gc_phases.c src/gc_mon.c src/gc_items.c src/gc_encounter.c src/gc_battle.c src/gc_save.c src/gc_stubs.c src/gc_system.c src/gc_mystery.c src/gc_reward.c src/gc_reward_lifecycle.c src/gc_learnset.c gen/out/gc_evolutions.c gen/out/gc_starters.c gen/out/gc_trainers.c gen/out/gc_dex.c gen/out/gc_learnsets.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/reward_lifecycle_host_test $(BUILD)/reward_lifecycle_test $(BUILD)/resume_test $(BUILD)/itembridge_test $(BUILD)/libgc.a $(BUILD)/gcplay $(BUILD)/savetest $(BUILD)/system_save_test $(BUILD)/dextest $(BUILD)/dexupdatetest $(BUILD)/level_evo_test $(BUILD)/biome_test $(BUILD)/voucher_egg_test $(BUILD)/mystery_test $(BUILD)/mystery_diff_test $(BUILD)/reward_test $(BUILD)/reward_diff_test $(BUILD)/switch_prompt_test $(BUILD)/reward_apply_test $(BUILD)/reward_apply_diff_test

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

$(BUILD)/resume_test: tests/resume_test.c tools/gcplay.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/resume_test.c $(BUILD)/libgc.a $(REWARD_LIB) $(AI_LIB) $(RUN_LIB) $(CORE_LIB) -lm -o $@

# the save checks, level/evo tests, biome tests, voucher/egg tests, then 200 soak runs twice: with a save/load round trip at every save point, the runs'
# digest must come out the same
test: all
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
	$(BUILD)/reward_apply_test
	@a=$$($(BUILD)/gcplay --auto 200 --jobs 8 | grep digest); b=$$($(BUILD)/gcplay --auto 200 --jobs 8 --save-test | grep digest); \
	echo "without saves: $$a"; echo "with saves:    $$b"; [ "$$a" = "$$b" ]

clean:
	rm -rf $(BUILD)

FORCE:
.PHONY: all clean test FORCE
-include $(OBJS:.o=.d)
