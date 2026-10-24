/* Reuse the strict input/locale/world helpers; its independent phase replay is
 * still built and run separately. No expected acquisition result comes from C. */
#define main retained_form_phase_main
#include "form_item_phase_host_test.c"
#undef main
static bool acquisitions[416][2][2][4][2][5];
int main(int argc,char **argv)
{
    CHECK(argc==2&&getenv("TURNTEST_LOCALES"));input=fopen(argv[1],"r");CHECK(input);
    CHECK(fgets(text,sizeof text,input)&&!strcmp(text,"FORMACQUIRE 1 416 66560\n"));line++;
    record('U');for(int k=0;k<3;k++){uint64_t id=wide();CHECK(id&&id<=UINT32_MAX);ids[k]=id;for(int j=0;j<k;j++)CHECK(ids[k]!=ids[j]);}finish();
    for(int i=0;i<416;i++){
        record('R');CHECK(bounded(0,415)==i);rule *f=&rules[i];f->species=bounded(1,GC_MAX_SPECIES_ID);f->from=bounded(0,255);f->to=bounded(0,255);f->item=bounded(0,199);finish();
        const pr_item_form_rule *p=&pr_item_form_rules[i];CHECK(p->species==f->species&&p->from==f->from&&p->to==f->to&&p->item==f->item);
    }
    setup();
    for(cid=0;cid<66560;cid++){
        record('C');CHECK(bounded(0,66559)==cid);int which=bounded(0,415),owner=bounded(0,2),rare=bounded(0,1),gate=bounded(0,3),duplicate=bounded(0,1),tier=bounded(0,4),native_result=bounded(0,1);finish();CHECK(owner!=1);
        CHECK(!acquisitions[which][owner/2][rare][gate][duplicate][tier]);acquisitions[which][owner/2][rare][gate][duplicate][tier]=true;
        record('B');int on_field=bounded(0,1),ability=bounded(0,PR_ABILITY_COUNT-1),base_ability=bounded(0,PR_ABILITY_COUNT-1),level=bounded(1,65536),nature=bounded(0,24),gender=bounded(-1,1),hp=bounded(0,INT32_MAX);finish();CHECK(on_field==(owner==0));
        record('M');int expected_rare=bounded(0,1),expected_item=bounded(0,199);uint32_t expected_owner=word();int active=bounded(0,1),stack=bounded(1,1),virtual=bounded(0,0),locked=bounded(1,1);finish();
        record('P');int balls[5];for(int k=0;k<5;k++)balls[k]=bounded(0,255);finish();
        record('Q');int kind=bounded(0,2),selected=bounded(-1,415);finish();CHECK(native_result==!!kind);CHECK((kind==1)==(selected>=0));
        record('T');char expected_text[4096];size_t len=strcspn(cursor,"\n");CHECK(cursor[len]=='\n'&&len<sizeof expected_text);memcpy(expected_text,cursor,len);expected_text[len]=0;CHECK((kind!=2)==!strcmp(expected_text,"-"));
        if(kind==2){CHECK(!(len%2));for(size_t k=0;k<len;k++)CHECK((expected_text[k]>='0'&&expected_text[k]<='9')||(expected_text[k]>='a'&&expected_text[k]<='f'));}
        memset(&profile,0,sizeof profile);for(int k=0;k<6;k++)profile.dex[pr_form_item_caught_species[k]].caught_attr=gate&2?1:0;
        for(int k=0;k<3;k++){r.party[k]=base;r.party[k].b.id=ids[k];r.party[k].b.ability=ABILITY_ID_LEVITATE;r.party[k].b.hp=1;r.party[k].b.on_field=k<2;}
        rule *fact=&rules[which];gc_mon *m=&r.party[owner];pr_pokemon *p=&m->b;p->species=fact->species;p->form=fact->from;p->level=level;p->nature=nature;m->base_nature=nature;m->has_custom_nature=false;p->gender=gender;p->hp=hp;
        p->base_ability=base_ability;p->ability=ability;p->ability_index=m->ability_index=0;p->lookup_ability_override=gate&1?ABILITY_ID_LEVITATE:-1;p->passive_flag=false;p->passive=0;
        r.n_items=duplicate;r.n_enemy_items=0;if(duplicate)r.items[0]=(gc_item){.key=rare?"FORM_CHANGE_ITEM":"RARE_FORM_CHANGE_ITEM",.arg=fact->item,.stack=1,.pokemon_id=ids[owner],.form_inactive=true,.non_transferable=true};
        memset(r.pokeballs,0,sizeof r.pokeballs);gc_rq_clear(&r);memset(&r.eng,0,sizeof r.eng);r.eng.current=-1;r.eng.n_levels=1;r.bt.engine=&r.eng;r.bt.n_unported=r.bt.n_deferred=0;r.ev_count=r.ev_head=0;r.ev_overflow=false;
        pr_rng global=r.bt.rng,battle=r.bt.battle_rng,run_rng=r.run.rng;pr_pokemon before=*p;
        gc_offer offer={.key=rare?"RARE_FORM_CHANGE_ITEM":"FORM_CHANGE_ITEM",.arg=fact->item,.tier=tier,.target=GC_TARGET_POKEMON};const char *why=NULL;
        /* Scene.addModifier's false means no activation, not rejected reward. */
        CHECK(gc_port_apply_reward(&r,&offer,owner,-1,&why)&&!why);
        CHECK(r.n_items==1&&r.n_enemy_items==0);gc_item *item=&r.items[0];CHECK(!strcmp(item->key,expected_rare?"RARE_FORM_CHANGE_ITEM":"FORM_CHANGE_ITEM")&&item->arg==expected_item&&item->pokemon_id==expected_owner&&item->form_inactive==!active&&item->stack==stack&&item->virtual_stack==virtual&&item->non_transferable==locked&&!item->detached&&!item->eviolite_base);
        for(int k=0;k<5;k++)CHECK(r.pokeballs[k]==balls[k]);
        CHECK(p->species==before.species&&p->form==before.form&&p->hp==before.hp&&p->ability==before.ability&&p->base_ability==before.base_ability);
        gc_rphase phase;if(kind==1){CHECK(gc_rq_next(&gc_int(&r)->rq,&phase));CHECK(phase.type==RP_REWARD_FORM&&phase.a==owner&&phase.b==rules[selected].to);}CHECK(!gc_rq_next(&gc_int(&r)->rq,&phase));
        int messages=0;gc_event ev;while(gc_run_next_event(&r,&ev)){CHECK(ev.kind==GC_EV_MESSAGE);messages++;char message[2048],encoded[4096];pr_text_source src={locale,NULL,"en"};size_t length=pr_msg_format(&ev.msg,&src,message,sizeof message);CHECK(length&&length<sizeof message);for(size_t k=0;k<length;k++)sprintf(encoded+2*k,"%02x",(unsigned char)message[k]);CHECK(!strcmp(encoded,expected_text));}
        CHECK(messages==(kind==2)&&!r.eng.overflow&&!r.ev_overflow&&!r.bt.n_unported&&!r.bt.n_deferred);
        CHECK(!memcmp(&global,&r.bt.rng,sizeof global)&&!memcmp(&battle,&r.bt.battle_rng,sizeof battle)&&!memcmp(&run_rng,&r.run.rng,sizeof run_rng));
    }
    CHECK(fgets(text,sizeof text,input)&&!strcmp(text,"END 416 66560\n")&&fgetc(input)==EOF);fclose(input);
    printf("Form acquisition: 66560 native inventory/phase/message/ball snapshots; %u checks\n",checks);return 0;
}
