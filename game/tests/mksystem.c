/*
 * mksystem: writes the system data file (save/system.sav) with chosen voucher balances and egg count into ./sdmc:/3ds/pokerogue/save/, so
 * the Capsule Machine (task C09) can be looked at on a real build (Azahar or a 3DS) without playing runs for vouchers.
 *
 *   make mksystem && (cd some/folder && /path/to/build/mksystem <regular> <plus> <premium> <golden> [eggs])
 *   then copy the `sdmc:` folder's 3ds/pokerogue/save/system.sav into the SD card's 3ds/pokerogue/save/ and pick Capsule Machine.
 *
 * The file is a new-install profile (gc_system_init_defaults: the starters a new save has) with the balances set and `eggs` placeholder
 * eggs (default 0) in storage, written with game-core's own serializer and the app's save.c; the tool loads it again and fails when it does
 * not come back identical. Nothing here is part of the app.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gc/system.h"
#include "save.h"

static gc_system S, back;
static uint8_t buf[1024 * 1024];

int main(int argc, char** argv) {
  if (argc < 5 || argc > 6) {
    fprintf(stderr, "usage: mksystem <regular> <plus> <premium> <golden> [eggs]\n");
    return 2;
  }
  mkdir("sdmc:", 0777);
  if (!save_init()) {
    fprintf(stderr, "cannot make the save folder\n");
    return 1;
  }
  gc_system_init_defaults(&S);
  for (int i = 0; i < 4; i++) S.voucher_counts[i] = (uint32_t)strtoul(argv[1 + i], NULL, 10);
  const int eggs = argc == 6 ? atoi(argv[5]) : 0;
  for (int i = 0; i < eggs && i < 99; i++) gc_system_add_egg(&S, 5000u + (uint32_t)i, 10, (uint8_t)(i % 3), (uint16_t)(1 + i % 100));
  const size_t n = gc_system_save(&S, buf, sizeof buf);
  const char* why = NULL;
  if (!n || !gc_system_load(&back, buf, n, &why) || back.n_eggs != S.n_eggs || memcmp(back.voucher_counts, S.voucher_counts, sizeof S.voucher_counts)) {
    fprintf(stderr, "the profile does not come back identical (%s)\n", why ? why : "?");
    return 1;
  }
  if (save_write("system", buf, n) != SAVE_OK) {
    fprintf(stderr, "cannot write system.sav\n");
    return 1;
  }
  printf("system.sav: %zu bytes, vouchers %u/%u/%u/%u, %d eggs\n", n, S.voucher_counts[0], S.voucher_counts[1], S.voucher_counts[2], S.voucher_counts[3], S.n_eggs);
  return 0;
}
