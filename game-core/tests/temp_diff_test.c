/* Independent strict replay of the additive real-class temporary-booster oracle. */
#include "../src/internal.h"
#include "temp-replay-bridge.inc"
#include "pr/battle.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *input;
static int line_no, guards;
static char line[4096];
static void die(const char *s) { fprintf(stderr, "invalid trace line %d: %s\n", line_no, s); exit(2); }
static char *record(char kind)
{
    if (!fgets(line, sizeof line, input)) die("unexpected EOF");
    line_no++;
    if (!strchr(line, '\n') || line[0] != kind || (line[1] != ' ' && line[1] != '\n')) die("record order/length");
    return line + 1;
}
static double number(char **s)
{
    char *end; double n = strtod(*s, &end);
    if (end == *s || !isfinite(n)) die("number");
    *s = end; return n;
}
static int integer(char **s, int lo, int hi)
{
    double n = number(s); if (floor(n) != n || n < lo || n > hi) die("integer range"); return n;
}
static void end(char *s) { while (isspace((unsigned char)*s)) s++; if (*s) die("extra fields"); }
static int item_id(char **s)
{
    char name[64]; int used;
    if (sscanf(*s, " %63s%n", name, &used) != 1) die("item name");
    *s += used;
    if (!strcmp(name, "TEMP_STAT_STAGE_BOOSTER")) return PR_ITEM_TEMP_STAT_STAGE_BOOSTER;
    if (!strcmp(name, "DIRE_HIT")) return PR_ITEM_DIRE_HIT;
    if (!strcmp(name, "LURE")) return PR_ITEM_LURE;
    if (!strcmp(name, "HEALING_CHARM")) return PR_ITEM_HEALING_CHARM;
    if (!strcmp(name, "LEFTOVERS")) return PR_ITEM_LEFTOVERS;
    die("item name"); return 0;
}
static int family(int item, int arg) { return item == PR_ITEM_TEMP_STAT_STAGE_BOOSTER ? arg - 1 : item == PR_ITEM_DIRE_HIT ? 6 : -1; }
static pr_owned_item read_item(void)
{
    char *s = record('H'); pr_owned_item it = {0}; it.item = item_id(&s);
    it.stack = integer(&s, 1, 2); it.owner = integer(&s, -1, 0); it.arg = integer(&s, 0, 6);
    it.battle_count = integer(&s, 0, 10); it.active = integer(&s, 0, 0); end(s);
    if (it.item == PR_ITEM_TEMP_STAT_STAGE_BOOSTER && !it.arg) die("stat arg");
    if (it.item != PR_ITEM_TEMP_STAT_STAGE_BOOSTER && it.arg) die("unexpected arg");
    return it;
}
static bool same(const pr_owned_item *a, const pr_owned_item *b)
{
    return a->item == b->item && a->stack == b->stack && a->owner == b->owner && a->arg == b->arg
        && a->battle_count == b->battle_count && a->active == b->active;
}
static int inventory(pr_reward_ctx *c, pr_reward_ctx *oracle, bool fill)
{
    char *s = record('I'); int n = integer(&s, 0, PR_REWARD_MAX_OWNED); end(s);
    int bad = !fill && n != c->n_items;
    oracle->n_items = n;
    for (int j = 0; j < n; j++) { pr_owned_item it = read_item(); oracle->items[j] = it;
        if (fill) c->items[j] = it;
        else if (j >= c->n_items || !same(&it, &c->items[j])) bad = 1;
    }
    if (fill) c->n_items = n;
    return bad;
}
static int effects(pr_reward_ctx *c, int base, char **s)
{
    /* Existing battle-core owns the raw effects; only inventory is newly ported. */
    gc_items_to_battle(&bridge_run); pr_battle bt = bridge_run.bt;
    int bad = 0;
    for (int stat = 0; stat < 8; stat++) { double v = base; pr_apply_temp_stat_stage_booster_modifiers(&bt, true, stat, &v); bad |= number(s) != v; }
    int crit = base; pr_apply_temp_crit_booster_modifiers(&bt, true, &crit); bad |= number(s) != crit;
    end(*s); return bad;
}
static void untouched(pr_reward_ctx before, pr_reward_ctx after)
{
    memset(before.items, 0, sizeof before.items); memset(after.items, 0, sizeof after.items);
    before.n_items = after.n_items = 0;
    if (memcmp(&before, &after, sizeof before)) die("non-inventory mutation");
}
static void refusal(pr_reward_ctx c)
{
    pr_reward_ctx before = c; int expired = 123;
    if (game_add_temp(&c, PR_ITEM_TEMP_STAT_STAGE_BOOSTER, 1) != PR_REWARD_TEMP_UNSUPPORTED
        || game_lapse_temp(&c, &expired) || expired != 123 || memcmp(&c, &before, sizeof c)) die("refusal mutation");
   
    guards++;
}
static void selftest(void)
{
    pr_reward_ctx c = {0}; c.n_items = -1; refusal(c); c.n_items = PR_REWARD_MAX_OWNED + 1; refusal(c);
    c.n_items = 1; c.items[0] = (pr_owned_item){.item=PR_ITEM_TEMP_STAT_STAGE_BOOSTER,.arg=1,.owner=-1,.stack=1,.battle_count=4};
    pr_reward_ctx base = c;
#define GUARD(field, value) do { c=base; c.items[0].field=value; refusal(c); } while (0)
    GUARD(item, PR_ITEM_COUNT_); GUARD(owner, 0); GUARD(owner, -2); GUARD(arg, 0); GUARD(arg, 7);
    GUARD(stack, 0); GUARD(stack, -1); GUARD(stack, 3); GUARD(battle_count, 0); GUARD(battle_count, -1); GUARD(battle_count, 6);
#undef GUARD
    c=base; c.items[0].item=PR_ITEM_DIRE_HIT; refusal(c);
    c=base; pr_reward_ctx before=c;
    int args[]={-1,0,7,65535};
    for(int j=0;j<4;j++) { if(game_add_temp(&c,PR_ITEM_TEMP_STAT_STAGE_BOOSTER,args[j])!=PR_REWARD_TEMP_UNSUPPORTED || memcmp(&before,&c,sizeof c))die("arg mutation");
    guards++; }
    if(game_add_temp(&c,PR_ITEM_DIRE_HIT,1)!=PR_REWARD_TEMP_UNSUPPORTED || game_add_temp(&c,PR_ITEM_LURE,0)!=PR_REWARD_TEMP_UNSUPPORTED || memcmp(&before,&c,sizeof c))die("item guards");
    guards+=2;
    if(game_add_temp(NULL,PR_ITEM_DIRE_HIT,0)!=PR_REWARD_TEMP_UNSUPPORTED || game_lapse_temp(NULL,NULL))die("null guards");
    guards+=2;
    c.n_items=PR_MAX_MODIFIERS;
    for(int j=0;j<c.n_items;j++) c.items[j]=(pr_owned_item){.item=PR_ITEM_HEALING_CHARM,.owner=-1,.stack=1};
    before=c;
    if(game_add_temp(&c,PR_ITEM_DIRE_HIT,0)!=PR_REWARD_TEMP_UNSUPPORTED || memcmp(&before,&c,sizeof c))die("capacity");
    guards++;
    c.items[0]=base.items[0];
    if(game_add_temp(&c,PR_ITEM_TEMP_STAT_STAGE_BOOSTER,1)!=PR_REWARD_TEMP_REFRESHED || c.items[0].battle_count!=5 || c.items[0].stack!=1)die("capacity refresh");
    before=c; if(game_add_temp(&c,PR_ITEM_TEMP_STAT_STAGE_BOOSTER,1)!=PR_REWARD_TEMP_FULL || memcmp(&before,&c,sizeof c))die("full mutation");
    guards+=2;
    c=base; c.items[0].battle_count=1; c.items[1]=(pr_owned_item){.item=PR_ITEM_LURE,.owner=-1,.stack=2,.battle_count=10}; c.n_items=2;
    int expired=-1; if(!game_lapse_temp(&c,&expired)||expired!=1||c.n_items!=1||c.items[0].battle_count!=10)die("opaque lapse");
    printf("TEMPBOOSTER guards=%d refusal/capacity checks passed; opaque Lure expiry preserved\n",guards);
}
int main(int argc,char **argv)
{
    selftest(); if(argc==2&&!strcmp(argv[1],"--selftest"))return 0;
    if(argc!=2||!(input=fopen(argv[1],"r")))die("usage: tempboostertest trace");
    char *s=record('B'); integer(&s,1,1); int n=integer(&s,120,100000);end(s);
    int stats[34]={0},fails=0;stats[0]=n;
    for(int i=0;i<n;i++) {
        pr_run run={0};pr_reward_ctx c={0},oracle={0};c.run=&run;c.wave=123;memset(c.party,0x42,sizeof c.party);
        s=record('C');integer(&s,i,i);int item=item_id(&s),arg=integer(&s,0,6),which=family(item,arg);
        int ticks=integer(&s,1,7),base=integer(&s,-6,6);end(s);if(which<0||which>6)die("family");
        s=record('R');double rng[4];for(int j=0;j<4;j++)rng[j]=number(&s);end(s);
        run.rng=(pr_rng){.c=rng[0],.s0=rng[1],.s1=rng[2],.s2=rng[3]};pr_run beforeRun=run;
        inventory(&c,&oracle,true);int duplicate=0;
        for(int j=0;j<c.n_items;j++)if(family(c.items[j].item,c.items[j].arg)>=0) {
            stats[32]+=c.items[j].stack==2;
            for(int k=0;k<j;k++)if(c.items[k].item==c.items[j].item&&c.items[k].arg==c.items[j].arg)duplicate=1;
        }stats[31]+=duplicate;
        pr_reward_ctx before=c;int status=game_add_temp(&c,item,arg);untouched(before,c);
        s=record('A');int expected=integer(&s,1,3),sound=integer(&s,0,1);
        int bad=status!=expected||sound!=(status==PR_REWARD_TEMP_REFRESHED);bad|=effects(&c,base,&s);
        stats[(expected==2?3:expected==3?10:17)+which]++;bad|=inventory(&c,&oracle,false);
        for(int tick=0;tick<ticks;tick++) {
            for(int j=0;j<oracle.n_items;j++){int f=family(oracle.items[j].item,oracle.items[j].arg);if(f>=0&&oracle.items[j].battle_count==1)stats[24+f]++;}
            before=c;int expired=-1;if(!game_lapse_temp(&c,&expired))die("unsupported lapse");untouched(before,c);
            s=record('T');integer(&s,tick,tick);bad|=integer(&s,0,PR_REWARD_MAX_OWNED)!=expired;bad|=effects(&c,base,&s);
            bad|=inventory(&c,&oracle,false);stats[33]++;
        }
        s=record('E');integer(&s,i,i);double actual[4]={run.rng.c,run.rng.s0,run.rng.s1,run.rng.s2};
        for(int j=0;j<4;j++)bad|=number(&s)!=actual[j];
        end(s);bad|=memcmp(&run,&beforeRun,sizeof run)!=0;
        if(bad){if(fails<8)fprintf(stderr,"FAIL TEMPBOOSTER CASE %d\n",i);fails++;}
    }
    s=record('Z');for(int j=0;j<34;j++)if(integer(&s,0,10000000)!=stats[j])die("footer counts");end(s);
    if(fgets(line,sizeof line,input))die("trailing record");
    fclose(input);
    printf("TEMPBOOSTER cases=%d identical=%d failed=%d skipped=0 thrown=0 duplicate=%d stackTwo=%d ticks=%d\n",n,n-fails,fails,stats[31],stats[32],stats[33]);
    for(int f=0;f<7;f++)printf("family%d added=%d refreshed=%d full=%d expired=%d\n",f,stats[3+f],stats[10+f],stats[17+f],stats[24+f]);
    return fails?1:0;
}
