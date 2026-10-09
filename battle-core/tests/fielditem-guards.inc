/* Canonical admission is separate from the original raw helper contract. */
static int fielditem_guard_checks(void)
{
    int checks=0;
    for(int mode=0;mode<9;mode++) {
        pr_modifier m={.cls=PR_MOD_FIELD_EFFECT,.stack_count=1};
        if(mode==1)m.stack_count=2;
        if(mode==2)m.virtual_stack_count=1;
        if(mode==3)m.unknown_subclass=true;
        if(mode==4)m.stack_count=0;
        if(mode==5)m.stack_count=3;
        if(mode==6)m.virtual_stack_count=-1;
        if(mode==7)m.virtual_stack_count=2;
        if(mode==8)m.cls=PR_MOD_HELD_ITEM;
        pr_modifier before=m;
        if(pr_mod_is_ported_item(&m)!=(mode<3)||memcmp(&m,&before,sizeof m))return 1;
        checks++;
    }
    for(int mode=0;mode<10;mode++) {
        pr_battle bt={0};pr_pokemon p={0};p.id=10;p.is_player=mode!=9;
        int side=p.is_player?0:1;bt.n_mods[side]=1;
        bt.mods[side][0]=(pr_modifier){.cls=PR_MOD_FIELD_EFFECT,.pokemon_id=10,.stack_count=1};
        pr_modifier *m=&bt.mods[side][0];int want=7;
        if(mode==1){m->stack_count=2;want=9;}
        if(mode==2)m->virtual_stack_count=1;
        if(mode==3){m->pokemon_id=20;want=5;}
        if(mode==4){bt.mods[1-side][0]=*m;bt.n_mods[1-side]=1;bt.n_mods[side]=0;want=5;}
        if(mode==5)m->pokemon_id=-1;
        if(mode==6||mode==7){bt.n_mods[side]=2;bt.mods[side][1]=*m;bt.mods[side][1].stack_count=2;if(mode==7){m->pokemon_id=20;want=9;}}
        /* Raw synthetic stacks retain pinned real-only arithmetic, without flags. */
        if(mode==8){m->stack_count=3;m->virtual_stack_count=1;want=11;}
        pr_modifier before[2][PR_MAX_MODIFIERS];memcpy(before,bt.mods,sizeof before);
        int32_t duration=5;pr_rng rng=bt.rng,battle=bt.battle_rng;
        pr_apply_field_effect_modifier(&bt,&p,&duration);
        if(duration!=want||bt.n_unported||memcmp(before,bt.mods,sizeof before)||memcmp(&rng,&bt.rng,sizeof rng)||memcmp(&battle,&bt.battle_rng,sizeof battle))return 1;
        checks++;
    }
    printf("Field effect item guards: %d checks pass\n",checks);return 0;
}
