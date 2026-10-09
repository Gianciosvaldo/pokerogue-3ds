/* Strict additive actual reward choice, BattleEnd queue and named-timer save replay. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static FILE *fp;static char line[512];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"temp caller line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void numbers(char kind,double *v,int n){if(!fgets(line,sizeof line,fp)||line[0]!=kind||line[1]!=' '||!strchr(line,'\n'))exit(2);char *s=line+2,*e;for(int j=0;j<n;j++){v[j]=strtod(s,&e);if(e==s||!isfinite(v[j])||floor(v[j])!=v[j])exit(2);s=e;}while(isspace((unsigned char)*s))s++;if(*s)exit(2);}
static void setup(void){gc_run_new(&run,GAME_MODES_CLASSIC,"temp-caller",777);gc_starter st={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};memset(st.ivs,15,6);const char *why;CHECK(gc_run_set_starters(&run,&st,1,&why));gc_event ev;while(gc_run_next_event(&run,&ev)){};}
static void screen(gc_offer o){gc_rqueue *q=&gc_int(&run)->rq;gc_rq_reset(q);q->current=gc_rp(RP_SELECT_MODIFIER,0,0,0);q->waiting=true;gc_rq_push(&run,gc_rp(RP_COMMAND,0,0,0));memset(&run.screen,0,sizeof run.screen);run.screen.kind=GC_SCREEN_REWARD;run.screen.n_shop=run.screen.n_options=1;run.screen.shop[0]=run.screen.options[0]=o;}
static void battle_end(gc_run *r,int victory){gc_rq_clear(r);gc_rq_push(r,gc_rp(RP_BATTLE_END,victory,0,0));gc_rq_push(r,gc_rp(RP_BATTLE_END,victory,0,0));r->bt.successful_run=!victory;r->bt.money_scattered=0;r->bt.enemy_faints=0;gc_rphase p=gc_rp(RP_BATTLE_END,victory,0,0);CHECK(gc_phase_start(r,&p));CHECK(!gc_rq_has(r,RP_BATTLE_END));}
static void selftest(void){
 const char *why;for(int count=-1;count<=GC_MAX_ITEMS+1;count+=GC_MAX_ITEMS+2){setup();run.n_items=count;int expired=99;CHECK(!gc_temp_lapse(&run,&expired)&&expired==99);CHECK(gc_temp_add(&run,PR_ITEM_DIRE_HIT,0)==PR_REWARD_TEMP_UNSUPPORTED&&run.n_items==count);}
 for(int guard=0;guard<10;guard++) {
  setup();run.n_items=1;run.items[0]=(gc_item){.key="TEMP_STAT_STAGE_BOOSTER",.arg=STAT_ATK,.stack=1,.battle_count=4};
  switch(guard){case 0:run.items[0].battle_count=0;break;case 1:run.items[0].battle_count=6;break;case 2:run.items[0].stack=3;break;case 3:run.items[0].arg=0;break;case 4:run.items[0].pokemon_id=123;break;case 5:run.items[0].virtual_stack=1;break;case 6:run.items[0].detached=true;break;case 7:run.n_items=2;run.items[1]=(gc_item){.key="UNRECOGNIZED",.stack=1};break;case 8:run.run.has_challenges=true;break;case 9:run.items[0].key="DIRE_HIT";break;}
  gc_item before[GC_MAX_ITEMS];memcpy(before,run.items,sizeof before);int count=run.n_items,expired=99;
  CHECK(gc_temp_add(&run,PR_ITEM_DIRE_HIT,0)==PR_REWARD_TEMP_UNSUPPORTED);CHECK(!gc_temp_lapse(&run,&expired)&&expired==99);CHECK(run.n_items==count&&!memcmp(before,run.items,sizeof before));
  gc_items_to_battle(&run);if(guard!=7&&guard!=8)CHECK(run.bt.mods[0][0].unknown_subclass);
 }
 for(int timer=1;timer<=5;timer++){setup();run.n_items=1;run.items[0]=(gc_item){.key="DIRE_HIT",.arg=0,.stack=1,.battle_count=timer};gc_items_to_battle(&run);size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size&&gc_run_load(&loaded,saved,size,&why));CHECK(loaded.items[0].battle_count==timer);int expired=99;CHECK(gc_temp_lapse(&loaded,&expired));CHECK(expired==(timer==1)&&loaded.n_items==(timer>1));if(timer>1)CHECK(loaded.items[0].battle_count==timer-1);}
 setup();run.n_items=1;run.items[0]=(gc_item){.key="UNRECOGNIZED",.stack=1};int expired=-1;CHECK(gc_temp_lapse(&run,&expired)&&expired==0);
 setup();gc_item_add(&run,"DIRE_HIT",0,0,1);CHECK(run.items[0].battle_count==0);gc_items_to_battle(&run);CHECK(run.bt.mods[0][0].unknown_subclass);
 size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size&&gc_run_load(&loaded,saved,size,&why));CHECK(loaded.items[0].battle_count==0&&loaded.bt.mods[0][0].unknown_subclass);
 printf("Game temp host: %d checks passed\n",checks);
}
int main(int argc,char **argv){if(argc==2&&!strcmp(argv[1],"--selftest")){selftest();return 0;}if(argc!=2||!(fp=fopen(argv[1],"r")))return 2;int version,n,used=0;if(!fgets(line,sizeof line,fp)||sscanf(line,"GC_TEMP_CALLER %d %d%n",&version,&n,&used)!=2||version!=1||n<120||n>100000||strcmp(line+used,"\n"))return 2;int paid=0,full=0,ticksTotal=0;const char *why;
for(int ci=0;ci<n;ci++){
 double c[9],a[10];numbers('C',c,9);CHECK(c[0]==ci&&c[1]>=0&&c[1]<7&&c[3]>=0&&c[3]<5&&c[4]>=0&&c[4]<=2&&c[5]>=0&&c[5]<=5&&c[6]>=1&&c[6]<=7&&(c[7]==0||c[7]==1)&&(c[8]==0||c[8]==1));
 numbers('A',a,10);setup();int family=c[1],arg=c[2],tier=c[3],stack=c[4],duration=c[5],ticks=c[6],victory=c[7],purchase=c[8];
 CHECK(arg==(family==6?0:family+1));const char *key=family==6?"DIRE_HIT":"TEMP_STAT_STAGE_BOOSTER";
 if(stack){run.n_items=1;run.items[0]=(gc_item){.key=key,.arg=arg,.stack=stack,.battle_count=duration};}gc_items_to_battle(&run);run.money=9999+ci;uint32_t balls[5];for(int j=0;j<5;j++)balls[j]=run.pokeballs[j];pr_rng rng=run.run.rng;
 gc_offer offer={.key=key,.arg=arg,.tier=tier,.target=GC_TARGET_NONE,.cost=purchase?42:-1};screen(offer);gc_choice pick={purchase?GC_CH_SHOP:GC_CH_REWARD,0,-1,-1};bool accepted=gc_run_choose(&run,&pick,&why);
 CHECK(accepted==!(purchase&&duration!=5));CHECK(run.n_items==1&&run.items[0].stack==a[0]&&run.items[0].battle_count==a[1]);CHECK(a[2]==(duration==5));
 for(int j=0;j<5;j++){if((double)run.pokeballs[j]-balls[j]!=a[3+j])fprintf(stderr,"case%d tier%d ball%d actual%g expected%g\n",ci,tier,j,(double)run.pokeballs[j]-balls[j],a[3+j]);CHECK((double)run.pokeballs[j]-balls[j]==a[3+j]);}
 CHECK(run.money-(9999+ci)==a[8]&&a[9]==(purchase&&duration!=5));CHECK(!memcmp(&rng,&run.run.rng,sizeof rng));
 /* Restore only the queue/command presentation to its original legal encounter
  * save boundary; no turn or BattleEnd has executed yet. */
 gc_rq_clear(&run);gc_int(&run)->rq.current=gc_rp(RP_COMMAND,0,0,0);gc_int(&run)->rq.waiting=true;run.screen.kind=GC_SCREEN_COMMAND;
 CHECK(gc_run_can_save(&run));size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size&&gc_run_load(&loaded,saved,size,&why));CHECK(loaded.n_items==1&&loaded.items[0].battle_count==run.items[0].battle_count&&loaded.items[0].stack==run.items[0].stack);
 CHECK(loaded.bt.mods[0][0].cls==(family==6?PR_MOD_TEMP_CRIT_BOOSTER:PR_MOD_TEMP_STAT_STAGE_BOOSTER));
 CHECK(!pr_mod_is_ported_item(&loaded.bt.mods[0][0])); /* Full-turn whitelist stays bounded. */
 for(int tick=0;tick<ticks;tick++){double t[3];numbers('T',t,3);CHECK(t[0]==tick);battle_end(&run,victory);battle_end(&loaded,victory);CHECK(!gc_run_can_save(&run)&&!gc_run_can_save(&loaded));CHECK(run.n_items==(t[1]?1:0)&&loaded.n_items==run.n_items);if(run.n_items){CHECK(run.items[0].stack==t[1]&&run.items[0].battle_count==t[2]);CHECK(loaded.items[0].battle_count==t[2]);}ticksTotal++;}
 double e;numbers('E',&e,1);CHECK(e==ci);paid+=purchase;full+=duration==5;
}
double z;numbers('Z',&z,1);if(z!=n||fgetc(fp)!=EOF)return 2;fclose(fp);CHECK(paid&&full);printf("Game temp caller: %d cases, %d checks, paid%d full%d BattleEnd%d, exact\n",n,checks,paid,full,ticksTotal);return 0;}
