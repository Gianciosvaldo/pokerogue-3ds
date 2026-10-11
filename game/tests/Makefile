# Host build of the game's battle adapter check (no 3DS code). make; build/evtest [locales/en dir] [seed]
CC ?= gcc
CFLAGS ?= -O1 -g -std=gnu11 -Wall -Wno-unused-parameter -ffp-contract=off
BC := ../../battle-core
CPPFLAGS += -I. -I../include -I$(BC)/include -I$(BC)/gen/out
AI := ../../ai-core
CPPFLAGS += -I$(AI)/include -I$(AI)/gen/out
SRCS := evtest.c battle_setup.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
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
SW_SRCS := switchtest.c battle_setup.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/switchtest: $(SW_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SW_SRCS) -lm -o $@

# unit test for stat stage modifier indicators, status visual banners, and text banner queuing
BANNER_SRCS := stat_banner_test.c battle_setup.c ../source/battle_events.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/stat_banner_test: $(BANNER_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) -I../gen -I../../assets-pipeline/app-src/include $(CFLAGS) $(BANNER_SRCS) -lm -o $@
stat_banner_test: build/stat_banner_test
.PHONY: stat_banner_test

# unit test for Mega/Dynamax/Gigantamax form animation states, event conversion, sprite keys, and scaling
FORM_ANIM_SRCS := form_anim_test.c battle_setup.c ../source/battle_events.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/form_anim_test: $(FORM_ANIM_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) -I../gen -I../../assets-pipeline/app-src/include $(CFLAGS) $(FORM_ANIM_SRCS) -lm -o $@
form_anim_test: build/form_anim_test
.PHONY: form_anim_test

# unit test for map selection UI driven by game-core
GC := ../../game-core
RC := ../../run-core
APP := ../../assets-pipeline/app-src
RW := ../../reward-core
MAP_CPPFLAGS := $(CPPFLAGS) -I../menus -I$(GC)/include -I$(GC)/src -I$(GC)/gen/out -I$(RC)/include -I$(RC)/gen/out -I$(RW)/include -I$(RW)/gen/out -I$(APP)/include
MAP_SRCS := map_ui_test.c ../menus/biome_map.c ../menus/menus_names.c $(wildcard $(GC)/src/*.c) $(wildcard $(GC)/gen/out/*.c) $(wildcard $(RC)/src/*.c) $(wildcard $(RC)/gen/out/*.c) $(wildcard $(RW)/src/*.c) $(RW)/gen/out/reward_data.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/map_ui_test: $(MAP_SRCS)
	@mkdir -p build
	$(CC) $(MAP_CPPFLAGS) $(CFLAGS) $(MAP_SRCS) -lm -o $@
map_ui_test: build/map_ui_test
.PHONY: map_ui_test

# unit test for DNA Splicers target selection UI
SPLICER_CPPFLAGS := $(CPPFLAGS) -I../menus -I$(GC)/include -I$(GC)/gen/out -I$(RC)/include -I$(RC)/gen/out -I$(APP)/include
SPLICER_SRCS := splicer_ui_test.c ../menus/party.c ../menus/menus_names.c ../menus/menus_data.c $(BC)/gen/out/pr_data.c
build/splicer_ui_test: $(SPLICER_SRCS)
	@mkdir -p build
	$(CC) $(SPLICER_CPPFLAGS) $(CFLAGS) $(SPLICER_SRCS) -lm -o $@
splicer_ui_test: build/splicer_ui_test
.PHONY: splicer_ui_test

# unit test for shop UI and Lock Capsule visuals
SHOP_CPPFLAGS := $(CPPFLAGS) -I../menus -I$(GC)/include -I$(GC)/src -I$(GC)/gen/out -I$(RC)/include -I$(RC)/gen/out -I$(RW)/include -I$(RW)/gen/out -I$(APP)/include
SHOP_SRCS := shop_ui_test.c ../menus/shop_ui.c ../menus/menus_names.c ../menus/menus_data.c $(wildcard $(GC)/src/*.c) $(wildcard $(GC)/gen/out/*.c) $(wildcard $(RC)/src/*.c) $(wildcard $(RC)/gen/out/*.c) $(wildcard $(RW)/src/*.c) $(RW)/gen/out/reward_data.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/shop_ui_test: $(SHOP_SRCS)
	@mkdir -p build
	$(CC) $(SHOP_CPPFLAGS) $(CFLAGS) $(SHOP_SRCS) -lm -o $@
shop_ui_test: build/shop_ui_test
.PHONY: shop_ui_test

# unit test for summary UI, shiny tier parsing and passive ability slot
SUMMARY_CPPFLAGS := $(CPPFLAGS) -I../menus -I$(GC)/include -I$(GC)/src -I$(GC)/gen/out -I$(RC)/include -I$(RC)/gen/out -I$(APP)/include
SUMMARY_SRCS := summary_ui_test.c ../menus/summary.c ../menus/menus_names.c ../menus/menus_data.c $(BC)/gen/out/pr_data.c
build/summary_ui_test: $(SUMMARY_SRCS)
	@mkdir -p build
	$(CC) $(SUMMARY_CPPFLAGS) $(CFLAGS) $(SUMMARY_SRCS) -lm -o $@
summary_ui_test: build/summary_ui_test
.PHONY: summary_ui_test

# the app's run driver (run_screen.c + battle_events.c) over the real game-core run, with a fake battle screen and menus
# that check every screen event (the full-party release list is the real menus/party.c, through party_fake.c);
# make runtest && build/runtest [runs] [first]
RW := ../../reward-core
RUN_CPPFLAGS := $(CPPFLAGS) -I../menus -I../gen -I$(GC)/include -I$(GC)/gen/out -I$(RC)/include -I$(RC)/gen/out -I$(RW)/include -I$(RW)/gen/out -I$(APP)/include
RUN_SRCS := runtest.c party_fake.c ../menus/party.c ../menus/menus_names.c ../menus/menus_data.c ../source/starter_data.c ../source/run_screen.c ../source/battle_events.c ../source/run_saves.c $(APP)/source/save.c ../gen/gm_enum_names.c $(wildcard $(GC)/src/*.c) $(wildcard $(GC)/gen/out/*.c) $(wildcard $(RC)/src/*.c) $(wildcard $(RC)/gen/out/*.c) $(wildcard $(RW)/src/*.c) $(RW)/gen/out/reward_data.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/runtest: $(RUN_SRCS)
	@mkdir -p build
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) $(RUN_SRCS) -lm -o $@
runtest: build/runtest
.PHONY: runtest

# the run's save slots (run_saves.c + the app's save.c) over real game-core runs; make saves_test && build/saves_test
SAVES_SRCS := saves_test.c ../source/run_saves.c $(APP)/source/save.c $(wildcard $(GC)/src/*.c) $(wildcard $(GC)/gen/out/*.c) $(wildcard $(RC)/src/*.c) $(wildcard $(RC)/gen/out/*.c) $(wildcard $(RW)/src/*.c) $(RW)/gen/out/reward_data.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/saves_test: $(SAVES_SRCS)
	@mkdir -p build
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) $(SAVES_SRCS) -lm -o $@
saves_test: build/saves_test
.PHONY: saves_test

# the run driver with saving: every save point is written and the slot loads back as the run; with `resume` the run is also dropped and
# continued from its slot at every 4th save point, and the same seed must end the same way (wave and score) with and without it
runtest-save: build/runtest
	RUNTEST_SAVE=1 build/runtest 100 0
	RUNTEST_SAVE=resume build/runtest 100 0
	@for n in 0 1 2 3 4 5 6 7 8 9; do a=$$(RUNTEST_SAVE=1 build/runtest 1 $$n | grep -o 'digest [0-9a-f]*'); b=$$(RUNTEST_SAVE=resume build/runtest 1 $$n | grep -o 'digest [0-9a-f]*'); if [ "$$a" != "$$b" ] || [ -z "$$a" ]; then echo "seed $$n: the resumed run ended differently ($$a / $$b)"; exit 1; fi; done; echo "10 seeds: a run resumed at every 4th save point ends at the same wave and score: OK"
.PHONY: runtest-save

# the app's screen flow (app_flow.c) with fake screens and a fake run driver, over the real save slots; make flowtest && build/flowtest
FLOW_SRCS := flowtest.c ../source/app_flow.c ../source/run_saves.c ../source/system_data.c ../source/starter_data.c ../menus/menus_data.c $(APP)/source/save.c $(wildcard $(GC)/src/*.c) $(wildcard $(GC)/gen/out/*.c) $(wildcard $(RC)/src/*.c) $(wildcard $(RC)/gen/out/*.c) $(wildcard $(RW)/src/*.c) $(RW)/gen/out/reward_data.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/flowtest: $(FLOW_SRCS)
	@mkdir -p build
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) $(FLOW_SRCS) -lm -o $@
flowtest: build/flowtest
.PHONY: flowtest

# a run save one turn away from game over or from clearing the run, for looking at those screens on a real build (see mkfixture.c);
# make mkfixture && (cd /some/folder && /path/to/build/mkfixture clear)
build/mkfixture: mkfixture.c ../source/run_saves.c $(APP)/source/save.c $(wildcard $(GC)/src/*.c) $(wildcard $(GC)/gen/out/*.c) $(wildcard $(RC)/src/*.c) $(wildcard $(RC)/gen/out/*.c) $(wildcard $(RW)/src/*.c) $(RW)/gen/out/reward_data.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
	@mkdir -p build
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) $^ -lm -o $@
mkfixture: build/mkfixture
.PHONY: mkfixture

# the starter select screen's input rules (menus/starter_model.c), button by button; no game-core in it
build/startertest: startertest.c ../menus/starter_model.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) -I../menus -I$(APP)/include $(CFLAGS) -fsanitize=address,undefined $^ -lm -o $@
# the same rules built with upstream's 9 x 9 grid (-DSM_UPSTREAM_GRID): the checks taken from upstream's numbers
build/startertest_upstream: startertest.c ../menus/starter_model.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) -DSM_UPSTREAM_GRID -I../menus -I$(APP)/include $(CFLAGS) -fsanitize=address,undefined $^ -lm -o $@
startertest: build/startertest build/startertest_upstream
.PHONY: startertest

# the starter select's data (starter_data.c) and the system data on the SD card (system_data.c) over real game-core and the real
# save.c; make starter_data_test && build/starter_data_test [random systems]
CORE_SRCS := $(wildcard $(GC)/src/*.c) $(wildcard $(GC)/gen/out/*.c) $(wildcard $(RC)/src/*.c) $(wildcard $(RC)/gen/out/*.c) $(wildcard $(RW)/src/*.c) $(RW)/gen/out/reward_data.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
SDATA_SRCS := starter_data_test.c ../source/starter_data.c ../source/system_data.c ../menus/menus_data.c $(APP)/source/save.c $(CORE_SRCS)
build/starter_data_test: $(SDATA_SRCS)
	@mkdir -p build
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined $(SDATA_SRCS) -lm -o $@
starter_data_test: build/starter_data_test
.PHONY: starter_data_test

# the starter select screen itself (menus/starter.c, the host and the data) with a fake drawing layer, over real game-core;
# make starter_ui_test && build/starter_ui_test [fuzz sessions]
SUI_SRCS := starter_ui_test.c ../menus/starter.c ../menus/starter_model.c ../menus/menus_data.c ../menus/menus_names.c ../source/starter_host.c ../source/starter_data.c $(CORE_SRCS)
build/starter_ui_test: $(SUI_SRCS)
	@mkdir -p build
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined $(SUI_SRCS) -lm -o $@
starter_ui_test: build/starter_ui_test
.PHONY: starter_ui_test
