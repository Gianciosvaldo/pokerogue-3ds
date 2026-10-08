/* Independent strict replay of the additive real-class Soothe Bell oracle. */
#include "pr/reward.h"
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
    if (!strcmp(name, "SOOTHE_BELL")) return PR_ITEM_SOOTHE_BELL;
    if (!strcmp(name, "HEALING_CHARM")) return PR_ITEM_HEALING_CHARM;
    if (!strcmp(name, "LEFTOVERS")) return PR_ITEM_LEFTOVERS;
    die("item name"); return 0;
}
static pr_owned_item read_item(void)
{
    char *s=record('H');pr_owned_item it={0};it.item=item_id(&s);
    it.stack=integer(&s,1,it.item==PR_ITEM_SOOTHE_BELL?3:1);it.owner=integer(&s,-1,5);
    it.arg=integer(&s,0,0);it.battle_count=integer(&s,0,0);it.active=integer(&s,0,0);end(s);return it;
}
static bool same(const pr_owned_item *a,const pr_owned_item *b)
{
    return a->item==b->item&&a->stack==b->stack&&a->owner==b->owner&&a->arg==b->arg&&a->battle_count==b->battle_count&&a->active==b->active;
}
static int inventory(pr_reward_ctx *c,pr_reward_ctx *oracle,bool fill)
{
    char *s=record('I');int n=integer(&s,0,PR_REWARD_MAX_OWNED);end(s);int bad=!fill&&n!=c->n_items;oracle->n_items=n;
    for(int j=0;j<n;j++){pr_owned_item it=read_item();oracle->items[j]=it;if(it.owner>=c->n_party)die("owner");
        if(fill)c->items[j]=it;else if(j>=c->n_items||!same(&it,&c->items[j]))bad=1;}
    if(fill)c->n_items=n;
    return bad;
}
static void unchanged_other(pr_reward_ctx a,pr_reward_ctx b)
{
    memset(a.items,0,sizeof a.items);memset(b.items,0,sizeof b.items);a.n_items=b.n_items=0;
    if(memcmp(&a,&b,sizeof a))die("non-inventory mutation");
}
static pr_reward_ctx fixture(void)
{
    pr_reward_ctx c={0};c.n_party=2;c.party[0].id=1;c.party[1].id=2;return c;
}
static void refusal(pr_reward_ctx c)
{
    pr_reward_ctx old=c;double amount=123;int index=456;
    if(pr_reward_add_soothe_bell(&c,0)!=PR_REWARD_SOOTHE_UNSUPPORTED||pr_reward_soothe_bell_gain(&c,0,6,&amount,&index)
        ||amount!=123||index!=456||memcmp(&c,&old,sizeof c))die("refusal mutation");
    guards++;
}
static void selftest(void)
{
    pr_reward_ctx c;
#define GUARD(field,value) do{c=fixture();c.field=value;refusal(c);}while(0)
    GUARD(n_party,0);GUARD(n_party,7);GUARD(n_items,-1);GUARD(n_items,257);GUARD(party[1].id,1);
#undef GUARD
#define ITEM_GUARD(field,value) do{c=fixture();c.n_items=1;c.items[0]=(pr_owned_item){.item=PR_ITEM_SOOTHE_BELL,.owner=0,.stack=1};c.items[0].field=value;refusal(c);}while(0)
    ITEM_GUARD(item,PR_ITEM_COUNT_);ITEM_GUARD(owner,-2);ITEM_GUARD(owner,2);ITEM_GUARD(arg,1);ITEM_GUARD(battle_count,1);
    ITEM_GUARD(stack,0);ITEM_GUARD(stack,-1);ITEM_GUARD(stack,4);
#undef ITEM_GUARD
    c=fixture();pr_reward_ctx old=c;double amount=123;int index=456;
    for(int j=-1;j<=2;j+=3){if(pr_reward_add_soothe_bell(&c,j)!=PR_REWARD_SOOTHE_UNSUPPORTED||pr_reward_soothe_bell_gain(&c,j,6,&amount,&index)||memcmp(&c,&old,sizeof c)||amount!=123||index!=456)die("target guard");
    guards++;}
    double invalid[]={NAN,INFINITY,-1,1000001};
    for(int j=0;j<4;j++){if(pr_reward_soothe_bell_gain(&c,0,invalid[j],&amount,&index)||amount!=123||index!=456)die("base guard");
    guards++;}
    if(pr_reward_add_soothe_bell(NULL,0)!=PR_REWARD_SOOTHE_UNSUPPORTED||pr_reward_soothe_bell_gain(NULL,0,6,&amount,&index)||pr_reward_soothe_bell_gain(&c,0,6,NULL,&index)||index!=456)die("null guard");
    guards+=3;
    c.n_items=PR_REWARD_MAX_OWNED;for(int j=0;j<c.n_items;j++)c.items[j]=(pr_owned_item){.item=PR_ITEM_HEALING_CHARM,.owner=-1,.stack=1};old=c;
    if(pr_reward_add_soothe_bell(&c,0)!=PR_REWARD_SOOTHE_UNSUPPORTED||memcmp(&old,&c,sizeof c))die("capacity");
    guards++;
    c.items[0]=(pr_owned_item){.item=PR_ITEM_SOOTHE_BELL,.owner=0,.stack=2};
    if(pr_reward_add_soothe_bell(&c,0)!=PR_REWARD_SOOTHE_STACKED||c.items[0].stack!=3||c.n_items!=256)die("capacity stack");
    old=c;
    if(pr_reward_add_soothe_bell(&c,0)!=PR_REWARD_SOOTHE_FULL||memcmp(&old,&c,sizeof c))die("full refusal");
    guards+=2;
    c=fixture();c.n_items=2;c.items[0]=(pr_owned_item){.item=PR_ITEM_SOOTHE_BELL,.owner=-1,.stack=1};c.items[1]=(pr_owned_item){.item=PR_ITEM_SOOTHE_BELL,.owner=0,.stack=3};old=c;
    if(!pr_reward_soothe_bell_gain(&c,0,2.5,&amount,&index)||amount!=3||index!=0||memcmp(&c,&old,sizeof c))die("wildcard/first/floor");
    if(!pr_reward_soothe_bell_gain(&c,1,6,&amount,NULL)||amount!=9)die("wildcard optional output");
    printf("SOOTHEBELL guards=%d no-mutation/capacity checks passed; wildcard/first/floor checks passed\n",guards);
}
int main(int argc,char **argv)
{
    selftest();if(argc==2&&!strcmp(argv[1],"--selftest"))return 0;
    if(argc!=2||!(input=fopen(argv[1],"r")))die("usage: soothebelltest trace");
    char *s=record('S');integer(&s,1,1);int n=integer(&s,120,100000);end(s);int stats[11]={0},fails=0;stats[0]=n;
    for(int i=0;i<n;i++){
        pr_run run={0};pr_reward_ctx c={0},oracle={0};c.run=&run;memset(c.party,0x42,sizeof c.party);
        s=record('C');integer(&s,i,i);c.n_party=integer(&s,2,6);int target=integer(&s,0,c.n_party-1);double base=number(&s);end(s);if(base<0||base>1000000)die("base");
        s=record('P');for(int j=0;j<c.n_party;j++){double id=number(&s);if(id!=floor(id)||id<0||id>UINT32_MAX)die("id");c.party[j].id=id;}end(s);
        s=record('R');double rng[4];for(int j=0;j<4;j++)rng[j]=number(&s);end(s);run.rng=(pr_rng){.c=rng[0],.s0=rng[1],.s1=rng[2],.s2=rng[3]};pr_run oldRun=run;
        inventory(&c,&oracle,true);int duplicate=0;
        for(int j=0;j<c.n_items;j++)if(c.items[j].item==PR_ITEM_SOOTHE_BELL)for(int k=0;k<j;k++)if(c.items[k].item==c.items[j].item&&c.items[k].owner==c.items[j].owner)duplicate=1;
        stats[7]+=duplicate;stats[8]+=base==0;stats[9]+=base!=floor(base);
        double amount=-1;int index=-2;pr_reward_ctx old=c;
        if(!pr_reward_soothe_bell_gain(&c,target,base,&amount,&index)||memcmp(&c,&old,sizeof c))die("gain invariant");
        s=record('G');double expectedAmount=number(&s);int expectedIndex=integer(&s,-1,c.n_items-1);end(s);
        int bad=amount!=expectedAmount||index!=expectedIndex;
        int status=pr_reward_add_soothe_bell(&c,target);unchanged_other(old,c);
        s=record('A');int expected=integer(&s,1,3);expectedAmount=number(&s);expectedIndex=integer(&s,-1,PR_REWARD_MAX_OWNED-1);end(s);stats[2+expected]++;
        old=c;if(!pr_reward_soothe_bell_gain(&c,target,base,&amount,&index)||memcmp(&c,&old,sizeof c))die("gain invariant");
        bad|=status!=expected||amount!=expectedAmount||index!=expectedIndex;bad|=inventory(&c,&oracle,false);
        if(expectedIndex>=oracle.n_items)die("matched index");
        stats[6]+=expectedIndex>=0&&oracle.items[expectedIndex].owner==-1;
        s=record('E');integer(&s,i,i);double actual[4]={run.rng.c,run.rng.s0,run.rng.s1,run.rng.s2};for(int j=0;j<4;j++)bad|=number(&s)!=actual[j];
        stats[10]+=integer(&s,0,1);end(s);bad|=memcmp(&run,&oldRun,sizeof run)!=0;
        if(bad){if(fails<8)fprintf(stderr,"FAIL SOOTHEBELL CASE %d\n",i);fails++;}
    }
    s=record('Z');for(int j=0;j<11;j++)if(integer(&s,0,10000000)!=stats[j])die("footer counts");end(s);
    if(fgets(line,sizeof line,input))die("trailing record");
    fclose(input);
    printf("SOOTHEBELL cases=%d identical=%d failed=%d skipped=0 thrown=0 full=%d added=%d stacked=%d wildcard=%d duplicate=%d zero=%d fractional=%d singularDiffersFromAll=%d\n",n,n-fails,fails,stats[3],stats[4],stats[5],stats[6],stats[7],stats[8],stats[9],stats[10]);return fails?1:0;
}
