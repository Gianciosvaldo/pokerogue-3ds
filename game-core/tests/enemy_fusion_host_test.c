#include "../src/internal.h"
#include <stdio.h>
#include <string.h>
#include "fixtures/enemy-fusion.native.inc"
static gc_run run,saved,loaded;
static unsigned char save_data[GC_SAVE_MAX];
static int checks,case_no;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"enemy fusion case%d line%d: %s\n",case_no,__LINE__,#x);return 1;}}while(0)
static const char*locale(void*ctx,const char*file,int fallback,size_t*len){(void)ctx;if(fallback)return NULL;const char*s=!strcmp(file,"proof")?"{\"name\":\"{{name}}\"}":!strcmp(file,"pokemon")?source_pokemon_json:NULL;if(s)*len=strlen(s);return s;}
static int compare_identity(gc_run*r,const native_enemy_frame*f){
 gc_mon*m=&r->enemy[0];pr_pokemon*p=&m->b;
 CHECK(p->id==f->id&&p->species==f->species&&p->form==f->form&&p->gender==f->gender);
 CHECK(p->fusion_species==f->fusion&&p->fusion_form==f->fusionForm&&p->fusion_ability_index==f->fusionAbilityIndex);
 CHECK(p->fusion_gender_known==!!f->fusion);if(f->fusion)CHECK(p->fusion_gender==f->fusionGender);
 CHECK(m->ability_index==f->abilityIndex&&p->ability_index==f->abilityIndex&&p->base_ability==f->ability&&p->ability==f->ability&&pr_get_ability(p,false)==f->ability);
 CHECK(p->nature==f->nature&&p->tera_type==f->tera&&p->friendship==f->friendship);
 CHECK(m->variant==f->variant&&m->luck==f->luck&&!p->shiny&&!p->has_trainer&&!m->trainer_slot);
 for(int i=0;i<6;i++)CHECK(p->ivs[i]==f->ivs[i]);
 return 0;
}
static int roundtrip(const native_enemy_frame*f){
 /* Raw source constructor frames placed in an actual command/save window.
  * This checks serialization/rebinding, not natural encounter/summon order. */
 gc_run_new(&saved,native_enemy_cases[case_no].mode,"enemy-projection-save",777);
 gc_starter starter={.species=SPECIES_ID_MEW,.tera_type=-1,.moves={MOVE_ID_SPLASH}};memset(starter.ivs,15,6);const char*why=NULL;
 CHECK(gc_run_set_starters(&saved,&starter,1,&why)&&gc_run_can_save(&saved));
 saved.enemy[0]=run.enemy[0];saved.enemy[0].b.on_field=true;saved.n_enemy=1;saved.n_enemy_items=saved.n_items=0;
 saved.bt.n_party[1]=1;saved.bt.party[1][0]=saved.bt.field[2]=&saved.enemy[0].b;
 gc_int(&saved)->enemy_start_species[0]=f->species;gc_int(&saved)->enemy_start_form[0]=f->form;gc_items_to_battle(&saved);
 size_t size=gc_run_save(&saved,save_data,sizeof save_data);CHECK(size&&gc_run_load(&loaded,save_data,size,&why)&&!loaded.load_dropped);
 CHECK(!compare_identity(&loaded,f));CHECK(loaded.bt.field[2]==&loaded.enemy[0].b&&loaded.enemy[0].b.on_field);
 CHECK(loaded.enemy[0].b.hp==f->hp&&loaded.enemy[0].exp==f->exp);
 for(int i=0;i<6;i++)CHECK(loaded.enemy[0].b.stats[i]==f->stats[i]);
 return 0;
}
int main(void){int fused=0,forms=0,hidden=0,wide=0;
 for(case_no=0;case_no<572;case_no++){
  const native_enemy_frame*f=&native_enemy_cases[case_no].mon;
  gc_run_new(&run,native_enemy_cases[case_no].mode,"native-enemy-projection",1);
  run.wave=run.bt.wave_index=1;run.bt.game_mode=run.run.mode;run.battle_type=BATTLE_TYPE_WILD;
  run.run.rng=run.bt.rng=native_enemy_cases[case_no].post_rng;pr_rng before=run.run.rng;
  pr_enemy_gen g={.id=f->id,.form=f->form,.gender=f->gender,.nature=f->nature,.ability_index=f->abilityIndex,
   .fusion_species=f->fusion?f->fusion:-1,.fusion_form=f->fusionForm,.fusion_gender=f->fusionGender,.fusion_ability_index=f->fusionAbilityIndex,
   .tera_type=f->tera,.friendship=-1,.variant=f->variant,.luck=f->luck};
  for(int i=0;i<6;i++)g.ivs[i]=f->ivs[i];
  gc_mon_new_enemy(&run,&run.enemy[0],f->species,native_enemy_cases[case_no].level,&g,0);
  CHECK(!compare_identity(&run,f));CHECK(!memcmp(&before,&run.run.rng,sizeof before));
  gc_mon_set_move(&run.enemy[0],0,MOVE_ID_SPLASH);run.enemy[0].b.pp_used[0]=3;run.enemy[0].b.pp_up[0]=1;run.enemy[0].b.max_pp_override[0]=7;
  gc_refresh_enemy(&run,&run.enemy[0],f->species,native_enemy_cases[case_no].level,&g,0);
  CHECK(!memcmp(&before,&run.run.rng,sizeof before));
  CHECK(!compare_identity(&run,f));CHECK(run.enemy[0].b.n_moves==1&&run.enemy[0].b.moves[0]==MOVE_ID_SPLASH&&run.enemy[0].b.pp_used[0]==0&&run.enemy[0].b.pp_up[0]==1&&run.enemy[0].b.max_pp_override[0]==7);
  run.enemy[0].b.pp_used[0]=run.enemy[0].b.pp_up[0]=0;run.enemy[0].b.max_pp_override[0]=-1;
  run.n_enemy=1;gc_enemy_finish_party_items(&run,NULL,0);CHECK(!compare_identity(&run,f));
  pr_pokemon*p=&run.enemy[0].b;CHECK(p->level==native_enemy_cases[case_no].level&&p->hp==f->hp&&run.enemy[0].exp==f->exp);
  for(int i=0;i<6;i++)CHECK(p->stats[i]==f->stats[i]);
  int8_t types[PR_MAX_TYPES];int n=pr_get_types(p,types,PR_TYPES_NO_TERA|PR_TYPES_BYPASS_SUMMON|PR_TYPES_IGNORE_THIRD);
  CHECK(n==f->ntypes);for(int i=0;i<n;i++)CHECK(types[i]==f->types[i]);
  pr_text_source source={locale,NULL,"en"};pr_msg message=pr_msg1("proof:name",pr_arg_pokemon_name(&run.bt,"name",p));char name[256];pr_msg_format(&message,&source,name,sizeof name);
  if(strcmp(name,f->name))fprintf(stderr,"name actual[%s] expected[%s]\n",name,f->name);CHECK(!strcmp(name,f->name));
  CHECK(!run.n_enemy_items&&!run.n_items);
  CHECK(!roundtrip(f));
  fused+=!!f->fusion;forms+=f->fusionForm>0;hidden+=f->fusion&&f->fusionAbilityIndex==2;wide+=p->level>65535;
 }
 printf("Enemy constructor projection: %d checks, %d fused, %d nonzero forms, %d hidden slots, %d wide levels\n",checks,fused,forms,hidden,wide);return 0;
}
