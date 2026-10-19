/* Source counter-only projection of the actual owning cleanup. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static pr_battle bt;static pr_engine e;static pr_pokemon p[4];
int main(int argc,char **argv){
 if(argc!=2)return 2;FILE *f=fopen(argv[1],"r");if(!f)return 2;int version,n;char line[1024],extra;
 if(!fgets(line,sizeof line,f)||sscanf(line,"TERA_CLEANUP %d %d %c",&version,&n,&extra)!=2||version!=1||n<2048||n>100000)return 2;
 int fail=0;pr_closures_init();
 for(int i=0;i<n;i++){
  int id,kind,sp[4],fusion[4];double before,after;
  if(!fgets(line,sizeof line,f)||sscanf(line,"C %d %d %d %d %d %d %d %d %d %d %lf %lf %c",&id,&kind,&sp[0],&fusion[0],&sp[1],&fusion[1],&sp[2],&fusion[2],&sp[3],&fusion[3],&before,&after,&extra)!=12||id!=i||kind<0||kind>3||!isfinite(before)||before<0||floor(before)!=before||!isfinite(after)||after<0||floor(after)!=after)return 2;
  memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);memset(p,0,sizeof p);bt.engine=&e;e.current=-1;e.n_levels=1;bt.player_teras_used_known=true;bt.player_teras_used=before;
  for(int k=0;k<4;k++){if(sp[k]<=0||sp[k]>65535||fusion[k]<0||fusion[k]>65535)return 2;p[k].species=sp[k];if(!pr_pokemon_species(&p[k]))return 2;p[k].fusion_species=fusion[k];p[k].id=100+k;p[k].is_player=k<2;p[k].stats[0]=p[k].hp=1;p[k].ability_override=-1;bt.party[k/2][k%2]=&p[k];}bt.n_party[0]=bt.n_party[1]=2;
  pr_prepare_player_cleanup(&bt,&e,kind!=0);
  if(bt.player_teras_used!=after||!bt.player_teras_used_known){if(fail<5)fprintf(stderr,"counter case%d native%.17g C%.17g\n",i,after,bt.player_teras_used);fail++;}
 }
 int done;if(!fgets(line,sizeof line,f)||sscanf(line,"TERA_CLEANUP_COMPLETE %d %c",&done,&extra)!=1||done!=n||fgetc(f)!=EOF)return 2;
 printf("Tera cleanup counter: %d compared, %d failures\n",n,fail);return fail?1:0;
}
