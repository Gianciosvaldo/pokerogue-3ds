/* play_time_test: the play-time counter of Game Stats (system_data.c: sysd_play_tick, sysd_play_seconds, and the fold into the profile at
 * sysd_save). The profile transaction (profile_tx.c) hashes the profile at begin and at execute, so play time must not change gc_system
 * between saves; it is folded in only inside sysd_save, after the gate.
 *
 *   make play_time_test && build/play_time_test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "save.h"
#include "system_data.h"

static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "play_time_test line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static char* card;
static size_t card_len;
static int writes;
static bool allow = true;
static save_result io_read(const char* name, char** out, size_t* len, bool* used_backup) {
  (void)name;
  if (used_backup) *used_backup = false;
  if (!card) return SAVE_NOT_FOUND;
  *out = malloc(card_len + 1);
  memcpy(*out, card, card_len);
  (*out)[card_len] = 0;
  *len = card_len;
  return SAVE_OK;
}
static save_result io_write(const char* name, const void* data, size_t len) {
  (void)name;
  free(card);
  card = malloc(len + 1);
  memcpy(card, data, len);
  card_len = len;
  writes++;
  return SAVE_OK;
}
static bool gate(void) { return allow; }

int main(void) {
  const sysd_io io = {io_read, io_write};
  sysd_set_io(&io);
  sysd_set_gate(gate);
  sysd_reset();

  /* before the profile is read there is nothing to add to */
  sysd_play_tick(4000);
  CHECK(sysd_play_seconds() == 0);
  gc_system* s = sysd_get();
  CHECK(s->gstats.play_time == 0 && sysd_play_seconds() == 0);

  /* ticks accumulate outside the profile: gc_system does not change between saves */
  for (int i = 0; i < 10; i++) sysd_play_tick(100);
  CHECK(s->gstats.play_time == 0);
  CHECK(sysd_play_seconds() == 1);   /* 1000 ms */
  sysd_play_tick(2500);
  CHECK(sysd_play_seconds() == 3);   /* 3500 ms */
  CHECK(s->gstats.play_time == 0);

  /* a long step (sleep mode, a long SD load) counts for at most SYSD_PLAY_TICK_MAX_MS */
  sysd_play_tick(10 * 60 * 1000u);
  CHECK(sysd_play_seconds() == 3 + SYSD_PLAY_TICK_MAX_MS / 1000 + 0 /* 3500 + 5000 = 8500 ms */ );

  /* a save folds the whole seconds in and keeps the remainder: 8500 ms -> 8 s + 500 ms */
  CHECK(sysd_save());
  CHECK(s->gstats.play_time == 8 && sysd_play_seconds() == 8 && writes == 1);
  sysd_play_tick(500);
  CHECK(sysd_play_seconds() == 9);   /* the 500 ms kept + 500 ms */

  /* a refused gate: nothing is folded and nothing is written */
  allow = false;
  sysd_play_tick(3000);
  CHECK(!sysd_save());
  CHECK(s->gstats.play_time == 8 && writes == 1);
  allow = true;

  /* with less than a second since the last save the bytes are unchanged and nothing is written again */
  CHECK(sysd_save());   /* folds 3 s + 1 s... */
  CHECK(s->gstats.play_time == 12);
  const int w = writes;
  CHECK(sysd_save() && writes == w);   /* no new whole second: the same bytes */

  /* it comes back from the card */
  sysd_reset();
  s = sysd_get();
  CHECK(s->gstats.play_time == 12 && sysd_play_seconds() == 12);

  /* the counter saturates instead of wrapping */
  s->gstats.play_time = 0xFFFFFFFFu - 1;
  for (int i = 0; i < 5; i++) sysd_play_tick(SYSD_PLAY_TICK_MAX_MS);
  CHECK(sysd_play_seconds() == 0xFFFFFFFFu);
  CHECK(sysd_save() && s->gstats.play_time == 0xFFFFFFFFu);

  printf("play_time_test: %d checks passed\n", checks);
  return 0;
}
