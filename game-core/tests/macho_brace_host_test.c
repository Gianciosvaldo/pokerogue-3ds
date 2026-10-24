/* Exact pinned native stats and battle EXP-owner observations. */
#include "../src/internal.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
static gc_run r, back;
static pr_battle battle;
static gc_rqueue initial_command_queue;
static unsigned char save[GC_SAVE_MAX];
static FILE *input;
static unsigned checks, line;
static char text[2048], *cursor;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"Macho line %u C%d: %s\n",line,__LINE__,#x); exit(1); } } while(0)
static void record(char tag) { CHECK(fgets(text,sizeof text,input)); line++; CHECK(text[0]==tag&&text[1]==' '&&strchr(text,'\n')); cursor=text+2; }
static int64_t number(void) { char *end; errno=0; long long n=strtoll(cursor,&end,10); CHECK(!errno&&end!=cursor); cursor=end; return n; }
static void finish(void) { CHECK(!strcmp(cursor,"\n")); }
static int bounded(int low,int high) { int64_t n=number(); CHECK(n>=low&&n<=high); return (int)n; }
static void stats(pr_pokemon *p,char tag,bool compare,bool friendship) {
    record(tag); int hp=bounded(0,INT32_MAX), f=friendship?bounded(0,255):0;
    if(compare) { CHECK(p->hp==hp); if(friendship)CHECK(p->friendship==f); }
    else { p->hp=hp;if(friendship)p->friendship=f; }
    for(int j=0;j<6;j++){int n=bounded(1,INT32_MAX);if(compare)CHECK(p->stats[j]==n);else p->stats[j]=n;} finish();
}
static void setup(int rotation) {
    gc_run_new(&r,GAME_MODES_CLASSIC,"native-macho",777);
    gc_starter starters[3]={0};const char *why;
    for(int j=0;j<3;j++){starters[j].species=(int[]){SPECIES_ID_BULBASAUR,SPECIES_ID_CHARMANDER,SPECIES_ID_SQUIRTLE}[j];starters[j].tera_type=-1;}
    CHECK(gc_run_set_starters(&r,starters,3,&why));r.n_items=r.n_enemy_items=0;r.bt.n_unported=0;
    for(int j=0;j<3;j++)r.party_order[j]=(j+rotation)%3;
    gc_event e;while(gc_run_next_event(&r,&e)){}initial_command_queue=gc_int(&r)->rq;gc_rq_clear(&r);
}
static void inventory(char tag,bool compare) {
    record(tag);int n=bounded(0,4);finish();if(compare)CHECK(n==r.n_items);else r.n_items=n;
    for(int j=0;j<n;j++){
        record('I');int bell=bounded(0,1),owner=bounded(0,2),stock=bounded(1,bell?3:50),virt=bounded(0,50-stock);finish();
        const char *key=bell?"SOOTHE_BELL":"MYSTERY_ENCOUNTER_MACHO_BRACE";uint32_t id=r.party[r.party_order[owner]].b.id;
        if(compare){gc_item *it=&r.items[j];CHECK(!strcmp(it->key,key)&&it->pokemon_id==id&&it->stack==stock&&it->virtual_stack==virt&&it->non_transferable==!bell);}
        else r.items[j]=(gc_item){.key=key,.pokemon_id=id,.stack=stock,.virtual_stack=virt,.non_transferable=!bell,.arg=0};
    }
}
static void mons(char tag,bool compare,int scenario,bool fused) {
    for(int j=0;j<3;j++){
        record(tag);int64_t id=number();CHECK(id>0&&id<=UINT32_MAX);
        int species=bounded(1,UINT16_MAX),fusion=bounded(0,UINT16_MAX),level=bounded(1,65536),nature=bounded(0,24),base=bounded(0,PR_ABILITY_COUNT-1),ability=bounded(0,PR_ABILITY_COUNT-1),active=bounded(0,1),hp=bounded(0,INT32_MAX),friendship=bounded(0,255);
        CHECK((species==(int[]){151,292,6}[(scenario==2||scenario==4)?2-j:j]&&fusion==(fused?151:0)));
        gc_mon *m=&r.party[r.party_order[j]];pr_pokemon *p=&m->b;
        if(compare)CHECK(p->id==(uint32_t)id&&p->species==species&&p->fusion_species==fusion&&p->level==level&&p->nature==nature&&p->base_ability==base&&p->ability==ability&&p->on_field==active&&p->hp==hp&&p->friendship==friendship);
        else {p->id=id;p->species=species;p->fusion_species=fusion;p->form=p->fusion_form=0;p->level=level;p->nature=nature;p->base_ability=base;p->ability=ability;p->on_field=active;p->hp=hp;p->friendship=friendship;p->passive=0;p->is_player=true;m->base_nature=nature;m->has_custom_nature=false;m->pokerus=false;}
        for(int k=0;k<6;k++){int v=bounded(0,31);if(compare)CHECK(p->ivs[k]==v);else p->ivs[k]=v;}
        for(int k=0;k<6;k++){int v=bounded(1,INT32_MAX);if(compare)CHECK(p->stats[k]==v);else p->stats[k]=v;}finish();
    }
}
static void roundtrip(void) {
    const char *why=NULL;size_t n=gc_run_save(&r,save,sizeof save);CHECK(n);bool loaded=gc_run_load(&back,save,n,&why);if(!loaded||back.load_dropped)fprintf(stderr,"save: loaded %d dropped %d why %s\n",loaded,back.load_dropped,why?why:"none");CHECK(loaded&&!back.load_dropped);
    CHECK(back.n_items==r.n_items);
    for(int j=0;j<r.n_items;j++)CHECK(!strcmp(back.items[j].key,r.items[j].key)&&back.items[j].pokemon_id==r.items[j].pokemon_id&&back.items[j].stack==r.items[j].stack&&back.items[j].non_transferable==r.items[j].non_transferable);
    for(int j=0;j<r.n_party;j++){pr_pokemon *p=&r.party[j].b,*q=&back.party[j].b;CHECK(p->hp==q->hp&&p->friendship==q->friendship&&!memcmp(p->stats,q->stats,sizeof p->stats));}
}
static void capture_setup(int stack,int level) {
    setup(0);r.n_party=6;r.bt.n_party[0]=6;r.n_enemy=1;r.double_battle=r.bt.double_battle=false;
    for(int j=0;j<6;j++){
        if(j>=3){r.party[j]=r.party[1];r.party[j].b.id+=j*100;}
        r.party_order[j]=j;r.bt.party[0][j]=&r.party[j].b;
        r.party[j].b.on_field=j==0;
    }
    pr_pokemon *p=&r.party[0].b;p->species=SPECIES_ID_MEW;p->level=level;p->base_ability=p->ability=ABILITY_ID_SYNCHRONIZE;p->nature=0;p->passive=0;p->passive_flag=false;
    memset(p->ivs,31,6);p->n_moves=1;p->moves[0]=MOVE_ID_SPLASH;
    r.party[0].base_nature=0;r.party[0].has_custom_nature=false;
    r.party[0].exp=gc_level_total_exp(level,gc_species(SPECIES_ID_MEW)->growth_rate);
    r.enemy[0].b.species=SPECIES_ID_MAGIKARP;r.enemy[0].b.form=0;r.enemy[0].b.level=2;r.enemy[0].b.on_field=true;r.enemy[0].b.hp=10;
    r.bt.field[0]=p;r.bt.field[1]=NULL;r.bt.field[2]=&r.enemy[0].b;r.bt.field[3]=NULL;
    gc_item_add(&r,"MYSTERY_ENCOUNTER_MACHO_BRACE",0,p->id,stack);gc_items_to_battle(&r);
    gc_int(&r)->thrown_ball=POKEBALL_TYPE_MASTER_BALL;
}
int main(int argc,char **argv) {
    CHECK(argc==2);input=fopen(argv[1],"r");CHECK(input);CHECK(fgets(text,sizeof text,input)&&!strcmp(text,"MACHO 1 32 7500 1152 12\n"));line++;
    const int stacks[]={0,1,49,50},owner_stacks[]={1,49,50},levels[]={1,5,100,1000,65536},species[]={151,292,6},real[]={1,2,3,24,25,26,47,48,49,50};
    for(int i=0;i<32;i++){
        record('A');CHECK(bounded(0,31)==i);int stock=bounded(0,50),part=bounded(0,1),defeated=bounded(0,1),fainted=bounded(0,1),level=bounded(1,65536),cap=bounded(1,65536);finish();
        CHECK(stock==stacks[i/8]&&part==((i/4)%2)&&defeated==((i/2)%2)&&fainted==i%2&&level==1000&&cap==10);
        setup(0);r.n_party=1;gc_mon *m=&r.party[0];m->b.species=SPECIES_ID_MEW;m->b.level=level;m->b.base_ability=m->b.ability=ABILITY_ID_SYNCHRONIZE;memset(m->b.ivs,31,6);
        record('B');CHECK(bounded(0,50)==stock);int hp=bounded(0,INT32_MAX),friendship=bounded(0,255);m->b.hp=hp;m->b.friendship=friendship;for(int k=0;k<6;k++)m->b.stats[k]=bounded(1,INT32_MAX);finish();
        if(stock){gc_item_add(&r,"MYSTERY_ENCOUNTER_MACHO_BRACE",0,m->b.id,stock);}
        gc_items_to_battle(&r);r.bt.n_participants=part;r.bt.participants[0]=m->b.id;
        if(defeated){gc_rphase phase=gc_rp(RP_VICTORY,0,0,0);CHECK(gc_phase_start(&r,&phase));}
        record('E');int want=bounded(0,50);CHECK((stock?r.items[0].stack:0)==want);CHECK(m->b.hp==bounded(0,INT32_MAX));CHECK(m->b.friendship==bounded(0,255));for(int k=0;k<6;k++)CHECK(m->b.stats[k]==bounded(1,INT32_MAX));finish();
    }
    for(int i=0;i<7500;i++){
        record('S');CHECK(bounded(0,7499)==i);pr_pokemon p={0};p.id=7;p.is_player=true;p.species=bounded(1,UINT16_MAX);p.fusion_species=bounded(0,UINT16_MAX);p.level=bounded(1,65536);p.nature=bounded(0,24);int stack=bounded(1,50),virt=bounded(0,50-stack);p.base_ability=bounded(0,PR_ABILITY_COUNT-1);p.ability=bounded(0,PR_ABILITY_COUNT-1);
        CHECK(p.species==species[i/2500]&&p.fusion_species==((i/1250)%2?151:0)&&p.level==levels[(i/250)%5]&&p.nature==(i/10)%25&&stack==real[i%10]&&virt==((stack<50&&i%2)?1:0));
        for(int k=0;k<6;k++){p.ivs[k]=bounded(0,31);}
        finish();stats(&p,'B',false,false);pr_pokemon before=p;
        memset(&battle,0,sizeof battle);battle.mods[0][0]=(pr_modifier){.cls=PR_MOD_INCREMENTING_STAT,.pokemon_id=7,.stack_count=stack,.virtual_stack_count=virt};battle.n_mods[0]=1;
        CHECK(pr_mod_is_ported_item(&battle.mods[0][0]));pr_recalculate_stats(&battle,&p);CHECK(!battle.n_unported);stats(&p,'E',true,false);
        r.n_items=1;r.n_enemy_items=0;r.bt.n_unported=0;r.party[0].b=before;
        r.items[0]=(gc_item){.key="MYSTERY_ENCOUNTER_MACHO_BRACE",.arg=0,.pokemon_id=7,.stack=stack,.virtual_stack=virt,.non_transferable=true};
        gc_items_to_battle(&r);CHECK(r.bt.mods[0][0].cls==PR_MOD_INCREMENTING_STAT&&pr_mod_is_ported_item(&r.bt.mods[0][0]));
        pr_recalculate_stats(&r.bt,&r.party[0].b);CHECK(!r.bt.n_unported&&r.party[0].b.hp==p.hp&&!memcmp(r.party[0].b.stats,p.stats,sizeof p.stats));
    }
    for(int i=0;i<1152;i++){
        record('O');CHECK(bounded(0,1151)==i);int fused=bounded(0,1),scenario=bounded(0,5),mask=bounded(0,7),stock=bounded(1,50),defeated=bounded(0,1),level=bounded(1,1000),exp=bounded(0,INT32_MAX),cap=bounded(1,65536),enemy=bounded(1,UINT16_MAX),form=bounded(0,255),enemyLevel=bounded(1,65536);finish();
        int key=i%576;CHECK(fused==i/576&&scenario==key/96&&mask==(key/12)%8&&stock==owner_stacks[(key/4)%3]&&defeated==(key/2)%2&&level==(key%2?1000:5)&&cap==10);
        setup(i%3);mons('B',false,scenario,fused);inventory('J',false);gc_items_to_battle(&r);r.n_enemy=1;r.enemy[0].b.species=enemy;r.enemy[0].b.form=form;r.enemy[0].b.level=enemyLevel;
        r.bt.n_participants=0;for(int j=0;j<3;j++)if(mask&(1<<j))r.bt.participants[r.bt.n_participants++]=r.party[r.party_order[j]].b.id;
        pr_rng rng=r.run.rng,brng=r.bt.rng;
        if(defeated){gc_rphase phase=gc_rp(RP_VICTORY,0,0,0);CHECK(gc_phase_start(&r,&phase));}
        else{pr_exp_party pp;gc_exp_party_build(&r,&pp,r.bt.participants,r.bt.n_participants);gc_exp_party_queue(&r,&pp,exp,false);}
        CHECK(!memcmp(&rng,&r.run.rng,sizeof rng)&&!memcmp(&brng,&r.bt.rng,sizeof brng));mons('E',true,scenario,fused);inventory('K',true);
        record('Q');int n=bounded(0,3);finish();gc_rphase phase;
        for(int j=0;j<n;j++){record('P');int party=bounded(0,2),active=bounded(0,1),amount=bounded(0,INT32_MAX);finish();CHECK(gc_rq_next(&gc_int(&r)->rq,&phase));CHECK(phase.type==RP_EXP&&phase.a==r.party_order[party]&&phase.b==active&&phase.d==amount);}
        if(defeated){CHECK(gc_rq_next(&gc_int(&r)->rq,&phase)&&phase.type==RP_CONTINUE);}
        CHECK(!gc_rq_next(&gc_int(&r)->rq,&phase)); CHECK(!gc_int(&r)->rq.overflow); CHECK(!r.stubs[GC_STUB_REWARD_APPLY]); if(r.bt.n_unported)fprintf(stderr,"owner %d unsupported %d first %d\n",i,r.bt.n_unported,r.bt.unported[0]); CHECK(!r.bt.n_unported);roundtrip();
    }
    for(int i=0;i<12;i++){
        record('C');CHECK(bounded(0,11)==i);int stock=bounded(1,50),level=bounded(1,1000),part=bounded(0,1);CHECK(bounded(0,1)==1&&bounded(0,1)==1);finish();
        CHECK(stock==owner_stacks[i/4]&&level==(i%4<2?5:1000)&&part==i%2);
        capture_setup(stock,level);pr_pokemon *p=&r.party[0].b;
        record('B');CHECK(bounded(1,50)==stock);p->hp=bounded(0,INT32_MAX);p->friendship=bounded(0,255);for(int k=0;k<6;k++)p->stats[k]=bounded(1,INT32_MAX);finish();
        pr_pokemon before=*p;r.bt.n_participants=part;r.bt.participants[0]=p->id;
        pr_rng rng=r.run.rng,brng=r.bt.rng;
        gc_rphase phase=gc_rp(RP_CATCH,0,POKEBALL_TYPE_MASTER_BALL,0);
        CHECK(!gc_phase_start(&r,&phase));bool ok=false;const char *why=NULL;
        CHECK(gc_catch_choose(&r,&phase,&(gc_choice){GC_CH_PICK,-1,0,0},&why,&ok)&&ok);
        CHECK(r.n_party==6&&r.enemy[0].b.hp==0&&r.items[0].stack==stock);
        phase=gc_rp(RP_VICTORY,0,0,0);CHECK(gc_phase_start(&r,&phase));
        record('E');int want=bounded(1,50);CHECK(r.items[0].stack==want);CHECK(p->hp==bounded(0,INT32_MAX));CHECK(p->friendship==bounded(0,255));for(int k=0;k<6;k++)CHECK(p->stats[k]==bounded(1,INT32_MAX));finish();
        pr_pokemon after=*p;pr_exp_party pp;gc_exp_party_build(&r,&pp,r.bt.participants,r.bt.n_participants);gc_exp_party_queue(&r,&pp,17,false);
        CHECK(r.items[0].stack==want&&!memcmp(p,&after,sizeof after)&&!memcmp(&rng,&r.run.rng,sizeof rng)&&!memcmp(&brng,&r.bt.rng,sizeof brng));
        CHECK(!gc_run_can_save(&r)&&!gc_run_save(&r,save,sizeof save));
        if(part){
            /* Retain a complete real command/ball/catch-answer route as well as
             * the native catch-success callback boundary compared above. */
            capture_setup(stock,level);gc_int(&r)->rq=initial_command_queue;p=&r.party[0].b;*p=before;r.pokeballs[POKEBALL_TYPE_MASTER_BALL]=1;
            gc_event e;while(gc_run_next_event(&r,&e)){}
            CHECK(gc_run_choose(&r,&(gc_choice){GC_CH_BALL,POKEBALL_TYPE_MASTER_BALL,0,0},&why));
            CHECK(gc_rq_has(&r,RP_VICTORY)&&r.items[0].stack==stock);
            while(gc_run_next_event(&r,&e)){}
            CHECK(gc_run_choose(&r,&(gc_choice){GC_CH_PICK,-1,0,0},&why));
            CHECK(r.items[0].stack==want&&p->hp==after.hp&&p->friendship==after.friendship&&!memcmp(p->stats,after.stats,sizeof p->stats));
            CHECK(!r.bt.n_unported&&!r.eng.overflow);
            CHECK(r.screen.kind==GC_SCREEN_REWARD);while(gc_run_next_event(&r,&e)){}
            CHECK(gc_run_choose(&r,&(gc_choice){GC_CH_SKIP,0,0,0},&why));
            while(gc_run_next_event(&r,&e)){}
            if(r.screen.kind==GC_SCREEN_CHECK_SWITCH)CHECK(gc_run_choose(&r,&(gc_choice){GC_CH_CONFIRM,0,0,0},&why));
            CHECK(gc_run_can_save(&r));roundtrip();
        }
    }
    for(int k=0;k<9;k++){
        setup(0);gc_item_add(&r,"MYSTERY_ENCOUNTER_MACHO_BRACE",0,r.party[0].b.id,1);
        switch(k){case 0:r.items[0].stack=0;break;case 1:r.items[0].stack=51;break;
        case 2:r.items[0].virtual_stack=-1;break;case 3:r.items[0].virtual_stack=50;break;
        case 4:r.items[0].arg=1;break;case 5:r.items[0].pokemon_id=0;break;
        case 6:r.items[0].detached=true;break;default:break;}
        gc_items_to_battle(&r);pr_modifier *m=&r.bt.mods[0][0];
        if(k==7){m->is_transferable=true;}if(k==8){m->unknown_subclass=true;}
        CHECK(!pr_mod_is_ported_item(m));
        CHECK(m->cls==PR_MOD_INCREMENTING_STAT||m->unknown_subclass);
    }
    for(int k=0;k<8;k++){
        pr_modifier m={.cls=PR_MOD_INCREMENTING_STAT,.pokemon_id=7,.stack_count=1};
        switch(k){case 0:m.stack_count=0;break;case 1:m.stack_count=51;break;
        case 2:m.virtual_stack_count=-1;break;case 3:m.virtual_stack_count=50;break;
        case 4:m.pokemon_id=-1;break;case 5:m.is_transferable=true;break;
        case 6:m.unknown_subclass=true;break;case 7:m.stack_count=m.virtual_stack_count=INT32_MAX;break;}
        CHECK(!pr_mod_is_ported_item(&m));
    }
    CHECK(fgets(text,sizeof text,input)&&!strcmp(text,"END 32 7500 1152 12\n")&&fgetc(input)==EOF);fclose(input);
    printf("Macho Brace: 32 retained audits, 7500 native stat snapshots, 1152 native EXP-owner snapshots, 12 native capture/PartyExp owners, 6 full capture command routes, %u checks\n",checks);return 0;
}
