/* Compile against pre-wide-combat headers/libraries, not the current tree.
 * Emits eight actual old gc_run_save buffers for the migration regression. */
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;static unsigned char data[GC_SAVE_MAX];
int main(int argc,char **argv){
 if(argc!=2||sizeof(((pr_battler_tag *)0)->hp)!=4)return 2;
 const int32_t values[]={INT32_MIN,-1,0,1,128,255,65535,INT32_MAX};FILE *f=fopen(argv[1],"wb");if(!f)return 2;uint32_t count=sizeof values/sizeof *values;fwrite(&count,4,1,f);
 for(uint32_t i=0;i<count;i++){
  gc_run_new(&run,GAME_MODES_CLASSIC,"legacy-substitute-hp",777);gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};memset(s.ivs,15,6);const char *why;if(!gc_run_set_starters(&run,&s,1,&why))return 1;gc_event ev;while(gc_run_next_event(&run,&ev)){}
  pr_pokemon *p=&run.party[run.party_order[0]].b;p->n_tags=2;p->tags[0]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_SUBSTITUTE,.source_id=p->id,.source_move=MOVE_ID_SUBSTITUTE,.hp=values[i],.in_focus=true};p->tags[1]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_AQUA_RING,.source_id=p->id,.source_move=MOVE_ID_AQUA_RING,.turn_count=4};
  size_t n=gc_run_save(&run,data,sizeof data);if(!n||n>UINT32_MAX)return 1;uint32_t size=n;fwrite(&size,4,1,f);fwrite(data,1,n,f);
 }
 if(fclose(f))return 1;printf("Actual legacy int32 Substitute saves: %u\n",count);return 0;
}
