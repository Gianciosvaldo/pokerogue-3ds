/* The name a Pokemon is shown under: its species name, or for a fusion (DNA Splicers, Spliced Endless) the two names
 * blended the way upstream's getFusedSpeciesName does (battle-core/src/text.c pr_fused_species_name). Header-only so
 * every screen can use it over the app's own i18n_species_name. */
#ifndef FUSED_NAME_H
#define FUSED_NAME_H

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "pr/text.h"

static inline void fused_mon_name(char* out, size_t cap, int species, int fusion_species) {
  char a[96];
  snprintf(a, sizeof a, "%s", i18n_species_name(species));
  if (fusion_species > 0) {
    char b[96], fused[128];
    snprintf(b, sizeof b, "%s", i18n_species_name(fusion_species));
    if (pr_fused_species_name(a, b, fused, sizeof fused) > 0) {
      snprintf(out, cap, "%s", fused);
      return;
    }
  }
  snprintf(out, cap, "%s", a);
}

/* getNameToRender(): the nickname when the Pokemon has one (battle-core's pr_pokemon.nickname, plain UTF-8, "" = none), else its species name or the
 * fusion's blended name. A disguise hides the nickname: pass NULL for it. */
static inline void mon_display_name(char* out, size_t cap, int species, int fusion_species, const char* nickname) {
  if (nickname && nickname[0])
    snprintf(out, cap, "%s", nickname);
  else
    fused_mon_name(out, cap, species, fusion_species);
}

#endif
