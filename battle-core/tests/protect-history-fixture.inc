#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int pt_int(FILE*f,int lo,int hi){long long n;if(fscanf(f,"%lld",&n)!=1||n<lo||n>hi){fprintf(stderr,"invalid Protect integer\n");exit(2);}return(int)n;}
static void pt_word(FILE*f,const char*want){char word[48];if(fscanf(f,"%47s",word)!=1||strcmp(word,want)){fprintf(stderr,"expected Protect %s\n",want);exit(2);}}
typedef struct{pr_rng rng;int live;}pt_state;
static pt_state pt_states[32];static int pt_n_states,pt_turn;static char pt_seed[64];
static const int pt_moves[]={182,197,203,588,596,661,743,792,852,908};
static const int pt_history_moves[]={182,197,203,588,596,661,743,792,852,908,501,469};
static void pt_header(FILE*f){pt_word(f,"PROTECTHISTORY");pt_int(f,1,1);pt_int(f,1224,1224);pt_n_states=pt_int(f,1,32);pt_word(f,"BATTLE");if(fscanf(f,"%63s",pt_seed)!=1||strcmp(pt_seed,"protect-history-native"))exit(2);pt_turn=pt_int(f,13,13);
 for(int i=0;i<pt_n_states;i++){pt_word(f,"STATE");pt_int(f,i,i);pt_states[i].live=pt_int(f,0,1);if(pt_states[i].live){pr_rng*r=&pt_states[i].rng;if(fscanf(f,"%lf%lf%lf%lf",&r->c,&r->s0,&r->s1,&r->s2)!=4||!isfinite(r->c)||r->c<0||r->c>2091639||!isfinite(r->s0)||r->s0<0||r->s0>=1||!isfinite(r->s1)||r->s1<0||r->s1>=1||!isfinite(r->s2)||r->s2<0||r->s2>=1)exit(2);}}
}
static int pt_rngsame(const pr_rng*a,const pr_rng*b){return a->c==b->c&&a->s0==b->s0&&a->s1==b->s1&&a->s2==b->s2;}
static void pt_case(FILE*f,int i,pr_pokemon*p,int*history_move,int*mode){static const int lengths[]={0,1,7,8,9,19,20,33,34,40,100,645,646,647,648,65535,65536};pt_word(f,"CASE");pt_int(f,i,i);int n=pt_int(f,lengths[i/72],lengths[i/72]);*history_move=pt_int(f,pt_history_moves[i/6%12],pt_history_moves[i/6%12]);*mode=pt_int(f,1+i%6,1+i%6);
 pr_reset_summon_data(p);p->on_field=true;for(int k=0;k<n;k++)pr_push_move_history(p,(pr_turn_move){.move=*history_move,.use_mode=*mode,.result=MOVE_RESULT_SUCCESS,.n_targets=1,.targets={0}});
}
static void pt_stage(pr_pokemon*p,int stage,int history_move,int mode){if(stage==1)pr_push_move_history(p,(pr_turn_move){.move=history_move,.use_mode=mode,.result=MOVE_RESULT_FAIL,.n_targets=1,.targets={0}});if(stage==2)pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_QUICK_GUARD,.use_mode=mode,.result=MOVE_RESULT_SUCCESS,.n_targets=1,.targets={0}});if(stage==3)pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_SPLASH,.use_mode=mode,.result=MOVE_RESULT_SUCCESS,.n_targets=1,.targets={0}});if(stage==4)pr_reset_summon_data(p);}
static void pt_footer(FILE*f){pt_word(f,"COMPLETE");pt_int(f,1224,1224);char extra[2];if(fscanf(f,"%1s",extra)!=EOF)exit(2);}
static void pt_apply_state(pr_battle*bt,int before){bt->battle_rng=pt_states[before].rng;bt->battle_rng_live=pt_states[before].live;pr_seed_from_ascii(&bt->battle_seed,pt_seed);bt->turn=pt_turn;}
static int pt_after(const pr_battle*bt,int after){return bt->battle_rng_live==pt_states[after].live&&(!bt->battle_rng_live||pt_rngsame(&bt->battle_rng,&pt_states[after].rng));}
static void pt_gates(FILE*f){static pr_battle bt;static pr_pokemon p,t;static pr_engine e;pt_word(f,"GATES");pt_int(f,24,24);for(int i=0;i<24;i++){
 pt_word(f,"GATE");int queued=pt_int(f,i/12,i/12),move=pt_int(f,pt_history_moves[i%12],pt_history_moves[i%12]),expected=pt_int(f,0,1);
 memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.field[0]=&p;bt.field[2]=&t;bt.engine=&e;e.n_levels=1;p.on_field=t.on_field=true;p.hp=t.hp=100;
 if(queued){int ph=pr_phase_new(&e,PH_MOVE,2);e.pool[ph].pokemon=&t;e.pool[ph].move.id=MOVE_ID_SPLASH;pr_phase_push(&bt,&e,ph);}
 pr_rng before=bt.rng,battle=bt.battle_rng;int got=pr_move_apply_conditions(&bt,move,&p,&t,3);
 if(got!=expected||bt.n_unported||e.overflow||!pt_rngsame(&before,&bt.rng)||!pt_rngsame(&battle,&bt.battle_rng)){fprintf(stderr,"native Protect phase gate %d mismatch\n",i);exit(1);}
 }}
