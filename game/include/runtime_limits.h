#ifndef GAME_RUNTIME_LIMITS_H
#define GAME_RUNTIME_LIMITS_H

/* libctru's 32 KiB default is smaller than the nested battle-effect path.
 * Keep the real ARM startup allocation and guarded command regression aligned. */
#define POKEROGUE_MAIN_STACK_BYTES (512u * 1024u)

#endif
