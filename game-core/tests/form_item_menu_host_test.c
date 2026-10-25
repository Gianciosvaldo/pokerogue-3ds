/* Strict native party eligibility observations, including full bench ownership. */
#include "../src/internal.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
static gc_run r;
static FILE *input;
static char text[512], *cursor;
static unsigned checks, line;
static bool enums[200], seen[2][200][16][3][8];
static uint32_t ids[3];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"form menu line%u C%d: %s\n",line,__LINE__,#x);exit(1);}}while(0)
static void record(char tag){CHECK(fgets(text,sizeof text,input));line++;CHECK(text[0]==tag&&text[1]==' '&&strchr(text,'\n'));cursor=text+2;}
static int64_t number(void){char *end;errno=0;long long n=strtoll(cursor,&end,10);CHECK(!errno&&end!=cursor);cursor=end;return n;}
static int bounded(int lo,int hi){int64_t n=number();CHECK(n>=lo&&n<=hi);return n;}
static void finish(void){CHECK(!strcmp(cursor,"\n"));}
int main(int argc,char **argv){
    CHECK(argc==2);input=fopen(argv[1],"r");CHECK(input);CHECK(fgets(text,sizeof text,input)&&!strcmp(text,"FORMMENU 1 159 122112\n"));line++;
    record('U');for(int k=0;k<3;k++){int64_t id=number();CHECK(id>0&&id<=UINT32_MAX);ids[k]=id;for(int j=0;j<k;j++)CHECK(ids[j]!=ids[k]);r.party[k].b.id=id;}finish();r.n_party=3;
    for(int k=0;k<159;k++){record('E');int item=bounded(0,199);finish();CHECK(!enums[item]&&pr_form_item_known(item));enums[item]=true;}
    for(int k=-1;k<=200;k++)CHECK(pr_form_item_known(k)==(k>=0&&k<200&&enums[k]));
    pr_seedstr seed;pr_seed_from_ascii(&seed,"form-menu-global");pr_rng_sow(&r.bt.rng,&seed);
    pr_seed_from_ascii(&seed,"form-menu-battle");pr_rng_sow(&r.bt.battle_rng,&seed);
    pr_seed_from_ascii(&seed,"form-menu-run");pr_rng_sow(&r.run.rng,&seed);r.bt.rng_live=r.bt.battle_rng_live=true;
    for(int i=0;i<122112;i++){
        record('C');CHECK(bounded(0,122111)==i);int species=bounded(151,800),item=bounded(0,199),flags=bounded(0,15),owner=bounded(0,2),enemy=bounded(0,1),foreign=bounded(0,1),reverse=bounded(0,1);finish();
        CHECK((species==151||species==800)&&enums[item]);int shape=enemy|(foreign<<1)|(reverse<<2);
        CHECK(!seen[species==800][item][flags][owner][shape]);seen[species==800][item][flags][owner][shape]=true;
        r.party[owner].b.species=species;
        record('B');int counts[2]={bounded(0,4),bounded(0,4)};finish();r.n_items=counts[0];r.n_enemy_items=counts[1];CHECK(counts[!enemy]==0&&counts[enemy]>=3);
        for(int side=0;side<2;side++)for(int k=0;k<counts[side];k++){
            record('M');int rare=bounded(0,1),arg=bounded(0,199);int64_t id=number();CHECK(id==ids[foreign?(owner+1)%3:owner]);
            int active=bounded(0,1);CHECK(bounded(1,1)==1&&bounded(0,0)==0&&bounded(1,1)==1);finish();CHECK(enums[arg]);
            gc_item *stock=side?r.enemy_items:r.items;stock[k]=(gc_item){.key=rare?"RARE_FORM_CHANGE_ITEM":"FORM_CHANGE_ITEM",.arg=arg,.pokemon_id=id,.stack=1,.form_inactive=!active,.non_transferable=true};
        }
        record('O');int n=bounded(0,4),expected[4];for(int k=0;k<n;k++){expected[k]=bounded(0,3);if(k)CHECK(expected[k]>expected[k-1]);}finish();
        pr_rng global=r.bt.rng,battle=r.bt.battle_rng,run_rng=r.run.rng;
        for(int capacity=0;capacity<=4;capacity++){
            int actual[5]={-99,-99,-99,-99,-99};CHECK(gc_form_item_options(&r,owner,capacity?actual:NULL,capacity)==n);
            for(int k=0;k<5;k++)CHECK(actual[k]==(k<n&&k<capacity?expected[k]:-99));
        }
        CHECK(!memcmp(&global,&r.bt.rng,sizeof global)&&!memcmp(&battle,&r.bt.battle_rng,sizeof battle)&&!memcmp(&run_rng,&r.run.rng,sizeof run_rng));
    }
    CHECK(fgets(text,sizeof text,input)&&!strcmp(text,"END 159 122112\n")&&fgetc(input)==EOF);fclose(input);
    for(int sp=0;sp<2;sp++)for(int item=0;item<200;item++)for(int flags=0;flags<16;flags++)for(int owner=0;owner<3;owner++)for(int shape=0;shape<8;shape++)CHECK(seen[sp][item][flags][owner][shape]==enums[item]);
    CHECK(!gc_form_item_options(NULL,0,NULL,0)&&!gc_form_item_options(&r,-1,NULL,0)&&!gc_form_item_options(&r,3,NULL,0)&&!gc_form_item_options(&r,0,NULL,1));
    printf("Form party eligibility: 122112 exact native lists; %u checks\n",checks);return 0;
}
