/*
 * run_text: the words the slot list and the result screens use for a run's summary (run_saves.h): game mode, biome, money,
 * numbers. Locale strings are upstream's (data.pak); where upstream has none the text is English.
 */
#ifndef GM_RUN_TEXT_H
#define GM_RUN_TEXT_H

#ifdef __cplusplus
extern "C" {
#endif

/* "Classic" (gameMode:classic...) */
const char* rt_mode_name(int mode, char* out, int n);
/* "Grassy Field" (biome:grass) */
const char* rt_biome_name(int biome, char* out, int n);
/* 1234567 -> "1,234,567" (upstream's default money format) */
const char* rt_number(long v, char* out, int n);
/* "₽1,234" (battleScene:moneyOwned) */
const char* rt_money(double money, char* out, int n);

#ifdef __cplusplus
}
#endif
#endif
