/* Names and small lookups the menus need from battle-core's generated tables
 * (read-only). English only, like the rest of the beta. */
#include <stddef.h>

#include "menus.h"
#include "pr_data.h"

const char* menu_move_name(uint16_t id) {
  if (id < PR_MOVE_COUNT && pr_moves[id].id == id) return pr_moves[id].name;
  return "???";
}
