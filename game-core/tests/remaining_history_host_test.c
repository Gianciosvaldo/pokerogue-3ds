/* Native observations through the real codec, without replacing save gates. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static void saved_observations(int row,int stage,const pr_pokemon*subject);
#define RH_OBSERVE(row,stage,subject) saved_observations(row,stage,subject)
#define main remaining_source_main
#include "../../battle-core/tests/remaining-history-test.c"
#undef main
static int roundtrips;
static void saved_observations(int row,int stage,const pr_pokemon*subject){
 setup(1);pr_pokemon *p=&r.party[0].b;*p=*subject;r.bt.field[0]=p;
 /* Checkpoint remains the real command save point created by setup. */
 CHECK(gc_run_can_save(&r));size_t n=gc_run_save(&r,save,sizeof save);const char*why=NULL;
 if(!n||!gc_run_load(&back,save,n,&why)||back.load_dropped){fprintf(stderr,"remaining save row%d stage%d bytes%zu reason%s\n",row,stage,n,why?why:"-");CHECK(false);}
 p=back.bt.field[0];CHECK(p&&p->id==subject->id);roundtrips++;
 for(int q=0;q<5;q++){
  back.bt.n_unported=0;pr_rng global=back.bt.rng,local=back.bt.battle_rng;
  int got=q==0?pr_move_apply_conditions(&back.bt,MOVE_ID_CONVERSION_2,back.bt.field[2],p,4):check_bypass_acc_and_invuln(&back.bt,p,back.bt.field[q-1],MOVE_ID_TACKLE);
  CHECK(got==(remaining_cases[row].expected[stage][q]>0)&&!back.bt.n_unported);
  CHECK(rngsame(global,back.bt.rng)&&rngsame(local,back.bt.battle_rng));
 }
 CHECK(p->history_all_known==subject->history_all_known&&p->history_all_present==subject->history_all_present&&p->history_aim_serial==subject->history_aim_serial);
 if(subject->history_all_present&2)CHECK(!memcmp(&p->history_aim,&subject->history_aim,sizeof p->history_aim));
}
static unsigned char modified[GC_SAVE_MAX];
static uint32_t rh_hash(const char*s){uint32_t h=2166136261u;while(*s){h^=(unsigned char)*s++;h*=16777619u;}return h;}
static uint32_t rh_read32(const unsigned char*p){uint32_t n;memcpy(&n,p,4);return n;}
static void rh_crc(size_t n){uint32_t c=~0u;for(size_t i=20;i<n;i++){c^=modified[i];for(int j=0;j<8;j++)c=c>>1^((0u-(c&1))&0xedb88320u);}c=~c;memcpy(modified+16,&c,4);}
static void codec_guards(void){
 setup(1);pr_pokemon*p=r.bt.field[0];pr_reset_summon_data(p);
 pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_LOCK_ON,.use_mode=6,.result=2,.n_targets=1,.targets={3}});
 for(int i=0;i<9;i++)pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_NONE,.use_mode=6,.result=4});
 size_t n=gc_run_save(&r,save,sizeof save),sec=20;CHECK(n>0);
 while(sec+8<=n&&rh_read32(save+sec)!=rh_hash("party_all_moves"))sec+=8+rh_read32(save+sec+4);
 CHECK(sec+8<=n);size_t block=sec+8,data=block+2+3*6+2,mdata=data+6*r.n_party+2+5*6+2;
 const char*why=NULL;
 for(int fault=0;fault<12;fault++){
  memcpy(modified,save,n);
  switch(fault){
   case 0:modified[data]=7;break;
   case 1:modified[data]=0;modified[data+1]=1;break;
   case 2:modified[data+1]=2;break;
   case 3:memset(modified+data+2,0,4);break;
   case 4:modified[mdata]=MOVE_ID_TACKLE;modified[mdata+1]=0;break;
   case 5:modified[mdata+2]=0;break;
   case 6:modified[mdata+3]=5;break;
   case 7:modified[mdata+4]=4;break;
   case 8:modified[mdata+8]=5;break;
   case 9:modified[block+2+4]=2;break;
   case 10:memcpy(modified+block+2+6,modified+block+2,6);break;
   case 11:modified[data]=1;modified[data+1]=3;break;
  }
  rh_crc(n);CHECK(!gc_run_load(&back,modified,n,&why));
 }
 size_t bytes=8+rh_read32(save+sec+4);memcpy(modified,save,n);memcpy(modified+n,save+sec,bytes);uint32_t size=n+bytes-20;memcpy(modified+12,&size,4);rh_crc(n+bytes);CHECK(!gc_run_load(&back,modified,n+bytes,&why));
 // Strip only this optional extension: older prefix must become unknown.
 memcpy(modified,save,n);memmove(modified+sec,modified+sec+bytes,n-sec-bytes);n-=bytes;size=n-20;memcpy(modified+12,&size,4);rh_crc(n);
 CHECK(gc_run_load(&back,modified,n,&why)&&!back.load_dropped);p=back.bt.field[0];bool unknown=false;
 CHECK(!pr_history_any_move(&back.bt,p,&unknown)&&unknown);CHECK(!pr_last_aim_move(&back.bt,p,&unknown)&&unknown);
 pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_MIND_READER,.use_mode=6,.result=2,.n_targets=1,.targets={2}});
 CHECK(pr_history_any_move(&back.bt,p,&unknown)&&!unknown&&pr_last_aim_move(&back.bt,p,&unknown)&&!unknown);
 n=gc_run_save(&back,modified,sizeof modified);CHECK(n&&gc_run_load(&r,modified,n,&why)&&!r.load_dropped);
}
int main(void){codec_guards();int result=remaining_source_main();CHECK(!result&&roundtrips==1920);printf("Remaining history: %d native caller save roundtrips; %d checks passed\n",roundtrips,checks);return 0;}
