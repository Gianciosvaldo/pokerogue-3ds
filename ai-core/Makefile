# Host build of ai-core + its test. It links battle-core's library (the battle state, the damage
# path, the RNG and the generated tables) and never changes battle-core; the 3DS build compiles the
# same sources.
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
BATTLE_CORE ?= ../battle-core
CPPFLAGS += -Iinclude -I$(BATTLE_CORE)/include -I$(BATTLE_CORE)/gen/out
BUILD ?= build
CORE_LIB := $(BUILD)/battle-core/libprcore.a
SRCS := src/ai.c src/ai_score.c src/ai_damage.c gen/out/ai_data.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/libai.a $(BUILD)/aitest $(BUILD)/illusiontest $(BUILD)/doublestest $(BUILD)/trainerheldtest

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(CORE_LIB): FORCE
	$(MAKE) -s -C $(BATTLE_CORE) BUILD=$(abspath $(BUILD))/battle-core $(abspath $(CORE_LIB))

$(BUILD)/libai.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/aitest: tests/aitest.c $(BUILD)/libai.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/illusiontest: tests/illusiontest.c $(BUILD)/libai.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

# Task 74: the doubles suite's replay includes tests/aitest.c (unchanged) and reuses its reader
$(BUILD)/doublestest: tests/doublestest.c tests/aitest.c $(BUILD)/libai.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/doublestest.c $(BUILD)/libai.a $(CORE_LIB) -lm -o $@

$(BUILD)/berrytest: tests/berrytest.c $(BUILD)/libai.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

test-berry: $(BUILD)/berrytest
	$(BUILD)/berrytest

clean:
	rm -rf $(BUILD)

FORCE:
.PHONY: all clean FORCE test-berry
-include $(OBJS:.o=.d)

$(BUILD)/trainerheldtest: tests/trainerheldtest.c tests/aitest.c $(BUILD)/libai.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/trainerheldtest.c $(BUILD)/libai.a $(CORE_LIB) -lm -o $@
