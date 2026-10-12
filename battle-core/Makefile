# Host build of the battle core + tests. The 3DS build compiles the same sources
# with devkitARM (see README).
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
CPPFLAGS += -Iinclude -Igen/out
BUILD ?= build
SRCS := src/rng.c src/hooks.c src/pokemon.c src/damage.c src/move_attrs.c src/closures.c src/closures_moves.c src/closures_conditions.c src/ab_attrs.c src/closures_abilities.c src/modifiers.c src/tags.c src/battler_tags.c src/arena_tags.c src/history.c src/engine.c src/phases.c src/move_effects.c src/called_moves.c src/switching.c src/commands.c src/js_math.c src/text.c src/events.c gen/out/pr_data.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/party-status-cure-test $(BUILD)/camouflage-test $(BUILD)/average-stats-test $(BUILD)/speedboosttest $(BUILD)/growthtest $(BUILD)/psych-up-test $(BUILD)/trainerswitchtest $(BUILD)/tempturntest $(BUILD)/fielditemtest $(BUILD)/statitemtest $(BUILD)/crititemtest $(BUILD)/typeboosttest $(BUILD)/goldentest $(BUILD)/souldewtest $(BUILD)/multilenstest $(BUILD)/reviverseedtest $(BUILD)/widelenstest $(BUILD)/statusorbtest $(BUILD)/dancertest $(BUILD)/doublestest $(BUILD)/itemremovaltest $(BUILD)/berrytest $(BUILD)/weatherformtest $(BUILD)/illusiontest $(BUILD)/libprcore.a $(BUILD)/rng_cli $(BUILD)/difftest $(BUILD)/turntest $(BUILD)/bosssegmenttest

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/libprcore.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/rng_cli: tests/rng_cli.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/difftest: tests/difftest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/turntest: tests/turntest.c tests/optional-phase-map.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/turntest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/bosssegmenttest: tests/bosssegmenttest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

gen: gen/out/pr_data.c
gen/out/pr_data.c: gen/gen_tables.py $(wildcard gen/upstream/*.json)
	python3 gen/gen_tables.py

clean:
	rm -rf $(BUILD)
.PHONY: all clean gen
-include $(OBJS:.o=.d)

$(BUILD)/illusiontest: tests/illusiontest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@


$(BUILD)/weatherformtest: tests/weatherformtest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/itemremovaltest: tests/itemremovaltest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/berrytest: tests/berrytest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/dancertest: tests/dancertest.c tests/turntest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/dancertest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/doublestest: tests/doublestest.c tests/turntest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/doublestest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/statusorbtest: tests/statusorbtest.c tests/status-orb-replay.inc tests/statusorb-guards.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/statusorbtest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/widelenstest: tests/widelenstest.c tests/wide-lens-replay.inc tests/widelens-guards.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/widelenstest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/reviverseedtest: tests/reviverseedtest.c tests/reviverseed-guards.inc tests/turntest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/reviverseedtest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/multilenstest: tests/multilenstest.c tests/multi-lens-replay.inc tests/multilens-guards.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/multilenstest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/goldentest: tests/goldentest.c tests/golden-punch-replay.inc tests/golden-guards.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/goldentest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/souldewtest: tests/souldewtest.c tests/soul-dew-replay.inc tests/soul-guards.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/souldewtest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/typeboosttest: tests/typeboosttest.c tests/attack-type-replay.inc tests/typeboost-guards.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/typeboosttest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/crititemtest: tests/crititemtest.c tests/critical-item-replay.inc tests/crititem-guards.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/crititemtest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/statitemtest: tests/statitemtest.c tests/effective-stat-item-replay.inc tests/statitem-guards.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/statitemtest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/fielditemtest: tests/fielditemtest.c tests/field-effect-replay.inc tests/fielditem-guards.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/fielditemtest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/tempturntest: tests/tempturntest.c tests/temporary-turn-replay.inc tests/tempturn-guards.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/tempturntest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/trainerswitchtest: tests/trainerswitchtest.c tests/trainer-switch-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/trainerswitchtest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/growthtest: tests/growthtest.c tests/growth-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/growthtest.c $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/psych-up-replay.inc: tests/growth-replay.inc tools/build-psych-up-replay.py
	mkdir -p $(BUILD)
	python3 tools/build-psych-up-replay.py $< $@

$(BUILD)/psych-up-test: tests/psych-up-test.c $(BUILD)/psych-up-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/psych-up-test.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/speedboosttest: tests/speedboosttest.c tests/speed-boost-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/speedboosttest.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/average-stats-replay.inc: tests/growth-replay.inc tools/build-average-stats-replay.py
	mkdir -p $(BUILD)
	python3 tools/build-average-stats-replay.py $< $@
$(BUILD)/average-stats-test: tests/average-stats-test.c $(BUILD)/average-stats-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/average-stats-test.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/camouflage-replay.inc: tests/growth-replay.inc tools/build-camouflage-replay.py
	@mkdir -p $(BUILD)
	python3 tools/build-camouflage-replay.py $< $@
$(BUILD)/camouflage-test: tests/camouflage-test.c $(BUILD)/camouflage-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/camouflage-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/camouflage-helper-test
$(BUILD)/camouflage-helper-test: tests/camouflage-helper-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/party-status-cure-replay.inc: tests/growth-replay.inc tools/build-party-status-cure-replay.py
	@mkdir -p $(BUILD)
	python3 tools/build-party-status-cure-replay.py $< $@
$(BUILD)/party-status-cure-test: tests/party-status-cure-test.c $(BUILD)/party-status-cure-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/party-status-cure-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/party-status-cure-boundary
test: party-status-cure-boundary
party-status-cure-boundary: $(BUILD)/party-status-cure-boundary
	$(BUILD)/party-status-cure-boundary
$(BUILD)/party-status-cure-boundary: tests/party-status-cure-boundary.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/fell-stinger-test
$(BUILD)/fell-stinger-replay.inc: tests/growth-replay.inc tools/build-fell-stinger-replay.py
	@mkdir -p $(BUILD)
	python3 tools/build-fell-stinger-replay.py $< $@
$(BUILD)/fell-stinger-test: tests/fell-stinger-test.c $(BUILD)/fell-stinger-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/fell-stinger-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/fell-stinger-helper-test
$(BUILD)/fell-stinger-helper-test: tests/fell-stinger-helper-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/bad-dreams-test
$(BUILD)/bad-dreams-replay.inc: tests/growth-replay.inc tools/build-bad-dreams-replay.py
	@mkdir -p $(BUILD)
	python3 tools/build-bad-dreams-replay.py $< $@
$(BUILD)/bad-dreams-test: tests/bad-dreams-test.c $(BUILD)/bad-dreams-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/bad-dreams-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/bad-dreams-helper-test
$(BUILD)/bad-dreams-helper-test: tests/bad-dreams-helper-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/cheek-pouch-test
$(BUILD)/cheek-pouch-replay.inc: tests/growth-replay.inc tools/build-cheek-pouch-replay.py
	@mkdir -p $(BUILD)
	python3 tools/build-cheek-pouch-replay.py $< $@
$(BUILD)/cheek-pouch-test: tests/cheek-pouch-test.c $(BUILD)/cheek-pouch-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/cheek-pouch-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/cheek-pouch-helper-test
$(BUILD)/cheek-pouch-helper-test: tests/cheek-pouch-helper-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

# Additive optional queue transport; established turntest remains untouched.
all: $(BUILD)/optional-phase-test $(BUILD)/optional-phase-fixtures-test
$(BUILD)/optional-phase-replay.inc: tests/turntest.c tools/build-optional-phase-replay.py
	@mkdir -p $(BUILD)
	python3 tools/build-optional-phase-replay.py $< $@
$(BUILD)/optional-phase-test: tests/optional-phase-test.c tests/optional-phase-map.inc $(BUILD)/optional-phase-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests -I$(BUILD) tests/optional-phase-test.c $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/optional-phase-fixtures-test: tests/optional-phase-fixtures-test.c tests/optional-phase-map.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests tests/optional-phase-fixtures-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/poltergeist-message-test
$(BUILD)/poltergeist-message-test: tests/poltergeist-message-test.c tests/turntest.c tests/optional-phase-map.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/poltergeist-message-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/moodytest
$(BUILD)/moodytest: tests/moodytest.c tests/speed-boost-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/moodytest.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/consecutive-move-test
$(BUILD)/consecutive-move-test: tests/consecutive-move-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/consecutive-move-test.c $(BUILD)/libprcore.a -lm -o $@
