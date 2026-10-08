/* Additive loaded-inventory bridge; owning source comparator remains unchanged. */
#include "../src/internal.h"
static void inventory_multi_turn(pr_battle *,pr_engine *);
static int inventory_turns, mapped[2], virtual_cases, enhanced[2], count_effects[2], damage_effects[2];
static bool in_inventory_turn;
void __real_pr_apply_multi_hit_count_modifiers(pr_battle*,const pr_pokemon*,int,int*);
void __wrap_pr_apply_multi_hit_count_modifiers(pr_battle *bt,const pr_pokemon *p,int move,int *count) {
 int old=count?*count:0;__real_pr_apply_multi_hit_count_modifiers(bt,p,move,count);
 if(in_inventory_turn&&p&&count&&*count!=old)count_effects[p->is_player?0:1]++;
}
void __real_pr_apply_multi_hit_damage_modifiers(pr_battle*,const pr_pokemon*,int,double*);
void __wrap_pr_apply_multi_hit_damage_modifiers(pr_battle *bt,const pr_pokemon *p,int move,double *mult) {
 double old=mult?*mult:0;__real_pr_apply_multi_hit_damage_modifiers(bt,p,move,mult);
 if(in_inventory_turn&&p&&mult&&*mult!=old)damage_effects[p->is_player?0:1]++;
}
#define pr_run_turn inventory_multi_turn
/* Additive contract guard; established turn comparator remains byte-for-byte unchanged. */
#include <stdarg.h>
#include <stdio.h>
static int guarded_printf(const char *format, ...);
static int message_cases, message_same, message_differ, message_unported;
static int reported_stats, reported_parse;
#define printf guarded_printf
#define main established_turn_main
#include "../../battle-core/tests/multi-lens-replay.inc"
#undef main
#undef pr_run_turn
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


#include "../../battle-core/tests/multilens-guards.inc"

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) {
        fprintf(stderr, "usage: TURNTEST_LOCALES=<upstream>/locales/en multilenstest <trace>\n");
        return 2;
    }
    if (multilens_guard_checks()) { fprintf(stderr, "Multi Lens guard checks failed\n"); return 1; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("trace"); return 2; }
    char line[LINE_LEN];
    int version, wanted, cases = 0, ends = 0, footer = 0, inputs = 0, ran = 0;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# MULTILENS_CONTRACT %d %d", &version, &wanted) != 2
        || version != 1 || wanted <= 0) { fclose(f); return 2; }
    while (fgets(line, sizeof line, f)) {
        if (footer) { fclose(f); fprintf(stderr, "records after completion\n"); return 2; }
        if (!strncmp(line, "CASE ", 5)) {
            int id;
            if (sscanf(line, "CASE %d", &id) != 1 || id != cases || cases != ends) { fclose(f); return 2; }
            cases++; inputs = 0; ran = 0;
        } else if (!strncmp(line, "MOD ", 4) && !ran) {
            char cls[80]; int side; long long pid;
            if (sscanf(line, "MOD side=%d cls=%79s pid=%lld", &side, cls, &pid) != 3
                || side < 0 || side > 1 || pid < -1 || strcmp(cls, "PokemonMultiHitModifier")) {
                fclose(f); return 2;
            }
            inputs++;
        } else if (!strncmp(line, "RUN", 3)) {
            if (ran || inputs != 1) { fclose(f); return 2; }
            ran = 1;
        } else if (!strncmp(line, "END ", 4)) {
            if (!ran || cases != ends + 1) { fclose(f); return 2; }
            ends++;
        } else if (!strncmp(line, "# MULTILENS_COMPLETE ", 21)) {
            int count;
            if (sscanf(line, "# MULTILENS_COMPLETE %d", &count) != 1 || count != wanted) { fclose(f); return 2; }
            footer = 1;
        }
    }
    fclose(f);
    if (!footer || cases != wanted || ends != wanted) {
        fprintf(stderr, "incomplete Multi Lens trace: requested %d cases %d ends %d footer %d\n", wanted, cases, ends, footer);
        return 2;
    }
    int status = established_turn_main(argc, argv);
    if (status) return status;
    int covered = 0;
    for (int i = 0; i < n_ran_phase; i++) if (!strcmp(ran_phase[i].name, "TurnStartPhase")) covered = ran_phase[i].n;
    if (covered != wanted) { fprintf(stderr, "Multi Lens coverage %d/%d\n", covered, wanted); return 1; }
    if (message_cases != wanted || message_same != wanted || message_differ || message_unported || reported_parse || reported_stats) {
        fprintf(stderr, "Multi Lens message comparison incomplete or different\n"); return 1;
    }
    if(inventory_turns!=wanted||!mapped[0]||!mapped[1]||!virtual_cases||!enhanced[0]||!enhanced[1]||!count_effects[0]||!count_effects[1]||!damage_effects[0]||!damage_effects[1])return 1;
    printf("Multi actual count gains %d/%d damage multipliers %d/%d\n",count_effects[0],count_effects[1],damage_effects[0],damage_effects[1]);
    printf("Multi inventory: %d turns, mapped %d/%d virtual%d observed hitCount>1 %d/%d\n",inventory_turns,mapped[0],mapped[1],virtual_cases,enhanced[0],enhanced[1]);
    return 0;
}

static void inventory_multi_turn(pr_battle *bt,pr_engine *e) {
 static gc_run run;memset(&run,0,sizeof run);run.bt=*bt;run.money=bt->money;bool virtual=false;
 pr_pokemon *active[2]={bt->field[0],bt->field[2]};
 for(int side=0;side<2;side++) {
  gc_item *items=side?run.enemy_items:run.items;int *n=side?&run.n_enemy_items:&run.n_items;
  for(int j=0;j<bt->n_mods[side];j++) {
   const pr_modifier *m=&bt->mods[side][j];
   if(m->cls!=PR_MOD_MULTI_HIT||m->unknown_subclass||m->pokemon_id < -1||m->pokemon_id>UINT32_MAX
      ||m->stack_count<1||m->virtual_stack_count<0||(int64_t)m->stack_count+m->virtual_stack_count>2)exit(2);
   items[(*n)++]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=m->stack_count,.pokemon_id=m->pokemon_id==-1?0:(uint32_t)m->pokemon_id,.virtual_stack=m->virtual_stack_count};
   mapped[side]++;virtual|=m->virtual_stack_count>0;
  }
 }
 virtual_cases+=virtual;pr_rng rng=bt->rng;gc_items_to_battle(&run);
 if(memcmp(&rng,&run.bt.rng,sizeof rng))exit(1);
 memcpy(bt->mods,run.bt.mods,sizeof bt->mods);memcpy(bt->n_mods,run.bt.n_mods,sizeof bt->n_mods);in_inventory_turn=true;pr_run_turn(bt,e);in_inventory_turn=false;
 for(int side=0;side<2;side++)if(active[side]&&active[side]->hit_count>1)enhanced[side]++;
 run.bt=*bt;gc_items_from_battle(&run);gc_items_from_battle(&run);gc_items_to_battle(&run);
 for(int side=0;side<2;side++) {
  if(run.bt.n_mods[side]!=bt->n_mods[side]){fprintf(stderr,"Multi rebuild count turn%d\n",inventory_turns);exit(1);}
  for(int j=0;j<bt->n_mods[side];j++){const pr_modifier *a=&run.bt.mods[side][j],*b=&bt->mods[side][j];
   if(a->cls!=b->cls||a->pokemon_id!=b->pokemon_id||a->stack_count!=b->stack_count||a->virtual_stack_count!=b->virtual_stack_count||a->is_transferable!=b->is_transferable){fprintf(stderr,"Multi rebuild fields turn%d\n",inventory_turns);exit(1);}}
 }
 inventory_turns++;
}
