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
$(BUILD)/render_test: tests/render_test.c ../game/source/football_render.c ../game/include/football_render.h tests/stubs/citro2d.h src/playcall_ui.c src/football_playbook.c src/evolution_ui.c include/football_playbook.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D__3DS__ -Itests/stubs -I../game/include -I../game/menus -I../assets-pipeline/app-src/include tests/render_test.c ../game/source/football_render.c src/playcall_ui.c src/football_playbook.c src/evolution_ui.c ../battle-core/gen/out/pr_data.c -lm -o $@
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

MATCH_SRCS = src/football_rules.c src/football_match.c src/football_playbook.c
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

MODE_SRCS = src/football_mode.c src/football_playoff_match.c src/playoff_season.c src/football_stats.c $(MATCH_SRCS) src/playcall_ui.c src/evolution_ui.c src/football_shop_ui.c
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
