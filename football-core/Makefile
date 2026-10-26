CC ?= cc
CPPFLAGS += -Iinclude -I../battle-core/gen/out
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror
BUILD ?= build
.PHONY: test clean
test: $(BUILD)/stats_test
	$(BUILD)/stats_test
$(BUILD)/stats_test: tests/stats_test.c src/football_stats.c include/football_stats.h ../battle-core/gen/out/pr_data.c
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/stats_test.c src/football_stats.c ../battle-core/gen/out/pr_data.c -lm -o $@
clean:
	rm -rf $(BUILD)

CPPFLAGS += -I../game-core/include -I../run-core/include -I../battle-core/include
CORE_LIBS = ../game-core/build/libgc.a ../game-core/build/reward-core/libreward.a ../game-core/build/ai-core/libai.a ../game-core/build/run-core/librun.a ../game-core/build/run-core/battle-core/libprcore.a
.PHONY: season-test core-libs
core-libs:
	$(MAKE) -C ../game-core build/libgc.a build/reward-core/libreward.a build/ai-core/libai.a build/run-core/librun.a build/run-core/battle-core/libprcore.a
season-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/season_test.c src/football_stats.c src/playoff_season.c $(CORE_LIBS) -lm -o $(BUILD)/season_test
	$(BUILD)/season_test
test: season-test

.PHONY: playcall-test
playcall-test: $(BUILD)/playcall_test
	$(BUILD)/playcall_test
$(BUILD)/playcall_test: tests/playcall_test.c src/playcall_ui.c src/football_playbook.c include/playcall_ui.h include/football_playbook.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/playcall_test.c src/playcall_ui.c src/football_playbook.c -o $@
test: playcall-test

.PHONY: render-test
render-test: $(BUILD)/render_test
	$(BUILD)/render_test
$(BUILD)/render_test: tests/render_test.c ../game/source/football_render.c ../game/include/football_render.h tests/stubs/citro2d.h src/football_defense.c src/playcall_ui.c src/football_playbook.c src/evolution_ui.c include/football_playbook.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D__3DS__ -Itests/stubs -I../game/include -I../game/menus -I../assets-pipeline/app-src/include tests/render_test.c ../game/source/football_render.c src/football_defense.c src/playcall_ui.c src/football_playbook.c src/evolution_ui.c ../battle-core/gen/out/pr_data.c -lm -o $@
test: render-test

.PHONY: evolution-ui-test
evolution-ui-test: $(BUILD)/evolution_ui_test
	$(BUILD)/evolution_ui_test
$(BUILD)/evolution_ui_test: tests/evolution_ui_test.c src/evolution_ui.c include/evolution_ui.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/evolution_ui_test.c src/evolution_ui.c -o $@
test: evolution-ui-test

.PHONY: shop-ui-test
shop-ui-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/shop_ui_test.c src/football_stats.c src/playoff_season.c src/football_shop_ui.c $(CORE_LIBS) -lm -o $(BUILD)/shop_ui_test
	$(BUILD)/shop_ui_test
test: shop-ui-test

.PHONY: shop-render-test
shop-render-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D__3DS__ -Itests/stubs -I../game/include -I../game/menus -I../assets-pipeline/app-src/include tests/shop_render_test.c ../game/source/football_shop_render.c ../game/menus/item_ui.c ../game/gen/gm_item_ui.c src/football_stats.c src/playoff_season.c src/playcall_ui.c src/football_playbook.c src/football_shop_ui.c $(CORE_LIBS) -lm -o $(BUILD)/shop_render_test
	$(BUILD)/shop_render_test
test: shop-render-test

MATCH_SRCS = src/football_rules.c src/football_match.c src/football_playbook.c src/football_contact.c src/football_defense.c src/football_pass.c
.PHONY: rules-test match-test
rules-test: $(BUILD)/rules_test
	$(BUILD)/rules_test
$(BUILD)/rules_test: tests/rules_test.c src/football_rules.c include/football_rules.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/rules_test.c src/football_rules.c -o $@
match-test: $(BUILD)/match_test
	$(BUILD)/match_test
$(BUILD)/match_test: tests/match_test.c $(MATCH_SRCS) src/football_stats.c include/football_match.h include/football_rules.h include/football_playbook.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/match_test.c $(MATCH_SRCS) src/football_stats.c ../battle-core/gen/out/pr_data.c -lm -o $@
test: rules-test match-test

.PHONY: playoff-match-test
playoff-match-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/playoff_match_test.c $(MATCH_SRCS) src/football_stats.c src/playoff_season.c src/football_playoff_match.c $(CORE_LIBS) -lm -o $(BUILD)/playoff_match_test
	$(BUILD)/playoff_match_test
test: playoff-match-test

.PHONY: match-view-test
match-view-test: $(BUILD)/match_view_test
	$(BUILD)/match_view_test
$(BUILD)/match_view_test: tests/match_view_test.c ../game/source/football_match_view.c ../game/include/football_match_view.h $(MATCH_SRCS) include/football_match.h include/football_playbook.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D__3DS__ -Itests/stubs -I../game/include -I../game/menus -I../assets-pipeline/app-src/include tests/match_view_test.c ../game/source/football_match_view.c $(MATCH_SRCS) src/football_stats.c ../battle-core/gen/out/pr_data.c -lm -o $@
test: match-view-test

MODE_SRCS = src/football_save.c src/football_mode.c src/football_playoff_match.c src/playoff_season.c src/football_stats.c $(MATCH_SRCS) src/playcall_ui.c src/evolution_ui.c src/football_shop_ui.c
.PHONY: mode-test
mode-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/mode_test.c $(MODE_SRCS) $(CORE_LIBS) -lm -o $(BUILD)/mode_test
	$(BUILD)/mode_test
test: mode-test

.PHONY: screen-test
screen-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D__3DS__ -DFOOTBALL_SCREEN_TEST -Itests/stubs -I../game/include -I../game/menus -I../assets-pipeline/app-src/include tests/screen_test.c ../game/source/football_screen.c $(MODE_SRCS) $(CORE_LIBS) -lm -o $(BUILD)/screen_test
	$(BUILD)/screen_test
test: screen-test

.PHONY: mega-test
mega-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/mega_test.c $(MODE_SRCS) $(CORE_LIBS) -lm -o $(BUILD)/mega_test
	$(BUILD)/mega_test
test: mega-test

.PHONY: contact-test
contact-test: $(BUILD)/contact_test
	$(BUILD)/contact_test
$(BUILD)/contact_test: tests/contact_test.c src/football_contact.c include/football_contact.h include/football_stats.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/contact_test.c src/football_contact.c -o $@
test: contact-test

.PHONY: save-test
save-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/save_test.c $(MODE_SRCS) $(CORE_LIBS) -lm -o $(BUILD)/save_test
	$(BUILD)/save_test
test: save-test

.PHONY: save-file-test
save-file-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I../game/include -I../assets-pipeline/app-src/include tests/save_file_test.c ../game/source/football_saves.c ../assets-pipeline/app-src/source/save.c $(MODE_SRCS) $(CORE_LIBS) -lm -o $(BUILD)/save_file_test
	$(BUILD)/save_file_test
test: save-file-test

.PHONY: defense-test
defense-test: $(BUILD)/defense_test
	$(BUILD)/defense_test
$(BUILD)/defense_test: tests/defense_test.c $(MATCH_SRCS) src/football_stats.c include/football_defense.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/defense_test.c $(MATCH_SRCS) src/football_stats.c ../battle-core/gen/out/pr_data.c -lm -o $@
test: defense-test

.PHONY: pass-test
pass-test: $(BUILD)/pass_test
	$(BUILD)/pass_test
$(BUILD)/pass_test: tests/pass_test.c src/football_pass.c src/football_playbook.c src/football_defense.c include/football_pass.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/pass_test.c src/football_pass.c src/football_playbook.c src/football_defense.c -o $@
test: pass-test

.PHONY: tactics-test
tactics-test: $(BUILD)/tactics_test
	$(BUILD)/tactics_test
$(BUILD)/tactics_test: tests/tactics_test.c $(MATCH_SRCS) src/football_stats.c include/football_contact.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/tactics_test.c $(MATCH_SRCS) src/football_stats.c ../battle-core/gen/out/pr_data.c -lm -o $@
test: tactics-test

.PHONY: balance-report
balance-report: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/balance_report.c $(MODE_SRCS) $(CORE_LIBS) -lm -o $(BUILD)/balance_report

.PHONY: balance-baseline-report balance-test
balance-baseline-report: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I../reward-core/include -I../reward-core/gen/out -I../ai-core/include -DFB_DIFFICULTY_STEP_Q8=64 -DGC_FOOTBALL_EXP_FACTOR=500 tools/balance_report.c ../game-core/src/gc_football.c $(MODE_SRCS) $(CORE_LIBS) -lm -o $(BUILD)/balance_baseline
balance-test: balance-report
	$(BUILD)/balance_report 512 > $(BUILD)/balance-report.json
	python3 tools/check_balance_report.py $(BUILD)/balance-report.json
test: balance-test

.PHONY: live-test
live-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/live_test.c src/football_camera.c src/football_roster.c $(LIVE_SRCS) src/football_stats.c src/football_rules.c src/football_playbook.c $(CORE_LIBS) -lm -o $(BUILD)/live_test
	$(BUILD)/live_test
test: live-test

.PHONY: career-test live-screen-check
career-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/career_test.c src/football_career.c src/football_career_save.c src/football_roster.c $(LIVE_SRCS) src/football_stats.c src/football_rules.c src/football_playbook.c $(CORE_LIBS) -lm -o $(BUILD)/career_test
	$(BUILD)/career_test
live-screen-check:
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only -D__3DS__ -Itests/stubs -I../game/include -I../game/menus -I../assets-pipeline/app-src/include ../game/source/football_live_screen.c ../game/source/football_live_art.c ../game/source/football_live_fx.c ../game/source/football_menu.c
test: career-test live-screen-check

.PHONY: live-screen-test
live-screen-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D__3DS__ -DFOOTBALL_LIVE_SCREEN_TEST -Itests/stubs -I../game/include -I../game/menus -I../assets-pipeline/app-src/include tests/live_screen_test.c ../game/source/football_live_screen.c ../game/source/football_menu.c src/football_career.c src/football_career_save.c src/football_camera.c src/football_roster.c $(LIVE_SRCS) src/football_stats.c src/football_rules.c src/football_playbook.c $(CORE_LIBS) -lm -o $(BUILD)/live_screen_test
	$(BUILD)/live_screen_test
test: live-screen-test

LIVE_SRCS = src/football_live.c src/football_live_ai.c src/football_live_defense.c src/football_live_physics.c src/football_live_special.c

.PHONY: live-systems-test
live-systems-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/live_systems_test.c src/football_roster.c $(LIVE_SRCS) src/football_stats.c src/football_rules.c src/football_playbook.c $(CORE_LIBS) -lm -o $(BUILD)/live_systems_test
	$(BUILD)/live_systems_test
test: live-systems-test

.PHONY: ai-test
ai-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/ai_test.c src/football_roster.c $(LIVE_SRCS) src/football_stats.c src/football_rules.c src/football_playbook.c $(CORE_LIBS) -lm -o $(BUILD)/ai_test
	$(BUILD)/ai_test
test: ai-test

.PHONY: special-test
special-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/special_test.c src/football_roster.c $(LIVE_SRCS) src/football_stats.c src/football_rules.c src/football_playbook.c $(CORE_LIBS) -lm -o $(BUILD)/special_test
	$(BUILD)/special_test
test: special-test

.PHONY: kickoff-test
kickoff-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/kickoff_test.c src/football_roster.c $(LIVE_SRCS) src/football_stats.c src/football_rules.c src/football_playbook.c $(CORE_LIBS) -lm -o $(BUILD)/kickoff_test
	$(BUILD)/kickoff_test
test: kickoff-test

.PHONY: candy-test
candy-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/candy_test.c src/football_career.c src/football_career_save.c src/football_roster.c $(LIVE_SRCS) src/football_stats.c src/football_rules.c src/football_playbook.c $(CORE_LIBS) -lm -o $(BUILD)/candy_test
	$(BUILD)/candy_test
test: candy-test

.PHONY: fx-test
fx-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/fx_test.c src/football_roster.c $(LIVE_SRCS) src/football_stats.c src/football_rules.c src/football_playbook.c $(CORE_LIBS) -lm -o $(BUILD)/fx_test
	$(BUILD)/fx_test
test: fx-test

.PHONY: menu-test
menu-test: $(BUILD)/menu_test
	$(BUILD)/menu_test
$(BUILD)/menu_test: tests/menu_test.c tests/menu_fakes.h ../game/source/football_menu.c ../game/include/football_menu.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D__3DS__ -Itests/stubs -I../game/include -I../game/menus -I../assets-pipeline/app-src/include tests/menu_test.c ../game/source/football_menu.c -lm -o $@
test: menu-test

.PHONY: live-art-test
live-art-test: $(BUILD)/live_art_test
	$(BUILD)/live_art_test
$(BUILD)/live_art_test: tests/live_art_test.c tests/menu_fakes.h ../game/source/football_live_art.c ../game/source/football_live_fx.c ../game/source/football_menu.c src/football_live_physics.c src/football_camera.c src/football_defense.c src/football_playbook.c src/football_rules.c
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D__3DS__ -Itests/stubs -I../game/include -I../game/menus -I../assets-pipeline/app-src/include $^ -lm -o $@
test: live-art-test
