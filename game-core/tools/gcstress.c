/* Additive host diagnostic. Only fixture setup bypasses starter selection.
 * All later actions use gc_run_choose and the real phase queue. */
#define _POSIX_C_SOURCE 200809L
#include "../src/internal.h"
#include "endgame-diagnostic.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
typedef struct {
 int wave,turns,screen,battle,enemy,enemy_level,enemy_hp,enemy_max_hp,enemy_segment,enemy_segments,enemy_moves[4],alive,pp,steps,saves,status;
 int switches,balls,rewards,shops,refusals,screen_clear;
 uint32_t digest,save_max;
 uint32_t stubs[GC_STUB_COUNT_],unported[GC_MAX_UNPORTED],run_core[8];
 char reason[96];
} diag_result;
static const int diag_species[6]={SPECIES_ID_MEWTWO,SPECIES_ID_KYOGRE,SPECIES_ID_GROUDON,SPECIES_ID_RAYQUAZA,SPECIES_ID_METAGROSS,SPECIES_ID_BLISSEY};
static const uint16_t diag_moves[6][4]={
 {MOVE_ID_PSYCHIC,MOVE_ID_AURA_SPHERE,MOVE_ID_ICE_BEAM,MOVE_ID_RECOVER},
 {MOVE_ID_SURF,MOVE_ID_THUNDERBOLT,MOVE_ID_ICE_BEAM,MOVE_ID_EARTHQUAKE},
 {MOVE_ID_EARTHQUAKE,MOVE_ID_FLAMETHROWER,MOVE_ID_DRAGON_CLAW,MOVE_ID_THUNDERBOLT},
 {MOVE_ID_DRAGON_CLAW,MOVE_ID_EARTHQUAKE,MOVE_ID_FLAMETHROWER,MOVE_ID_ICE_BEAM},
 {MOVE_ID_METEOR_MASH,MOVE_ID_ZEN_HEADBUTT,MOVE_ID_EARTHQUAKE,MOVE_ID_BULLET_PUNCH},
 {MOVE_ID_FLAMETHROWER,MOVE_ID_ICE_BEAM,MOVE_ID_THUNDERBOLT,MOVE_ID_SOFT_BOILED}};
static void diag_setup(gc_run *run,gc_system *sys,int index){
 char seed[32];snprintf(seed,sizeof seed,"endgame%08d",index);gc_run_new(run,GAME_MODES_CLASSIC,seed,(uint32_t)index);gc_system_init_defaults(sys);run->sys=sys;
 /* Explicit one-time fixture: six level-200, IV31, neutral-nature Pokemon,
  * ordinary abilities/PP, no held items; 10,000 money, 50 Ultra +25 Master balls. */
 for(int i=0;i<6;i++){gc_starter s={.species=diag_species[i],.tera_type=-1};memset(s.ivs,31,6);memcpy(s.moves,diag_moves[i],sizeof s.moves);gc_mon_new_player(run,&run->party[i],&s,200);run->party_order[i]=i;}
 run->n_party=6;run->money=10000;run->pokeballs[POKEBALL_TYPE_ULTRA_BALL]=50;run->pokeballs[POKEBALL_TYPE_MASTER_BALL]=25;
 memset(&run->screen,0,sizeof run->screen);gc_rq_push(run,gc_rp(RP_NEW_BATTLE,0,0,0));gc_run_until_screen(run);
}
static void fold(uint32_t *h,uint32_t n){*h=(*h^n)*16777619u;}
static void diag_snapshot(const gc_run *run,diag_result *r){
 r->wave=run->wave;r->screen=run->screen.kind;r->battle=run->battle_type;r->alive=r->pp=0;
 const gc_mon *enemy=gc_field_mon(run,1);if(enemy){r->enemy=enemy->b.species;r->enemy_level=enemy->b.level;r->enemy_hp=enemy->b.hp;r->enemy_max_hp=enemy->b.stats[0];r->enemy_segment=enemy->b.boss_segment_index;r->enemy_segments=enemy->b.boss_segments;for(int i=0;i<4;i++)r->enemy_moves[i]=enemy->b.moves[i];}
 for(int i=0;i<run->n_party;i++){const pr_pokemon *p=&run->party[i].b;r->alive+=p->hp>0;for(int j=0;j<p->n_moves;j++){int n=gc_move_max_pp(p,j)-p->pp_used[j];if(n>0)r->pp+=n;}}
 memcpy(r->stubs,run->stubs,sizeof r->stubs);memcpy(r->unported,run->unported,sizeof r->unported);memcpy(r->run_core,run->run_core_unported,sizeof r->run_core);
}
static void diag_one(int index,int saved,int steps,int fd,diag_result *r){
 static gc_run run;static gc_system sys;static uint8_t buf[GC_SAVE_MAX],sysbuf[GC_SAVE_MAX];diag_setup(&run,&sys,index);r->digest=2166136261u;
 int saved_wave=-1,profile_wave=-1,reward_wave=-1,attempts=0,last_snapshot=-1;const char *why=NULL;
 for(int step=0;step<steps;step++){
  r->steps=step;gc_event ev;while(gc_run_next_event(&run,&ev)){if(ev.kind==GC_EV_TURN)r->turns++;}
  diag_snapshot(&run,r);
  if(last_snapshot!=run.wave){last_snapshot=run.wave;pwrite(fd,r,sizeof *r,0);}
  if(run.screen.kind==GC_SCREEN_CLEAR||run.screen.kind==GC_SCREEN_GAME_OVER){r->screen_clear=run.screen.kind==GC_SCREEN_CLEAR;r->status=0;snprintf(r->reason,sizeof r->reason,"%s",r->screen_clear?"port-clear-screen":"party-defeat");break;}
  if(run.wave!=profile_wave&&gc_run_can_save(&run)){
   profile_wave=run.wave;size_t nr=gc_run_save(&run,buf,sizeof buf),ns=gc_system_save(&sys,sysbuf,sizeof sysbuf);
   if(!nr||!ns){r->status=1;snprintf(r->reason,sizeof r->reason,"comparison-save-refused");break;}
   fold(&r->digest,nr);for(size_t i=0;i<nr;i++)fold(&r->digest,buf[i]);fold(&r->digest,ns);for(size_t i=0;i<ns;i++)fold(&r->digest,sysbuf[i]);
  }
  if(saved&&run.wave!=saved_wave&&gc_run_can_save(&run)){
   size_t n=gc_run_save(&run,buf,sizeof buf);if(!n){r->status=1;snprintf(r->reason,sizeof r->reason,"save-refused");break;}
   saved_wave=run.wave;r->saves++;if(n>r->save_max)r->save_max=n;memset(&run,0xa5,sizeof run);
   if(!gc_run_load(&run,buf,n,&why)||run.load_dropped){r->status=1;snprintf(r->reason,sizeof r->reason,"load:%s",why?why:"dropped fields");break;}run.sys=&sys;
  }
  if(reward_wave!=run.wave){reward_wave=run.wave;attempts=0;}
  gc_choice choice;if(!diag_choice(&run,&choice)){r->status=1;snprintf(r->reason,sizeof r->reason,"no-diagnostic-choice");break;}
  if(run.screen.kind==GC_SCREEN_REWARD&&++attempts>24){choice=(gc_choice){.kind=GC_CH_SKIP};r->refusals++;}
  /* A rejected reward may be the pinned paid-mutation/error path. Preserve its
   * real side effects and then use the legal skip, never repair state. */
  if(!gc_run_choose(&run,&choice,&why)){
   r->refusals++;
   if(run.screen.kind!=GC_SCREEN_REWARD){r->status=1;snprintf(r->reason,sizeof r->reason,"choice:%s",why?why:"refused");break;}
   choice=(gc_choice){.kind=GC_CH_SKIP};if(!gc_run_choose(&run,&choice,&why)){r->status=1;snprintf(r->reason,sizeof r->reason,"skip:%s",why?why:"refused");break;}
  }
  r->switches+=choice.kind==GC_CH_POKEMON||choice.kind==GC_CH_PICK;r->balls+=choice.kind==GC_CH_BALL;r->rewards+=choice.kind==GC_CH_REWARD;r->shops+=choice.kind==GC_CH_SHOP;
  fold(&r->digest,run.wave);fold(&r->digest,run.score);uint64_t wallet;memcpy(&wallet,&run.money,sizeof wallet);fold(&r->digest,wallet);fold(&r->digest,wallet>>32);
  fold(&r->digest,choice.kind);fold(&r->digest,choice.a);fold(&r->digest,choice.b);fold(&r->digest,choice.c);
  for(int i=0;i<run.n_party;i++){const pr_pokemon *p=&run.party[i].b;fold(&r->digest,p->id);fold(&r->digest,p->species);fold(&r->digest,p->hp);fold(&r->digest,p->status);fold(&r->digest,p->level);for(int j=0;j<4;j++){fold(&r->digest,p->moves[j]);fold(&r->digest,p->pp_used[j]);}}
  double rngs[8]={run.run.rng.c,run.run.rng.s0,run.run.rng.s1,run.run.rng.s2,run.bt.rng.c,run.bt.rng.s0,run.bt.rng.s1,run.bt.rng.s2};for(int i=0;i<8;i++){uint64_t bits;memcpy(&bits,&rngs[i],sizeof bits);fold(&r->digest,bits);fold(&r->digest,bits>>32);}
  r->steps=step+1;
  if(step+1==steps){r->status=1;snprintf(r->reason,sizeof r->reason,"step-limit");}
 }
 diag_snapshot(&run,r);size_t ns=gc_system_save(&sys,sysbuf,sizeof sysbuf);fold(&r->digest,ns);for(size_t i=0;i<ns;i++)fold(&r->digest,sysbuf[i]);for(int i=0;i<GC_STUB_COUNT_;i++)fold(&r->digest,r->stubs[i]);
  for(int i=0;i<GC_MAX_UNPORTED;i++)fold(&r->digest,r->unported[i]);
 pwrite(fd,r,sizeof *r,0);
}
static bool diag_integer(const char *s,int min,int max,int *out){char *end;errno=0;long n=strtol(s,&end,10);if(!*s||*end||errno||n<min||n>max)return false;*out=n;return true;}
int main(int argc,char **argv){
 int n=100,first=0,jobs=4,timeout=60,steps=200000,saved=0;
 for(int i=1;i<argc;i++){
  if(!strcmp(argv[i],"--save-test")){saved=1;continue;}
  int *target=NULL,min=1,max=100000;
  if(!strcmp(argv[i],"--runs"))target=&n;else if(!strcmp(argv[i],"--first")){target=&first;min=0;max=1000000;}else if(!strcmp(argv[i],"--jobs")){target=&jobs;max=64;}else if(!strcmp(argv[i],"--timeout")){target=&timeout;max=3600;}else if(!strcmp(argv[i],"--steps")){target=&steps;max=1000000;}
  if(!target||i+1>=argc||!diag_integer(argv[++i],min,max,target)){fprintf(stderr,"usage: gcstress [--runs 1..100000] [--first 0..1000000] [--jobs 1..64] [--timeout 1..3600] [--steps 1..1000000] [--save-test]\n");return 2;}
 }
 if(first>1000000-(n-1))return 2;
 printf("fixture v1: wave1, six level200 IV31 neutral mons (150,382,383,384,376,242), ordinary abilities/PP, no held stock; money10000, Poke5/Ultra50/Master25; no subsequent direct state repairs\n");fflush(stdout);
 struct {pid_t pid;int fd,index;} slot[64];int active=0,next=0,ended=0,stuck=0,crashed=0,timed=0,reached=0,clear=0,totalturns=0,saves=0;uint32_t digest=0,maxsave=0,stubs[GC_STUB_COUNT_]={0},unported[GC_MAX_UNPORTED]={0},rc[8]={0};
 while(next<n||active){
  while(next<n&&active<jobs){char path[]="/tmp/gcstress-XXXXXX";int fd=mkstemp(path);if(fd<0){perror("mkstemp");return 2;}unlink(path);diag_result initial={.status=3};pwrite(fd,&initial,sizeof initial,0);pid_t pid=fork();if(pid<0){close(fd);perror("fork");return 2;}
   if(!pid){alarm(timeout);diag_result r={.status=3};diag_one(first+next,saved,steps,fd,&r);close(fd);_exit(0);}
   slot[active].pid=pid;slot[active].fd=fd;slot[active].index=first+next;active++;next++;
  }
  int status;pid_t done=wait(&status);if(done<0){if(errno==EINTR)continue;perror("wait");return 2;}int k=0;while(k<active&&slot[k].pid!=done)k++;if(k==active)continue;
  diag_result r={0};ssize_t got=pread(slot[k].fd,&r,sizeof r,0);close(slot[k].fd);int index=slot[k].index;slot[k]=slot[--active];
  if(WIFSIGNALED(status)){r.status=WTERMSIG(status)==SIGALRM?3:2;snprintf(r.reason,sizeof r.reason,"signal-%d-last-wave-snapshot",WTERMSIG(status));}else if(got!=sizeof r||!WIFEXITED(status)||WEXITSTATUS(status)){r.status=2;snprintf(r.reason,sizeof r.reason,"worker-transport");}
  ended+=r.status==0;stuck+=r.status==1;crashed+=r.status==2;timed+=r.status==3;reached+=r.wave>=100;clear+=r.screen_clear;totalturns+=r.turns;saves+=r.saves;if(r.save_max>maxsave)maxsave=r.save_max;digest^=r.digest;
  printf("run %d seed endgame%08d status%d wave%d turns%d steps%d screen%d battle%d enemy%d level%d enemy_hp%d/%d boss%d/%d moves%d,%d,%d,%d alive%d pp%d switches%d balls%d rewards%d shops%d refusals%d reason=%s digest=%08x\n",index,index,r.status,r.wave,r.turns,r.steps,r.screen,r.battle,r.enemy,r.enemy_level,r.enemy_hp,r.enemy_max_hp,r.enemy_segment,r.enemy_segments,r.enemy_moves[0],r.enemy_moves[1],r.enemy_moves[2],r.enemy_moves[3],r.alive,r.pp,r.switches,r.balls,r.rewards,r.shops,r.refusals,r.reason,r.digest);
  for(int i=0;i<GC_STUB_COUNT_;i++)stubs[i]+=r.stubs[i];
  for(int i=0;i<GC_MAX_UNPORTED;i++)unported[i]+=r.unported[i];
  for(int i=0;i<8;i++)rc[i]+=r.run_core[i];
 }
 printf("diagnostic %d: reached100=%d ended=%d port_clear_screens=%d stuck=%d crashed=%d timeout=%d turns=%d digest=%08x\n",n,reached,ended,clear,stuck,crashed,timed,totalturns,digest);
 if(saved)printf("diagnostic save/load: roundtrips=%d maxbytes=%u\n",saves,maxsave);
 for(int i=0;i<GC_STUB_COUNT_;i++)if(stubs[i])printf("stub %s %u\n",gc_stub_name[i],stubs[i]);
  for(int i=0;i<8;i++)if(rc[i])printf("run-core flag%d %u\n",i,rc[i]);
  for(int i=0;i<GC_MAX_UNPORTED;i++)if(unported[i])printf("unported %s %u\n",pr_ai_unported_name(i),unported[i]);
 return crashed||timed||stuck?1:0;
}
