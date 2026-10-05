# Host build of reward-core + its test. It links run-core's and battle-core's libraries (the run, the RNG and
# the generated tables) and never changes either; the 3DS build compiles the same sources.
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
BATTLE_CORE ?= ../battle-core
RUN_CORE ?= ../run-core
CPPFLAGS += -Iinclude -Igen/out -I$(RUN_CORE)/include -I$(BATTLE_CORE)/include -I$(BATTLE_CORE)/gen/out
BUILD ?= build
CORE_LIB := $(BUILD)/battle-core/libprcore.a
RUN_LIB := $(BUILD)/run-core/librun.a
SRCS := src/reward.c src/pools.c src/victory.c src/trainer_rewards.c src/reward_apply.c src/memory.c src/candy.c gen/out/reward_data.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/libreward.a $(BUILD)/rewardstest $(BUILD)/victorytest $(BUILD)/applytest $(BUILD)/memorytest $(BUILD)/candytest $(BUILD)/evolutionstatetest $(BUILD)/learnsetinputtest $(BUILD)/trainerrewardstest

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(CORE_LIB): FORCE
	$(MAKE) -s -C $(BATTLE_CORE) BUILD=$(abspath $(BUILD))/battle-core $(abspath $(CORE_LIB))

$(RUN_LIB): FORCE
	$(MAKE) -s -C $(RUN_CORE) BUILD=$(abspath $(BUILD))/run-core $(abspath $(RUN_LIB))

$(BUILD)/libreward.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/rewardstest: tests/rewardstest.c $(BUILD)/libreward.a $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/victorytest: tests/victorytest.c $(BUILD)/libreward.a $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/applytest: tests/applytest.c $(BUILD)/libreward.a $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/memorytest: tests/memorytest.c $(BUILD)/libreward.a $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/candytest: tests/candytest.c $(BUILD)/libreward.a $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/evolutionstatetest: tests/evolutionstatetest.c $(BUILD)/libreward.a $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/learnsetinputtest: tests/learnsetinputtest.c $(BUILD)/libreward.a $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/trainerrewardstest: tests/trainerrewardstest.c $(BUILD)/libreward.a $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

clean:
	rm -rf $(BUILD)

FORCE:
.PHONY: all clean FORCE
-include $(OBJS:.o=.d)
