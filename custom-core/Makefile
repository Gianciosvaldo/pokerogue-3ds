# Host build and tests of custom-core. Portable C99, no dependency on the other folders except tests/layout_test.c, which
# reads battle-core's generated enum header (read-only) to guard the ID ceilings.
#
#   make test            unit + layout tests, the same under ASan/UBSan, and the mutation check
#   make unit-test       tests built with the project's usual flags
#   make san-test        the same tests under AddressSanitizer and UBSan
#   make mutation-test   every mutation in tools/mutations.py must make the unit tests fail
#   make report          sizeof of the in-memory contract and the encoded-size bounds
#   make arm-check       compile (no link, no run) for the 3DS CPU flags; needs arm-none-eabi-gcc, e.g. the devkitarm image
CC ?= gcc
CFLAGS ?= -O2 -g -std=c99 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -Iinclude
BUILD ?= build
BATTLE_GEN ?= ../battle-core/gen/out
SRCS := src/status.c src/history.c src/nemesis.c src/schedule.c src/ids.c src/meta.c src/reducer.c
OBJS := $(SRCS:%.c=$(BUILD)/%.o)
TESTS := $(BUILD)/model_test $(BUILD)/reducer_test $(BUILD)/layout_test
ARM_CC ?= arm-none-eabi-gcc
ARM_SIZE ?= arm-none-eabi-size
ARM_FLAGS ?= -march=armv6k -mtune=mpcore -mfpu=vfp -mfloat-abi=hard -mtp=soft -O2 -std=c99 -Wall -Wextra -Wpedantic -Werror

test: unit-test san-test mutation-test

unit-test: $(TESTS)
	$(BUILD)/model_test
	$(BUILD)/reducer_test
	$(BUILD)/layout_test $(BATTLE_GEN)/pr_enums.h

san-test:
	$(MAKE) --no-print-directory BUILD=$(BUILD)/san CFLAGS="-O1 -g -std=c99 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-sanitize-recover=undefined" unit-test

mutation-test:
	python3 tools/mutations.py

report: $(BUILD)/layout_test
	$(BUILD)/layout_test --report

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/libcc.a: $(OBJS)
	ar rcs $@ $^

$(BUILD)/model_test: tests/model_test.c tests/check.h $(BUILD)/libcc.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/model_test.c $(BUILD)/libcc.a -o $@

$(BUILD)/reducer_test: tests/reducer_test.c tests/check.h $(BUILD)/libcc.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/reducer_test.c $(BUILD)/libcc.a -o $@

$(BUILD)/layout_test: tests/layout_test.c tests/check.h $(BUILD)/libcc.a
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/layout_test.c $(BUILD)/libcc.a -o $@

# Compile-only: the tests cannot run on a bare-metal toolchain, but compiling them evaluates layout_test's static
# assertions (encoded maximum within 64 KiB, in-memory contract within 128 KiB) with the ARM type sizes.
arm-check:
	@mkdir -p $(BUILD)/arm
	for f in $(SRCS) tests/model_test.c tests/reducer_test.c tests/layout_test.c; do \
		$(ARM_CC) $(CPPFLAGS) $(ARM_FLAGS) -c $$f -o $(BUILD)/arm/$$(basename $$f .c).o || exit 1; \
	done
	$(ARM_SIZE) $(addprefix $(BUILD)/arm/,$(notdir $(SRCS:.c=.o)))

clean:
	rm -rf $(BUILD)

.PHONY: test unit-test san-test mutation-test report arm-check clean
-include $(OBJS:.o=.d)
