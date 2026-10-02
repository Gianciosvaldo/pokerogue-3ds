# Host build of run-core + its test. It links battle-core's library (the RNG, V8's Math.pow and
# the generated tables) and never changes battle-core; the 3DS build compiles the same sources.
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
BATTLE_CORE ?= ../battle-core
CPPFLAGS += -Iinclude -I$(BATTLE_CORE)/include -I$(BATTLE_CORE)/gen/out
BUILD ?= build
CORE_LIB := $(BUILD)/battle-core/libprcore.a
SRCS := src/wave.c src/shop.c src/biome.c src/evolution.c gen/out/run_biomes.c gen/out/run_evolutions.c src/exp.c gen/out/run_exp.c src/upkeep.c src/newbattle.c gen/out/run_trainers.c src/pokemongen.c gen/out/run_pokemongen.c src/trainerspecies.c gen/out/run_trainerspecies.c src/trainerparty.c src/wildenemy.c src/enemyitems.c gen/out/run_enemyitems.c src/daily.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/librun.a $(BUILD)/wavetest $(BUILD)/shoptest $(BUILD)/biometest $(BUILD)/encountertest $(BUILD)/newbattletest $(BUILD)/exptest $(BUILD)/partyexptest $(BUILD)/upkeeptest $(BUILD)/battlegentest $(BUILD)/pokemongentest $(BUILD)/trainerspeciestest $(BUILD)/trainerpartytest $(BUILD)/wildenemytest $(BUILD)/lucktest $(BUILD)/enemyitemstest $(BUILD)/enemybuffstest $(BUILD)/dailytest

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(CORE_LIB): FORCE
	$(MAKE) -s -C $(BATTLE_CORE) BUILD=$(abspath $(BUILD))/battle-core $(abspath $(CORE_LIB))

$(BUILD)/librun.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/wavetest: tests/wavetest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

clean:
	rm -rf $(BUILD)

FORCE:
.PHONY: all clean FORCE
-include $(OBJS:.o=.d)

$(BUILD)/shoptest: tests/shoptest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/biometest: tests/biometest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/encountertest: tests/encountertest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/newbattletest: tests/newbattletest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/exptest: tests/exptest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/partyexptest: tests/partyexptest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/upkeeptest: tests/upkeeptest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/battlegentest: tests/battlegentest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/pokemongentest: tests/pokemongentest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/trainerspeciestest: tests/trainerspeciestest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/trainerpartytest: tests/trainerpartytest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/wildenemytest: tests/wildenemytest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/lucktest: tests/lucktest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/enemyitemstest: tests/enemyitemstest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/enemybuffstest: tests/enemybuffstest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/dailytest: tests/dailytest.c $(BUILD)/librun.a $(CORE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
