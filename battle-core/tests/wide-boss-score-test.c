/* Additive wide reader of the unchanged boss-score source protocol.
 * Original 255-bound reader remains unchanged; numeric API, not full turns. */
#include "pr/engine.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pr_battle bt;static pr_engine e;
static char line[512];static FILE *fp;
static void bad(void){fprintf(stderr,"malformed boss score trace\n");exit(2);}
static void read_line(void){if(!fgets(line,sizeof line,fp)||!strchr(line,'\n'))bad();}
int main(int argc,char **argv){
 if(argc!=2||(fp=fopen(argv[1],"r"))==NULL)return 2;
 int n;char extra;read_line();if(sscanf(line,"BOSS_SCORE 1 %d %c",&n,&extra)!=1||n!=1000)bad();
 int fails=0,flagged=0;const int classes[]={PR_MOD_ATTACK_TYPE_BOOSTER,PR_MOD_BASE_STAT,PR_MOD_BASE_STAT_TOTAL,PR_MOD_BASE_STAT_FLAT,PR_MOD_INCREMENTING_STAT,PR_MOD_TURN_HEAL};
 for(int i=0;i<n;i++){
  int id,sp,form,level,cap,wave,segs,nm,iv[6];double initial,want;
  read_line();if(sscanf(line,"C %d %d %d %d %d %d %d %lf %lf %d %c",&id,&sp,&form,&level,&cap,&wave,&segs,&initial,&want,&nm,&extra)!=10||id!=i||sp<1||sp>=PR_SPECIES_COUNT||form<0||form>1||level<1||level>1000||cap<1||wave<1||segs<0||segs>8||nm<0||nm>8||!isfinite(initial)||!isfinite(want)||initial<0||want<initial)bad();
  read_line();if(sscanf(line,"I %d %d %d %d %d %d %c",iv,iv+1,iv+2,iv+3,iv+4,iv+5,&extra)!=6)bad();for(int j=0;j<6;j++)if(iv[j]<0||iv[j]>31)bad();
  int mods[8][4];for(int j=0;j<nm;j++){read_line();if(sscanf(line,"M %d %d %d %d %c",mods[j],mods[j]+1,mods[j]+2,mods[j]+3,&extra)!=4||mods[j][0]<0||mods[j][0]>5||mods[j][1]<0||mods[j][1]>1||mods[j][2]<0||mods[j][2]>1||mods[j][3]<1||mods[j][3]>99)bad();}
  read_line();if(strcmp(line,"E\n"))bad();
  for(int caller=0;caller<2;caller++){
   memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;bt.engine=&e;bt.max_exp_level=cap;bt.wave_index=wave;bt.turn=1;bt.battle_score=initial;
   pr_pokemon a={0},d={0};a.id=1;a.species=SPECIES_ID_RATTATA;a.is_player=true;a.on_field=true;a.hp=a.stats[STAT_HP]=100;a.ability_override=ABILITY_ID_BALL_FETCH;
   d=a;d.id=2;d.is_player=false;d.species=sp;d.form=form;d.level=level;d.hp=caller?0:100;d.is_boss=segs!=0;d.boss_segments=segs;for(int j=0;j<6;j++)d.ivs[j]=iv[j];
   bt.field[0]=&a;bt.field[2]=&d;bt.party[0][0]=&a;bt.party[1][0]=&d;bt.n_party[0]=bt.n_party[1]=1;
   pr_seed_from_ascii(&bt.wave_seed,"score-wave");
   pr_seedstr seed;pr_seed_from_ascii(&seed,"score-helper");pr_rng_sow(&bt.rng,&seed);bt.rng_live=true;pr_seed_from_ascii(&bt.battle_seed,"score-battle");pr_rng oldrng=bt.rng;
   for(int j=0;j<nm;j++){int side=mods[j][1];bt.mods[side][bt.n_mods[side]++]=(pr_modifier){.cls=classes[mods[j][0]],.pokemon_id=mods[j][2]?2:1,.stack_count=mods[j][3],.move_type=0,.mult=0.2};}
   pr_battle expected=bt; if(!caller)(void)pr_battle_rand_seed_int(&expected,256,0);
   int ph=pr_phase_new(&e,caller?PH_FAINT:PH_ATTEMPT_CAPTURE,2);e.pool[ph].prevent_instant_revive=true;e.pool[ph].sw_slot=POKEBALL_TYPE_MASTER_BALL;pr_phase_run(&bt,&e,ph);
   if(bt.n_unported)flagged++;
   if(bt.battle_score!=want||e.overflow||memcmp(&oldrng,&bt.rng,sizeof oldrng)||bt.battle_rng_live!=expected.battle_rng_live||(bt.battle_rng_live&&(bt.battle_rng.c!=expected.battle_rng.c||bt.battle_rng.s0!=expected.battle_rng.s0||bt.battle_rng.s1!=expected.battle_rng.s1||bt.battle_rng.s2!=expected.battle_rng.s2))){if(fails<20)fprintf(stderr,"case%d caller%d score %.17g expected %.17g\n",i,caller,bt.battle_score,want);fails++;}
  }
 }
 read_line();int footer;if(sscanf(line,"BOSS_SCORE_COMPLETE %d %c",&footer,&extra)!=1||footer!=n||fgetc(fp)!=EOF)bad();
 printf("Boss score API cases%d actual callers%d exact%d mismatches%d separately unsupported contexts%d\n",n,2*n,2*n-fails,fails,flagged);return fails?1:0;
}
