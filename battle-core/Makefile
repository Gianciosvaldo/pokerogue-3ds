# Host build of the battle core + tests. The 3DS build compiles the same sources
# with devkitARM (see README).
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off
CPPFLAGS += -Iinclude -Igen/out
BUILD ?= build
SRCS := src/form_items.c gen/out/pr_item_forms.c src/pickup.c src/item_transfer.c src/rng.c src/hooks.c src/pokemon.c src/damage.c src/move_attrs.c src/closures.c src/closures_moves.c src/closures_conditions.c src/ab_attrs.c src/closures_abilities.c src/modifiers.c src/tags.c src/battler_tags.c src/arena_tags.c src/history.c src/engine.c src/phases.c src/move_effects.c src/called_moves.c src/switching.c src/commands.c src/js_math.c src/js_number.c src/text.c src/events.c gen/out/pr_data.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)

all: $(BUILD)/transform-test
$(BUILD)/transform-test: tests/transform-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/transform-turn-test
$(BUILD)/transform-turn-replay.inc: tests/speed-boost-replay.inc tools/build-transform-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-transform-turn-proof.py replay $@
$(BUILD)/transform-turn-test: tests/transform-turn-test.c $(BUILD)/transform-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/transform-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/flee-phase-test
$(BUILD)/flee-phase-test: tests/flee-phase-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/boss-score-test
$(BUILD)/boss-score-test: tests/boss-score-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

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

# Normal builds use the committed tables without needing an upstream dump.
# Explicit regeneration still requires the dump; regenerate automatically when
# those inputs are present and change.
gen:
	python3 gen/gen_tables.py
ifneq ($(wildcard gen/upstream/enums.json),)
gen/out/pr_data.c: gen/gen_tables.py gen/closure_identity.py $(wildcard gen/upstream/*.json)
	python3 gen/gen_tables.py
endif

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
all: $(BUILD)/knockout-stats-test
$(BUILD)/knockout-stats-test: tests/knockout-stats-test.c $(BUILD)/fell-stinger-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/knockout-stats-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/leave-weather-test
$(BUILD)/leave-weather-test: tests/leave-weather-test.c tests/trainer-switch-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/leave-weather-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/wonder-skin-test
$(BUILD)/wonder-skin-replay.inc: tests/speed-boost-replay.inc tools/build-wonder-skin-replay.py
	@mkdir -p $(BUILD)
	python3 tools/build-wonder-skin-replay.py $< $@
$(BUILD)/wonder-skin-test: tests/wonder-skin-test.c $(BUILD)/wonder-skin-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/wonder-skin-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/wonder-skin-helper-test
$(BUILD)/wonder-skin-helper-test: tests/wonder-skin-helper-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/wonder-skin-helper-test.c $(BUILD)/libprcore.a -lm -o $@

# Source-backed priority bracket turns, using an additive owning projection.
all: $(BUILD)/priority-brackets-test
$(BUILD)/priority-brackets-replay.inc: tests/speed-boost-replay.inc tools/build-priority-brackets-replay.py
	@mkdir -p $(BUILD)
	python3 tools/build-priority-brackets-replay.py $< $@
$(BUILD)/priority-brackets-test: tests/priority-brackets-test.c $(BUILD)/priority-brackets-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/priority-brackets-test.c $(BUILD)/libprcore.a -lm -o $@

# Protean/Libero actual source turns; unchanged projection and additive strict guard.
all: $(BUILD)/protean-test
$(BUILD)/protean-test: tests/protean-test.c tests/speed-boost-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/protean-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/protean-helper-test
$(BUILD)/protean-helper-test: tests/protean-helper-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

# Defending-field priority immunity: actual source turns, unchanged projection.
all: $(BUILD)/field-priority-test
$(BUILD)/field-priority-test: tests/field-priority-test.c tests/speed-boost-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/field-priority-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/multi-hit-abilities-test
$(BUILD)/multi-hit-abilities-test: tests/multi-hit-abilities-test.c tests/speed-boost-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/multi-hit-abilities-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/multi-hit-abilities-helper-test
$(BUILD)/multi-hit-abilities-helper-test: tests/multi-hit-abilities-helper-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/beat-up-test
$(BUILD)/beat-up-test: tests/beat-up-test.c tests/speed-boost-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/beat-up-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/beat-up-helper-test
$(BUILD)/beat-up-helper-test: tests/beat-up-helper-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/berry-turn-test
$(BUILD)/berry-turn-replay.inc: tests/speed-boost-replay.inc tools/build-berry-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-berry-turn-proof.py replay $@
$(BUILD)/berry-turn-test: tests/berry-turn-test.c $(BUILD)/berry-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/berry-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/receiver-test
$(BUILD)/receiver-test: tests/receiver-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/ability-lookup-test
$(BUILD)/ability-lookup-test: tests/ability-lookup-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/form-lifecycle-test
$(BUILD)/form-lifecycle-test: tests/form-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/battle-bond-test
$(BUILD)/battle-bond-test: tests/battle-bond-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/battle-bond-turn-test
$(BUILD)/battle-bond-turn-test: tests/battle-bond-turn-test.c $(BUILD)/fell-stinger-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -Itests -I$(BUILD) $(CFLAGS) tests/battle-bond-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/reset-arena-test
$(BUILD)/reset-arena-test: tests/reset-arena-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/copying-test $(BUILD)/copying-turn-test
$(BUILD)/copying-test: tests/copying-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/copying-turn-replay.inc: tests/speed-boost-replay.inc tools/build-transform-turn-proof.py tools/build-copying-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-copying-turn-proof.py replay $@
$(BUILD)/copying-turn-test: tests/copying-turn-test.c $(BUILD)/copying-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/copying-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/native-form-test
$(BUILD)/native-form-test: tests/native-form-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/native-form-turn-test
$(BUILD)/native-form-turn-replay.inc: tests/speed-boost-replay.inc tools/build-transform-turn-proof.py tools/build-native-form-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-native-form-turn-proof.py replay $@
$(BUILD)/native-form-turn-test: tests/native-form-turn-test.c $(BUILD)/native-form-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/native-form-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/gas-lifecycle-test $(BUILD)/arena-remove-all-test $(BUILD)/imprison-test
$(BUILD)/gas-lifecycle-test: tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/arena-remove-all-test: tests/arena-remove-all-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/arena-remove-all-test.c $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/imprison-test: tests/imprison-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/imprison-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/gas-turn-test
$(BUILD)/gas-turn-replay.inc: tests/speed-boost-replay.inc tools/build-transform-turn-proof.py tools/build-gas-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-gas-turn-proof.py replay $@
$(BUILD)/gas-turn-test: tests/gas-turn-test.c $(BUILD)/gas-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/gas-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/stance-change-test $(BUILD)/stance-turn-test
$(BUILD)/stance-change-test: tests/stance-change-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/stance-change-test.c $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/stance-turn-replay.inc: tests/speed-boost-replay.inc tools/build-transform-turn-proof.py tools/build-stance-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-stance-turn-proof.py replay $@
$(BUILD)/stance-turn-test: tests/stance-turn-test.c $(BUILD)/stance-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/stance-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/form-interaction-test $(BUILD)/form-interaction-turn-test
$(BUILD)/form-interaction-test: tests/form-interaction-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/form-interaction-test.c $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/form-interaction-turn-replay.inc: tests/speed-boost-replay.inc tools/build-transform-turn-proof.py tools/build-form-interaction-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-form-interaction-turn-proof.py replay $@
$(BUILD)/form-interaction-turn-test: tests/form-interaction-turn-test.c $(BUILD)/form-interaction-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/form-interaction-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/weather-form-test
$(BUILD)/weather-form-test: tests/weather-form-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/weather-form-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/weather-turn-test
$(BUILD)/weather-turn-replay.inc: tests/speed-boost-replay.inc tools/build-transform-turn-proof.py tools/build-weather-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-weather-turn-proof.py replay $@
$(BUILD)/weather-turn-test: tests/weather-turn-test.c $(BUILD)/weather-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/weather-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/ally-lookup-test
$(BUILD)/ally-lookup-test: tests/ally-lookup-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/pending-heal-test $(BUILD)/pending-turn-test
$(BUILD)/pending-heal-test: tests/pending-heal-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/pending-heal-test.c $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/pending-turn-replay.inc: tests/speed-boost-replay.inc tools/build-transform-turn-proof.py tools/build-pending-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-pending-turn-proof.py replay $@
$(BUILD)/pending-turn-test: tests/pending-turn-test.c $(BUILD)/pending-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/pending-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/form-fusion-test $(BUILD)/stat-order-turn-test
$(BUILD)/form-fusion-test: tests/form-fusion-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/form-fusion-test.c $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/stat-order-turn-replay.inc: tests/speed-boost-replay.inc tools/build-transform-turn-proof.py tools/build-native-form-turn-proof.py tools/build-stat-order-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-stat-order-turn-proof.py replay $@
$(BUILD)/stat-order-turn-test: tests/stat-order-turn-test.c $(BUILD)/stat-order-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/stat-order-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/fusion-name-test
$(BUILD)/fusion-name-test: tests/fusion-name-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/fusion-message-test
$(BUILD)/fusion-message-test: tests/fusion-message-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/fusion-wish-test
$(BUILD)/fusion-wish-test: tests/fusion-wish-test.c tests/fusion-message-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/fusion-wish-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/fusion-callback-test $(BUILD)/transform-callback-test $(BUILD)/fusion-callback-turn-test $(BUILD)/transform-callback-turn-test
$(BUILD)/fusion-callback-test: tests/fusion-callback-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/transform-callback-test: tests/transform-callback-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
$(BUILD)/fusion-callback-turn-replay.inc: tests/speed-boost-replay.inc tools/build-transform-turn-proof.py tools/build-native-form-turn-proof.py tools/build-fusion-callback-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-fusion-callback-turn-proof.py replay $@
$(BUILD)/fusion-callback-turn-test: tests/fusion-callback-turn-test.c $(BUILD)/fusion-callback-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/fusion-callback-turn-test.c $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/transform-callback-turn-replay.inc: tests/speed-boost-replay.inc tools/build-transform-turn-proof.py tools/build-transform-callback-turn-proof.py
	@mkdir -p $(BUILD)
	python3 tools/build-transform-callback-turn-proof.py replay $@
$(BUILD)/transform-callback-turn-test: tests/transform-callback-turn-test.c $(BUILD)/transform-callback-turn-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/transform-callback-turn-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/interlude-test
$(BUILD)/interlude-test: tests/interlude-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/interlude-test.c $(BUILD)/libprcore.a -lm -o $@
all: $(BUILD)/interlude-end-test
$(BUILD)/interlude-end-test: tests/interlude-end-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/interlude-end-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/final-boss-test
$(BUILD)/final-boss-test: tests/final-boss-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/final-boss-form-test
$(BUILD)/final-boss-form-test: tests/final-boss-form-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/item-transfer-test
$(BUILD)/item-transfer-test: tests/item-transfer-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/encounter-end-test
$(BUILD)/encounter-end-test: tests/encounter-end-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/summon-entry-test
$(BUILD)/summon-entry-test: tests/summon-entry-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/summon-entry-host-test
summon-entry-host-test: $(BUILD)/summon-entry-host-test
	$(BUILD)/summon-entry-host-test
$(BUILD)/summon-entry-host-test: tests/summon-entry-host-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@
.PHONY: summon-entry-host-test

all: $(BUILD)/helping-hand-test
$(BUILD)/helping-hand-test: tests/helping-hand-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/acupressure-test
$(BUILD)/acupressure-test: tests/acupressure-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/after-you-test
$(BUILD)/after-you-test: tests/after-you-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/suppression-test
$(BUILD)/suppression-test: tests/suppression-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/wide-level-test
$(BUILD)/wide-level-test: tests/wide-level-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/wide-boss-score-test
$(BUILD)/wide-boss-score-test: tests/wide-boss-score-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/flame-burst-test
$(BUILD)/flame-burst-test: tests/flame-burst-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/flame-burst-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/partner-cures-test
$(BUILD)/partner-cures-test: tests/partner-cures-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/partner-cures-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/attention-test
$(BUILD)/attention-test: tests/attention-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/attention-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/commander-test
$(BUILD)/commander-test: tests/commander-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/commander-test.c $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/move-theft-test: tests/move-theft-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/berry-moves-test: tests/berry-moves-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -Wl,--wrap=pr_apply_ab_attrs -lm -o $@

all: $(BUILD)/xerneas-active-test
$(BUILD)/xerneas-active-test: tests/xerneas-active-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/bad-dreams-doubles-test
$(BUILD)/bad-dreams-doubles-replay.c: tests/doublestest.c tools/build-bad-dreams-doubles-replay.py
	mkdir -p $(@D)
	python3 tools/build-bad-dreams-doubles-replay.py $< $@
$(BUILD)/bad-dreams-doubles-test: $(BUILD)/bad-dreams-doubles-replay.c tests/turntest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -Itests $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/wide-substitute-test
$(BUILD)/wide-substitute-frame.inc: tests/gas-lifecycle-test.c tools/build-wide-substitute-frame.py
	mkdir -p $(@D)
	python3 tools/build-wide-substitute-frame.py $< $@
WIDE_AI_LIB := $(BUILD)/wide-ai/libai.a
$(WIDE_AI_LIB): $(wildcard ../ai-core/src/*.[ch] ../ai-core/include/pr/*.h include/pr/*.h) ../ai-core/gen/out/ai_data.c
	$(MAKE) -s -C ../ai-core BUILD=$(abspath $(BUILD)/wide-ai) BATTLE_CORE=$(abspath .) $(abspath $(WIDE_AI_LIB))
$(BUILD)/wide-substitute-test: tests/wide-substitute-test.c $(BUILD)/wide-substitute-frame.inc src/phases.c $(WIDE_AI_LIB) $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I../ai-core/include -Itests -I$(BUILD) $(CFLAGS) $< $(WIDE_AI_LIB) $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/wide-combat-test
$(BUILD)/wide-combat-replay.c: tests/doublestest.c tools/build-wide-combat-replay.py
	mkdir -p $(@D)
	python3 tools/build-wide-combat-replay.py $< $@
$(BUILD)/wide-combat-test: $(BUILD)/wide-combat-replay.c tests/turntest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -Itests $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/stat-rearrangement-test
$(BUILD)/stat-rearrangement-test: tests/stat-rearrangement-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/quash-test
$(BUILD)/quash-test: tests/quash-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/wide-highest-stat-test
$(BUILD)/wide-highest-stat-test: tests/wide-highest-stat-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/spectral-thief-test
$(BUILD)/spectral-thief-test: tests/spectral-thief-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/hp-split-test
$(BUILD)/hp-split-test: tests/hp-split-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/ability-moves-test
$(BUILD)/ability-moves-test: tests/ability-moves-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/hp-cost-boosts-test
$(BUILD)/hp-cost-boosts-test: tests/hp-cost-boosts-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/pledge-test
$(BUILD)/pledge-test: tests/pledge-test.c tests/gas-lifecycle-test.c src/phases.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/type-moves-test
$(BUILD)/type-moves-test: tests/type-moves-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -Wl,--wrap=pr_battle_rand_seed_int -lm -o $@

all: $(BUILD)/secret-power-test
$(BUILD)/secret-power-test: tests/secret-power-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -Wl,--wrap=pr_battle_rand_seed_int -lm -o $@

all: $(BUILD)/zero-to-hero-test
$(BUILD)/zero-to-hero-test: tests/zero-to-hero-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/form-block-damage-test
$(BUILD)/form-block-damage-test: tests/form-block-damage-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/form-block-damage-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/meloetta-forms-test
$(BUILD)/meloetta-forms-test: tests/meloetta-forms-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/meloetta-forms-test.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/revival-blessing-test
$(BUILD)/revival-blessing-test: tests/revival-blessing-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) tests/revival-blessing-test.c $(BUILD)/libprcore.a -Wl,--wrap=pr_battle_rand_seed_int -Wl,--wrap=pr_revival_blessing -lm -o $@

# Same strict full-turn comparator and coverage floors, with the new native UI fixture policy.
all: $(BUILD)/revival-turn-test
$(BUILD)/revival-turn-test: $(BUILD)/wide-combat-replay.c tests/turntest.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -Itests $(CFLAGS) -DTURNTEST_REVIVAL_FAINTED_PICK $(BUILD)/wide-combat-replay.c $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/ball-fetch-test
$(BUILD)/ball-fetch-test: tests/ball-fetch-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/gulp-missile-test
$(BUILD)/gulp-missile-test: tests/gulp-missile-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) $< $(BUILD)/libprcore.a -Wl,--wrap=pr_gulp_missile_tag -lm -o $@

all: $(BUILD)/tera-phase-test
$(BUILD)/tera-phase-test: tests/tera-phase-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/tera-cleanup-test
$(BUILD)/tera-cleanup-test: tests/tera-cleanup-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

tera-phase-host-test: $(BUILD)/tera-phase-test
	$(BUILD)/tera-phase-test --selftest
.PHONY: tera-phase-host-test

all: $(BUILD)/keldeo-forms-test
$(BUILD)/keldeo-forms-test: tests/keldeo-forms-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/mimicry-test
$(BUILD)/mimicry-test: tests/mimicry-test.c tests/keldeo-forms-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/entry-allies-test
$(BUILD)/entry-allies-test: tests/entry-allies-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/costar-test
$(BUILD)/costar-test: tests/costar-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/poison-puppeteer-test
$(BUILD)/poison-puppeteer-test: tests/poison-puppeteer-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/biome-callback-test
$(BUILD)/biome-callback-test: tests/biome-callback-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/double-chance-test
$(BUILD)/double-chance-test: tests/double-chance-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/double-generation-test
$(BUILD)/double-generation-test: tests/double-generation-test.c tests/double-chance-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a $(BUILD)/double-chance-run/librun.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -I../run-core/include $(CFLAGS) $< $(BUILD)/double-chance-run/librun.a $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/double-chance-run/librun.a: $(wildcard include/pr/*.h gen/out/*.h ../run-core/src/*.c ../run-core/include/pr/*.h ../run-core/gen/out/*.[ch])
	$(MAKE) -s -C ../run-core BUILD=$(abspath $(BUILD))/double-chance-run $(abspath $@)

$(BUILD)/src/js_number.o: $(wildcard vendor/ryu/*)

all: $(BUILD)/honey-money-test
$(BUILD)/honey-money-test: tests/honey-money-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/money-number-test
$(BUILD)/money-number-test: tests/money-number-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/honey-owner-test
$(BUILD)/honey-owner-test: tests/honey-owner-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/scene-transfer-test
$(BUILD)/scene-transfer-test: tests/scene-transfer-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/grip-claw-test
$(BUILD)/grip-claw-test: tests/grip-claw-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/baton-switch-test
$(BUILD)/baton-switch-test: tests/baton-switch-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

all: $(BUILD)/pickup-test
$(BUILD)/pickup-test: tests/pickup-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/stat-loot-test
$(BUILD)/stat-loot-test: tests/stat-loot-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/fused-held-replay.inc: tests/effective-stat-item-replay.inc tests/critical-item-replay.inc tools/build-fused-held-replay.py
	@mkdir -p $(@D)
	python3 tools/build-fused-held-replay.py $@
all: $(BUILD)/fused-held-test
$(BUILD)/fused-held-test: tests/fused-held-test.c tests/statitem-guards.inc tests/crititem-guards.inc $(BUILD)/fused-held-replay.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) -Itests $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

all: $(BUILD)/last-resort-history-test
$(BUILD)/last-resort-history-test: tests/last-resort-history-test.c tests/last-resort-history-fixture.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/held-form-items-test: tests/held-form-items-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

.PHONY: held-form-items-host-test
held-form-items-host-test: $(BUILD)/held-form-items-test
	python3 tools/check-held-form-items-proof.py

$(BUILD)/nonvirtual-history-test: tests/nonvirtual-history-test.c tests/nonvirtual-history-fixture.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/nonvirtual-history-test.c $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/nonvirtual-history-guards-test: tests/nonvirtual-history-guards-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

$(BUILD)/protect-history-test: tests/protect-history-test.c tests/protect-history-fixture.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/protect-history-test.c $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/protect-history-guards-test: tests/protect-history-guards-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -lm -o $@

build/remaining-history-test: tests/remaining-history-test.c tests/remaining-history.native.inc src/phases.c build/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/remaining-history-test.c build/libprcore.a -lm -o $@
build/remaining-history-guards-test: tests/remaining-history-guards-test.c build/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/remaining-history-guards-test.c build/libprcore.a -lm -o $@
remaining-history-host-test: build/remaining-history-test build/remaining-history-guards-test
	build/remaining-history-test
	build/remaining-history-guards-test
.PHONY: remaining-history-host-test

build/faint-history-guards-test: tests/faint-history-guards-test.c build/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/faint-history-guards-test.c build/libprcore.a -lm -o $@

build/faint-history-test: tests/faint-history-test.c tests/faint-history.native.inc ../ai-core/src/ai.c build/libprcore.a
	$(MAKE) -C ../ai-core build/libai.a
	$(CC) $(CPPFLAGS) -I../ai-core/include -I../ai-core/gen/out $(CFLAGS) tests/faint-history-test.c ../ai-core/build/libai.a build/libprcore.a -lm -o $@
faint-history-host-test: build/faint-history-test build/faint-history-guards-test
	build/faint-history-test
	build/faint-history-guards-test
.PHONY: faint-history-host-test

$(BUILD)/received-history-test: tests/received-history-test.c tests/received-history.native.inc src/phases.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/received-history-guards-test: tests/received-history-guards-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/received-command-test: tests/received-command-test.c tests/received-command.native.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@
received-history-host-test: $(BUILD)/received-history-test $(BUILD)/received-history-guards-test $(BUILD)/received-command-test
	$(BUILD)/received-history-test
	$(BUILD)/received-history-guards-test
	$(BUILD)/received-command-test

.PHONY: received-history-host-test

$(BUILD)/turn-order-test: tests/turn-order-test.c tests/turn-order.native.inc src/engine.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/turn-order-guards-test: tests/turn-order-guards-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@
$(BUILD)/turn-order-command-test: tests/turn-order-command-test.c tests/turn-order-command.native.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -Wl,--wrap=pr_phase_run -lm -o $@
turn-order-host-test: $(BUILD)/turn-order-test $(BUILD)/turn-order-guards-test $(BUILD)/turn-order-command-test
	$(BUILD)/turn-order-test
	$(BUILD)/turn-order-guards-test
	$(BUILD)/turn-order-command-test
.PHONY: turn-order-host-test

$(BUILD)/turn-actor-command-test: tests/turn-actor-command-test.c tests/turn-actor-command.native.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -Wl,--wrap=pr_phase_run -lm -o $@
$(BUILD)/turn-actors-test: tests/turn-actors-test.c tests/turn-actors.native.inc src/engine.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@
turn-actors-host-test: $(BUILD)/turn-actor-command-test $(BUILD)/turn-actors-test $(BUILD)/turn-actors-guards-test
	$(BUILD)/turn-actor-command-test
	$(BUILD)/turn-actors-test
	$(BUILD)/turn-actors-guards-test
.PHONY: turn-actors-host-test
$(BUILD)/turn-actors-guards-test: tests/turn-actors-guards-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@

$(BUILD)/event-delivery-test: tests/event-delivery-test.c tests/event-delivery.native.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -DEVENT_DELIVERY_STREAM $< $(BUILD)/libprcore.a -Wl,--wrap=pr_phase_run -lm -o $@
$(BUILD)/event-delivery-guards-test: tests/event-delivery-guards-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@
event-delivery-host-test: $(BUILD)/event-delivery-test $(BUILD)/event-delivery-guards-test
	$(BUILD)/event-delivery-test
	$(BUILD)/event-delivery-guards-test
.PHONY: event-delivery-host-test

$(BUILD)/trace-capacity-test: tests/trace-capacity-test.c tests/trace-capacity.native.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -Wl,--wrap=pr_phase_run -lm -o $@
$(BUILD)/trace-capacity-guards-test: tests/trace-capacity-guards-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libprcore.a -lm -o $@
trace-capacity-host-test: $(BUILD)/trace-capacity-test $(BUILD)/trace-capacity-guards-test
	$(BUILD)/trace-capacity-test
	$(BUILD)/trace-capacity-guards-test
.PHONY: trace-capacity-host-test

$(BUILD)/tailwind-owner-test: tests/tailwind-owner-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/tailwind-owner-test.c $(BUILD)/libprcore.a -lm -o $@
.PHONY: tailwind-owner-host-test
tailwind-owner-host-test: $(BUILD)/tailwind-owner-test
	python3 tools/check-tailwind-owner-proof.py --replay-only

$(BUILD)/arena-capacity-test: tests/arena-capacity-test.c tests/tailwind-owner-test.c tests/gas-lifecycle-test.c $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/arena-capacity-test.c $(BUILD)/libprcore.a -lm -o $@
.PHONY: arena-capacity-host-test
arena-capacity-host-test: $(BUILD)/arena-capacity-test
	python3 tools/check-arena-capacity-proof.py --replay-only

$(BUILD)/substitute-max-hp-test: tests/substitute-max-hp-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/substitute-max-hp-test.c $(BUILD)/libprcore.a -lm -o $@
.PHONY: substitute-max-hp-host-test
substitute-max-hp-host-test: $(BUILD)/substitute-max-hp-test
	python3 tools/check-substitute-max-hp-proof.py --replay-only

.PHONY: pending-max-hp-host-test
pending-max-hp-host-test: $(BUILD)/pending-heal-test
	python3 tools/check-pending-max-hp-proof.py --replay-only

$(BUILD)/max-hp-moves-test: tests/max-hp-moves-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/max-hp-moves-test.c $(BUILD)/libprcore.a -lm -o $@
.PHONY: max-hp-moves-host-test
max-hp-moves-host-test: $(BUILD)/max-hp-moves-test
	python3 tools/check-max-hp-moves-proof.py --replay-only

$(BUILD)/residual-max-hp-test: tests/residual-max-hp-test.c $(BUILD)/wide-substitute-frame.inc $(BUILD)/libprcore.a
	$(CC) $(CPPFLAGS) -I$(BUILD) $(CFLAGS) tests/residual-max-hp-test.c $(BUILD)/libprcore.a -lm -o $@
.PHONY: residual-max-hp-host-test
residual-max-hp-host-test: $(BUILD)/residual-max-hp-test
	python3 tools/check-residual-max-hp-proof.py --replay-only
