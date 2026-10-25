/* EncounterPhase's actual intro boundary, before summon/entry callbacks. */
#include "../src/internal.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
static gc_run r;
static jmp_buf stopped;
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"encounter intro C%d: %s\n",__LINE__,#x);exit(1);}}while(0)
bool __wrap_gc_battle_setup_field(gc_run* run,bool reset){CHECK(run==&r&&!reset);longjmp(stopped,1);return false;}
static const char* locale(void* ctx,const char* name,int lang,size_t* length){
    static char data[2][40000],names[2][32];static size_t sizes[2];static int count;
    for(int i=0;i<count;i++)if(!strcmp(names[i],name)){*length=sizes[i];return data[i];}
    CHECK(count<2&&strlen(name)<32);char path[128];snprintf(path,sizeof path,"tests/fixtures/form-item-owner/en/%s.json",name);
    FILE* f=fopen(path,"rb");CHECK(f);sizes[count]=fread(data[count],1,sizeof data[0]-1,f);CHECK(fgetc(f)==EOF);fclose(f);
    strcpy(names[count],name);*length=sizes[count];return data[count++];
}
int main(void){
    /* Exact pinned battle.json keys/English text and EncounterPhase branch split;
     * this is a regression boundary, not a complete encounter parity matrix. */
    const char* keys[2][2]={{"battle:singleWildAppeared","battle:multiWildAppeared"},{"battle:trainerAppeared","battle:trainerAppearedDouble"}};
    const char* expected[2][2]={{"A wild Mew appeared!","A wild Mew\nand Bulbasaur appeared!"},{"Test Trainer\nwould like to battle!","Test Trainer\nwould like to battle!"}};
    for(int trainer=0;trainer<2;trainer++)for(int doubles=0;doubles<2;doubles++){
        gc_run_new(&r,GAME_MODES_CLASSIC,"intro-text",777);r.wave=1;r.battle_type=trainer?BATTLE_TYPE_TRAINER:BATTLE_TYPE_WILD;
        r.double_battle=doubles;r.n_enemy=1+doubles;r.enemy[0].b.species=SPECIES_ID_MEW;r.enemy[1].b.species=SPECIES_ID_BULBASAUR;
        r.trainer_names[GC_TNAME_TITLED]="Test Trainer";
        if(!setjmp(stopped))gc_new_battle_start(&r,false,false);
        gc_event ev;CHECK(gc_run_next_event(&r,&ev)&&ev.kind==GC_EV_SAVE_POINT);
        CHECK(gc_run_next_event(&r,&ev)&&ev.kind==GC_EV_MESSAGE&&!strcmp(ev.msg.key,keys[trainer][doubles]));
        char text[256];pr_text_source source={locale,NULL,"en"};CHECK(pr_msg_format(&ev.msg,&source,text,sizeof text)<sizeof text);
        CHECK(!strcmp(text,expected[trainer][doubles])&&!gc_run_next_event(&r,&ev));
    }
    printf("Encounter introductions: wild/trainer singles/doubles, both names and pinned line breaks; %d checks\n",checks);return 0;
}
