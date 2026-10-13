/* custom-core, built as one translation unit.
 *
 * game/Makefile names every object by its file name only, and custom-core/src/history.c has the same name as battle-core/src/history.c,
 * so the folder cannot simply be added to SOURCES. custom-core is read-only here (it has its own build, tests and CI), so this file
 * includes its sources instead of renaming them. The host tests build it the same way. */
#include "../../custom-core/src/status.c"
#include "../../custom-core/src/history.c"
#include "../../custom-core/src/nemesis.c"
#include "../../custom-core/src/schedule.c"
#include "../../custom-core/src/ids.c"
#include "../../custom-core/src/meta.c"
#include "../../custom-core/src/reducer.c"
#include "../../custom-core/src/codec.c"
#include "../../custom-core/src/commit.c"
#include "../../custom-core/src/scheduler.c"
