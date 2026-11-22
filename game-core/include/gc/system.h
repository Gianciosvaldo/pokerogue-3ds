/* game-core: overall system data across runs (Dex entries, starter data, unlocks).
 * Follows upstream GameData (src/system/game-data.ts). */
#ifndef GC_SYSTEM_H
#define GC_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GC_MAX_SPECIES_ID 9000
#define GC_MAX_EGGS 100

/* Forward declarations */
typedef struct gc_run gc_run;
typedef struct gc_mon gc_mon;

/* Per-species Dex data entry (upstream DexEntry) */
typedef struct {
    uint64_t seen_attr;
    uint64_t caught_attr;
    uint32_t nature_attr;
    uint32_t seen_count;
    uint32_t caught_count;
    uint32_t hatched_count;
    uint8_t ivs[6];
    uint64_t ribbon_flags; /* RibbonData payload; independent of win counters */
} gc_dex_entry;

/* Per-starter data entry (upstream StarterDataEntry) */
typedef struct {
    uint32_t ability_attr;
    uint32_t passive_attr;
    uint32_t value_reduction;
    int32_t candy_count;
    uint16_t friendship;
    uint8_t egg_moves;
    uint16_t moveset[4];   /* 0 if default / empty */
    uint16_t classic_win_count;
    uint8_t preference_flags; /* 1 nature, 2 appearance, 4 ability, 8 form, 16 gender; absent in older saves */
    uint8_t nature_preference, appearance_preference, ability_preference;
    uint8_t form_preference;   /* StarterPreferences.formIndex, valid with flag 8 */
    uint8_t gender_preference; /* StarterPreferences.female (0 male, 1 female), valid with flag 16 */
} gc_starter_entry;

/* A starter's nickname, kept across runs (upstream StarterPreferences.nickname, set by the starter select's Rename and given to the Pokemon the
 * starter becomes: SelectStarterPhase's starterPokemon.nickname). Plain UTF-8, at most 20 UTF-16 units: 20 three-byte characters (60 bytes) is the
 * most such a text can take, hence 61 with the terminator (game-core checks it against PR_NICKNAME_MAX_CHARS). */
#define GC_STARTER_NICKNAME_BYTES 61
/* How many starters can carry a nickname at once. Upstream has no limit (572 starters here); the table is kept small because every gc_system is
 * copied whole in places and several live in the app (see gc_system_set_starter_nickname). */
#define GC_STARTER_NICKNAMES 128
typedef struct {
    uint16_t species;                           /* a starter's species id */
    char text[GC_STARTER_NICKNAME_BYTES];       /* NUL-terminated, never empty, zero-filled after the terminator */
} gc_starter_nickname;

/* Upstream Unlocks */
typedef struct {
    bool endless_mode;
    bool mini_black_hole;
    bool spliced_endless_mode;
    bool eviolite;
} gc_unlocks;

/* An Egg held by system data (upstream EggData / Egg) */
typedef struct {
    uint32_t id;
    int32_t hatch_waves;  /* waves remaining to hatch */
    uint8_t tier;         /* EggTier */
    uint16_t species;     /* species ID (0 = random) */
    uint8_t source_type,variant_tier,egg_move_index;
    bool is_shiny,override_hidden_ability;
    uint64_t timestamp_ms;
} gc_egg;

/* VoucherType enum values */
enum {
    GC_VOUCHER_REGULAR = 0,
    GC_VOUCHER_PLUS = 1,
    GC_VOUCHER_PREMIUM = 2,
    GC_VOUCHER_GOLDEN = 3,
};

/* Stat milestones tracked across the system profile */
typedef struct {
    uint32_t classic_wins;
    uint32_t ribbons_owned; /* first counted roots (GameStats.ribbonsOwned) */
    uint32_t eggs_pulled,rare_eggs_pulled,epic_eggs_pulled,legendary_eggs_pulled,manaphy_eggs_pulled;
    uint32_t endless_wins;
    uint32_t total_battles;
    uint32_t total_catches;
    uint32_t total_hatches;
    uint32_t total_shinies;
    uint32_t highest_wave;
    uint32_t highest_damage;
    uint64_t total_money_earned;
} gc_stat_milestones;

/* Upstream's GameStats (src/system/game-stats.ts, 35 counters) is split in two: gc_stat_milestones above (the ones the port had first, which keep their
 * names) and this, the rest. Which field is which upstream counter, as the Game Stats screen reads them (game/source/gamestats_screen.c):
 *   playTime = gstats.play_time (seconds)          battles = stats.total_battles          classicSessionsPlayed = gstats.classic_sessions_played
 *   sessionsWon = stats.classic_wins               ribbonsOwned = stats.ribbons_owned     dailyRunSessionsPlayed / Won = gstats.daily_run_sessions_*
 *   endlessSessionsPlayed = gstats.endless_sessions_played                                 highestEndlessWave = gstats.highest_endless_wave
 *   highestLevel / highestMoney / highestHeal = gstats.highest_*                           highestDamage = stats.highest_damage
 *   pokemonSeen / Defeated / Hatched = gstats.pokemon_*                                    pokemonCaught = stats.total_catches
 *   {subLegendary,legendary,mythical,shiny}Pokemon{Seen,Caught,Hatched} = gstats.<kind>_{seen,caught,hatched} (the caught ones count a catch's
 *   prevolution stages too, as upstream does)                                              pokemonFused = gstats.pokemon_fused
 *   trainersDefeated = gstats.trainers_defeated    eggsPulled and its four tiers = stats.*_eggs_pulled
 * Not the same thing: stats.total_hatches counts the eggs hatched (one each: the custom achievements' hatch counts read it), gstats.pokemon_hatched is
 * upstream's, which counts a hatched species' prevolution stages as well (a Pikachu egg is 2). stats.total_shinies, highest_wave, total_money_earned
 * and endless_wins are from before this and nothing writes them.
 * A section of its own in the save ("gstats"), written only once something is counted, so a profile with none saves the bytes it always saved and a
 * save from before it loads with zeros. Counters stop at UINT32_MAX. */
typedef struct {
    uint32_t play_time;
    uint32_t classic_sessions_played, daily_run_sessions_played, daily_run_sessions_won, endless_sessions_played;
    uint32_t highest_endless_wave, highest_level, highest_money, highest_heal;
    uint32_t pokemon_seen, pokemon_defeated, pokemon_hatched;
    uint32_t sub_legendary_seen, sub_legendary_caught, sub_legendary_hatched;
    uint32_t legendary_seen, legendary_caught, legendary_hatched;
    uint32_t mythical_seen, mythical_caught, mythical_hatched;
    uint32_t shiny_seen, shiny_caught, shiny_hatched;
    uint32_t pokemon_fused, trainers_defeated;
} gc_game_stats;

/* What the custom achievements count (gc_achv.c; no upstream equivalent). Only ever grows. */
typedef struct {
    uint32_t jojo_beaten;   /* bit i: the JoJo challenger i (the table's order in custom-core) has been beaten (i = 0..31) */
    uint32_t jojo_beaten_hi; /* bit i - 32: the same for the challengers 32..63 (a profile saved before they existed has none: 0) */
    uint32_t jojo_battles;  /* JoJo battles won */
    uint32_t jojo_clean;    /* ... of them with none of the party fainted */
    uint32_t run_tag;       /* the run run_jojo counts in (a hash of its seed) */
    uint8_t run_jojo;       /* challengers beaten in that run */
    uint8_t best_run_jojo;  /* the most in one run */
    uint32_t custom_wins;   /* Classic (Bizarre) runs won (called Custom runs before the mode was named; the saved field keeps its name) */
    uint32_t best_wave;     /* the highest wave any run reached */
    uint8_t feats;          /* GC_FEAT_* bits: won a run with that feat at least once */
    uint8_t hatch_feats;    /* GC_HATCH_* bits: hatched an Epic egg / a Legendary egg / a shiny from an egg */
    uint32_t fb_wins, fb_titles, fb_shutouts, fb_blowouts; /* football playoff matches won / titles won / wins conceding nothing / wins by 21 or more */
    uint32_t jojo_clean_mask, jojo_clean_mask_hi; /* bit i (i - 32 in the second word): challenger i was beaten with none of the party fainted (a profile saved before: 0) */
    uint32_t darby_wins;    /* D'Arby the Gambler beaten (a profile saved before: 0) */
    uint32_t darby_rematch_wins;   /* his rematch won (a profile saved before: 0) */
} gc_achv_progress;

/* Overall system data (upstream GameData) */
typedef struct {
    gc_dex_entry dex[GC_MAX_SPECIES_ID + 1];           /* 1-indexed by SpeciesId */
    gc_starter_entry starters[GC_MAX_SPECIES_ID + 1];  /* 1-indexed by SpeciesId (starters only) */
    gc_unlocks unlocks;
    gc_egg eggs[GC_MAX_EGGS];
    int n_eggs;
    uint32_t voucher_counts[4];                         /* by VoucherType */
    uint8_t voucher_unlocks[512];                       /* 1 if voucher for trainer_type is unlocked */
    uint8_t achievement_flags[256];                     /* 1 if achievement_id is unlocked */
    bool classic_voucher_unlocked; /* achievement voucher, separate from trainer unlocks */
    uint32_t egg_pity[4],unlock_pity[4];
    gc_stat_milestones stats;                          /* profile stat milestones */
    gc_game_stats gstats;                              /* the rest of upstream's GameStats (see gc_game_stats) */
    gc_achv_progress achv;                             /* custom achievements (gc_achv.c) */
    int n_starter_nicknames;                           /* the first n entries below are in use */
    gc_starter_nickname starter_nicknames[GC_STARTER_NICKNAMES];   /* sorted by species, one per species; the rest all zero (so a profile compares and saves the same however it got here) */
} gc_system;

/* Initialize system data with new-save defaults (upstream initDexData + initStarterData) */
void gc_system_init_defaults(gc_system *sys);

/* A starter's nickname: "" when it has none (also for a species that is not a starter or a NULL profile). The pointer stays good until the next
 * gc_system_set_starter_nickname. */
const char *gc_system_starter_nickname(const gc_system *sys, uint16_t species);
/* Sets, replaces or (with "" or NULL) clears a starter's nickname. 0 = done; otherwise nothing changed:
 *  - GC_STARTER_NICK_NOT_A_STARTER: no such starter (or no profile);
 *  - GC_STARTER_NICK_BAD_TEXT: `text` is not what gc_nickname_valid accepts (invalid UTF-8, a control character, more than 20 UTF-16 units);
 *  - GC_STARTER_NICK_FULL: a new nickname when GC_STARTER_NICKNAMES starters already have one (replacing or clearing one always works).
 * Memory: the table makes every gc_system 8,192 bytes bigger (760,248 to 768,440; the count fits in padding); a table with a slot for every starter
 * would be 40 KB, and the app holds a profile in four places at once (system_data.c, gc_classic.c, gc_gacha.c, gc_achv.c) plus a temporary copy in
 * gacha_host.c. */
enum { GC_STARTER_NICK_OK = 0, GC_STARTER_NICK_NOT_A_STARTER, GC_STARTER_NICK_BAD_TEXT, GC_STARTER_NICK_FULL };
int gc_system_set_starter_nickname(gc_system *sys, uint16_t species, const char *text);

/* Recording upstream's GameStats (gc_gamestats.c). Each takes the profile (NULL: nothing) and is called where the upstream line it names runs. The seen,
 * caught and hatched counters are written by gc_system_set_pokemon_seen / _caught_ex themselves (GameData.setPokemonSeen / setPokemonSpeciesCaught). */
/* SelectStarterPhase's classicSessionsPlayed / endlessSessionsPlayed (a Classic or Challenge run, else Endless); TitlePhase's dailyRunSessionsPlayed */
void gc_stats_session_started(gc_system *sys, int game_mode);
/* BattleEndPhase.start: battles++; in an Endless mode highestEndlessWave = wave + 1 when that is higher; a won trainer battle: trainersDefeated++ */
void gc_stats_battle_end(gc_system *sys, int game_mode, int wave, bool victory, bool trainer);
/* VictoryPhase.start: pokemonDefeated++ (the caller leaves a mystery encounter that prevents game stats updates out) */
void gc_stats_pokemon_defeated(gc_system *sys);
/* LevelUpPhase.start: highestLevel = the level when higher */
void gc_stats_level_up(gc_system *sys, int level);
/* PlayerPokemon.fuse: pokemonFused++ */
void gc_stats_pokemon_fused(gc_system *sys);
/* MoveEffectPhase's hit of a player's Pokemon: highestDamage; PokemonHealPhase healing one: highestHeal (battle-core's pr_game_stat_fn) */
void gc_stats_damage(gc_system *sys, int32_t damage);
void gc_stats_heal(gc_system *sys, int32_t amount);
/* GameData.loadSession: highestMoney = the saved run's money when higher */
void gc_stats_run_loaded(gc_system *sys, double money);
/* The scene's one-second timer (battle-scene.ts: gameStats.playTime++): the app adds the seconds it has run */
void gc_stats_add_play_time(gc_system *sys, uint32_t seconds);

/* Dex and starter data helpers */
uint64_t gc_mon_dex_attr(const gc_mon *m);
/* the dex attributes a species can ever unlock (the mask setPokemonSpeciesCaught ANDs a catch with) */
uint64_t gc_species_full_unlocks_data(uint16_t species_id);
/* PokemonForm.isStarterSelectable by form index (bit f = form f): the forms the starter select offers. Bit 0 only for a species with one form. */
uint64_t gc_species_starter_forms(uint16_t species_id);
int16_t gc_species_prevolution(uint16_t species_id);
uint16_t gc_species_root_id(uint16_t species_id);
/* PokemonSpecies.getRootSpeciesId(true): stops at the first starter on the way down */
uint16_t gc_species_root_for_starter(uint16_t species_id);
int getStarterValueFriendshipCap(int starter_cost);

void gc_system_set_pokemon_seen(gc_system *sys, const gc_mon *m, bool increment_count, bool trainer);
bool gc_system_set_pokemon_caught(gc_system *sys, const gc_mon *m, bool increment_count, bool from_egg);
/* gameData.setPokemonCaught(pokemon, incrementCount, fromEgg, showMessage) for a catch: `show_message` is
 * upstream's own argument (a new starter's "added as a starter" message, then its prevolution), `daily` the Daily
 * Run's candy rule. Every species the message is for goes in `report`, in the order it shows. */
#define GC_CAUGHT_STARTED_MAX 8
typedef struct {
    uint16_t started[GC_CAUGHT_STARTED_MAX];
    int n_started;
} gc_caught_report;
bool gc_system_set_pokemon_caught_ex(gc_system *sys, const gc_mon *m, bool increment_count, bool from_egg,
                                     bool show_message, bool daily, gc_caught_report *report);
void gc_system_update_species_dex_ivs(gc_system *sys, uint16_t species_id, const uint8_t ivs[6]);
/* GameData.unlockSpeciesNature: requires an unlocked root, then marks the
 * exact species and every prevolution, without caught counts or candy. */
void gc_system_unlock_species_nature(gc_system *sys, uint16_t species_id, uint8_t nature);
bool gc_system_add_starter_candy(gc_system *sys, uint16_t species_id, int32_t count);
void gc_system_add_friendship(gc_system *sys, const gc_run *run, gc_mon *m, int friendship, bool capped);
/* The starter's candy bar (its friendship toward the species' friendship cap, set by its starter cost): adds points and gives a candy for every full
 * bar, returning how many. gc_system_add_friendship feeds it the Pokemon's gain; the football mode feeds it the points its plays earn. */
int gc_system_add_starter_friendship(gc_system *sys, uint16_t starter_id, int points);

/* Egg and Voucher helpers */
int gc_system_increment_egg_steps(gc_system *sys);
bool gc_system_add_egg(gc_system *sys, uint32_t id, int32_t hatch_waves, uint8_t tier, uint16_t species);
bool gc_system_has_voucher_unlock(const gc_system *sys, int trainer_type);
bool gc_system_unlock_voucher(gc_system *sys, int trainer_type, int voucher_type);
void gc_system_add_voucher_count(gc_system *sys, int voucher_type, int count);

/* Stable local slots for the named Classic victory achievements. */
enum { GC_ACHV_CLASSIC_VICTORY=192, GC_ACHV_UNEVOLVED_CLASSIC,
 GC_ACHV_10_RIBBONS, GC_ACHV_25_RIBBONS, GC_ACHV_50_RIBBONS,
 GC_ACHV_75_RIBBONS, GC_ACHV_100_RIBBONS };
#define GC_RIBBON_CLASSIC UINT64_C(0x08000000)
/* Achievements, Ribbons, and Stat Milestones helpers */
bool gc_system_has_achievement(const gc_system *sys, int achievement_id);
bool gc_system_unlock_achievement(gc_system *sys, int achievement_id);
void gc_system_add_classic_win(gc_system *sys, uint16_t species_id);
bool gc_system_repair_unlocks(gc_system *sys);   /* Endless + Mini Black Hole from a recorded Classic win (older-build profiles); true if it changed anything */
/* The port's own Classic (Bizarre) mode (JoJo trainers and the Pillar Men on top of Classic): open once a Classic run has been won, as Endless
 * is. It reads the recorded win (or the Endless unlock) and keeps no flag of its own, so no profile needs repairing and the save format is
 * untouched. */
bool gc_system_bizarre_unlocked(const gc_system *sys);
uint16_t gc_system_get_ribbons(const gc_system *sys, uint16_t species_id);

/* SD Card System Data Serializer */
/* Direct hidden-ability purchases are a user-requested extension at the same
 * base-species candy cost as the pinned native passive unlock. */
int gc_system_starter_unlock_cost(uint16_t species);
int gc_system_starter_reduction_cost(const gc_system *sys,uint16_t species);
bool gc_system_buy_starter_reduction(gc_system *sys,uint16_t species);
bool gc_system_buy_starter_unlock(gc_system *sys, uint16_t species, bool hidden_ability);
size_t gc_system_save(const gc_system *sys, void *buf, size_t cap);
bool gc_system_load(gc_system *sys, const void *buf, size_t len, const char **why);

#endif
