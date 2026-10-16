/* Historical fixture generator: compile against merge815ceee, never a wide ABI. */
#include "../../src/internal.h"
#include <stdio.h>
static gc_run run;
static unsigned char data[GC_SAVE_MAX];
int main(int argc,char **argv){
 if(argc!=2||sizeof run.party[0].b.level!=1||sizeof run.party[0].met_level!=1)return 2;
 gc_run_new(&run,GAME_MODES_CLASSIC,"wide-legacy-save",777);
 gc_starter starters[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_TACKLE}},{.species=SPECIES_ID_CHARMANDER,.tera_type=-1,.moves={MOVE_ID_SCRATCH}}};
 for(int k=0;k<2;k++)memset(starters[k].ivs,15,sizeof starters[k].ivs);
 const char *why=NULL;if(!gc_run_set_starters(&run,starters,2,&why))return 2;gc_event ev;while(gc_run_next_event(&run,&ev)){}
 for(int k=0;k<2;k++){gc_mon *p=&run.party[k];p->b.level=k?127:255;p->met_level=k?127:255;p->exp=gc_level_total_exp(p->b.level,gc_species(p->b.species)->growth_rate);pr_calculate_stats(&run.bt,&p->b);p->b.hp=p->b.stats[STAT_HP];}
 if(!gc_run_can_save(&run))return 2;size_t n=gc_run_save(&run,data,sizeof data);if(!n)return 2;
 FILE *f=fopen(argv[1],"wb");if(!f||fwrite(data,1,n,f)!=n||fclose(f))return 2;
 printf("native legacy save: level-size%zu met-level-size%zu bytes%zu\n",sizeof run.party[0].b.level,sizeof run.party[0].met_level,n);return 0;
}
