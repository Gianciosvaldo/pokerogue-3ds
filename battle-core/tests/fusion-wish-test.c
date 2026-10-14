/* Native WishAttr -> two PositionalTagPhases, with Source-frozen names. */
#define main fusion_message_replay_main
#include "fusion-message-test.c"
#undef main
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 FILE *f=fopen(argv[1],"r");if(!f)return 2;
 char line[8192],extra;unsigned version,n,seed;
 if(!fgets(line,sizeof line,f)||sscanf(line,"FUSIONWISH %u %u %u %c",&version,&n,&seed,&extra)!=3||version!=1||n<1024||n>1000000)return 2;
 pr_closures_init();const pr_text_source src={locale_get,NULL,"en"};unsigned failed=0;
 for(unsigned i=0;i<n;i++) {
  unsigned idx;int v[13],maxhp,heal;char nh[2048],ah[2048],name[1024],message[1024],actual[2048];
  if(!fgets(line,sizeof line,f)||sscanf(line,"W %u %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %2047s %2047s %c",&idx,&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7],&v[8],&v[9],&v[10],&v[11],&v[12],&maxhp,&heal,nh,ah,&extra)!=18||idx!=i||maxhp<1||heal<1||!decode(nh,name,sizeof name)||!decode(ah,message,sizeof message))return 2;
  if(!valid_identity(v))return 2;
  pr_pokemon p={0};p.species=v[0];p.form=v[1];p.fusion_species=v[2];p.fusion_form=v[3];p.is_player=v[4];p.has_trainer=v[5];p.illusion.active=v[7];p.illusion.species=v[8];p.illusion.form=v[9];p.illusion.fusion_species=v[10];p.illusion.fusion_form=v[11];p.transformed=v[12];p.transform_species=SPECIES_ID_MAGIKARP;p.hp=p.stats[STAT_HP]=maxhp;
  p.on_field=true;pr_battle bt={0};bt.wave_index=v[6];bt.game_mode=GAME_MODES_CLASSIC;int slot=p.is_player?0:2;bt.field[slot]=&p;
  static pr_engine e;memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;
  int ph=pr_phase_new(&e,PH_MOVE_EFFECT,slot);e.pool[ph].move_id=MOVE_ID_WISH;
  const pr_move *move=&pr_moves[MOVE_ID_WISH];int attr=-1;for(int k=0;k<move->nattr;k++)if(pr_attrs[move->attr0+k].cls==PR_AC_WishAttr)attr=move->attr0+k;if(attr<0)return 2;
  pr_apply_move_effect(&bt,&e,ph,attr,&p,&p);
  if(bt.n_pos_tags!=1)return 1;
  pr_msg initial=pr_msg1("battle:singleWildAppeared",(pr_msg_arg){"pokemonName",PR_ARG_POKEMON,bt.pos_tags[0].pokemon_name,NULL,bt.pos_tags[0].fusion_name});pr_msg expected=pr_msg1(initial.key,(pr_msg_arg){"pokemonName",PR_ARG_STR,0,name,0});char want[2048];pr_msg_format(&initial,&src,actual,sizeof actual);pr_msg_format(&expected,&src,want,sizeof want);if(strcmp(actual,want))failed++;
  p.species=SPECIES_ID_MAGIKARP;p.form=0;p.fusion_species=0;p.illusion.active=false;
  for(int k=0;k<2;k++){int pos=pr_phase_new(&e,PH_POSITIONAL_TAG,-1);e.current=pos;pr_phase_run(&bt,&e,pos);if(!k&&(bt.n_pos_tags!=1||bt.pos_tags[0].turn_count!=1))return 1;}
  int found=0;for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){pr_phase *q=&e.pool[e.levels[l][k]];if(q->type!=PH_POKEMON_HEAL)continue;found++;pr_msg_format(&q->msg,&src,actual,sizeof actual);if(q->heal_hp!=heal||q->battler!=slot||strcmp(actual,message)){if(failed<12)fprintf(stderr,"wish case%u expected [%s] got [%s] hp%g/%d\n",i,message,actual,q->heal_hp,heal);failed++;}}
  if(found!=1||bt.n_pos_tags||bt.n_unported||e.overflow)return 1;
 }
 unsigned done;if(!fgets(line,sizeof line,f)||sscanf(line,"FUSIONWISH_COMPLETE %u %c",&done,&extra)!=1||done!=n||fgetc(f)!=EOF)return 2;fclose(f);printf("fusion Wish: %u cases, %u failures (seed %u)\n",n,failed,seed);return failed?1:0;
}
