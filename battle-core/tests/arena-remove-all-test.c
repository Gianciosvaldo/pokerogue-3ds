/* Strict complete-fixture guard for actual standard arena removal callbacks. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
int main(int argc,char **argv)
{
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input)return 2;
 double h[2];read_vector("ARENA_REMOVE ",h,2);if(h[0]!=1)bad();int n=integer(h[1],64,100000),failures=0;
 for(int c=0;c<n;c++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;bt.engine=&e;bt.ignoring_effect_source=-1;
  pr_pokemon p[2]={{0}};double in[4];read_vector("C ",in,4);if(in[0]!=c)bad();int count=integer(in[3],1,PR_MAX_ARENA_TAGS);
  for(int b=0;b<2;b++){p[b].species=b?SPECIES_ID_MAGIKARP:SPECIES_ID_RATTATA;if(in[b+1]<0||in[b+1]>UINT32_MAX||in[b+1]!=floor(in[b+1]))bad();p[b].id=(uint32_t)in[b+1];p[b].hp=1;p[b].is_player=b==0;p[b].on_field=true;bt.field[b*2]=bt.party[b][0]=&p[b];bt.n_party[b]=1;}
  bt.n_arena_tags=count;
  for(int j=0;j<count;j++){
   double a[6];read_vector("A ",a,6);pr_arena_tag *t=&bt.arena_tags[j];t->type=integer(a[0],1,ARENA_TAG_TYPE_PENDING_HEAL);t->side=integer(a[1],0,2);t->turn_count=integer(a[2],0,100);t->max_duration=integer(a[3],0,100);t->source_move=integer(a[4],0,PR_MOVE_COUNT-1);t->source_id=(int64_t)a[5];if(a[5]<-1||a[5]>UINT32_MAX||a[5]!=t->source_id||t->type==ARENA_TAG_TYPE_NEUTRALIZING_GAS||t->type==ARENA_TAG_TYPE_IMPRISON||t->type==ARENA_TAG_TYPE_PENDING_HEAL)bad();
  }
  pr_arena_remove_all_tags(&bt,&e);bool ok=!bt.n_arena_tags&&!bt.n_unported&&!e.overflow;
  double q[1];read_vector("Q ",q,1);int expected=integer(q[0],0,PR_MAX_ARENA_TAGS),got=0;
  for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++){
   pr_phase *ph=&e.pool[e.levels[l][j]];if(ph->type!=PH_MESSAGE)bad();char text[1024],hex[2048];pr_text_source src={locale_get,NULL,"en"};size_t len=pr_msg_format(&ph->msg,&src,text,sizeof text);if(len>=sizeof text)bad();for(size_t k=0;k<len;k++)snprintf(hex+2*k,3,"%02x",(unsigned char)text[k]);hex[2*len]=0;
   if(got++>=expected){ok=false;continue;}char *want=line_read("T ");want[strcspn(want,"\n")]=0;if(strlen(want)%2||strspn(want,"0123456789abcdef")!=strlen(want))bad();if(strcmp(hex,want)){if(c<20)fprintf(stderr,"case%d bulk text C%s JS%s\n",c,hex,want);ok=false;}
  }
  while(got<expected){line_read("T ");got++;ok=false;}
  if(strcmp(line_read("E"),"\n"))bad();if(!ok)failures++;
 }
 double footer[1];read_vector("ARENA_REMOVE_COMPLETE ",footer,1);if(footer[0]!=n||fgetc(input)!=EOF)bad();
 printf("arena bulk removal: %d complete, %d exact, %d failures\n",n,n-failures,failures);return failures?1:0;
}
