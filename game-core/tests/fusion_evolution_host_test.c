/* Paired native direct owners; legal save/restore and phase ownership checked separately. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
#include "fixtures/fusion-evolution.native.inc"
static int case_no;
#undef CHECK
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"fusion line%d case%d: %s\n",__LINE__,case_no,#x);exit(1);}}while(0)
static const char*locale(void*ctx,const char*file,int fallback,size_t*len){(void)ctx;if(fallback)return NULL;const char*s=!strcmp(file,"proof")?"{\"name\":\"{{name}}\"}":!strcmp(file,"pokemon")?source_pokemon_json:NULL;if(s)*len=strlen(s);return s;}
static void compare(gc_run*run,const native_evo_frame*f){gc_mon*m=&run->party[0];pr_pokemon*p=&m->b;
 CHECK(p->id==f->id&&p->species==f->species&&p->form==f->form&&p->fusion_species==f->fusion&&p->fusion_form==f->fusionForm);
 CHECK(p->gender==f->gender&&p->fusion_gender_known&&p->fusion_gender==f->fusionGender);
 CHECK(m->ability_index==f->abilityIndex&&p->ability_index==f->abilityIndex&&p->fusion_ability_index==f->fusionAbilityIndex&&pr_get_ability(p,false)==f->ability);
 CHECK(p->level==f->level&&p->nature==f->nature&&p->hp==f->hp&&p->friendship==f->friendship&&m->pause_evolutions==f->pause);
 for(int k=0;k<6;k++){CHECK(p->ivs[k]==f->ivs[k]);CHECK(p->stats[k]==f->stats[k]);}
 CHECK(run->n_items==f->nstock);for(int k=0;k<run->n_items;k++)CHECK(!strcmp(run->items[k].key,"EVOLUTION_TRACKER_GIMMIGHOUL")&&run->items[k].pokemon_id==f->stock[k].owner&&run->items[k].stack==f->stock[k].stack&&run->items[k].virtual_stack==f->stock[k].virtual_stack);
 CHECK(rngsame(run->bt.rng,f->rng[0])&&rngsame(run->bt.battle_rng,f->rng[1]));
 pr_reward_ctx *projected=gc_reward_ctx(run);CHECK(projected->party[0].fusion_gender==f->fusionGender);
 CHECK(rngsame(run->bt.rng,f->rng[0])&&rngsame(run->bt.battle_rng,f->rng[1]));
 pr_text_source source={locale,NULL,"en"};pr_msg msg=pr_msg1("proof:name",pr_arg_pokemon_name(&run->bt,"name",p));char name[256];pr_msg_format(&msg,&source,name,sizeof name);if(strcmp(name,f->name))fprintf(stderr,"name got[%s] expected[%s]\n",name,f->name);CHECK(!strcmp(name,f->name));
}
static void install(const native_evo_frame*f){
 gc_run_new(&r,GAME_MODES_CLASSIC,"fusion-owner-save",777);gc_starter starter={.species=SPECIES_ID_MEW,.tera_type=-1,.moves={MOVE_ID_SPLASH}};memset(starter.ivs,15,6);const char*why=NULL;CHECK(gc_run_set_starters(&r,&starter,1,&why));drain(&r);if(!gc_run_can_save(&r))fprintf(stderr,"save initial double%d screen%d point%d inbattle%d current%d levels%d\n",r.double_battle,r.screen.kind,gc_int(&r)->at_save_point,r.in_battle,gc_int(&r)->rq.current.type,gc_int(&r)->rq.n_levels);CHECK(gc_run_can_save(&r));
 gc_mon*m=&r.party[0];pr_pokemon*p=&m->b;p->id=f->id;p->species=f->species;p->form=f->form;p->fusion_species=f->fusion;p->fusion_form=f->fusionForm;p->gender=f->gender;p->fusion_gender=f->fusionGender;p->fusion_gender_known=true;m->ability_index=p->ability_index=f->abilityIndex;p->fusion_ability_index=f->fusionAbilityIndex;p->level=f->level;p->nature=f->nature;p->hp=f->hp;p->friendship=f->friendship;p->passive=p->passive_flag=0;p->custom_ability=p->fusion_custom_ability=p->custom_passive=-1;m->pause_evolutions=f->pause;
 for(int k=0;k<6;k++){p->stats[k]=f->stats[k];p->ivs[k]=f->ivs[k];}pr_refresh_ability(p);
 r.run.mode=r.bt.game_mode=GAME_MODES_CLASSIC;r.wave=r.bt.wave_index=source_env[1];r.biome=source_env[0];r.wave_cycle_offset=native_evo_cases[case_no].offset;r.bt.weather=source_env[3];r.bt.terrain=source_env[4];pr_seed_from_ascii(&r.run.seed,source_seed);r.bt.rng=r.run.rng=f->rng[0];r.bt.battle_rng=f->rng[1];r.bt.rng_live=r.bt.battle_rng_live=true;r.n_items=f->nstock;
 for(int k=0;k<f->nstock;k++)r.items[k]=(gc_item){.key="EVOLUTION_TRACKER_GIMMIGHOUL",.arg=0,.pokemon_id=f->stock[k].owner,.stack=f->stock[k].stack,.virtual_stack=f->stock[k].virtual_stack,.non_transferable=true};
 gc_items_to_battle(&r);
}
static void roundtrip(const native_evo_frame*f){const char*why=NULL;size_t n=gc_run_save(&r,save,sizeof save);if(!n)fprintf(stderr,"save refusal %d\n",r.screen.kind);CHECK(n&&gc_run_load(&back,save,n,&why)&&!back.load_dropped);compare(&back,f);}
#include "fusion-evolution-save-guards.inc"
static void controller(int index){
 case_no=index;install(&native_evo_cases[index].before);memset(&gc_int(&r)->rq,0,sizeof gc_int(&r)->rq);gc_int(&r)->rq.n_levels=1;
 gc_rphase up=gc_rp(RP_LEVEL_UP,0,r.party[0].b.level-1,r.party[0].b.level);CHECK(gc_phase_start(&r,&up));
 gc_rphase request;int found=0;while(gc_rq_next(&gc_int(&r)->rq,&request)){if(request.type!=RP_EVOLUTION)continue;found++;CHECK(request.d==native_evo_cases[index].fusion);CHECK(gc_evo_data[request.b].species==native_evo_cases[index].species);CHECK(!gc_phase_start(&r,&request)&&r.screen.kind==GC_SCREEN_EVOLVE);
 bool ok=false;const char*why=NULL;CHECK(gc_phase_choose(&r,&request,&(gc_choice){GC_CH_EVOLVE,0,0,0},&why,&ok)&&ok);break;}
 CHECK(found==1);compare(&r,&native_evo_cases[index].after);
}
int main(void){int primary=0,secondary=0;for(case_no=0;case_no<1440;case_no++){install(&native_evo_cases[case_no].before);compare(&r,&native_evo_cases[case_no].before);roundtrip(&native_evo_cases[case_no].before);bool fusion=false;int e=gc_mon_get_evolution_target(&r,&r.party[0],&fusion);CHECK((e>=0)==native_evo_cases[case_no].selected);if(e>=0){CHECK(fusion==native_evo_cases[case_no].fusion&&gc_evo_data[e].species==native_evo_cases[case_no].species&&gc_evo_data[e].level==native_evo_cases[case_no].level&&gc_evo_data[e].item==native_evo_cases[case_no].item);const gc_evo*chosen=&gc_evo_data[e];const char*pre_key=chosen->pre_form==-1?NULL:gc_form(fusion?r.party[0].b.fusion_species:r.party[0].b.species,chosen->pre_form>=0?chosen->pre_form:0)->form_key;const char*evo_key=chosen->evo_form==-1?NULL:gc_form(chosen->species,chosen->evo_form>=0?chosen->evo_form:0)->form_key;CHECK((pre_key==NULL)==(native_evo_cases[case_no].pre_form==NULL));if(pre_key)CHECK(!strcmp(pre_key,native_evo_cases[case_no].pre_form));CHECK((evo_key==NULL)==(native_evo_cases[case_no].evo_form==NULL));if(evo_key)CHECK(!strcmp(evo_key,native_evo_cases[case_no].evo_form));gc_mon_evolve_target(&r,&r.party[0],e,fusion);if(fusion)secondary++;else primary++;}compare(&r,&native_evo_cases[case_no].after);roundtrip(&native_evo_cases[case_no].after);CHECK(!r.bt.n_unported);}
 for(int family=0;family<3;family++){bool found=false;for(int i=0;i<1440;i++)if(native_evo_cases[i].selected&&native_evo_cases[i].before.fusion==((int[]){SPECIES_ID_CATERPIE,SPECIES_ID_TRAPINCH,SPECIES_ID_GIMMIGHOUL}[family])&&native_evo_cases[i].before.fusionAbilityIndex==2){found=true;case_no=i;install(&native_evo_cases[i].before);r.party[0].b.ability_lookup_known=false;bool fusion;int e=gc_mon_get_evolution_target(&r,&r.party[0],&fusion);CHECK(e>=0&&fusion);gc_mon_evolve_target(&r,&r.party[0],e,true);compare(&r,&native_evo_cases[i].after);break;}CHECK(found);}
 save_guards();
 for(int i=0;i<1440;i++)if(native_evo_cases[i].selected)controller(i);
 printf("Fusion evolution: 1440 native selection/mutation cases, primary%d secondary%d, %d checks\n",primary,secondary,checks);return 0;}
