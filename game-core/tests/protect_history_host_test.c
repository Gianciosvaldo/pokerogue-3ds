#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
#include "../../battle-core/tests/protect-history-fixture.inc"
static unsigned char changed[GC_SAVE_MAX];
static uint32_t ph_hash(const char*s){uint32_t h=2166136261u;while(*s){h^=(unsigned char)*s++;h*=16777619u;}return h;}
static uint32_t ph_u32(const unsigned char*s){uint32_t n;memcpy(&n,s,4);return n;}
static void ph_crc(size_t length){uint32_t crc=~0u;for(size_t i=20;i<length;i++){crc^=changed[i];for(int j=0;j<8;j++)crc=crc>>1^((0u-(crc&1))&0xedb88320u);}crc=~crc;memcpy(changed+16,&crc,4);}
static void ph_schema(size_t length){const char*why=NULL;memcpy(changed,save,length);size_t sec=20;while(sec+8<=length&&ph_u32(changed+sec)!=ph_hash("party_protect"))sec+=8+ph_u32(changed+sec+4);CHECK(sec+8<=length);size_t block=sec+8,data=block+2+12+2;
 changed[data+4]=2;ph_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);uint16_t bad=1;memcpy(changed+block+2+4,&bad,2);ph_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);memcpy(changed+block+2+6,changed+block+2,6);ph_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);uint16_t zero=0;memcpy(changed+block+2+12,&zero,2);ph_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);size_t bytes=8+ph_u32(changed+sec+4);CHECK(length+bytes<=sizeof changed);memcpy(changed+length,changed+sec,bytes);uint32_t body=(uint32_t)(length+bytes-20);memcpy(changed+12,&body,4);ph_crc(length+bytes);CHECK(!gc_run_load(&back,changed,length+bytes,&why));
 memcpy(changed,save,length);memmove(changed+sec,changed+sec+bytes,length-sec-bytes);length-=bytes;body=(uint32_t)(length-20);memcpy(changed+12,&body,4);ph_crc(length);CHECK(gc_run_load(&back,changed,length,&why)&&!back.load_dropped);
 bool unknown=false;pr_pokemon*p=back.bt.field[0];back.bt.n_unported=0;(void)pr_protect_streak(&back.bt,p,&unknown);CHECK(unknown&&back.bt.n_unported&&!p->history_protect_complete);
 size_t partial=gc_run_save(&back,changed,sizeof changed);CHECK(partial&&gc_run_load(&back,changed,partial,&why)&&!back.load_dropped);
 CHECK(!back.bt.field[0]->history_protect_complete);p=back.bt.field[0];pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_SPLASH,.use_mode=MOVE_USE_MODE_NORMAL,.result=MOVE_RESULT_SUCCESS});back.bt.n_unported=0;CHECK(pr_protect_streak(&back.bt,p,&unknown)==0&&!unknown&&!back.bt.n_unported&&p->history_protect_complete);
 pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_QUICK_GUARD,.use_mode=MOVE_USE_MODE_REFLECTED,.result=MOVE_RESULT_SUCCESS});CHECK(pr_protect_streak(&back.bt,p,&unknown)==1&&!unknown);
}
int main(int argc,char**argv){CHECK(argc==2);FILE*f=fopen(argv[1],"r");CHECK(f);pt_header(f);pr_closures_init();int observations=0,roundtrips=0;
 for(int i=0;i<1224;i++){setup(1);r.n_items=r.n_enemy_items=0;gc_items_to_battle(&r);pr_pokemon*p=r.bt.field[0],*target=r.bt.field[2];int history_move,mode;pt_case(f,i,p,&history_move,&mode);p->hp=target->hp=100;CHECK(!pr_phase_exists(&r.eng,PH_MOVE,-1));
  for(int stage=0;stage<5;stage++){pt_word(f,"STAGE");pt_int(f,stage,stage);pt_stage(p,stage,history_move,mode);
   int before[40],after[40],expected[40];
   for(int q=0;q<40;q++){pt_word(f,"OBS");pt_int(f,pt_moves[q/4],pt_moves[q/4]);pt_int(f,q%4,q%4);before[q]=pt_int(f,0,pt_n_states-1);after[q]=pt_int(f,0,pt_n_states-1);expected[q]=pt_int(f,0,1);}
   pt_apply_state(&r.bt,before[(i+stage)%4]);const char*why=NULL;size_t size=gc_run_save(&r,save,sizeof save);CHECK(size&&gc_run_load(&back,save,size,&why)&&!back.load_dropped);roundtrips++;
   CHECK(back.bt.field[0]->history_protect_count==p->history_protect_count&&back.bt.field[0]->history_protect_complete==p->history_protect_complete);
   CHECK(rngsame(r.bt.rng,back.bt.rng)&&rngsame(r.bt.battle_rng,back.bt.battle_rng)&&rngsame(r.run.rng,back.run.rng)&&r.bt.battle_rng_live==back.bt.battle_rng_live);
   for(int q=0;q<40;q++)for(int restored=0;restored<2;restored++){
    gc_run*run=restored?&back:&r;run->bt.engine=&run->eng;pt_apply_state(&run->bt,before[q]);pr_rng global=run->bt.rng,local=run->run.rng;run->bt.n_unported=0;
    int got=pr_move_apply_conditions(&run->bt,pt_moves[q/4],run->bt.field[0],run->bt.field[2],4);
    if(got!=expected[q]||run->bt.n_unported||!pt_after(&run->bt,after[q])){fprintf(stderr,"Protect save case%d stage%d move%d rng%d restored%d got%d expected%d unsupported%d\n",i,stage,pt_moves[q/4],q%4,restored,got,expected[q],run->bt.n_unported);CHECK(false);}
    CHECK(rngsame(global,run->bt.rng)&&rngsame(local,run->run.rng));observations++;
   }
   if(stage==0&&(i==288||i==1152))ph_schema(size);
  }pt_word(f,"END");
 }pt_gates(f);pt_footer(f);fclose(f);CHECK(observations==489600&&roundtrips==6120);printf("Protect save/condition: %d native observations live+restored, %d roundtrips, %d checks passed\n",observations,roundtrips,checks);return 0;}
