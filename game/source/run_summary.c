/* Drawing a run's summary (see run_summary.h). */
#include "run_summary.h"

#include <stdio.h>

#include "i18n.h"
#include "party_icon.h"
#include "run_text.h"
#include "ui_text.h"

void rsum_draw(const rsv_info* in, float x, float y) {
  if (in->status != RSV_OK) return;
  char a[96], b[96], buf[200], lvl[48];
  rt_mode_name(in->mode, a, sizeof a);
  i18n_t(lvl, sizeof lvl, "saveSlotSelectUiHandler:wave", NULL);
  snprintf(buf, sizeof buf, "%s - %s %d%s", a, lvl, in->wave, in->from_backup ? " (backup)" : "");
  text_draw(TS_WINDOW, x, y, buf, NULL);
  text_draw(TS_WINDOW, x, y + 18, rt_biome_name(in->biome, a, sizeof a), NULL);
  i18n_t(lvl, sizeof lvl, "menu:score", NULL);
  snprintf(buf, sizeof buf, "%s %s", lvl, rt_number(in->score, b, sizeof b));
  text_draw(TS_WINDOW, x, y + 36, buf, NULL);
  text_draw(TS_MONEY, x, y + 54, rt_money(in->money, a, sizeof a), NULL);
  i18n_t(lvl, sizeof lvl, "saveSlotSelectUiHandler:lv", NULL);
  for (int i = 0; i < in->n_party; i++) {
    const float cx = x + 14 + 48 * i;
    char lv[64];
    pi_draw(&in->party[i], cx, y + 82, 1.0f);
    snprintf(lv, sizeof lv, "%s%d", lvl, in->party[i].level);
    text_opts o = text_defaults();
    o.origin_x = 0.5f;
    o.font_px = 54;
    o.color = in->party[i].fainted ? RR_RGBA(0xf8, 0x58, 0x58, 255) : RR_RGBA(0xf8, 0xf8, 0xf8, 255);
    o.shadow = RR_RGBA(0x42, 0x42, 0x42, 255);
    text_draw(TS_PARTY, cx, y + 118, lv, &o);
  }
}
