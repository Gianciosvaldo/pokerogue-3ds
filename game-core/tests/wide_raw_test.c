/* Independent complete-inventory replay of real canonical Wide Lens .add. */
#include "../src/internal.h"
#include "wide-raw-bridge.inc"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAMILY PR_ITEM_WIDE_LENS
#define CAP 3
static FILE *input;
static int line_no, guards;
static char line[4096];
static void die(const char *s) { fprintf(stderr,"invalid trace line %d: %s\n",line_no,s);exit(2); }
static char *record(char kind)
{
    if(!fgets(line,sizeof line,input))die("unexpected EOF");
    line_no++;
    if(!strchr(line,'\n')||line[0]!=kind||(line[1]!=' '&&line[1]!='\n'))die("record order/length");
    return line+1;
}
static double number(char **s)
{
    char *end;double n=strtod(*s,&end);if(end==*s||!isfinite(n))die("number");*s=end;return n;
}
static int integer(char **s,int lo,int hi)
{
    double n=number(s);if(floor(n)!=n||n<lo||n>hi)die("integer range");return n;
}
static void end(char *s){while(isspace((unsigned char)*s))s++;if(*s)die("extra fields");}
static pr_owned_item read_item(void)
{
    char *s=record('H'),name[64];int used;pr_owned_item it={0};
    if(sscanf(s," %63s%n",name,&used)!=1)die("item name");
    s+=used;
    if(!strcmp(name,"WIDE_LENS"))it.item=FAMILY;
    else if(!strcmp(name,"HEALING_CHARM"))it.item=PR_ITEM_HEALING_CHARM;
    else if(!strcmp(name,"LEFTOVERS"))it.item=PR_ITEM_LEFTOVERS;
    else die("unknown item");
    it.stack=integer(&s,1,it.item==FAMILY?CAP:1);it.owner=integer(&s,-1,5);
    it.arg=integer(&s,0,0);it.battle_count=integer(&s,0,0);it.active=integer(&s,0,1);end(s);return it;
}
static bool same(const pr_owned_item *a,const pr_owned_item *b)
{
    return a->item==b->item&&a->stack==b->stack&&a->owner==b->owner&&a->arg==b->arg&&a->battle_count==b->battle_count&&a->active==b->active;
}
static int inventory(pr_reward_ctx *c,bool fill)
{
    char *s=record('I');int n=integer(&s,0,PR_REWARD_MAX_OWNED);end(s);int bad=!fill&&n!=c->n_items;
    for(int j=0;j<n;j++){pr_owned_item it=read_item();if(it.owner>=c->n_party)die("owner");
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
    pr_reward_ctx old=c;
    if(game_wide_add(&c,FAMILY,0)!=PR_REWARD_HELD_UNSUPPORTED||memcmp(&c,&old,sizeof c))die("refusal mutation");
    guards++;
}
static void selftest(void)
{
    pr_reward_ctx c;
#define GUARD(field,value) do{c=fixture();c.field=value;refusal(c);}while(0)
    GUARD(n_party,0);GUARD(n_party,7);GUARD(n_items,-1);GUARD(n_items,PR_REWARD_MAX_OWNED+1);GUARD(party[1].id,1);
#undef GUARD
    /* Retain the old 257-entry boundary as an admitted input; the invalid
     * count guard above follows the expanded storage ceiling. */
    c=fixture();c.n_items=257;pr_reward_ctx old_boundary=c;
    if(game_wide_add(&c,FAMILY,0)!=PR_REWARD_HELD_ADDED||c.n_items!=258||memcmp(c.items,old_boundary.items,257*sizeof c.items[0]))die("old capacity admission");
    guards++;

#define ITEM_GUARD(field,value) do{c=fixture();c.n_items=1;c.items[0]=(pr_owned_item){.item=FAMILY,.owner=0,.stack=1};c.items[0].field=value;refusal(c);}while(0)
    ITEM_GUARD(item,PR_ITEM_COUNT_);ITEM_GUARD(owner,-2);ITEM_GUARD(owner,2);ITEM_GUARD(arg,1);ITEM_GUARD(battle_count,1);
    ITEM_GUARD(stack,0);ITEM_GUARD(stack,-1);ITEM_GUARD(stack,CAP+1);
#undef ITEM_GUARD
    c=fixture();pr_reward_ctx old=c;
    for(int j=-1;j<=2;j+=3){if(game_wide_add(&c,FAMILY,j)!=PR_REWARD_HELD_UNSUPPORTED||memcmp(&c,&old,sizeof c))die("target guard");guards++;}
    int invalid[]={-1,PR_ITEM_COUNT_,PR_ITEM_LEFTOVERS,PR_ITEM_SOOTHE_BELL};
    for(int j=0;j<4;j++){if(game_wide_add(&c,invalid[j],0)!=PR_REWARD_HELD_UNSUPPORTED||memcmp(&c,&old,sizeof c))die("family guard");guards++;}
    if(game_wide_add(NULL,FAMILY,0)!=PR_REWARD_HELD_UNSUPPORTED)die("null guard");
    guards++;
    c.n_items=PR_MAX_MODIFIERS;for(int j=0;j<c.n_items;j++)c.items[j]=(pr_owned_item){.item=PR_ITEM_HEALING_CHARM,.owner=-1,.stack=1};old=c;
    if(game_wide_add(&c,FAMILY,0)!=PR_REWARD_HELD_UNSUPPORTED||memcmp(&old,&c,sizeof c))die("capacity");
    guards++;
    if(CAP>1){c.items[0]=(pr_owned_item){.item=FAMILY,.owner=0,.stack=CAP-1,.active=true};
        if(game_wide_add(&c,FAMILY,0)!=PR_REWARD_HELD_STACKED||c.items[0].stack!=CAP||!c.items[0].active||c.n_items!=PR_MAX_MODIFIERS)die("capacity stack");
        guards++;}
    c.items[0]=(pr_owned_item){.item=FAMILY,.owner=0,.stack=CAP};old=c;
    if(game_wide_add(&c,FAMILY,0)!=PR_REWARD_HELD_FULL||memcmp(&old,&c,sizeof c))die("full refusal");
    guards++;
    c=fixture();c.n_items=2;c.items[0]=(pr_owned_item){.item=FAMILY,.owner=-1,.stack=CAP,.active=true};c.items[1]=(pr_owned_item){.item=FAMILY,.owner=1,.stack=CAP};old=c;
    if(game_wide_add(&c,FAMILY,0)!=PR_REWARD_HELD_ADDED||c.n_items!=3||!same(&c.items[0],&old.items[0])||!same(&c.items[1],&old.items[1])||c.items[2].owner!=0||c.items[2].stack!=1||c.items[2].active)die("wildcard/owner/append");
    guards++;
    c=fixture();c.n_items=2;c.items[0]=(pr_owned_item){.item=FAMILY,.owner=0,.stack=CAP};c.items[1]=(pr_owned_item){.item=FAMILY,.owner=0,.stack=1};old=c;
    if(game_wide_add(&c,FAMILY,0)!=PR_REWARD_HELD_FULL||memcmp(&c,&old,sizeof c))die("first full duplicate");
    guards++;
    printf("WIDEHELD guards=%d no-mutation/capacity/first-match checks passed\n",guards);
}
int main(int argc,char **argv)
{
    selftest();if(argc==2&&!strcmp(argv[1],"--selftest"))return 0;
    if(argc!=2||!(input=fopen(argv[1],"r")))die("usage: wideheldtest trace");
    char *s=record('S');integer(&s,1,1);int n=integer(&s,120,100000);end(s);int stats[10]={0},fails=0;stats[0]=n;
    for(int i=0;i<n;i++){
        pr_run run={0};pr_reward_ctx c={0};c.run=&run;memset(c.party,0x42,sizeof c.party);
        s=record('C');integer(&s,i,i);c.n_party=integer(&s,2,6);int target=integer(&s,0,c.n_party-1);
        stats[8]+=integer(&s,0,1);stats[9]+=integer(&s,0,1);end(s);
        s=record('P');for(int j=0;j<c.n_party;j++){double id=number(&s);if(id!=floor(id)||id<0||id>UINT32_MAX)die("id");c.party[j].id=id;}end(s);
        s=record('R');double rng[4];for(int j=0;j<4;j++)rng[j]=number(&s);end(s);run.rng=(pr_rng){.c=rng[0],.s0=rng[1],.s1=rng[2],.s2=rng[3]};pr_run oldRun=run;
        inventory(&c,true);int wild=0,duplicate=0;
        for(int j=0;j<c.n_items;j++)if(c.items[j].item==FAMILY){wild|=c.items[j].owner==-1;
            for(int k=0;k<j;k++)if(c.items[k].item==FAMILY&&c.items[k].owner==c.items[j].owner)duplicate=1;}
        stats[6]+=wild;stats[7]+=duplicate;pr_reward_ctx old=c;
        int status=game_wide_add(&c,FAMILY,target);unchanged_other(old,c);
        s=record('A');int expected=integer(&s,1,3);end(s);stats[2+expected]++;
        int bad=status!=expected;bad|=inventory(&c,false);
        s=record('E');integer(&s,i,i);double actual[4]={run.rng.c,run.rng.s0,run.rng.s1,run.rng.s2};for(int j=0;j<4;j++)bad|=number(&s)!=actual[j];end(s);bad|=memcmp(&run,&oldRun,sizeof run)!=0;
        if(bad){if(fails<8)fprintf(stderr,"FAIL WIDEHELD CASE %d\n",i);fails++;}
    }
    s=record('Z');for(int j=0;j<10;j++)if(integer(&s,0,10000000)!=stats[j])die("footer counts");end(s);
    if(fgets(line,sizeof line,input))die("trailing record");
    fclose(input);
    if(!stats[3]||!stats[4]||(CAP>1&&!stats[5])||!stats[6]||!stats[7]||!stats[8]||!stats[9])die("unexercised coverage");
    printf("WIDEHELD cases=%d identical=%d failed=%d skipped=0 thrown=0 full=%d added=%d stacked=%d wildcard=%d duplicate=%d reordered=%d fainted=%d\n",n,n-fails,fails,stats[3],stats[4],stats[5],stats[6],stats[7],stats[8],stats[9]);return fails?1:0;
}
