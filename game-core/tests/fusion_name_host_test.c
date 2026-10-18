/* Actual sacrifice, faint pick, entry application, legal saves and arena reset. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX],mutated[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"fusion host line%d: %s (screen%d, wave%d)\n",__LINE__,#x,run.screen.kind,run.wave);exit(1);}}while(0)
static void drain(void){gc_event e;while(gc_run_next_event(&run,&e)){} }
static void choose(gc_choice c){const char *why=NULL;CHECK(gc_run_choose(&run,&c,&why));drain();}
static const pr_arena_tag *tag(const gc_run *r){return pr_arena_get_tag_on_side(&r->bt,ARENA_TAG_TYPE_PENDING_HEAL,ARENA_TAG_SIDE_BOTH);}
static uint32_t hashname(const char *s){uint32_t h=2166136261u;for(;*s;s++)h=(h^(unsigned char)*s)*16777619u;return h;}
static uint32_t crc32(const unsigned char *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=c>>1^(0xedb88320u&(0u-(c&1)));}return ~c;}
static unsigned read16(size_t at){uint16_t x;memcpy(&x,saved+at,2);return x;}
static uint32_t read32(size_t at){uint32_t x;memcpy(&x,saved+at,4);return x;}
static size_t pending_section(size_t n){for(size_t at=20;at+8<=n;at+=8+read32(at+4))if(read32(at)==hashname("pending-heals"))return at;CHECK(false);return 0;}
static size_t pending_field(size_t n,const char *name,bool descriptor){
 size_t at=pending_section(n)+8;unsigned tags=read16(at);at+=2;
 for(unsigned t=0;t<tags;t++){unsigned nf=read16(at);at+=2;size_t off=0,found=0,desc=0;for(unsigned f=0;f<nf;f++){if(read32(at)==hashname(name)){found=off;desc=at;}off+=read16(at+4);at+=6;}unsigned count=read16(at);at+=2;if(count&&desc)return descriptor?desc:at+found;at+=off*count;}
 CHECK(false);return 0;
}
static size_t pos_field(size_t n,const char *name,bool descriptor) {
 size_t at=20;while(at+8<=n&&read32(at)!=hashname("arena"))at+=8+read32(at+4);CHECK(at+8<=n);at+=8;
 unsigned nf=read16(at);at+=2;size_t rec=0;for(unsigned i=0;i<nf;i++){rec+=read16(at+4);at+=6;}unsigned count=read16(at);at+=2+rec*count;
 nf=read16(at);at+=2;size_t off=0,found=0,desc=0;for(unsigned i=0;i<nf;i++){if(read32(at)==hashname(name)){found=off;desc=at;}off+=read16(at+4);at+=6;}count=read16(at);at+=2;CHECK(count&&desc);return descriptor?desc:at+found;
}
static void damaged_wish_checks(size_t n) {
 const char *why=NULL;
 for(int i=0;i<4;i++){memcpy(mutated,saved,n);size_t at=pos_field(n,i%2?"fusion_name":"pokemon_name",i>=2);uint32_t bad=i>=2?hashname("missing-wish-name"):(i?pr_msg_pokemon(65535,0,0):0);memcpy(mutated+at,&bad,4);uint32_t crc=crc32(mutated+20,n-20);memcpy(mutated+16,&crc,4);CHECK(!gc_run_load(&loaded,mutated,n,&why));CHECK(why&&strstr(why,"wish effects"));}
}
static void damaged_save_checks(size_t n){
 const char *why=NULL;const char *fields[]={"target_index","restore_pp","move_id","pokemon_name","target_index",NULL,"fusion_name","fusion_name"};
 for(int k=0;k<8;k++){
  memcpy(mutated,saved,n);size_t at=fields[k]?pending_field(n,fields[k],k==4||k==7):pending_section(n);
  if(k<2)mutated[at]=k?2:4;else if(k==2){uint16_t bad=65535;memcpy(mutated+at,&bad,2);}else if(k==3){uint32_t bad=0;memcpy(mutated+at,&bad,4);}else if(k==6){uint32_t bad=pr_msg_pokemon(65535,0,0);memcpy(mutated+at,&bad,4);}else{uint32_t hidden=hashname("missing-pending-state");memcpy(mutated+at,&hidden,4);}
  uint32_t crc=crc32(mutated+20,n-20);memcpy(mutated+16,&crc,4);CHECK(!gc_run_load(&loaded,mutated,n,&why));CHECK(why&&strstr(why,"healing effects"));
 }
}
static void roundtrip(void){const char *why=NULL;CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));const pr_arena_tag *a=tag(&run),*b=tag(&loaded);CHECK(a&&b&&a->n_pending_heals==1&&b->n_pending_heals==1);CHECK(!memcmp(a->pending_heals,b->pending_heals,sizeof a->pending_heals[0]));CHECK(!loaded.load_dropped);CHECK(loaded.bt.n_pos_tags==run.bt.n_pos_tags);CHECK(loaded.bt.pos_tags[0].pokemon_name==run.bt.pos_tags[0].pokemon_name&&loaded.bt.pos_tags[0].fusion_name==run.bt.pos_tags[0].fusion_name);damaged_save_checks(n);damaged_wish_checks(n);}
int main(int argc,char **argv){
 if(argc!=1&&argc!=3)return 2;
 const char *why=NULL;
 for(int lunar=0;lunar<2;lunar++)for(int state=0;state<4;state++){
  gc_run_new(&run,GAME_MODES_CLASSIC,"pending-heal-commands",777);gc_starter ss[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_HEALING_WISH,MOVE_ID_TACKLE}},{.species=SPECIES_ID_SQUIRTLE,.tera_type=-1,.moves={MOVE_ID_TACKLE,MOVE_ID_SPLASH}}};for(int k=0;k<2;k++)memset(ss[k].ivs,15,sizeof ss[k].ivs);CHECK(gc_run_set_starters(&run,ss,2,&why));drain();
  for(int k=0;k<2;k++){pr_pokemon *p=&run.party[k].b;p->level=22;p->custom_ability=ABILITY_ID_BALL_FETCH;p->ability_override=-1;pr_refresh_ability(p);pr_recalculate_stats(&run.bt,p);p->hp=p->stats[STAT_HP];run.party[k].exp=gc_level_total_exp(22,gc_species(p->species)->growth_rate);}
  pr_pokemon *p=run.bt.field[0],*incoming=&run.party[run.party_order[1]].b,*foe=run.bt.field[2];CHECK(p&&foe);uint32_t source_id=p->id;if(lunar)p->moves[0]=MOVE_ID_LUNAR_DANCE;p->fusion_species=SPECIES_ID_CHARIZARD;p->fusion_form=1;pr_recalculate_stats(&run.bt,p);p->hp=p->stats[STAT_HP];
  if(state==1)incoming->hp-=3;if(state==2)incoming->pp_used[0]=2;if(state==3){incoming->status=STATUS_EFFECT_TOXIC;incoming->status_toxic_turns=2;}
  foe->n_moves=1;foe->moves[0]=MOVE_ID_SPLASH;foe->pp_used[0]=foe->pp_up[0]=0;foe->max_pp_override[0]=-1;foe->custom_ability=ABILITY_ID_BALL_FETCH;foe->ability_override=-1;pr_refresh_ability(foe);foe->level=70;pr_recalculate_stats(&run.bt,foe);foe->hp=foe->stats[STAT_HP];run.enemy[0].exp=gc_level_total_exp(70,gc_species(foe->species)->growth_rate);
  uint32_t frozen=pr_arg_pokemon(&run.bt,"",p).fusion_v;choose((gc_choice){GC_CH_FIGHT,0,-1,0});CHECK(p->hp==0&&run.screen.kind==GC_SCREEN_SWITCH);CHECK(tag(&run)&&tag(&run)->n_pending_heals==1&&tag(&run)->pending_heals[0].source_id==source_id);
  CHECK(tag(&run)->pending_heals[0].fusion_name==frozen);p->fusion_species=SPECIES_ID_MAGIKARP;p->fusion_form=0;choose((gc_choice){GC_CH_PICK,1,0,0});CHECK(run.bt.field[0]==incoming&&incoming->hp==incoming->stats[STAT_HP]);CHECK(!incoming->status);CHECK(incoming->pp_used[0]==(state==2&&!lunar?2:0));CHECK(!run.bt.n_unported&&!run.eng.overflow);
  bool retained=state==0||(state==2&&!lunar);CHECK((tag(&run)&&tag(&run)->n_pending_heals>0)==retained);
  if(!retained)continue;
  foe=run.bt.field[2];foe->hp=1;choose((gc_choice){GC_CH_FIGHT,0,-1,0});
  for(int step=0;step<25&&!gc_run_can_save(&run);step++){
   gc_choice c={0};switch(run.screen.kind){case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;case GC_SCREEN_LEARN_MOVE:c.kind=GC_CH_FORGET;c.a=-1;break;case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;c.a=1;break;case GC_SCREEN_BIOME:c.kind=GC_CH_BIOME;c.a=0;break;default:CHECK(false);}choose(c);
  }
  /* A legal save window preserves the native positional Wish record too. */
  pr_msg_arg named=pr_arg_pokemon(&run.bt,"",p);
  pr_positional_tag wish={.type=POSITIONAL_TAG_TYPE_WISH,.turn_count=2,.target_index=0,.source_id=p->id,.source_move=MOVE_ID_WISH,.heal_hp=17,.pokemon_name=named.v,.fusion_name=named.fusion_v};pr_pos_add(&run.bt,&run.eng,&wish);
  roundtrip();run.wave=10;gc_new_battle(&run);drain();CHECK(run.wave==11&&!tag(&run));CHECK(!run.bt.n_unported&&!run.eng.overflow);
  CHECK(gc_run_can_save(&run));size_t legacy_n=gc_run_save(&run,saved,sizeof saved);CHECK(legacy_n>0);memcpy(mutated,saved,legacy_n);uint32_t hidden=hashname("old-no-pending-section");size_t section_at=pending_section(legacy_n);memcpy(mutated+section_at,&hidden,4);uint32_t old_crc=crc32(mutated+20,legacy_n-20);memcpy(mutated+16,&old_crc,4);CHECK(gc_run_load(&loaded,mutated,legacy_n,&why));CHECK(!tag(&loaded));

 }
 if(argc==3)for(int i=0;i<2;i++){
  FILE *f=fopen(argv[i+1],"rb");CHECK(f);size_t n=fread(saved,1,sizeof saved,f);CHECK(n>0&&!ferror(f)&&fgetc(f)==EOF);fclose(f);CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(loaded.load_dropped==1);const pr_arena_tag *old=tag(&loaded);CHECK(i?old&&old->n_pending_heals==1&&!old->pending_heals[0].fusion_name:!old);
  run=loaded;n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);old=tag(&loaded);CHECK(i?old&&old->n_pending_heals==1&&!old->pending_heals[0].fusion_name:!old);
 }
 printf("Fused names commands/save/cleanup: %d checks passed\n",checks);return 0;
}
