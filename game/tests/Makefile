# Host build of the game's battle adapter check (no 3DS code). make; build/evtest [locales/en dir] [seed]
CC ?= gcc
CFLAGS ?= -O1 -g -std=gnu11 -Wall -Wno-unused-parameter -ffp-contract=off
BC := ../../battle-core
CPPFLAGS := -I../include -I$(BC)/include -I$(BC)/gen/out
SRCS := evtest.c ../source/battle_setup.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c
build/evtest: $(SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRCS) -lm -o $@
clean:
	rm -rf build
.PHONY: clean
