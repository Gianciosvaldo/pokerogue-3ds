/* Names and small lookups the menus need from battle-core's generated tables
 * (read-only). English only, like the rest of the beta. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "menus.h"
#include "pr_data.h"

const pr_species* menu_species(uint16_t id) {
  int lo = 0, hi = PR_SPECIES_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (pr_species_table[mid].id == id) return &pr_species_table[mid];
    if (pr_species_table[mid].id < id)
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  return NULL;
}

/* formKey of the form ("" for none) */
const char* menu_form_key(uint16_t species, int form) {
  const pr_species* sp = menu_species(species);
  if (!sp || sp->nforms <= 1) return "";
  if (form >= sp->nforms) form = sp->nforms - 1;
  const char* k = pr_forms[sp->form0 + form].form_key;
  return k ? k : "";
}

/* PokemonSpecies.getCryKey */
void menu_cry_key(const menu_mon* m, char* out, size_t n) {
  static const char* const own_cry[] = {
      "mega", "mega-x", "mega-y", "mega-z", "primal", "gigantamax", "gigantamax-single", "gigantamax-rapid",
      "eternamax", "white", "black", "therian", "sky", "gorging", "gulping", "lowkey", "no-ice", "hangry",
      "crowned", "rapid-strike", "four", "droopy", "stretchy", "hero", "roaming", "complete", "10-complete", "10",
      "10-pc", "super", "unbound", "pau", "pompom", "sensu", "dusk", "midnight", "school", "dawn-wings",
      "dusk-mane", "ultra"};
  unsigned id = m->species;
  /* regional forms (2000+) share the base cry, except Galarian Slowpoke,
   * Eternal Floette and Bloodmoon Ursaluna */
  if (id >= 2000 && id != 4079 && id != 2670 && id != 8901) id %= 2000;
  const char* fk = menu_form_key((uint16_t)id, m->form);
  const char* suffix = "";
  for (size_t i = 0; i < sizeof own_cry / sizeof own_cry[0]; i++)
    if (!strcmp(fk, own_cry[i])) suffix = fk;
  if (!strcmp(fk, "mega-original") || !strcmp(fk, "mega-curly") || !strcmp(fk, "mega-droopy") ||
      !strcmp(fk, "mega-stretchy"))
    suffix = "mega";
  if ((id == 876 || id == 916) && !strcmp(fk, "female")) suffix = fk; /* Indeedee, Oinkologne */
  if (id == 898 && (!strcmp(fk, "ice") || !strcmp(fk, "shadow"))) suffix = fk; /* Calyrex */
  snprintf(out, n, "cry/%u%s%s", id, *suffix ? "-" : "", suffix);
}
