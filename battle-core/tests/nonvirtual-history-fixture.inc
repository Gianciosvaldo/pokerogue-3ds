#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int nv_number(FILE *f,int lo,int hi){long long n;if(fscanf(f,"%lld",&n)!=1||n<lo||n>hi){fprintf(stderr,"invalid nonvirtual integer\n");exit(2);}return(int)n;}
static void nv_word(FILE*f,const char*want){char word[40];if(fscanf(f,"%39s",word)!=1||strcmp(word,want)){fprintf(stderr,"expected %s\n",want);exit(2);}}
static pr_turn_move nv_move(FILE*f){pr_turn_move m={0};m.move=nv_number(f,0,PR_MOVE_COUNT-1);m.use_mode=nv_number(f,1,6);m.result=nv_number(f,0,255);if(m.result>4&&m.result!=255)exit(2);m.n_targets=nv_number(f,0,4);for(int k=0;k<m.n_targets;k++)m.targets[k]=nv_number(f,0,3);return m;}
typedef struct{pr_pokemon user;pr_turn_move add,reset_add,expected[6][4];int present[6][4],owners[6][5],length;}nv_case;
static void nv_header(FILE*f){nv_word(f,"NVHISTORY");nv_number(f,1,1);nv_number(f,972,972);}
static void nv_read(FILE*f,int index,nv_case*c){
 memset(c,0,sizeof*c);nv_word(f,"CASE");nv_number(f,index,index);
 static const int lengths[]={0,1,7,8,9,64,65535,65536,65537};
 c->length=nv_number(f,lengths[index/108],lengths[index/108]);nv_number(f,1+index/18%6,1+index/18%6);
 const int first[]={MOVE_ID_NONE,MOVE_ID_TACKLE,MOVE_ID_STRUGGLE};nv_number(f,first[index/6%3],first[index/6%3]);nv_number(f,index%6,index%6);
 int runs=nv_number(f,0,2),total=0;c->user.on_field=true;
 for(int r=0;r<runs;r++){nv_word(f,"RUN");int n=nv_number(f,1,c->length-total);pr_turn_move m=nv_move(f);for(int k=0;k<n;k++)pr_push_move_history(&c->user,m);total+=n;}
 if(total!=c->length)exit(2);
 nv_word(f,"ADD");c->add=nv_move(f);nv_word(f,"RESETADD");c->reset_add=nv_move(f);
 for(int s=0;s<6;s++){nv_word(f,"STAGE");nv_number(f,s,s);for(int q=0;q<4;q++){nv_word(f,"RESULT");if((c->present[s][q]=nv_number(f,0,1)))c->expected[s][q]=nv_move(f);}
  nv_word(f,"OWNERS");for(int k=0;k<5;k++)c->owners[s][k]=nv_number(f,0,1);}
 nv_word(f,"END");
}
static int nv_equal(const pr_turn_move*a,const pr_turn_move*b){return a&&a->move==b->move&&a->use_mode==b->use_mode&&a->result==b->result&&a->n_targets==b->n_targets&&!memcmp(a->targets,b->targets,a->n_targets);}
static void nv_stage(pr_pokemon*p,const nv_case*c,int stage){if(stage==1)pr_push_move_history(p,c->add);if(stage==2)for(int k=0;k<9;k++)pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_SPLASH,.use_mode=MOVE_USE_MODE_INDIRECT,.targets={2},.n_targets=1,.result=4});if(stage==3)pr_reset_summon_data(p);if(stage==4)pr_push_move_history(p,c->reset_add);if(stage==5)for(int k=0;k<9;k++)pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=MOVE_USE_MODE_NORMAL,.targets={2},.n_targets=1,.result=4});}
static void nv_footer(FILE*f){nv_word(f,"COMPLETE");nv_number(f,972,972);char tail[2];if(fscanf(f,"%1s",tail)!=EOF)exit(2);}

static void nv_movesets(pr_pokemon*subject,pr_pokemon*other){
 const int a[]={MOVE_ID_TACKLE,MOVE_ID_SPLASH,MOVE_ID_STRUGGLE,MOVE_ID_GROWL},b[]={MOVE_ID_DISABLE,MOVE_ID_MIMIC,MOVE_ID_SKETCH,MOVE_ID_SPITE};
 subject->n_moves=other->n_moves=4;subject->hp=other->hp=100;other->on_field=true;
 for(int k=0;k<4;k++){subject->moves[k]=a[k];other->moves[k]=b[k];subject->max_pp_override[k]=other->max_pp_override[k]=-1;}
}
static const int nv_owner_moves[]={MOVE_ID_DISABLE,MOVE_ID_MIMIC,MOVE_ID_SKETCH,MOVE_ID_SPITE,MOVE_ID_INSTRUCT};

static void nv_aliases(FILE*f){
 nv_word(f,"ALIASES");nv_number(f,12,12);
 for(int i=0;i<12;i++){
  pr_pokemon p={0};pr_battle bt={0};static pr_engine e;memset(&e,0,sizeof e);p.on_field=true;
  nv_word(f,"ALIAS");nv_number(f,i,i);const int lengths[]={0,9,65536};int prefix=nv_number(f,lengths[i/4],lengths[i/4]);
  int mode=nv_number(f,i/2%2?MOVE_USE_MODE_FOLLOW_UP:MOVE_USE_MODE_NORMAL,i/2%2?MOVE_USE_MODE_FOLLOW_UP:MOVE_USE_MODE_NORMAL);
  int move=nv_number(f,i%2?MOVE_ID_STRUGGLE:MOVE_ID_TACKLE,i%2?MOVE_ID_STRUGGLE:MOVE_ID_TACKLE);
  for(int k=0;k<prefix;k++)pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=MOVE_USE_MODE_INDIRECT,.result=4});
  int8_t targets[]={2};int list=pr_targets_new(&e,targets,1);
  pr_push_history_shared(&bt,&e,&p,(pr_turn_move){.move=move,.use_mode=mode,.result=1},list);
  int fresh=-1;
  for(int s=0;s<7;s++){
   if(s==1){e.target_lists[list].v[0]=1;pr_targets_changed(&bt,&e,list);}
   if(s==2)for(int k=0;k<9;k++)pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=MOVE_USE_MODE_INDIRECT,.result=4});
   if(s==3){e.target_lists[list].v[1]=3;e.target_lists[list].n=2;pr_targets_changed(&bt,&e,list);}
   if(s==4){pr_reset_summon_data(&p);int8_t next[]={0};fresh=pr_targets_new(&e,next,1);pr_push_history_shared(&bt,&e,&p,(pr_turn_move){.move=MOVE_ID_GROWL,.use_mode=MOVE_USE_MODE_NORMAL,.result=1},fresh);for(int k=0;k<9;k++)pr_push_move_history(&p,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=MOVE_USE_MODE_INDIRECT,.result=4});}
   if(s==5){e.target_lists[list].v[0]=2;pr_targets_changed(&bt,&e,list);}
   if(s==6){e.target_lists[fresh].v[0]=3;pr_targets_changed(&bt,&e,fresh);}
   nv_word(f,"ASTAGE");nv_number(f,s,s);
   for(int q=0;q<4;q++){
    nv_word(f,"RESULT");int present=nv_number(f,0,1);pr_turn_move expected={0};if(present)expected=nv_move(f);
    bool unknown=true;bt.n_unported=0;const pr_turn_move*got=pr_last_non_virtual_move(&bt,&p,q/2,q%2,&unknown);
    if(unknown||bt.n_unported||((got!=NULL)!=present)||(got&&!nv_equal(got,&expected))){fprintf(stderr,"shared-target native mismatch case%d stage%d query%d\n",i,s,q);exit(1);}
   }
  }nv_word(f,"AEND");
 }
}
