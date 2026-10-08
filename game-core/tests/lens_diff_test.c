/* Additive Wide Lens inventory boundary using unchanged owning source oracle. */
#include "../src/internal.h"
static void inventory_lens_turn(pr_battle *, pr_engine *);
static int inventory_turns, mapped[2], virtual_cases;
#define pr_run_turn inventory_lens_turn
/* Additive contract guard; established turn comparator remains byte-for-byte unchanged. */
#include <stdarg.h>
#include <stdio.h>
static int guarded_printf(const char *format, ...);
static int message_cases, message_same, message_differ, message_unported;
static int reported_stats, reported_parse;
#define printf guarded_printf
#define main established_turn_main
#include "../../battle-core/tests/wide-lens-replay.inc"
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





#include "../../battle-core/tests/widelens-guards.inc"

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) {
        fprintf(stderr, "usage: TURNTEST_LOCALES=<upstream>/locales/en widelenstest <trace>\n");
        return 2;
    }
    if (widelens_guard_checks()) { fprintf(stderr, "Wide Lens guard checks failed\n"); return 1; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("trace"); return 2; }
    char line[LINE_LEN];
    int version, wanted, cases = 0, ends = 0, footer = 0;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# WIDELENS_CONTRACT %d %d", &version, &wanted) != 2
        || version != 1 || wanted <= 0) { fclose(f); return 2; }
    while (fgets(line, sizeof line, f)) {
        if (footer) { fclose(f); fprintf(stderr, "records after completion\n"); return 2; }
        if (!strncmp(line, "CASE ", 5)) {
            int id;
            if (sscanf(line, "CASE %d", &id) != 1 || id != cases || cases != ends) { fclose(f); return 2; }
            cases++;
        } else if (!strncmp(line, "END ", 4)) {
            if (cases != ends + 1) { fclose(f); return 2; }
            ends++;
        } else if (!strncmp(line, "# WIDELENS_COMPLETE ", 20)) {
            int count;
            if (sscanf(line, "# WIDELENS_COMPLETE %d", &count) != 1 || count != wanted) { fclose(f); return 2; }
            footer = 1;
        }
    }
    fclose(f);
    if (!footer || cases != wanted || ends != wanted) {
        fprintf(stderr, "incomplete Wide Lens trace: requested %d cases %d ends %d footer %d\n", wanted, cases, ends, footer);
        return 2;
    }
    int status = established_turn_main(argc, argv);
    if (status) return status;
    int covered = 0;
    for (int i = 0; i < n_ran_phase; i++) if (!strcmp(ran_phase[i].name, "TurnStartPhase")) covered = ran_phase[i].n;
    if (covered != wanted) { fprintf(stderr, "Wide Lens coverage %d/%d\n", covered, wanted); return 1; }
    if (message_cases != wanted || message_same != wanted || message_differ || message_unported || reported_parse || reported_stats) {
        fprintf(stderr, "Wide Lens message comparison incomplete or different\n"); return 1;
    }
    if(inventory_turns!=wanted||mapped[0]!=wanted||mapped[1]!=wanted||virtual_cases!=(wanted+4)/12)return 1;
    printf("Lens inventory: %d turns, mappings %d/%d, virtual cases %d, inventories unchanged\n",inventory_turns,mapped[0],mapped[1],virtual_cases);
    return 0;
}

static void inventory_lens_turn(pr_battle *bt,pr_engine *e)
{
 static gc_run run;memset(&run,0,sizeof run);run.bt=*bt;bool virtual=false;
 for(int side=0;side<2;side++) {
  if(bt->n_mods[side]!=1)exit(2);
  const pr_modifier *m=&bt->mods[side][0];
  if(m->cls!=PR_MOD_MOVE_ACCURACY_BOOSTER||m->num!=5||m->unknown_subclass||m->pokemon_id < -1||m->pokemon_id>UINT32_MAX
     ||m->stack_count<1||m->stack_count>3||m->virtual_stack_count<0||(int64_t)m->stack_count+m->virtual_stack_count>3)exit(2);
  gc_item *it=side?run.enemy_items:run.items;
  *it=(gc_item){.key="WIDE_LENS",.arg=-1,.stack=m->stack_count,.pokemon_id=m->pokemon_id==-1?0:(uint32_t)m->pokemon_id,.virtual_stack=m->virtual_stack_count};
  if(side)run.n_enemy_items=1;else run.n_items=1;
  mapped[side]++;virtual|=m->virtual_stack_count>0;
 }
 virtual_cases+=virtual;gc_item old[2]={run.items[0],run.enemy_items[0]};pr_rng rng=bt->rng;
 gc_items_to_battle(&run);if(memcmp(&rng,&run.bt.rng,sizeof rng))exit(1);
 memcpy(bt->mods,run.bt.mods,sizeof bt->mods);memcpy(bt->n_mods,run.bt.n_mods,sizeof bt->n_mods);pr_run_turn(bt,e);
 run.bt=*bt;gc_items_from_battle(&run);gc_items_to_battle(&run);
 if(run.n_items!=1||run.n_enemy_items!=1||strcmp(old[0].key,run.items[0].key)||strcmp(old[1].key,run.enemy_items[0].key)
    ||old[0].arg!=run.items[0].arg||old[1].arg!=run.enemy_items[0].arg||old[0].stack!=run.items[0].stack||old[1].stack!=run.enemy_items[0].stack
    ||old[0].pokemon_id!=run.items[0].pokemon_id||old[1].pokemon_id!=run.enemy_items[0].pokemon_id
    ||old[0].virtual_stack!=run.items[0].virtual_stack||old[1].virtual_stack!=run.enemy_items[0].virtual_stack){fprintf(stderr,"Lens inventory changed turn%d\n",inventory_turns);exit(1);}
 for(int side=0;side<2;side++) {
  const pr_modifier *a=&run.bt.mods[side][0],*b=&bt->mods[side][0];
  if(run.bt.n_mods[side]!=1||a->cls!=b->cls||a->pokemon_id!=b->pokemon_id||a->stack_count!=b->stack_count
     ||a->virtual_stack_count!=b->virtual_stack_count||a->num!=b->num||a->is_transferable!=b->is_transferable)exit(1);
 }
 inventory_turns++;
}
