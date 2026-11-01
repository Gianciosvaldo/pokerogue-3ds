/* Exercise the real fusion -> ReturnPhase boundary. Source getPlayerField()
 * derives slots from party order after splice; this is not a fusion-stat proof. */
#define main retained_fusion_host_main
#include "fusion_host_test.c"
#undef main
int main(void)
{
    setup(3,false);
    r.double_battle=r.bt.double_battle=true;
    r.party[1].b.on_field=true;r.bt.field[1]=&r.party[1].b;
    uint32_t survivor=r.party[1].b.id;
    screen(splicers(0),false);
    const char *why=NULL;
    CHECK(gc_port_apply_reward(&r,&(gc_offer){.key="DNA_SPLICERS",.arg=0,.target=GC_TARGET_FUSE},1,0,&why));
    CHECK(r.party[0].b.id==survivor&&r.party[0].b.on_field);
    CHECK(r.bt.field[0]==NULL); /* the existing immediate post-fusion contract */
    const int first=r.n_battle_events;
    gc_battle_cleanup_players(&r);
    CHECK(r.bt.field[0]==&r.party[0].b&&!r.party[0].b.on_field);
    int returns=0;
    const pr_event *events=gc_run_battle_event_data(&r);
    for(int i=first;i<r.n_battle_events;i++)if(events[i].type==PR_EV_SWITCH_OUT){
        int field=events[i].battler;CHECK(field==0);
        CHECK(events[i].field[field].pokemon_id_known&&events[i].field[field].pokemon_id==survivor);
        CHECK(events[i].field[field].species==SPECIES_ID_CHARMANDER);
        returns++;
    }
    CHECK(returns==1);
    /* A queued send-out must retain its state even when entry hazards have
     * already defeated the live Pokemon before the app drains those events. */
    setup(3,false);
    gc_starter shed={.species=SPECIES_ID_SHEDINJA,.tera_type=-1,.moves={MOVE_ID_SPLASH}};
    gc_mon_new_player(&r,&r.enemy[0],&shed,20);r.enemy[0].b.is_player=false;r.n_enemy=1;
    r.bt.n_arena_tags=1;r.bt.arena_tags[0]=(pr_arena_tag){.type=ARENA_TAG_TYPE_SPIKES,.side=ARENA_TAG_SIDE_ENEMY,.layers=1,.source_count=1,.source_id=r.party[0].b.id};
    r.n_battle_events=0;
    gc_battle_setup_field(&r,false);
    CHECK(r.enemy[0].b.hp==0);
    gc_event event;int summons=0;
    while(gc_run_next_event(&r,&event))if(event.kind==GC_EV_SUMMON&&event.a==1){
        CHECK(event.summon_snapshot&&event.summon_hp==1&&event.summon_max_hp==1);
        CHECK(event.summon_status==STATUS_EFFECT_NONE);summons++;
    }
    CHECK(summons==1);
    int damage=0,faints=0;events=gc_run_battle_event_data(&r);
    for(int k=0;k<r.n_battle_events;k++){
        if(events[k].type==PR_EV_DAMAGE&&events[k].battler==2){CHECK(events[k].field[2].hp==0&&events[k].id==HIT_RESULT_INDIRECT);damage++;}
        if(events[k].type==PR_EV_FAINT&&events[k].battler==2)faints++;
    }
    CHECK(damage==1&&faints==1);
    printf("Encounter event state: fusion return identity and pre-hazard summon state, %u checks passed\n",checks);
    return 0;
}
