/* Variable-count arena sections across the former 24-entry boundary. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include "fixtures/arena-capacity.native.inc"
static gc_run run,loaded;
static unsigned char saved[GC_SAVE_MAX];
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"arena capacity save line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void)
{
 const int counts[]={0,1,24,25,48};const char *why=NULL;
 CHECK(PR_MAX_ARENA_TAGS>=48);
 for(unsigned c=0;c<sizeof counts/sizeof counts[0];c++){
  gc_run_new(&run,GAME_MODES_CLASSIC,"arena-capacity-save",777);
  gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};
  CHECK(gc_run_set_starters(&run,&starter,1,&why));gc_event event;while(gc_run_next_event(&run,&event)){}
  CHECK(gc_run_can_save(&run));CHECK(run.bt.field[0]&&run.bt.field[2]);
  run.bt.n_arena_tags=counts[c];
  for(int i=0;i<counts[c];i++){
   run.bt.arena_tags[i]=native_tags[i];
   if(run.bt.arena_tags[i].source_id==101)run.bt.arena_tags[i].source_id=run.bt.field[0]->id;
   if(run.bt.arena_tags[i].source_id==104)run.bt.arena_tags[i].source_id=run.bt.field[2]->id;
  }
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));
  CHECK(loaded.load_dropped==0);CHECK(loaded.bt.n_arena_tags==counts[c]);
  for(int i=0;i<counts[c];i++){
   const pr_arena_tag *a=&run.bt.arena_tags[i],*b=&loaded.bt.arena_tags[i];
   CHECK(a->type==b->type);CHECK(a->side==b->side);CHECK(a->turn_count==b->turn_count);
   CHECK(a->max_duration==b->max_duration);CHECK(a->source_move==b->source_move);
   CHECK(a->source_id==b->source_id);CHECK(a->layers==b->layers);CHECK(a->source_count==b->source_count);
   CHECK(a->being_removed==b->being_removed);CHECK(a->n_pending_heals==b->n_pending_heals);
  }
 }
 printf("Arena capacity legal-save transport: %u checks passed\n",checks);return 0;
}
