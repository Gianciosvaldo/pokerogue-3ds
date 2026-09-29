# Host build of game-core + its PC text driver. It links run-core's and battle-core's libraries and
# never changes them; the 3DS build compiles the same sources.
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
BATTLE_CORE ?= ../battle-core
RUN_CORE ?= ../run-core
CPPFLAGS += -Iinclude -I$(RUN_CORE)/include -I$(BATTLE_CORE)/include -I$(BATTLE_CORE)/gen/out
BUILD ?= build
RUN_LIB := $(BUILD)/run-core/librun.a
CORE_LIB := $(BUILD)/run-core/battle-core/libprcore.a
SRCS := src/run.c src/phases.c src/mon.c src/items.c src/encounter.c src/battle.c src/stubs.c gen/out/gc_evolutions.c gen/out/gc_starters.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/libgc.a $(BUILD)/gcplay

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# run-core builds battle-core's library into its own build folder
$(RUN_LIB) $(CORE_LIB): FORCE
	$(MAKE) -s -C $(RUN_CORE) BUILD=$(abspath $(BUILD))/run-core $(abspath $(RUN_LIB)) $(abspath $(CORE_LIB))

$(BUILD)/libgc.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/gcplay: tools/gcplay.c $(BUILD)/libgc.a $(RUN_LIB) $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

clean:
	rm -rf $(BUILD)

FORCE:
.PHONY: all clean FORCE
-include $(OBJS:.o=.d)
