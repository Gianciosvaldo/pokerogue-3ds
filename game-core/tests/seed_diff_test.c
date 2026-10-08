/* Additive game inventory boundary. The owning battle producer/comparator
 * stay unchanged; rebuild inventory before the actual turn and reconcile afterward. */
#include "../src/internal.h"
static void inventory_seed_turn(pr_battle *, pr_engine *);
static int inventory_turns, seeded[2], consumed[2];
#define pr_run_turn inventory_seed_turn
/* Additive contract guard; established turn comparator remains byte-for-byte unchanged. */
#include <stdarg.h>
#include <stdio.h>
static int guarded_printf(const char *format, ...);
static int message_cases, message_same, message_differ, message_unported;
static int reported_stats, reported_parse;
#define printf guarded_printf
#define main established_turn_main
#include "../../battle-core/tests/turntest.c"
#undef pr_run_turn
#undef main
#undef printf

static int guarded_printf(const char *format, ...)
{
    char output[65536];
    va_list args; va_start(args, format);
    int n = vsnprintf(output, sizeof output, format, args);
    va_end(args);
    if (!strncmp(output, "texts of covered turns ", 23))
        sscanf(output, "texts of covered turns %d: identical %d, differ %d, a message with no text yet %d",
               &message_cases, &message_same, &message_differ, &message_unported);
    if (!strncmp(output, "cases ", 6)) {
        int total, skipped;
        sscanf(output, "cases %d (skipped %d: records not read yet), parse mismatches %d, stats mismatches %d",
               &total, &skipped, &reported_parse, &reported_stats);
    }
    fputs(output, stdout);
    return n;
}


#include "../../battle-core/tests/reviverseed-guards.inc"

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) {
        fprintf(stderr, "usage: TURNTEST_LOCALES=<upstream>/locales/en reviverseedtest <trace>\n");
        return 2;
    }
    if (reviver_guard_checks()) { fprintf(stderr, "Reviver Seed guard failure\n"); return 1; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("trace"); return 2; }
    char line[LINE_LEN];
    int version, wanted, cases = 0, ends = 0, footer = 0;
    int inputs = 0, ran = 0;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# REVIVER_CONTRACT %d %d", &version, &wanted) != 2
        || version != 1 || wanted <= 0) { fclose(f); return 2; }
    while (fgets(line, sizeof line, f)) {
        if (footer) { fclose(f); fprintf(stderr, "records after completion\n"); return 2; }
        if (!strncmp(line, "CASE ", 5)) {
            int id;
            if (sscanf(line, "CASE %d", &id) != 1 || id != cases || cases != ends) { fclose(f); return 2; }
            cases++; inputs = 0; ran = 0;
        } else if (!strncmp(line, "MOD ", 4) && !ran) {
            char cls[80]; int side, stack; long long pid;
            if (sscanf(line, "MOD side=%d cls=%79s pid=%lld stack=%d", &side, cls, &pid, &stack) != 4
                || side < 0 || side > 1 || pid < -1 || stack != 1) { fclose(f); return 2; }
            if (!strcmp(cls, "PokemonInstantReviveModifier")) inputs++;
            else if (strcmp(cls, "TurnHealModifier")) { fclose(f); return 2; }
        } else if (!strncmp(line, "RUN", 3)) {
            if (ran || inputs != 1) { fclose(f); return 2; }
            ran = 1;
        } else if (!strncmp(line, "END ", 4)) {
            if (!ran || cases != ends + 1) { fclose(f); return 2; }
            ends++;
        } else if (!strncmp(line, "# REVIVER_COMPLETE ", 19)) {
            int count;
            if (sscanf(line, "# REVIVER_COMPLETE %d", &count) != 1 || count != wanted) { fclose(f); return 2; }
            footer = 1;
        }
    }
    fclose(f);
    if (!footer || cases != wanted || ends != wanted) {
        fprintf(stderr, "incomplete Reviver Seed trace: requested %d cases %d ends %d footer %d\n", wanted, cases, ends, footer);
        return 2;
    }
    int status = established_turn_main(argc, argv);
    if (status) return status;
    int covered = 0;
    for (int i = 0; i < n_ran_phase; i++) if (!strcmp(ran_phase[i].name, "TurnStartPhase")) covered = ran_phase[i].n;
    if (covered != wanted) { fprintf(stderr, "Reviver Seed coverage %d/%d\n", covered, wanted); return 1; }
    if (message_cases != wanted || message_same != wanted || message_differ || message_unported || reported_parse || reported_stats) {
        fprintf(stderr, "Reviver Seed message comparison incomplete or different\n"); return 1;
    }
    if (inventory_turns != wanted || seeded[0] != wanted/2 || seeded[1] != wanted/2
        || consumed[0] != 7*wanted/20 || consumed[1] != 7*wanted/20) return 1;
    printf("Seed inventory: %d turns, seeded %d/%d consumed %d/%d; repeated sync stable\n",
           inventory_turns,seeded[0],seeded[1],consumed[0],consumed[1]);
    return 0;
}

static void inventory_seed_turn(pr_battle *bt, pr_engine *e)
{
 static gc_run run;
 memset(&run,0,sizeof run);run.bt=*bt;
 int old_seed[2]={0};
 for(int side=0;side<2;side++) {
  gc_item *items=side?run.enemy_items:run.items;int *n=side?&run.n_enemy_items:&run.n_items;
  for(int j=0;j<bt->n_mods[side];j++) {
   const pr_modifier *m=&bt->mods[side][j];
   if(m->unknown_subclass||m->virtual_stack_count||m->stack_count!=1||m->pokemon_id < -1||m->pokemon_id>UINT32_MAX)exit(2);
   const char *key=m->cls==PR_MOD_INSTANT_REVIVE?"REVIVER_SEED":m->cls==PR_MOD_TURN_HEAL?"LEFTOVERS":NULL;
   if(!key)exit(2);
   items[(*n)++]=(gc_item){key,-1,1,m->pokemon_id==-1?0:(uint32_t)m->pokemon_id};
   if(m->cls==PR_MOD_INSTANT_REVIVE){old_seed[side]++;seeded[side]++;}
  }
 }
 gc_items_to_battle(&run);memcpy(bt->mods,run.bt.mods,sizeof bt->mods);memcpy(bt->n_mods,run.bt.n_mods,sizeof bt->n_mods);
 pr_run_turn(bt,e);run.bt=*bt;gc_items_from_battle(&run);
 for(int side=0;side<2;side++) {
  gc_item *items=side?run.enemy_items:run.items;int n=side?run.n_enemy_items:run.n_items,remaining=0,expected=0;
  for(int j=0;j<n;j++)if(!strcmp(items[j].key,"REVIVER_SEED"))remaining+=items[j].stack;
  for(int j=0;j<bt->n_mods[side];j++)if(bt->mods[side][j].cls==PR_MOD_INSTANT_REVIVE)expected+=bt->mods[side][j].stack_count;
  if(remaining!=expected){fprintf(stderr,"inventory count turn%d side%d got%d expected%d tracked%d\n",inventory_turns,side,remaining,expected,gc_int(&run)->battle_item_count[side]);exit(1);}
  consumed[side]+=old_seed[side]-remaining;
 }
 gc_item before[GC_MAX_ITEMS],enemy[GC_MAX_ITEMS];int ni=run.n_items,ne=run.n_enemy_items;
 memcpy(before,run.items,sizeof before);memcpy(enemy,run.enemy_items,sizeof enemy);gc_items_from_battle(&run);
 if(ni!=run.n_items||ne!=run.n_enemy_items||memcmp(before,run.items,sizeof before)||memcmp(enemy,run.enemy_items,sizeof enemy)){fprintf(stderr,"repeated inventory sync turn%d\n",inventory_turns);exit(1);}
 gc_items_to_battle(&run);
 for(int side=0;side<2;side++) {
  if(run.bt.n_mods[side]!=bt->n_mods[side]){fprintf(stderr,"rebuild count turn%d side%d %d/%d\n",inventory_turns,side,run.bt.n_mods[side],bt->n_mods[side]);exit(1);}
  for(int j=0;j<bt->n_mods[side];j++) {
   const pr_modifier *a=&run.bt.mods[side][j],*b=&bt->mods[side][j];
   if(a->cls!=b->cls||a->pokemon_id!=b->pokemon_id||a->stack_count!=b->stack_count
      ||a->unknown_subclass!=b->unknown_subclass||a->is_transferable!=b->is_transferable){fprintf(stderr,"rebuild identity turn%d side%d\n",inventory_turns,side);exit(1);}
  }
 }
 inventory_turns++;
}
