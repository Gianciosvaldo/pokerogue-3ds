/* Strict transport of the native history and four owner observations. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int lr_number(FILE *f,int lo,int hi){long long n;if(fscanf(f,"%lld",&n)!=1||n<lo||n>hi){fprintf(stderr,"invalid Last Resort integer\n");exit(2);}return (int)n;}
static void lr_word(FILE *f,const char *want){char word[40];if(fscanf(f,"%39s",word)!=1||strcmp(word,want)){fprintf(stderr,"invalid Last Resort record, expected %s\n",want);exit(2);}}
typedef struct {pr_pokemon user;int expected[5],length,shape,mode,future_move;} lr_case;
static void lr_header(FILE *f){lr_word(f,"LRHISTORY");lr_number(f,2,2);lr_number(f,432,432);}
static void lr_read(FILE *f,int index,lr_case *c){
 memset(c,0,sizeof *c);lr_word(f,"CASE");lr_number(f,index,index);
 c->shape=lr_number(f,0,7);c->length=lr_number(f,0,1024);c->mode=lr_number(f,1,6);
 int n=lr_number(f,1,4),active=lr_number(f,0,1);if(active!=(c->shape==7))exit(2);c->future_move=lr_number(f,0,PR_MOVE_COUNT-1);
 c->user.n_moves=n;c->user.summon_moves_active=active;lr_word(f,"SET");
 for(int k=0;k<n;k++){int move=lr_number(f,0,PR_MOVE_COUNT-1);c->user.moves[k]=move;c->user.transform_moves[k]=move;}
 c->user.on_field=true;pr_rebuild_history_used_moves(&c->user);
 for(int j=0;j<c->length;){
  lr_word(f,"RUN");int repeat=lr_number(f,1,c->length-j);pr_turn_move tm={0};tm.move=lr_number(f,0,PR_MOVE_COUNT-1);tm.use_mode=lr_number(f,1,6);tm.result=lr_number(f,0,255);tm.n_targets=lr_number(f,0,4);
  for(int k=0;k<tm.n_targets;k++)tm.targets[k]=lr_number(f,0,3);
  for(int k=0;k<repeat;k++)pr_push_move_history(&c->user,tm);j+=repeat;
 }
 lr_word(f,"RESULT");for(int k=0;k<5;k++)c->expected[k]=lr_number(f,0,1);lr_word(f,"END");
}
static void lr_footer(FILE *f){lr_word(f,"COMPLETE");lr_number(f,432,432);char trailing[2];if(fscanf(f,"%1s",trailing)!=EOF)exit(2);}
static void lr_changed(pr_pokemon *p){p->summon_moves_active=false;p->n_moves=2;p->moves[0]=MOVE_ID_LAST_RESORT;p->moves[1]=MOVE_ID_SPLASH;}
