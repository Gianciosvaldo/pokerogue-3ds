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

all: $(BUILD)/libai.a $(BUILD)/aitest

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(CORE_LIB): FORCE
	$(MAKE) -s -C $(BATTLE_CORE) BUILD=$(abspath $(BUILD))/battle-core $(abspath $(CORE_LIB))

$(BUILD)/libai.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/aitest: tests/aitest.c $(BUILD)/libai.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

clean:
	rm -rf $(BUILD)

FORCE:
.PHONY: all clean FORCE
-include $(OBJS:.o=.d)
