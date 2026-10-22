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
