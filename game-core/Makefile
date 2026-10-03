# Host build of game-core + its PC text driver. It links run-core's, ai-core's and battle-core's
# libraries and never changes them; the 3DS build compiles the same sources.
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
BATTLE_CORE ?= ../battle-core
RUN_CORE ?= ../run-core
AI_CORE ?= ../ai-core
CPPFLAGS += -Iinclude -I$(RUN_CORE)/include -I$(AI_CORE)/include -I$(BATTLE_CORE)/include -I$(BATTLE_CORE)/gen/out
BUILD ?= build
RUN_LIB := $(BUILD)/run-core/librun.a
CORE_LIB := $(BUILD)/run-core/battle-core/libprcore.a
AI_LIB := $(BUILD)/ai-core/libai.a
SRCS := src/gc_run.c src/gc_phases.c src/gc_mon.c src/gc_items.c src/gc_encounter.c src/gc_battle.c src/gc_save.c src/gc_stubs.c src/gc_system.c src/gc_mystery.c gen/out/gc_evolutions.c gen/out/gc_starters.c gen/out/gc_trainers.c gen/out/gc_dex.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/libgc.a $(BUILD)/gcplay $(BUILD)/savetest $(BUILD)/dextest $(BUILD)/dexupdatetest $(BUILD)/level_evo_test $(BUILD)/biome_test $(BUILD)/voucher_egg_test $(BUILD)/mystery_test $(BUILD)/mystery_diff_test

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# run-core builds battle-core's library into its own build folder
$(RUN_LIB) $(CORE_LIB): FORCE
	$(MAKE) -s -C $(RUN_CORE) BUILD=$(abspath $(BUILD))/run-core $(abspath $(RUN_LIB)) $(abspath $(CORE_LIB))

# ai-core's library alone (its battle-core copy isn't needed: game-core links run-core's)
$(AI_LIB): FORCE
	$(MAKE) -s -C $(AI_CORE) BUILD=$(abspath $(BUILD))/ai-core $(abspath $(AI_LIB))

$(BUILD)/libgc.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/gcplay: tools/gcplay.c $(BUILD)/libgc.a $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/savetest: tools/savetest.c $(BUILD)/libgc.a $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/dextest: tests/dextest.c $(BUILD)/libgc.a $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/dexupdatetest: tests/dexupdatetest.c $(BUILD)/libgc.a $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/level_evo_test: tests/level_evo_test.c $(BUILD)/libgc.a $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/biome_test: tests/biome_test.c $(BUILD)/libgc.a $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/voucher_egg_test: tests/voucher_egg_test.c $(BUILD)/libgc.a $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/mystery_test: tests/mystery_test.c $(BUILD)/libgc.a $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/mystery_diff_test: tests/mystery_diff_test.c $(BUILD)/libgc.a $(AI_LIB) $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

# the save checks, level/evo tests, biome tests, voucher/egg tests, then 200 soak runs twice: with a save/load round trip at every save point, the runs'
# digest must come out the same
test: all
	$(BUILD)/savetest
	$(BUILD)/level_evo_test
	$(BUILD)/biome_test
	$(BUILD)/voucher_egg_test
	$(BUILD)/mystery_test
	@a=$($(BUILD)/gcplay --auto 200 --jobs 8 | grep digest); b=$($(BUILD)/gcplay --auto 200 --jobs 8 --save-test | grep digest); \
	echo "without saves: $a"; echo "with saves:    $b"; [ "$a" = "$b" ]

# Upstream-dependent Task 46 differential check. Generate /tmp/mystery_actions.txt
# (or gen/out/mystery_actions.txt) from pinned upstream 8555c08 before running this target.
mystery-diff: all
	$(BUILD)/mystery_diff_test

clean:
	rm -rf $(BUILD)

FORCE:
.PHONY: all clean test mystery-diff FORCE
-include $(OBJS:.o=.d)
