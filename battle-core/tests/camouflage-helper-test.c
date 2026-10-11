/* Additional method probes for source UNKNOWN fallbacks and parent refusal.
 * They are not legal whole-turn inputs or part of the 8000 caller comparisons. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
static pr_battle bt;static pr_engine engine;
static int numbers(const char *p,int v[9]){
 for(int k=0;k<9;k++){char *end;errno=0;long n=strtol(p,&end,10);if(end==p||errno||n<INT_MIN||n>INT_MAX)return 0;v[k]=(int)n;p=end;}
 return !strcmp(p,"\n");
}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 FILE *f=fopen(argv[1],"r");if(!f)return 2;char line[256];
 if(!fgets(line,sizeof line,f)||strcmp(line,"CAMOUFLAGE_HELPERS 1 3\n"))return 2;
 int attr=-1;const pr_move *m=&pr_moves[MOVE_ID_CAMOUFLAGE];
 for(int k=m->attr0;k<m->attr0+m->nattr;k++)if(pr_attrs[k].cls==PR_AC_CopyBiomeTypeAttr)attr=k;
 if(attr<0)return 1;
 int failures=0;
 for(int ci=0;ci<3;ci++){
  int v[9];
  if(!fgets(line,sizeof line,f)||strncmp(line,"H ",2)||!numbers(line+2,v))return 2;
  int id=v[0],biome=v[1],terrain=v[2],hp=v[3],n=v[4],type=v[5],added=v[6],messages=v[7],result=v[8];
  if(id!=ci||biome< -1||biome>50||terrain<0||terrain>255||hp<0||hp>1||n!=1||type< -1||type>POKEMON_TYPE_STELLAR||added!=POKEMON_TYPE_GHOST||messages<0||messages>1||result<0||result>1)return 2;
  memset(&bt,0,sizeof bt);memset(&engine,0,sizeof engine);pr_pokemon user={0},target={0};
  user.id=1;user.species=SPECIES_ID_RATTATA;user.hp=hp;user.is_player=true;user.added_type=POKEMON_TYPE_GHOST;user.n_summon_types=1;user.summon_types[0]=POKEMON_TYPE_FIRE;
  target.id=2;target.species=SPECIES_ID_BULBASAUR;target.hp=1;target.n_summon_types=2;target.summon_types[0]=POKEMON_TYPE_GRASS;target.summon_types[1]=POKEMON_TYPE_WATER;pr_pokemon before=target;
  bt.field[0]=&user;bt.field[2]=&target;bt.has_biome=true;bt.biome=biome;bt.terrain=terrain;bt.engine=&engine;
  engine.pool[0].move_id=MOVE_ID_CAMOUFLAGE;engine.used[0]=true;
  pr_apply_move_effect(&bt,&engine,0,attr,&user,&target);int queued=0;
  for(int k=0;k<PR_MAX_PHASES;k++)if(engine.used[k]&&engine.pool[k].type==PH_MESSAGE)queued++;
  if(user.n_summon_types!=n||user.summon_types[0]!=type||user.added_type!=added||queued!=messages||result!=(hp>0)||memcmp(&target,&before,sizeof target))failures++;
 }
 if(!fgets(line,sizeof line,f)||strcmp(line,"END 3\n")||fgetc(f)!=EOF)return 2;
 fclose(f);printf("Camouflage helper probes3 identical%d failures%d\n",3-failures,failures);return failures?1:0;
}
