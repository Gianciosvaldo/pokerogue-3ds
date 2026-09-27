# Host build of run-core + its test. It links battle-core's library (the RNG, V8's Math.pow and
# the generated tables) and never changes battle-core; the 3DS build compiles the same sources.
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
BATTLE_CORE ?= ../battle-core
CPPFLAGS += -Iinclude -I$(BATTLE_CORE)/include -I$(BATTLE_CORE)/gen/out
BUILD ?= build
CORE_LIB := $(BUILD)/battle-core/libprcore.a
SRCS := src/wave.c src/shop.c src/biome.c src/evolution.c gen/out/run_biomes.c gen/out/run_evolutions.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/librun.a $(BUILD)/wavetest $(BUILD)/shoptest $(BUILD)/biometest $(BUILD)/encountertest $(BUILD)/newbattletest

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(CORE_LIB): FORCE
	$(MAKE) -s -C $(BATTLE_CORE) BUILD=$(abspath $(BUILD))/battle-core $(abspath $(CORE_LIB))

$(BUILD)/librun.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/wavetest: tests/wavetest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

clean:
	rm -rf $(BUILD)

FORCE:
.PHONY: all clean FORCE
-include $(OBJS:.o=.d)

$(BUILD)/shoptest: tests/shoptest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/biometest: tests/biometest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/encountertest: tests/encountertest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/newbattletest: tests/newbattletest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
