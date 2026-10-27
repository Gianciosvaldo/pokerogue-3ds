/* Native item-selected primary/secondary targets and the real reward/phase owners. */
#include "../src/internal.h"
#include "fixtures/fusion-item-evolution.native.inc"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static unsigned checks,case_no;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"fusion item case%u line%d: %s\n",case_no,__LINE__,#x);exit(1);}}while(0)
static bool same_rng(const pr_rng*a,const pr_rng*b){return a->c==b->c&&a->s0==b->s0&&a->s1==b->s1&&a->s2==b->s2;}
static const char*locale(void*ctx,const char*file,int fallback,size_t*len){(void)ctx;if(fallback)return NULL;const char*s=!strcmp(file,"proof")?"{\"name\":\"{{name}}\"}":!strcmp(file,"pokemon")?item_evo_pokemon_json:NULL;if(s)*len=strlen(s);return s;}
static void compare(const native_item_evo_frame*f)
{
 gc_mon*m=&run.party[0];pr_pokemon*p=&m->b;
 CHECK(p->id==f->id&&p->species==f->species&&p->form==f->form&&p->fusion_species==f->fusion&&p->fusion_form==f->fusionForm);
 CHECK(p->gender==f->gender&&p->fusion_gender_known&&p->fusion_gender==f->fusionGender);
 CHECK(m->ability_index==f->abilityIndex&&p->ability_index==f->abilityIndex&&p->fusion_ability_index==f->fusionAbilityIndex&&pr_get_ability(p,false)==f->ability);
 CHECK(p->level==f->level&&p->nature==f->nature&&p->hp==f->hp&&p->friendship==f->friendship&&m->pause_evolutions==f->pause);
 for(int k=0;k<6;k++){CHECK(p->ivs[k]==f->ivs[k]);CHECK(p->stats[k]==f->stats[k]);}
 CHECK(p->n_moves==f->nmoves);for(int k=0;k<f->nmoves;k++){CHECK(p->moves[k]==f->moves[k][0]);CHECK(p->pp_used[k]==f->moves[k][1]);CHECK(p->pp_up[k]==f->moves[k][2]);CHECK(p->max_pp_override[k]==f->moves[k][3]);}
 CHECK(run.n_items==0&&same_rng(&run.run.rng,&f->rng[0])&&same_rng(&run.bt.rng,&f->rng[0])&&same_rng(&run.bt.battle_rng,&f->rng[1]));
 pr_text_source source={locale,NULL,"en"};pr_msg msg=pr_msg1("proof:name",pr_arg_pokemon_name(&run.bt,"name",p));char name[256];pr_msg_format(&msg,&source,name,sizeof name);CHECK(!strcmp(name,f->name));
}
static void install(const native_item_evo_frame*f)
{
 gc_run_new(&run,GAME_MODES_CLASSIC,"native-fusion-item",777);run.n_party=1;run.party_order[0]=0;gc_starter starter={.species=SPECIES_ID_MEW,.tera_type=-1};memset(starter.ivs,15,6);gc_mon*m=&run.party[0];gc_mon_new_player(&run,m,&starter,f->level);pr_pokemon*p=&m->b;
 p->id=f->id;p->species=f->species;p->form=f->form;p->fusion_species=f->fusion;p->fusion_form=f->fusionForm;p->gender=f->gender;p->fusion_gender=f->fusionGender;p->fusion_gender_known=true;m->ability_index=p->ability_index=f->abilityIndex;p->fusion_ability_index=f->fusionAbilityIndex;p->level=f->level;p->nature=f->nature;p->hp=f->hp;p->friendship=f->friendship;p->passive=p->passive_flag=0;p->custom_ability=p->fusion_custom_ability=p->custom_passive=-1;m->pause_evolutions=f->pause;
 for(int k=0;k<6;k++){p->ivs[k]=f->ivs[k];p->stats[k]=f->stats[k];}pr_refresh_ability(p);p->n_moves=0;for(int k=0;k<f->nmoves;k++){gc_mon_set_move(m,k,f->moves[k][0]);p->pp_used[k]=f->moves[k][1];p->pp_up[k]=f->moves[k][2];p->max_pp_override[k]=f->moves[k][3];}
 run.bt.party[0][0]=p;run.bt.n_party[0]=1;run.bt.field[0]=p;run.biome=item_evo_env[0];run.wave=run.bt.wave_index=item_evo_env[1];run.wave_cycle_offset=item_evo_env[2];run.bt.weather=item_evo_env[3];run.bt.terrain=item_evo_env[4];pr_seed_from_ascii(&run.run.seed,item_evo_seed);run.run.rng=run.bt.rng=f->rng[0];run.bt.battle_rng=f->rng[1];run.bt.rng_live=run.bt.battle_rng_live=true;gc_rq_reset(&gc_int(&run)->rq);
}
static void controller(const native_item_evo_case*n)
{
 install(&n->before);gc_rqueue*q=&gc_int(&run)->rq;q->current=gc_rp(RP_SELECT_MODIFIER,0,0,0);q->waiting=true;
 run.screen.kind=GC_SCREEN_REWARD;run.screen.n_options=1;run.screen.options[0]=(gc_offer){.key=n->item>50?"RARE_EVOLUTION_ITEM":"EVOLUTION_ITEM",.arg=n->item,.target=GC_TARGET_POKEMON};
 const char*why=NULL;bool chosen=gc_run_choose(&run,&(gc_choice){GC_CH_REWARD,0,0,-1},&why);CHECK(chosen==n->accepted);
 if(chosen){CHECK(run.screen.kind==GC_SCREEN_EVOLVE);gc_rphase request=q->current;CHECK(request.type==RP_EVOLUTION&&request.d==n->fusion);bool ok=false;CHECK(gc_phase_choose(&run,&request,&(gc_choice){GC_CH_EVOLVE,0,0,0},&why,&ok)&&ok);}
 else CHECK(run.screen.kind==GC_SCREEN_REWARD);
 compare(&n->after);CHECK(!run.bt.n_unported);
}
int main(void)
{
 int primary=0,secondary=0,rejected=0;
 for(case_no=0;case_no<2016;case_no++){
  const native_item_evo_case*n=&native_item_evo_cases[case_no];install(&n->before);compare(&n->before);
  bool fusion=false;int evo=gc_mon_item_evolution_target(&run,&run.party[0],n->item,&fusion);
  CHECK((evo>=0)==n->accepted);if(evo>=0)CHECK(fusion==n->fusion&&gc_evo_data[evo].species==n->species&&gc_evo_data[evo].level==n->level&&gc_evo_data[evo].item==n->item);compare(&n->before);
  gc_offer offer={.key=n->item>50?"RARE_EVOLUTION_ITEM":"EVOLUTION_ITEM",.arg=n->item,.target=GC_TARGET_POKEMON};const char*why=NULL;
  int status=gc_reward_lifecycle_apply(&run,&offer,0,&why);CHECK((status==GC_APPLY_DONE)==n->accepted);CHECK((status==GC_APPLY_DONE)==n->filter);CHECK(status!=GC_APPLY_UNHANDLED);compare(&n->before);
  gc_rphase request;bool queued=gc_rq_next(&gc_int(&run)->rq,&request);CHECK(queued==n->accepted);
  if(queued){CHECK(request.type==RP_EVOLUTION&&request.d==n->fusion&&request.c==n->lastLevel);const gc_evo*e=&gc_evo_data[request.b];CHECK(e->species==n->species&&e->level==n->level&&e->item==n->item);CHECK(!gc_phase_start(&run,&request)&&run.screen.kind==GC_SCREEN_EVOLVE);bool ok=false;CHECK(gc_phase_choose(&run,&request,&(gc_choice){GC_CH_EVOLVE,0,0,0},&why,&ok)&&ok);if(n->fusion)secondary++;else primary++;}else rejected++;
  compare(&n->after);CHECK(!run.bt.n_unported);
  controller(n);
 }
 for(int family=0;family<2;family++){
  bool found=false;for(case_no=0;case_no<2016;case_no++){
   const native_item_evo_case*n=&native_item_evo_cases[case_no];
   if(!n->accepted||!n->fusion||n->before.fusion!=((int[]){SPECIES_ID_SNORUNT,SPECIES_ID_KIRLIA}[family]))continue;
   found=true;install(&n->before);run.party[0].b.fusion_gender_known=false;gc_mon before=run.party[0];const char*why=NULL;gc_offer offer={.key="EVOLUTION_ITEM",.arg=n->item,.target=GC_TARGET_POKEMON};
   CHECK(gc_reward_lifecycle_apply(&run,&offer,0,&why)==GC_APPLY_REFUSED);CHECK(run.bt.n_unported>0);CHECK(!memcmp(&run.party[0],&before,sizeof before));CHECK(same_rng(&run.run.rng,&n->before.rng[0]));break;
  }CHECK(found);
 }
 CHECK(primary==144&&secondary==288&&rejected==1584);printf("native fusion item evolution: 2016 cases primary%d secondary%d rejected%d checks%u\n",primary,secondary,rejected,checks);return 0;
}
