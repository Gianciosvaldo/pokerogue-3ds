/*
 * gacha_host: the capsule machine's purchase logic (task C09; docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.15). No drawing, no ui.
 *
 * It connects the three things that already exist and adds no rule of its own:
 *   - game-core's gc_system_gacha_pull (the source-owned machines, options, costs, capacity, pity, guarantees, eggs and display order),
 *     which only profile_tx runs;
 *   - profile_tx (C08): the one owner of profile transactions: one accepted pull = one committed transaction = one immutable result;
 *   - the capsule screen (C07): shows that immutable result.
 * There is no second wallet, no second reward generator and no copy of the profile here: the vouchers and eggs shown are sysd_get()'s.
 *
 * The option table is READ FROM the source, not written here: gh_rules runs the real gc_system_gacha_pull once per option on a scratch
 * profile and reads which voucher it debits, how many, how many eggs it adds, and where the capacity stops (the labels of the cursor are
 * never used). The same rules, in the same order the source applies them (capacity before cost), give the screen its "cannot afford /
 * too many eggs" rows without asking the source to refuse; the source still decides every real purchase, and a refusal it gives is
 * handled (nothing changes, nothing is shown).
 *
 * A purchase:  gh_buy  ->  [begin: the request is saved]  ->  [execute: the pull runs ONCE and commits]  ->  GH_BOUGHT + the result
 *              ->  the result is shown (gh_fixture)  ->  gh_reveal_done(txn)
 * It is refused (GH_BUSY) while another purchase is unfinished (profile_tx pending) or a committed pull has not been shown yet, so no
 * second purchase id can be created until the first one is done. Whatever interrupts it, the same transaction finishes with the same
 * result (gh_resume, gh_unrevealed): the eggs are shown once, and never pulled again.
 *
 * Time and entropy come from the app (gh_set_env) and are supplied once per request; they never change a result that was committed.
 */
#ifndef GM_GACHA_HOST_H
#define GM_GACHA_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "capsule_model.h"
#include "profile_tx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GH_MACHINES 3
#define GH_OPTIONS 5
#define GH_VOUCHER_TYPES 4

/* What the source says an option is. */
typedef struct {
  uint8_t voucher;   /* the voucher type it debits (GC_VOUCHER_*) */
  uint8_t cost;      /* how many of them */
  uint8_t pulls;     /* how many eggs it adds */
} gh_rule;

/* Reads the table from game-core (once; cached). False when the source did not behave as a table (then no purchase is offered). */
bool gh_rules(gh_rule out[GH_OPTIONS], int* egg_capacity);

typedef struct {
  gh_rule rule;
  uint32_t have;            /* the balance of the voucher it debits */
  bool can_buy;
  const char* refusal_key;  /* NULL when it can be bought; else the source's own key ("egg:tooManyEggs", "egg:notEnoughVouchers") */
} gh_option;

typedef struct {
  uint32_t vouchers[GH_VOUCHER_TYPES];
  int n_eggs;
  int capacity;
  gh_option opt[GH_OPTIONS];
  bool locked;              /* nothing can be bought now (see lock_text) */
  char lock_text[PTX_WHY_LEN];
} gh_view;

/* The state of the profile as a machine screen shows it. False (view zeroed) only when the rules could not be read. */
bool gh_view_of(gh_view* v);

typedef enum {
  GH_BOUGHT,      /* committed: out->result is the immutable result */
  GH_SETTLED,     /* gh_resume finished a transaction that is not a pull (nothing to show) */
  GH_NOTHING,     /* gh_resume: nothing was pending */
  GH_REFUSED,     /* the source (or the table in front of it) refused: nothing was written, nothing changed; key says why */
  GH_BUSY,        /* another purchase is unfinished, or a committed pull has not been shown: nothing was written */
  GH_RETRY_IO,    /* the SD card refused a write before the commit point: nothing was spent; gh_resume (or gh_buy again) is safe */
  GH_UNUSABLE,    /* the purchase record or the profile cannot be used (why says so): nothing was written */
  GH_FATAL,       /* the live profile could not be put back after a failed commit: no more purchases this session */
  GH_BAD,         /* the request does not fit (machine / option out of range) */
} gh_status;

typedef struct {
  gh_status status;
  char key[PTX_KEY_LEN];   /* GH_REFUSED: the refusal's source key */
  char why[PTX_WHY_LEN];   /* GH_UNUSABLE / GH_FATAL */
  bool apply_pending;      /* GH_BOUGHT: the result is durable, a file is not caught up yet (profile_tx finishes it) */
  ptx_result result;       /* GH_BOUGHT */
} gh_outcome;

/* One accepted pull. Never pulls twice for the same call, never starts while another purchase is unfinished. */
void gh_buy(int machine, int option, gh_outcome* out);
/* Finishes the transaction a restart or a failed write left unfinished (the same transaction, the same result). GH_NOTHING when none. */
void gh_resume(gh_outcome* out);
/* A committed pull that was not shown yet (the oldest), for the screen to show; false when none. */
bool gh_unrevealed(ptx_result* out);
/* The pull txn was shown: records it (one write) so a restart does not show it again. False when the card refused: it is then not shown
 * again in this session, and may be after a restart (never pulled again). */
bool gh_reveal_done(uint32_t txn);

/* The capsule screen's fixture for a result: the eggs in the order the source gave for display (not the storage order), each with its
 * tier and a label: the species that will hatch from it (the pull already decided it: gc_egg.species) when the app set a namer, else the
 * egg's id ("Egg 000102"). False for a result that is not a committed pull. */
bool gh_fixture(const ptx_result* r, cap_fixture* f);
/* The app's species names (i18n_species_name). NULL: eggs are labelled by id. The returned text is copied at once. */
typedef const char* (*gh_species_name_fn)(int species);
void gh_set_species_names(gh_species_name_fn fn);

/* Words for the player. */
const char* gh_machine_name(int machine);
const char* gh_machine_note(int machine);
const char* gh_voucher_name(int type);
/* The sentence for an outcome that is not GH_BOUGHT (empty for GH_NOTHING / GH_SETTLED). */
void gh_message(const gh_outcome* o, char* out, size_t n);
const char* gh_refusal_text(const char* key);

/* The clock and the entropy text come from the app. now_ms: Unix milliseconds (0 .. 8.64e15). seed_text: fills a fresh NUL-terminated text
 * of 1..PTX_SEED_MAX bytes (called once per request and once for the first scene seed). NULL members: time() and a counter. */
typedef struct {
  uint64_t (*now_ms)(void);
  void (*seed_text)(char out[PTX_SEED_MAX + 1]);
} gh_env;
void gh_set_env(const gh_env* env);

/* The app's clock (Unix ms) and the legendary machine's featured species at that time (game-core's gc_gacha_featured_species), for the
 * machine's display only: neither is part of a purchase's inputs (those are fixed at begin time). */
uint64_t gh_clock_ms(void);
uint16_t gh_featured_species(void);

/* ---- for tests ---- */
void gh_reset(void);   /* a restart: forget this session's state (the table read from the source is kept) */

#ifdef __cplusplus
}
#endif
#endif
