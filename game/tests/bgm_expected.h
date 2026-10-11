/* What upstream's own Battle.getBgmOverride answered (generated: bgm_expected.c, by game/tools/dump-bgm.test.ts) */
#ifndef BGM_EXPECTED_H
#define BGM_EXPECTED_H
#include <stddef.h>
#include <stdint.h>

/* a wild battle with one enemy of `species` in `form`; NULL answers (no override) are not listed */
typedef struct {
  uint16_t species;
  uint8_t form, all_gens, end_biome;
  const char* expect;
} bgm_wild_case;

extern const char* const bgm_expected_wave[201]; /* every wave of a Classic run, a wild Magikarp in the Plains, all generations */
extern const bgm_wild_case bgm_expected_wild[];  /* wave 100, music preference x (Plains, End), sorted by the first four fields */
extern const int bgm_expected_wild_count;
extern const bgm_wild_case
    bgm_expected_final[]; /* wave 200 (the final boss): every species and form not listed gets battle_final_encounter */
extern const int bgm_expected_final_count;
/* by gm_trainer_bgms index, then [variant * 4 + all_gens * 2 + started] */
extern const char* const bgm_expected_trainer[][12];
extern const char* const bgm_expected_me_default;
#endif
