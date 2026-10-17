/* Actual old save buffers plus new numeric tag transport at a command boundary. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,copy;static unsigned char bytes[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"wide combat save line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(void){gc_run_new(&run,GAME_MODES_CLASSIC,"wide-combat-save",777);gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};memset(s.ivs,15,6);const char *why;CHECK(gc_run_set_starters(&run,&s,1,&why));gc_event ev;while(gc_run_next_event(&run,&ev)){}CHECK(gc_run_can_save(&run));}
static void verify(double hp){const pr_pokemon *p=&run.party[run.party_order[0]].b;CHECK(run.load_dropped==0);CHECK(p->n_tags==2);CHECK(p->tags[0].type==BATTLER_TAG_TYPE_SUBSTITUTE);CHECK(p->tags[0].hp==hp);CHECK(p->tags[0].source_id==p->id&&p->tags[0].source_move==MOVE_ID_SUBSTITUTE&&p->tags[0].in_focus);CHECK(p->tags[1].type==BATTLER_TAG_TYPE_AQUA_RING&&p->tags[1].turn_count==4&&p->tags[1].hp==0);}
int main(void){
 const int32_t old[]={INT32_MIN,-1,0,1,128,255,65535,INT32_MAX};FILE *f=fopen("tests/fixtures/substitute-hp-int32-saves.bin","rb");if(!f)f=fopen("game-core/tests/fixtures/substitute-hp-int32-saves.bin","rb");CHECK(f);uint32_t count;CHECK(fread(&count,4,1,f)==1&&count==8);const char *why;
 for(uint32_t i=0;i<count;i++){uint32_t n;CHECK(fread(&n,4,1,f)==1&&n<=sizeof bytes);CHECK(fread(bytes,1,n,f)==n);CHECK(gc_run_load(&run,bytes,n,&why));verify(old[i]);size_t len=gc_run_save(&run,bytes,sizeof bytes);CHECK(len);CHECK(gc_run_load(&run,bytes,len,&why));verify(old[i]);}
 CHECK(fgetc(f)==EOF);fclose(f);
 const double values[]={-154913848315.0,-2147483649.0,0,1,2147483648.0,4294967296.0,9007199254740991.0};
 for(size_t i=0;i<sizeof values/sizeof *values;i++){
  setup();pr_pokemon *p=&run.party[run.party_order[0]].b;p->n_tags=2;p->tags[0]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_SUBSTITUTE,.source_id=p->id,.source_move=MOVE_ID_SUBSTITUTE,.hp=values[i],.in_focus=true};p->tags[1]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_AQUA_RING,.source_id=p->id,.source_move=MOVE_ID_AQUA_RING,.turn_count=4};size_t n=gc_run_save(&run,bytes,sizeof bytes);CHECK(n);CHECK(gc_run_load(&copy,bytes,n,&why));run=copy;verify(values[i]);
 }
 const double invalid[]={NAN,INFINITY,1.5};
 for(size_t i=0;i<sizeof invalid/sizeof *invalid;i++){
  setup();pr_pokemon *p=&run.party[run.party_order[0]].b;p->n_tags=1;p->tags[0]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_SUBSTITUTE,.source_id=p->id,.source_move=MOVE_ID_SUBSTITUTE,.hp=invalid[i]};size_t n=gc_run_save(&run,bytes,sizeof bytes);CHECK(n);CHECK(!gc_run_load(&copy,bytes,n,&why));
 }
 puts("Wide combat tag save migration: actual 8 old saves retained");printf("Wide combat tag saves: %d checks passed\n",checks);return 0;
}
