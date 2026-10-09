/* Admission validates canonical identity independently from raw effective-stat math. */
static int statitem_guard_checks(void)
{
    int checks=0;
    for(int family=0;family<7;family++) for(int mode=0;mode<9;mode++) {
        pr_modifier m={.cls=family?PR_MOD_SPECIES_STAT_BOOSTER:PR_MOD_EVOLUTION_STAT_BOOSTER,
            .stack_count=1,.mult=family?2:1.5};
        if(family==0) m.stats_mask=(1u<<STAT_DEF)|(1u<<STAT_SPDEF);
        if(family==1) {m.stats_mask=(1u<<STAT_ATK)|(1u<<STAT_SPATK);m.n_species=1;m.species[0]=SPECIES_ID_PIKACHU;}
        if(family==2) {m.stats_mask=1u<<STAT_ATK;m.n_species=3;m.species[0]=SPECIES_ID_CUBONE;m.species[1]=SPECIES_ID_MAROWAK;m.species[2]=SPECIES_ID_ALOLA_MAROWAK;}
        if(family==3||family==4) {m.stats_mask=1u<<(family==3?STAT_DEF:STAT_SPD);m.n_species=1;m.species[0]=SPECIES_ID_DITTO;}
        if(family==5||family==6) {m.stats_mask=1u<<(family==5?STAT_SPDEF:STAT_SPATK);m.n_species=1;m.species[0]=SPECIES_ID_CLAMPERL;}
        if(mode==1)m.unknown_subclass=true;
        if(mode==2)m.stack_count=2;
        if(mode==3)m.stack_count=0;
        if(mode==4)m.virtual_stack_count=1;
        if(mode==5)m.virtual_stack_count=-1;
        if(mode==6)m.mult+=0.1;
        if(mode==7)m.stats_mask=1u<<STAT_HP;
        if(mode==8){m.n_species=1;m.species[0]=SPECIES_ID_MEW;}
        pr_modifier before=m;
        if(pr_mod_is_ported_item(&m)!=(mode==0)||memcmp(&m,&before,sizeof m))return 1;
        checks++;
    }
    printf("Effective stat item guards: %d checks pass\n",checks);return 0;
}
