/* Actual source permanent stats at explicitly bounded levels. */
#include "pr/battle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
static FILE *fp;static char line[512];static pr_battle bt;
static void bad(void){fprintf(stderr,"malformed wide-level trace: %s",line);exit(2);}
static void row(void){
 if(!fgets(line,sizeof line,fp)||!strchr(line,'\n'))bad();
 char *p=strchr(line,' ');
 while(p){while(isspace((unsigned char)*p))p++;if(!*p)break;
  char *end;errno=0;long long v=strtoll(p,&end,10);
  if(errno||end==p||v<INT32_MIN||v>INT32_MAX||(*end&&!isspace((unsigned char)*end)))bad();
  p=end;
 }
}
int main(int argc,char **argv){
 if(argc!=2||!(fp=fopen(argv[1],"r")))return 2;
 int n;char tail;row();if(sscanf(line,"WIDE_LEVEL 1 %d %c",&n,&tail)!=1||n!=16800)bad();
 int fail=0,seen[14]={0},natures[25]={0},modes[4]={0},hps[4]={0},fusions[2]={0};
 const int levels[]={1,199,200,201,254,255,256,257,511,1000,65535,65536,100000,1000000};
 for(int i=0;i<n;i++){
  int id,mode,sp,form,fusion,level,nature,raw,passive,hpMode,player,iv[6],want[7];row();
  if(sscanf(line,"C %d %d %d %d %d %d %d %d %d %d %d %c",&id,&mode,&sp,&form,&fusion,&level,&nature,&raw,&passive,&hpMode,&player,&tail)!=11||id!=i||(mode!=GAME_MODES_ENDLESS&&mode!=GAME_MODES_SPLICED_ENDLESS)||sp<=0||sp>=PR_SPECIES_COUNT||form<0||(fusion!=0&&fusion!=SPECIES_ID_PIKACHU)||nature<0||nature>=25||raw<0||raw>=PR_ABILITY_COUNT||(passive!=0&&passive!=ABILITY_ID_WONDER_GUARD)||hpMode<0||hpMode>3||player<0||player>1)bad();
  pr_pokemon identity={0};identity.species=sp;const pr_species *species=pr_pokemon_species(&identity);if(!species||form>=species->nforms)bad();
  int li=-1;for(int j=0;j<14;j++)if(level==levels[j])li=j;if(li<0)bad();seen[li]++;natures[nature]++;modes[mode]++;hps[hpMode]++;fusions[!!fusion]++;
  row();if(sscanf(line,"I %d %d %d %d %d %d %c",iv,iv+1,iv+2,iv+3,iv+4,iv+5,&tail)!=6)bad();for(int j=0;j<6;j++)if(iv[j]<0||iv[j]>31)bad();
  row();if(sscanf(line,"O %d %d %d %d %d %d %d %c",want,want+1,want+2,want+3,want+4,want+5,want+6,&tail)!=7)bad();for(int j=0;j<7;j++)if(want[j]<0||(j&&want[j]<1))bad();
  row();if(strcmp(line,"E\n"))bad();
  memset(&bt,0,sizeof bt);bt.game_mode=mode;pr_pokemon p={0};p.species=sp;p.form=form;p.fusion_species=fusion;p.level=level;p.nature=nature;p.base_ability=p.ability=raw;p.passive=passive;p.is_player=player;p.stats[STAT_HP]=123;
  for(int j=0;j<6;j++)p.ivs[j]=iv[j];
  p.hp=hpMode==1?0:hpMode==2?60:2000000000;
  pr_seedstr s;pr_seed_from_ascii(&s,"wide-level-C");pr_rng_sow(&bt.rng,&s);bt.rng_live=true;pr_rng before=bt.rng;
  if(!hpMode)pr_calculate_stats(&bt,&p);else pr_recalculate_stats(&bt,&p);
  bool match=p.hp==want[0]&&!bt.n_unported&&!memcmp(&before,&bt.rng,sizeof before)&&!bt.battle_rng_live;
  for(int j=0;j<6;j++)if(p.stats[j]!=want[j+1])match=false;
  if(!match){if(fail<10)fprintf(stderr,"wide stats case%d level%d hp%d/%d atk%d/%d\n",i,level,p.hp,want[0],p.stats[STAT_ATK],want[2]);fail++;}
 }
 row();int end;if(sscanf(line,"WIDE_LEVEL_COMPLETE %d %c",&end,&tail)!=1||end!=n||fgetc(fp)!=EOF)bad();
 for(int j=0;j<14;j++)if(!seen[j])bad();
 for(int j=0;j<25;j++)if(!natures[j])bad();
 for(int j=0;j<4;j++)if(!hps[j])bad();
 if(!modes[GAME_MODES_ENDLESS]||!modes[GAME_MODES_SPLICED_ENDLESS]||!fusions[0]||!fusions[1])bad();
 printf("Wide permanent stats: %d complete, %d exact, %d failures\n",n,n-fail,fail);return fail?1:0;
}
