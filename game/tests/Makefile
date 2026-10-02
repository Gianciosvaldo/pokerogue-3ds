# Host build of the game's battle adapter check (no 3DS code). make; build/evtest [locales/en dir] [seed]
CC ?= gcc
CFLAGS ?= -O1 -g -std=gnu11 -Wall -Wno-unused-parameter -ffp-contract=off
BC := ../../battle-core
CPPFLAGS += -I../include -I$(BC)/include -I$(BC)/gen/out
AI := ../../ai-core
CPPFLAGS += -I$(AI)/include -I$(AI)/gen/out
SRCS := evtest.c ../source/battle_setup.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/evtest: $(SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRCS) -lm -o $@
clean:
	rm -rf build
.PHONY: clean

# the battle animation player over every upstream animation file (needs libjansson)
ANIM_SRCS := animtest.c ../source/battle_anim.c ../source/tween.c ../gen/gm_enum_names.c $(BC)/gen/out/pr_data.c
build/animtest: $(ANIM_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) -I../gen -I../../assets-pipeline/app-src/include $(CFLAGS) -fsanitize=address,undefined $(ANIM_SRCS) -ljansson -lm -o $@
animtest: build/animtest
.PHONY: animtest

# unit test for animation details (tone gray and substitute target selection)
DETAILS_SRCS := anim_details_test.c ../source/battle_anim.c ../source/tween.c ../gen/gm_enum_names.c $(BC)/gen/out/pr_data.c
build/anim_details_test: $(DETAILS_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) -I../gen -I../../assets-pipeline/app-src/include $(CFLAGS) -fsanitize=address,undefined $(DETAILS_SRCS) -ljansson -lm -o $@
anim_details_test: build/anim_details_test
.PHONY: anim_details_test

# switching through the stand-in battle (a switch turn, a faint switch)
SW_SRCS := switchtest.c ../source/battle_setup.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/switchtest: $(SW_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SW_SRCS) -lm -o $@

# unit test for stat stage modifier indicators, status visual banners, and text banner queuing
BANNER_SRCS := stat_banner_test.c ../source/battle_setup.c ../source/battle_events.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/stat_banner_test: $(BANNER_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) -I../gen -I../../assets-pipeline/app-src/include $(CFLAGS) $(BANNER_SRCS) -lm -o $@
stat_banner_test: build/stat_banner_test
.PHONY: stat_banner_test

# unit test for map selection UI driven by game-core
GC := ../../game-core
RC := ../../run-core
APP := ../../assets-pipeline/app-src
MAP_CPPFLAGS := $(CPPFLAGS) -I../menus -I$(GC)/include -I$(GC)/src -I$(GC)/gen/out -I$(RC)/include -I$(RC)/gen/out -I$(APP)/include
MAP_SRCS := map_ui_test.c ../menus/biome_map.c ../menus/menus_names.c $(wildcard $(GC)/src/*.c) $(wildcard $(GC)/gen/out/*.c) $(wildcard $(RC)/src/*.c) $(wildcard $(RC)/gen/out/*.c) $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/map_ui_test: $(MAP_SRCS)
	@mkdir -p build
	$(CC) $(MAP_CPPFLAGS) $(CFLAGS) $(MAP_SRCS) -lm -o $@
map_ui_test: build/map_ui_test
.PHONY: map_ui_test

# unit test for DNA Splicers target selection UI
SPLICER_CPPFLAGS := $(CPPFLAGS) -I../menus -I$(APP)/include
SPLICER_SRCS := splicer_ui_test.c ../menus/party.c ../menus/menus_names.c ../menus/menus_data.c $(BC)/gen/out/pr_data.c
build/splicer_ui_test: $(SPLICER_SRCS)
	@mkdir -p build
	$(CC) $(SPLICER_CPPFLAGS) $(CFLAGS) $(SPLICER_SRCS) -lm -o $@
splicer_ui_test: build/splicer_ui_test
.PHONY: splicer_ui_test
