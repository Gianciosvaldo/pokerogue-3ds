/* The words for a run's summary (see run_text.h). */
#include "run_text.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gm_enum_names.h"
#include "i18n.h"

const char* rt_mode_name(int mode, char* out, int n) {
  const char* key = "gameMode:unknown";
  switch (mode) {
    case 0: key = "gameMode:classic"; break;
    case 1: key = "gameMode:endless"; break;
    case 2: key = "gameMode:endlessSpliced"; break;
    case 3: key = "gameMode:dailyRun"; break;
    case 4: key = "gameMode:challenge"; break;
  }
  return i18n_t(out, n, key, NULL);
}

const char* rt_biome_name(int biome, char* out, int n) {
  char camel[48], key[64];
  gm_enum_camel(GM_E_BIOME_ID, biome, camel, sizeof camel); /* "" for an unknown biome */
  if (!camel[0]) return i18n_t(out, n, "biome:unknownLocation", NULL);
  snprintf(key, sizeof key, "biome:%s", camel);
  return i18n_t(out, n, key, NULL);
}

const char* rt_number(long v, char* out, int n) {
  char digits[32];
  snprintf(digits, sizeof digits, "%ld", v < 0 ? -v : v);
  const int len = (int)strlen(digits);
  int j = 0;
  if (v < 0 && j < n - 1) out[j++] = '-';
  for (int i = 0; i < len && j < n - 1; i++) {
    if (i && (len - i) % 3 == 0 && j < n - 1) out[j++] = ',';
    if (j < n - 1) out[j++] = digits[i];
  }
  out[j] = 0;
  return out;
}

const char* rt_money(double money, char* out, int n) {
  char num[32];
  rt_number((long)floor(money), num, sizeof num);
  return i18n_t(out, n, "battleScene:moneyOwned", "formattedMoney", num, NULL);
}
