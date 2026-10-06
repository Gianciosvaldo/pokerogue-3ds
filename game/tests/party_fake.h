/* party_fake.c: the real party screen over a battle's party, for runtest (test code only). */
#ifndef PARTY_FAKE_H
#define PARTY_FAKE_H

#include <stdbool.h>

#include "pr/battle.h"

/* Opens menus' party screen in RELEASE mode over bt's player party; on_pick is party_screen.c's: (slot, PO_RELEASE) once a member is
 * released, (6, PO_CANCEL) when the player backs out */
void pf_open_release(const pr_battle* bt, void (*on_pick)(int slot, int option, void* arg), void* arg);
void pf_close(void);
bool pf_active(void);
/* The player's next action on the open screen: release the member at `slot` (the options list, then Release), or with a negative slot
 * press B */
void pf_release(int slot);
/* Farewell messages ("Goodbye, X!" and its nine siblings) the party screen has shown so far */
int pf_farewells(void);

#endif
