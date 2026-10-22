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
