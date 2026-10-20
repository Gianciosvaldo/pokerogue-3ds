/* Additive inventory/global-wallet boundary; owning source oracle unchanged. */
#include "../src/internal.h"
static void inventory_golden_turn(pr_battle *,pr_engine *);
static int inventory_turns, gains, mapped_golden[2], mapped_coins[2];
#define pr_run_turn inventory_golden_turn
/* Additive contract guard; established turn comparator remains byte-for-byte unchanged. */
#include <stdarg.h>
#include <stdio.h>
static int guarded_printf(const char *format, ...);
static int message_cases, message_same, message_differ, message_unported;
static int reported_stats, reported_parse;
#define printf guarded_printf
#define main established_turn_main
#include "../../battle-core/tests/golden-punch-replay.inc"
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


#include "../../battle-core/tests/golden-guards.inc"

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) {
        fprintf(stderr, "usage: TURNTEST_LOCALES=<upstream>/locales/en goldentest <trace>\n");
        return 2;
    }
    if (golden_guard_checks()) { fprintf(stderr, "Golden Punch guard checks failed\n"); return 1; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("trace"); return 2; }
    char line[LINE_LEN];
    int version, wanted, cases = 0, ends = 0, footer = 0, inputs = 0, ran = 0, wallets = 0;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# GOLDEN_CONTRACT %d %d", &version, &wanted) != 2
        || version != 1 || wanted <= 0) { fclose(f); return 2; }
    while (fgets(line, sizeof line, f)) {
        if (footer) { fclose(f); fprintf(stderr, "records after completion\n"); return 2; }
        if (!strncmp(line, "CASE ", 5)) {
            int id;
            if (sscanf(line, "CASE %d", &id) != 1 || id != cases || cases != ends) { fclose(f); return 2; }
            cases++; inputs = 0; ran = 0; wallets = 0;
        } else if (!strncmp(line, "MOD ", 4) && !ran) {
            char cls[80]; int side; long long pid;
            if (sscanf(line, "MOD side=%d cls=%79s pid=%lld", &side, cls, &pid) != 3
                || side < 0 || side > 1 || pid < -1
                || (strcmp(cls, "DamageMoneyRewardModifier") && strcmp(cls, "MoneyMultiplierModifier"))) {
                fclose(f); return 2;
            }
            if (!strcmp(cls, "DamageMoneyRewardModifier")) inputs++;
        } else if (!strncmp(line, "WALLET ", 7)) {
            if ((!ran && wallets) || (ran && wallets != 1)) { fclose(f); return 2; }
            (void)golden_wallet(line);
            wallets++;
        } else if (!strncmp(line, "RUN", 3)) {
            if (ran || inputs != 1 || wallets != 1) { fclose(f); return 2; }
            ran = 1;
        } else if (!strncmp(line, "END ", 4)) {
            if (!ran || wallets != 2 || cases != ends + 1) { fclose(f); return 2; }
            ends++;
        } else if (!strncmp(line, "# GOLDEN_COMPLETE ", 18)) {
            int count;
            if (sscanf(line, "# GOLDEN_COMPLETE %d", &count) != 1 || count != wanted) { fclose(f); return 2; }
            footer = 1;
        }
    }
    fclose(f);
    if (!footer || cases != wanted || ends != wanted) {
        fprintf(stderr, "incomplete Golden Punch trace: requested %d cases %d ends %d footer %d\n", wanted, cases, ends, footer);
        return 2;
    }
    int status = established_turn_main(argc, argv);
    if (status) return status;
    int covered = 0;
    for (int i = 0; i < n_ran_phase; i++) if (!strcmp(ran_phase[i].name, "TurnStartPhase")) covered = ran_phase[i].n;
    if (covered != wanted) { fprintf(stderr, "Golden Punch coverage %d/%d\n", covered, wanted); return 1; }
    if (message_cases != wanted || message_same != wanted || message_differ || message_unported || reported_parse || reported_stats) {
        fprintf(stderr, "Golden Punch message comparison incomplete or different\n"); return 1;
    }
    if(inventory_turns!=wanted||!gains||!mapped_golden[0]||!mapped_golden[1]||!mapped_coins[0]||!mapped_coins[1])return 1;
    printf("Golden inventory: %d turns, wallet gains %d, Golden %d/%d Coins %d/%d\n",inventory_turns,gains,mapped_golden[0],mapped_golden[1],mapped_coins[0],mapped_coins[1]);
    return 0;
}

static void inventory_golden_turn(pr_battle *bt,pr_engine *e)
{
 static gc_run run;memset(&run,0,sizeof run);run.bt=*bt;run.money=bt->money;double before=run.money;double payday=bt->money_scattered;
 for(int side=0;side<2;side++) {
  gc_item *items=side?run.enemy_items:run.items;int *n=side?&run.n_enemy_items:&run.n_items;
  for(int j=0;j<bt->n_mods[side];j++){
   const pr_modifier *m=&bt->mods[side][j];bool coin=m->cls==PR_MOD_MONEY_MULTIPLIER;
   if((!coin&&m->cls!=PR_MOD_DAMAGE_MONEY_REWARD)||m->unknown_subclass||m->pokemon_id < -1||m->pokemon_id>UINT32_MAX
      ||m->stack_count<1||m->virtual_stack_count<0||(int64_t)m->stack_count+m->virtual_stack_count>5||(coin&&m->pokemon_id!=0))exit(2);
   items[(*n)++]=(gc_item){.key=coin?"AMULET_COIN":"GOLDEN_PUNCH",.arg=-1,.stack=m->stack_count,.pokemon_id=coin||m->pokemon_id==-1?0:(uint32_t)m->pokemon_id,.virtual_stack=m->virtual_stack_count};
   (coin?mapped_coins:mapped_golden)[side]++;
  }
 }
 gc_items_to_battle(&run);gc_wallet_to_battle(&run);memcpy(bt->mods,run.bt.mods,sizeof bt->mods);memcpy(bt->n_mods,run.bt.n_mods,sizeof bt->n_mods);bt->money=run.bt.money;
 pr_run_turn(bt,e);run.bt=*bt;gc_wallet_from_battle(&run);gc_items_from_battle(&run);gc_items_to_battle(&run);
 if(run.money!=bt->money||bt->money_scattered!=payday){fprintf(stderr,"wallet/payday turn%d wallet%.17g/%.17g payday%.17g/%.17g\n",inventory_turns,run.money,bt->money,bt->money_scattered,payday);exit(1);}
 gains+=run.money>before;
 for(int side=0;side<2;side++) {
  if(run.bt.n_mods[side]!=bt->n_mods[side]){fprintf(stderr,"Golden rebuild count turn%d side%d %d/%d\n",inventory_turns,side,run.bt.n_mods[side],bt->n_mods[side]);exit(1);}
  for(int j=0;j<bt->n_mods[side];j++){const pr_modifier *a=&run.bt.mods[side][j],*b=&bt->mods[side][j];
   if(a->cls!=b->cls||a->pokemon_id!=b->pokemon_id||a->stack_count!=b->stack_count||a->virtual_stack_count!=b->virtual_stack_count||a->is_transferable!=b->is_transferable){fprintf(stderr,"Golden rebuild fields turn%d side%d cls%d/%d pid%lld/%lld virtual%d/%d transfer%d/%d\n",inventory_turns,side,a->cls,b->cls,(long long)a->pokemon_id,(long long)b->pokemon_id,a->virtual_stack_count,b->virtual_stack_count,a->is_transferable,b->is_transferable);exit(1);}}
 }
 inventory_turns++;
}
