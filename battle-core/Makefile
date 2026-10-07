# Host build of the battle core + tests. The 3DS build compiles the same sources
# with devkitARM (see README).
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
CPPFLAGS += -Iinclude -Igen/out
BUILD ?= build
SRCS := src/rng.c src/hooks.c src/pokemon.c src/damage.c src/move_attrs.c src/closures.c src/closures_moves.c src/closures_conditions.c src/ab_attrs.c src/closures_abilities.c src/modifiers.c src/tags.c src/battler_tags.c src/arena_tags.c src/history.c src/engine.c src/phases.c src/move_effects.c src/called_moves.c src/switching.c src/commands.c src/js_math.c src/text.c src/events.c gen/out/pr_data.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/statusorbtest $(BUILD)/dancertest $(BUILD)/doublestest $(BUILD)/itemremovaltest $(BUILD)/berrytest $(BUILD)/weatherformtest $(BUILD)/illusiontest $(BUILD)/libprcore.a $(BUILD)/rng_cli $(BUILD)/difftest $(BUILD)/turntest $(BUILD)/bosssegmenttest

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/libprcore.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/rng_cli: tests/rng_cli.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/difftest: tests/difftest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/turntest: tests/turntest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/bosssegmenttest: tests/bosssegmenttest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

gen: gen/out/pr_data.c
gen/out/pr_data.c: gen/gen_tables.py $(wildcard gen/upstream/*.json)
	python3 gen/gen_tables.py

clean:
	rm -rf $(BUILD)
.PHONY: all clean gen
-include $(OBJS:.o=.d)

$(BUILD)/illusiontest: tests/illusiontest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@


$(BUILD)/weatherformtest: tests/weatherformtest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/itemremovaltest: tests/itemremovaltest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/berrytest: tests/berrytest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/dancertest: tests/dancertest.c tests/turntest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/dancertest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/doublestest: tests/doublestest.c tests/turntest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/doublestest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/statusorbtest: tests/statusorbtest.c tests/status-orb-replay.inc tests/statusorb-guards.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/statusorbtest.c $(BUILD)/libprcore.a -lm -o $@
