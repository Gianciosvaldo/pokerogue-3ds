/* Actual retained native turn -> controller storage -> app conversion. */
#include "battle_event_store.h"
#include "battle_events.h"
#include "../../game-core/src/internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
static bool observe_native=true;
#define main retained_trace_main
#define __wrap_pr_phase_run native_phase_observer
#include "../../battle-core/tests/trace-capacity-test.c"
#undef main
#undef __wrap_pr_phase_run
#undef CHECK
void __wrap_pr_phase_run(pr_battle*b,pr_engine*eng,int ph){if(observe_native)native_phase_observer(b,eng,ph);else __real_pr_phase_run(b,eng,ph);}
static int store_checks;static bool fail_realloc,force_move;static int realloc_calls,moves;static void*tracked;static size_t tracked_size;
#define SC(x) do{store_checks++;if(!(x)){fprintf(stderr,"event store line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
void *__real_realloc(void*,size_t);
void *__wrap_realloc(void*p,size_t size){realloc_calls++;if(fail_realloc)return NULL;if(force_move&&p==tracked&&p){void*q=malloc(size);if(!q)return NULL;memcpy(q,p,tracked_size<size?tracked_size:size);memset(p,0xa5,tracked_size);free(p);tracked=q;tracked_size=size;moves++;return q;}return __real_realloc(p,size);}
static gc_run run,loaded;
#ifndef EVENT_STORE_LEGACY_SAVE
#define EVENT_STORE_LEGACY_SAVE "game-core/tests/fixtures/held-form.pr399.save"
#endif
const char*i18n_language(void){return "en";}
static const char*names_locale(void*ctx,const char*file,int fallback,size_t*len){if(!strcmp(file,"proof")){const char*s="{\"name\":\"{{name}}\"}";*len=strlen(s);return s;}return locale(ctx,file,fallback,len);}
const char*i18n_species_name(int species){static char name[8][96];static int cursor;char*out=name[cursor++%8];pr_pokemon mon={0};mon.species=species;pr_msg msg=pr_msg1("proof:name",pr_arg_pokemon_name(&bt,"name",&mon));pr_text_source source={names_locale,NULL,"en"};pr_msg_format(&msg,&source,out,96);return out;}
char*pak_read(const char*key,size_t*len){const char*prefix="locales/en/";if(strncmp(key,prefix,strlen(prefix)))return NULL;const char*name=key+strlen(prefix);size_t size=strlen(name);if(size<5||strcmp(name+size-5,".json"))return NULL;for(unsigned i=0;i<sizeof event_locales/sizeof*event_locales;i++)if(strlen(event_locales[i].name)==size-5&&!strncmp(name,event_locales[i].name,size-5)){size_t n=strlen(event_locales[i].json);char*copy=malloc(n+1);SC(copy);memcpy(copy,event_locales[i].json,n+1);*len=n;return copy;}return NULL;}
void bv_play(const bv_event*ev,int n,void(*done)(void*),void*arg){(void)ev;(void)n;if(done)done(arg);}
static void append_native(gc_run*r){for(int i=0;i<nc;i+=PR_MAX_EVENTS){int count=nc-i;if(count>PR_MAX_EVENTS)count=PR_MAX_EVENTS;gc_ev_battle_chunk(r,collected+i,count);}}
static void check_transport(gc_run*r){SC(r->n_battle_events==nc&&!r->battle_events_overflow&&!r->ev_overflow);const pr_event*data=gc_run_battle_event_data(r);SC(data!=r->battle_events);for(int i=0;i<nc;i++)SC(!memcmp(data+i,collected+i,sizeof*data));gc_event notice;int next=0,count=0;while(gc_run_next_event(r,&notice)){SC(notice.kind==GC_EV_BATTLE&&notice.a==next&&notice.b>0&&notice.b<=PR_MAX_EVENTS);next+=notice.b;count++;}SC(next==nc&&count==3);}
static void convert_native(gc_run*r){static bv_event output[400];static bv_mon shown[4];static bool present[4];for(int k=0;k<4;k++){int slot=(k%2)*2+(k>=2);be_mon(&bt,&p[k],&shown[slot]);shown[slot].hp=event_before_actors[k].hp;shown[slot].max_hp=event_before_actors[k].stats[0];shown[slot].hidden=false;for(int s=0;s<7;s++)shown[slot].stat_stages[s]=event_before_actors[k].stages[s];for(int q=0;q<2;q++)shown[slot].pp_left[q]=shown[slot].pp_max[q]-event_before_actors[k].pp[q];present[slot]=true;}
 int texts=0,damage=0,total=0;
 for(int start=0;start<nc;start+=PR_MAX_EVENTS){int count=nc-start;if(count>PR_MAX_EVENTS)count=PR_MAX_EVENTS;be_view view={0};for(int k=0;k<4;k++){view.mon[k]=shown[k];view.present[k]=present[k];}int out=be_convert_events(&bt,gc_run_battle_event_data(r)+start,count,start+count==nc,&view,output,400);SC(out>0&&out<400);total+=out;
  for(int i=0;i<out;i++){const bv_event*v=&output[i];if(v->type==BEV_MESSAGE){SC(texts<144);if(texts<144){if(strcmp(v->text,event_texts[texts]))fprintf(stderr,"text%d actual[%s] expected[%s]\n",texts,v->text,event_texts[texts]);SC(!strcmp(v->text,event_texts[texts]));}texts++;}if(v->type==BEV_DAMAGE&&v->battler==2){SC(damage<40);if(damage<40)SC(v->a==event_hits[damage].hp);damage++;}if(v->battler>=0&&v->battler<4){int slot=(v->battler%2)*2+(v->battler>=2);if(v->type==BEV_DAMAGE||v->type==BEV_HEAL)shown[slot].hp=v->a;if(v->type==BEV_MAX_HP)shown[slot].max_hp=v->a;}}
  for(int k=0;k<4;k++)present[k]=view.present[k];
 }
 SC(texts==144&&damage==40);printf("App native conversion: %d texts, %d hit HP snapshots, %d screen events\n",texts,damage,total);
}
static void check_cycle(gc_run*r,int n){SC(r->n_battle_events==n);const pr_event*data=gc_run_battle_event_data(r);for(int i=0;i<n;i++)SC(!memcmp(data+i,collected+i%nc,sizeof*data));}
static void fill_cycle(gc_run*r,int wanted){while(r->n_battle_events<wanted){int at=r->n_battle_events%nc,n=wanted-r->n_battle_events;if(n>PR_MAX_EVENTS)n=PR_MAX_EVENTS;if(n>nc-at)n=nc-at;gc_ev_battle_chunk(r,collected+at,n);}}
static bool invalid_reserve(void*ctx,const pr_event*old_data,int used,int required,pr_event**storage,int*capacity){(void)used;*storage=(intptr_t)ctx?(pr_event*)old_data:NULL;*capacity=(intptr_t)ctx?required-1:required;return true;}
static void cleanup_snapshot(void){gc_run_new(&run,GAME_MODES_CLASSIC,"cleanup-storage",1);bes_bind(&run);append_native(&run);check_transport(&run);run.n_party=run.bt.n_party[0]=1;run.party_order[0]=0;pr_pokemon*own=&run.party[0].b;*own=p[0];run.bt.field[0]=run.bt.party[0][0]=own;own->on_field=true;own->switch_out_status=false;own->n_move_queue=1;own->move_queue[0]=(pr_turn_move){.move=MOVE_ID_DIG,.use_mode=MOVE_USE_MODE_IGNORE_PP,.n_targets=1,.targets={2}};own->n_tags=2;own->tags[0]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_UNDERGROUND,.source_move=MOVE_ID_DIG,.source_id=-1,.turn_count=1};own->tags[1]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_CHARGING,.source_move=MOVE_ID_DIG,.source_id=own->id,.turn_count=1};gc_battle_end_cleanup(&run);SC(!own->n_move_queue&&!pr_find_battler_tag_class(own,PR_TAGCLASS_SEMI_INVULNERABLE));SC(run.n_battle_events==nc+1&&!run.battle_events_overflow);const pr_event*data=gc_run_battle_event_data(&run);for(int i=0;i<nc;i++)SC(!memcmp(data+i,collected+i,sizeof*data));SC(data[nc].type==PR_EV_NONE&&data[nc].field[0].visible);gc_event notice;SC(gc_run_next_event(&run,&notice)&&notice.kind==GC_EV_BATTLE&&notice.a==nc&&notice.b==1);SC(!gc_run_next_event(&run,&notice));}
int main(void){SC(retained_trace_main()==0&&nc==555);observe_native=false;
 gc_run_new(&run,GAME_MODES_CLASSIC,"event-store",1);append_native(&run);SC(run.n_battle_events==GC_MAX_BATTLE_EVENTS&&run.battle_events_overflow);SC(!memcmp(run.battle_events,collected,sizeof run.battle_events));
 gc_run_new(&run,GAME_MODES_CLASSIC,"event-store",1);bes_bind(&run);append_native(&run);check_transport(&run);convert_native(&run);
 /* Force realloc to move: real libc may legally extend the old allocation. */
 tracked=(void*)gc_run_battle_event_data(&run);tracked_size=(size_t)run.battle_event_capacity*sizeof(pr_event);force_move=true;append_native(&run);force_move=false;SC(moves>0&&!run.battle_events_overflow);check_cycle(&run,2*nc);int capacity=run.battle_event_capacity;fill_cycle(&run,capacity);check_cycle(&run,capacity);fail_realloc=true;int calls=realloc_calls;gc_ev_battle_chunk(&run,collected+capacity%nc,1);SC(run.battle_events_overflow&&realloc_calls>calls);check_cycle(&run,capacity);fail_realloc=false;
 /* Reusing an arena after new/load must copy the new inline prefix. */
 gc_run_new(&run,GAME_MODES_CLASSIC,"event-store-again",2);SC(!run.battle_event_reserve&&!run.battle_event_storage);bes_bind(&run);append_native(&run);check_transport(&run);
 gc_run_new(&run,GAME_MODES_CLASSIC,"event-store-failure",3);bes_shutdown();bes_bind(&run);calls=realloc_calls;fail_realloc=true;append_native(&run);SC(run.n_battle_events==GC_MAX_BATTLE_EVENTS&&run.battle_events_overflow&&realloc_calls>calls);SC(!memcmp(gc_run_battle_event_data(&run),collected,sizeof run.battle_events));fail_realloc=false;
 for(int bad=0;bad<2;bad++){gc_run_new(&run,GAME_MODES_CLASSIC,"bad-reserve",1);gc_run_set_battle_event_storage(&run,invalid_reserve,(void*)(intptr_t)bad);append_native(&run);SC(run.n_battle_events==GC_MAX_BATTLE_EVENTS&&run.battle_events_overflow&&!run.battle_event_storage);SC(!memcmp(run.battle_events,collected,sizeof run.battle_events));}
 gc_run_new(&run,GAME_MODES_CLASSIC,"event-store-recover",4);bes_bind(&run);append_native(&run);check_transport(&run);cleanup_snapshot();
 /* This is an actual older saved run, not a synthesized schema. */
 gc_run_new(&run,GAME_MODES_CLASSIC,"discard-previous-view",1);
 static unsigned char saved[GC_SAVE_MAX];FILE*f=fopen(EVENT_STORE_LEGACY_SAVE,"rb");SC(f);size_t n=fread(saved,1,sizeof saved,f);SC(n>20&&feof(f));fclose(f);const char*why=NULL;SC(gc_run_load(&loaded,saved,n,&why)&&!loaded.load_dropped&&!loaded.battle_event_reserve&&!loaded.battle_event_storage);gc_event notice;while(gc_run_next_event(&loaded,&notice)){}bes_bind(&loaded);append_native(&loaded);check_transport(&loaded);
 gc_run_new(&run,GAME_MODES_CLASSIC,"event-count-limit",1);static pr_event one;run.battle_event_storage=&one;run.battle_event_capacity=run.n_battle_events=INT_MAX;gc_ev_battle_chunk(&run,&one,1);SC(run.battle_events_overflow&&run.n_battle_events==INT_MAX);gc_run_new(&loaded,GAME_MODES_CLASSIC,"discard-loaded-view",1);bes_shutdown();
 printf("Controller/app event storage: %d checks passed\n",store_checks);return 0;}
