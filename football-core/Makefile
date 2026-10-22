CC ?= cc
CPPFLAGS += -Iinclude -I../battle-core/gen/out
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Werror
BUILD ?= build
.PHONY: test clean
test: $(BUILD)/stats_test
	./$(BUILD)/stats_test
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
	./$(BUILD)/season_test
test: season-test

.PHONY: playcall-test
playcall-test: $(BUILD)/playcall_test
	./$(BUILD)/playcall_test
$(BUILD)/playcall_test: tests/playcall_test.c src/playcall_ui.c include/playcall_ui.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/playcall_test.c src/playcall_ui.c -o $@
test: playcall-test

.PHONY: render-test
render-test: $(BUILD)/render_test
	./$(BUILD)/render_test
$(BUILD)/render_test: tests/render_test.c ../game/source/football_render.c ../game/include/football_render.h tests/stubs/citro2d.h src/playcall_ui.c src/evolution_ui.c
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D__3DS__ -Itests/stubs -I../game/include -I../game/menus -I../assets-pipeline/app-src/include tests/render_test.c ../game/source/football_render.c src/playcall_ui.c src/evolution_ui.c ../battle-core/gen/out/pr_data.c -lm -o $@
test: render-test

.PHONY: evolution-ui-test
evolution-ui-test: $(BUILD)/evolution_ui_test
	./$(BUILD)/evolution_ui_test
$(BUILD)/evolution_ui_test: tests/evolution_ui_test.c src/evolution_ui.c include/evolution_ui.h
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/evolution_ui_test.c src/evolution_ui.c -o $@
test: evolution-ui-test

.PHONY: shop-ui-test
shop-ui-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/shop_ui_test.c src/football_stats.c src/playoff_season.c src/football_shop_ui.c $(CORE_LIBS) -lm -o $(BUILD)/shop_ui_test
	./$(BUILD)/shop_ui_test
test: shop-ui-test

.PHONY: shop-render-test
shop-render-test: core-libs
	mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D__3DS__ -Itests/stubs -I../game/include -I../game/menus -I../assets-pipeline/app-src/include tests/shop_render_test.c ../game/source/football_shop_render.c ../game/menus/item_ui.c ../game/gen/gm_item_ui.c src/football_stats.c src/playoff_season.c src/playcall_ui.c src/football_shop_ui.c $(CORE_LIBS) -lm -o $(BUILD)/shop_render_test
	./$(BUILD)/shop_render_test
test: shop-render-test
