/* Native query and owning-condition observations through the real run codec. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
#include "../../battle-core/tests/nonvirtual-history-fixture.inc"
static nv_case capture;
static unsigned char changed[GC_SAVE_MAX];
static uint32_t nv_hash(const char*s){uint32_t h=2166136261u;while(*s){h^=(unsigned char)*s++;h*=16777619u;}return h;}
static uint32_t nv_u32(const unsigned char*s){uint32_t n;memcpy(&n,s,4);return n;}
static void nv_crc(size_t length){uint32_t crc=~0u;for(size_t i=20;i<length;i++){crc^=changed[i];for(int j=0;j<8;j++)crc=crc>>1^((0u-(crc&1))&0xedb88320u);}crc=~crc;memcpy(changed+16,&crc,4);}
static size_t nv_section(size_t length){size_t p=20;while(p+8<=length&&nv_u32(changed+p)!=nv_hash("party_last_moves"))p+=8+nv_u32(changed+p+4);CHECK(p+8<=length);return p;}
static void nv_queries(gc_run*run,int stage){
 pr_pokemon*subject=run->bt.field[0],*other=run->bt.field[2];
 pr_rng global=run->bt.rng,battle=run->bt.battle_rng,local=run->run.rng;
 for(int q=0;q<4;q++){bool unknown=true;run->bt.n_unported=0;const pr_turn_move*m=pr_last_non_virtual_move(&run->bt,subject,q/2,q%2,&unknown);
  CHECK(!unknown&&!run->bt.n_unported&&((m!=NULL)==capture.present[stage][q]));if(m)CHECK(nv_equal(m,&capture.expected[stage][q]));}
 for(int k=0;k<5;k++){run->bt.n_unported=0;CHECK(pr_move_apply_conditions(&run->bt,nv_owner_moves[k],other,subject,3)==capture.owners[stage][k]&&!run->bt.n_unported);}
 CHECK(rngsame(global,run->bt.rng)&&rngsame(battle,run->bt.battle_rng)&&rngsame(local,run->run.rng));
}
static void nv_schema(size_t length){
 const char*why=NULL;memcpy(changed,save,length);size_t sec=nv_section(length),block=sec+8,data=block+2+24+2;
 /* Two one-byte masks per party member, then four named TurnMoves each. */
 changed[data]=16;nv_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);changed[data]=0;changed[data+1]=1;nv_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);uint16_t bad=2;memcpy(changed+block+2+4,&bad,2);nv_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);memcpy(changed+block+2+6,changed+block+2,6);nv_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);uint16_t zero=0;memcpy(changed+block+2+24,&zero,2);nv_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);size_t moves=data+26*r.n_party,mdata=moves+2+5*6+2;
 changed[data]=changed[data+1]=15;
 /* Preserve semantic validity except for one tested invalid TurnMove field. */
 pr_turn_move m={.move=MOVE_ID_TACKLE,.use_mode=MOVE_USE_MODE_NORMAL,.result=MOVE_RESULT_SUCCESS,.n_targets=1,.targets={2}};
 for(int q=0;q<4;q++){size_t at=mdata+9*q;memcpy(changed+at,&m.move,2);changed[at+2]=m.use_mode;changed[at+3]=m.result;memcpy(changed+at+4,m.targets,4);changed[at+8]=m.n_targets;}
 unsigned char valid[36];memcpy(valid,changed+mdata,sizeof valid);
 const int offsets[]={0,2,3,4,8};const unsigned char invalid[]={0,7,5,4,5};
 for(int k=0;k<5;k++){memcpy(changed+mdata,valid,sizeof valid);changed[mdata+offsets[k]]=invalid[k];if(!k)changed[mdata+1]=0;nv_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));}
 memcpy(changed,save,length);size_t bytes=8+nv_u32(changed+sec+4);CHECK(length+bytes<=sizeof changed);memcpy(changed+length,changed+sec,bytes);
 uint32_t longer=(uint32_t)(length+bytes-20);memcpy(changed+12,&longer,4);nv_crc(length+bytes);CHECK(!gc_run_load(&back,changed,length+bytes,&why));
 /* A valid older save has no optional query section. Prefix gaps stay unknown. */
 memcpy(changed,save,length);memmove(changed+sec,changed+sec+bytes,length-sec-bytes);length-=bytes;uint32_t body=(uint32_t)(length-20);memcpy(changed+12,&body,4);nv_crc(length);
 CHECK(gc_run_load(&back,changed,length,&why)&&!back.load_dropped);
 pr_pokemon*p=back.bt.field[0];bool unknown=false;back.bt.n_unported=0;const pr_turn_move*last=pr_last_non_virtual_move(&back.bt,p,false,true,&unknown);
 if(p->n_move_history>PR_MAX_MOVE_HISTORY){CHECK(unknown&&back.bt.n_unported&&!last);
  pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_TACKLE,.use_mode=MOVE_USE_MODE_NORMAL});
  CHECK(pr_last_non_virtual_move(&back.bt,p,false,true,&unknown)&&!unknown);
 }else CHECK(!unknown&&!back.bt.n_unported);
 size_t partial=gc_run_save(&back,changed,sizeof changed);CHECK(partial&&gc_run_load(&back,changed,partial,&why)&&!back.load_dropped);
}
int main(int argc,char**argv){CHECK(argc==2);FILE*f=fopen(argv[1],"r");CHECK(f);nv_header(f);pr_closures_init();int roundtrips=0;
 for(int i=0;i<972;i++){
  nv_read(f,i,&capture);setup(1);r.n_items=r.n_enemy_items=0;gc_items_to_battle(&r);
  pr_pokemon*subject=r.bt.field[0],*other=r.bt.field[2];
  subject->n_move_history=capture.user.n_move_history;memcpy(subject->move_history,capture.user.move_history,sizeof subject->move_history);
  subject->history_serial=capture.user.history_serial;memcpy(subject->history_last_serial,capture.user.history_last_serial,sizeof subject->history_last_serial);
  subject->history_last_known=capture.user.history_last_known;subject->history_last_present=capture.user.history_last_present;memcpy(subject->history_last,capture.user.history_last,sizeof subject->history_last);
  pr_rebuild_history_used_moves(subject);nv_movesets(subject,other);
  subject->custom_ability=other->custom_ability=ABILITY_ID_NONE;pr_refresh_ability(subject);pr_refresh_ability(other);subject->passive_flag=other->passive_flag=false;
  for(int stage=0;stage<6;stage++){
   nv_stage(subject,&capture,stage);nv_queries(&r,stage);
   const char*why=NULL;size_t size=gc_run_save(&r,save,sizeof save);CHECK(size&&gc_run_load(&back,save,size,&why)&&!back.load_dropped);nv_queries(&back,stage);roundtrips++;
   CHECK(back.bt.field[0]->history_last_known==subject->history_last_known&&back.bt.field[0]->history_last_present==subject->history_last_present);
   for(int q=0;q<4;q++)if(subject->history_last_present&(1u<<q))CHECK(nv_equal(&subject->history_last[q],&back.bt.field[0]->history_last[q]));
   if(stage==0&&(i==324||i==432))nv_schema(size);
  }
 }
 nv_aliases(f);nv_footer(f);fclose(f);CHECK(roundtrips==5832);printf("Nonvirtual query/condition save: %d native roundtrips, %d checks passed\n",roundtrips,checks);return 0;
}
