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
TRANSFORM_VISUAL_SRCS := transform_visual_test.c battle_setup.c ../source/battle_events.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
build/transform_visual_test: $(TRANSFORM_VISUAL_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) -I../gen -I../../assets-pipeline/app-src/include $(CFLAGS) $(TRANSFORM_VISUAL_SRCS) -lm -o $@
transform_visual_test: build/transform_visual_test
.PHONY: transform_visual_test
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
CCORE := ../../custom-core
OBS_SRCS := ../source/combat_observer.c ../source/custom_core_build.c
RUN_CPPFLAGS := $(CPPFLAGS) -I$(CCORE)/include -I../menus -I../gen -I$(GC)/include -I$(GC)/gen/out -I$(RC)/include -I$(RC)/gen/out -I$(RW)/include -I$(RW)/gen/out -I$(APP)/include
RUN_SRCS := runtest.c snd_fake.c ../source/bgm.c ../source/bgm_choose.c ../source/bgm_player.c ../gen/gm_bgm_data.c party_fake.c ../menus/party.c ../menus/menus_names.c ../menus/menus_data.c ../source/starter_data.c ../source/run_screen.c ../source/battle_events.c ../source/run_saves.c ../source/custom_run.c ../source/custom_store.c ../source/profile_tx.c ../source/system_data.c $(OBS_SRCS) $(APP)/source/save.c ../gen/gm_enum_names.c $(wildcard $(GC)/src/*.c) $(wildcard $(GC)/gen/out/*.c) $(wildcard $(RC)/src/*.c) $(wildcard $(RC)/gen/out/*.c) $(wildcard $(RW)/src/*.c) $(RW)/gen/out/reward_data.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
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
FLOW_SRCS := flowtest.c ../source/app_flow.c ../source/custom_run.c ../source/profile_tx.c ../source/custom_store.c $(OBS_SRCS) ../source/run_saves.c ../source/system_data.c ../source/starter_data.c ../menus/menus_data.c $(APP)/source/save.c $(wildcard $(GC)/src/*.c) $(wildcard $(GC)/gen/out/*.c) $(wildcard $(RC)/src/*.c) $(wildcard $(RC)/gen/out/*.c) $(wildcard $(RW)/src/*.c) $(RW)/gen/out/reward_data.c $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c $(wildcard $(AI)/src/*.c $(AI)/gen/out/*.c)
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

# the combat observer (source/combat_observer.c over custom-core's reducer) on literal event fixtures; make observer_test && build/observer_test
OBSERVER_CPPFLAGS := $(CPPFLAGS) -I$(CCORE)/include -I$(GC)/include -I$(RC)/include
build/observer_test: observer_test.c $(OBS_SRCS)
	@mkdir -p build
	$(CC) $(OBSERVER_CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined observer_test.c $(OBS_SRCS) -o $@
observer_test: build/observer_test
.PHONY: observer_test

# the post-mortem screen and the result screen's way to it, over the real observer and reducer, with a fake drawing layer
PM_CPPFLAGS := $(CPPFLAGS) -I$(CCORE)/include -I$(GC)/include -I$(RC)/include -I../gen -I$(APP)/include
PM_SRCS := postmortem_test.c ../source/postmortem_screen.c ../source/result_screen.c ../gen/gm_enum_names.c $(OBS_SRCS)
build/postmortem_test: $(PM_SRCS)
	@mkdir -p build
	$(CC) $(PM_CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined $(PM_SRCS) -o $@
postmortem_test: build/postmortem_test
.PHONY: postmortem_test

# the Nemesis Radar (source/radar_screen.c) over custom-core's real validators, with a fake drawing layer; make radar_test && build/radar_test [fuzz sessions]
RADAR_CPPFLAGS := $(CPPFLAGS) -I$(CCORE)/include -I$(CCORE)/tests -I../gen -I$(APP)/include
RADAR_SRCS := radar_test.c ../source/radar_screen.c ../source/custom_core_build.c ../gen/gm_enum_names.c
build/radar_test: $(RADAR_SRCS)
	@mkdir -p build
	$(CC) $(RADAR_CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined $(RADAR_SRCS) -o $@
radar_test: build/radar_test
.PHONY: radar_test

# the capsule-machine presentation (source/capsule_model.c + capsule_screen.c) with a fake drawing layer; make capsule_test && build/capsule_test [fuzz sessions]
CAPSULE_SRCS := capsule_test.c ../source/capsule_model.c ../source/capsule_screen.c
build/capsule_test: $(CAPSULE_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) -I$(APP)/include $(CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined $(CAPSULE_SRCS) -lm -o $@
capsule_test: build/capsule_test
.PHONY: capsule_test

# a custom.sav with remembered enemies, for looking at the Radar on a real build (see mkcustom.c); make mkcustom && (cd /some/folder && /path/to/build/mkcustom few)
build/mkcustom: mkcustom.c ../source/custom_core_build.c $(APP)/source/save.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) -I$(CCORE)/include -I$(APP)/include $(CFLAGS) mkcustom.c ../source/custom_core_build.c $(APP)/source/save.c -o $@
mkcustom: build/mkcustom
.PHONY: mkcustom

# a system.sav with chosen vouchers and eggs, for looking at the Capsule Machine on a real build (see mksystem.c); make mksystem && (cd /some/folder && /path/to/build/mksystem 12 3 2 1)
build/mksystem: mksystem.c $(APP)/source/save.c build/ptx_core.a
	@mkdir -p build
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) mksystem.c $(APP)/source/save.c build/ptx_core.a -lm -o $@
mksystem: build/mksystem
.PHONY: mksystem

# the music: the chooser (source/bgm_choose.c) against upstream's own Battle.getBgmOverride (bgm_expected.c, dumped by tools/dump-bgm.test.ts:
# every wave, every species and form, every trainer), then the player (bgm_player.c) and the scene calls (bgm.c) over a fake DSP (snd_fake.c);
# make bgmtest && build/bgmtest [BGM_TRACKS=<one track name per line>]
BGM_SRCS := bgmtest.c snd_fake.c bgm_expected.c ../source/bgm_choose.c ../source/bgm.c ../source/bgm_player.c ../gen/gm_bgm_data.c ../gen/gm_enum_names.c $(wildcard $(RC)/src/*.c) $(wildcard $(RC)/gen/out/*.c) $(wildcard $(BC)/src/*.c) $(BC)/gen/out/pr_data.c
build/bgmtest: $(BGM_SRCS)
	@mkdir -p build
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined $(BGM_SRCS) -lm -o $@
bgmtest: build/bgmtest
.PHONY: bgmtest

# the profile transaction owner (profile_tx.c over system_data.c and custom_store.c, real game-core gacha and the real save.c) under fault
# injection; make profile_tx_test && build/profile_tx_test [fuzz scenarios]
PTX_SRCS := profile_tx_test.c ../source/profile_tx.c ../source/system_data.c ../source/custom_store.c ../source/custom_core_build.c $(APP)/source/save.c $(CORE_SRCS)
build/profile_tx_test: $(PTX_SRCS)
	@mkdir -p build
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined $(PTX_SRCS) -lm -o $@
profile_tx_test: build/profile_tx_test
.PHONY: profile_tx_test

# the same test with game-core compiled once into an archive: only the files under test are rebuilt (profile_tx_mutations.py uses it)
PTX_OWN := profile_tx_test.c ../source/profile_tx.c ../source/system_data.c ../source/custom_store.c ../source/custom_core_build.c $(APP)/source/save.c
build/ptx_core.a: $(CORE_SRCS)
	@mkdir -p build/ptxobj
	@for f in $(CORE_SRCS); do o=build/ptxobj/$$(echo $$f | tr '/.' '__').o; $(CC) $(RUN_CPPFLAGS) $(CFLAGS) -c $$f -o $$o || exit 1; done
	ar rcs $@ build/ptxobj/*.o
build/profile_tx_test_fast: $(PTX_OWN) build/ptx_core.a
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined $(PTX_OWN) build/ptx_core.a -lm -o $@
profile_tx_test_fast: build/profile_tx_test_fast
.PHONY: profile_tx_test_fast

# the capsule machine (task C09). gacha_host_test: the purchase logic (gacha_host.c over profile_tx.c, system_data.c and the real save.c, real
# game-core gacha) under fault injection; gacha_screen_test: the menu (gacha_screen.c) driven by buttons over the real capsule screen, with a fake
# drawing layer. Both link the game-core archive built once for profile_tx_test_fast (only the files under test are rebuilt; the test files are
# sanitized): make gacha_host_test gacha_screen_test && build/gacha_host_test [states] && build/gacha_screen_test [fuzz steps]
GH_OWN := gacha_host_test.c ../source/gacha_host.c ../source/capsule_model.c ../source/profile_tx.c ../source/system_data.c ../source/custom_store.c ../source/custom_core_build.c $(APP)/source/save.c
GS_OWN := gacha_screen_test.c ../source/gacha_screen.c ../source/capsule_screen.c $(wordlist 2,99,$(GH_OWN))
build/gacha_host_test: $(GH_OWN) build/ptx_core.a gacha_test_common.h
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined $(GH_OWN) build/ptx_core.a -lm -o $@
gacha_host_test: build/gacha_host_test
build/gacha_screen_test: $(GS_OWN) build/ptx_core.a gacha_test_common.h
	$(CC) $(RUN_CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined $(GS_OWN) build/ptx_core.a -lm -o $@
gacha_screen_test: build/gacha_screen_test
.PHONY: gacha_host_test gacha_screen_test

# the optional Nemesis rematch of a custom run (task C10): custom_run.c over the real game-core run (through ptx_core.a), the real store,
# profile_tx, run_saves and observer, with a mini driver that mirrors run_screen.c; make custom_run_test && build/custom_run_test [seeds]
CR_OWN := custom_run_test.c ../source/custom_run.c ../source/custom_store.c ../source/profile_tx.c ../source/system_data.c ../source/run_saves.c ../source/combat_observer.c ../source/custom_core_build.c $(APP)/source/save.c
build/custom_run_test: $(CR_OWN) build/ptx_core.a
	$(CC) $(RUN_CPPFLAGS) -I$(GC)/src $(CFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined $(CR_OWN) build/ptx_core.a -lm -o $@
custom_run_test: build/custom_run_test
.PHONY: custom_run_test

# the custom content metadata on the SD card (custom_store.c + custom-core's codec, over the app's real save.c); make store_test && build/store_test
STORE_SRCS := store_test.c ../source/custom_store.c ../source/custom_core_build.c $(APP)/source/save.c
build/store_test: $(STORE_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) -I$(CCORE)/include -I$(GC)/include -I$(GC)/gen/out -I$(RC)/include -I$(RC)/gen/out -I$(APP)/include $(CFLAGS) -fsanitize=address,undefined $(STORE_SRCS) -lm -o $@
store_test: build/store_test
.PHONY: store_test
