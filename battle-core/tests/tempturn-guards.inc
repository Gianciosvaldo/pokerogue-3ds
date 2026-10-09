/* Canonical side-wide admission is independent of raw helper math and game durations. */
static int tempturn_guard_checks(void)
{
    int checks=0;
    for(int family=0;family<7;family++)for(int mode=0;mode<12;mode++){
        pr_modifier m={.cls=family==6?PR_MOD_TEMP_CRIT_BOOSTER:PR_MOD_TEMP_STAT_STAGE_BOOSTER,
            .stack_count=mode==1?2:1,.stat=family==6?0:family+1,.mult=family==5?1:0.2};
        if(mode==2)m.unknown_subclass=true;
        if(mode==3)m.stack_count=0;
        if(mode==4)m.stack_count=3;
        if(mode==5)m.virtual_stack_count=1;
        if(mode==6)m.virtual_stack_count=-1;
        if(mode==7)m.pokemon_id=-1;
        if(mode==8)m.pokemon_id=123;
        if(mode==9)m.stat=family==6?1:0;
        if(mode==10)m.stat=STAT_EVA;
        if(mode==11){if(family==6)m.stat=STAT_ATK;else m.mult+=0.1;}
        pr_modifier before=m;
        if(pr_mod_is_ported_item(&m)!=(mode<2)||memcmp(&m,&before,sizeof m))return 1;
        checks++;
    }
    for(int family=0;family<7;family++){
        pr_battle bt={0};bt.n_mods[0]=1;
        bt.mods[0][0]=(pr_modifier){.cls=family==6?PR_MOD_TEMP_CRIT_BOOSTER:PR_MOD_TEMP_STAT_STAGE_BOOSTER,
            .stack_count=1,.stat=family==6?0:family+1,.mult=family==5?1:0.2};
        double value[3]={0};int crit[3]={0};
        for(int mode=0;mode<3;mode++){
            bt.mods[0][0].stack_count=mode?2:1;
            if(mode==2){bt.n_mods[0]=2;bt.mods[0][1]=bt.mods[0][0];}
            pr_apply_temp_stat_stage_booster_modifiers(&bt,true,family+1,&value[mode]);
            pr_apply_temp_crit_booster_modifiers(&bt,true,&crit[mode]);
        }
        if(value[0]!=value[1]||value[2]!=2*value[0]||crit[0]!=crit[1]||crit[2]!=2*crit[0])return 1;
        checks+=3;
    }
    printf("Temporary item guards: %d checks pass\n",checks);return 0;
}
