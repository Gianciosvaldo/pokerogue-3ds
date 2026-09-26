# Host build of the battle core + tests. The 3DS build compiles the same sources
# with devkitARM (see README).
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
CPPFLAGS += -Iinclude -Igen/out
BUILD ?= build
SRCS := src/rng.c src/hooks.c src/pokemon.c src/damage.c src/move_attrs.c src/closures.c src/closures_moves.c src/ab_attrs.c src/closures_abilities.c src/modifiers.c src/tags.c src/history.c src/js_math.c gen/out/pr_data.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/libprcore.a $(BUILD)/rng_cli $(BUILD)/difftest

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/libprcore.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/rng_cli: tests/rng_cli.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/difftest: tests/difftest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

gen: gen/out/pr_data.c
gen/out/pr_data.c: gen/gen_tables.py $(wildcard gen/upstream/*.json)
	python3 gen/gen_tables.py

clean:
	rm -rf $(BUILD)
.PHONY: all clean gen
-include $(OBJS:.o=.d)
