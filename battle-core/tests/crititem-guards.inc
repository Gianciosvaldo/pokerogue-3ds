/* Canonical identities; fused eligibility is covered by the native fused-held proof. */
static int crititem_guard_checks(void)
{
    int checks=0;
    for (int mode=0;mode<24;mode++) {
        pr_battle bt={0}; pr_pokemon p={0}; p.id=10; p.is_player=mode!=9;
        p.species=SPECIES_ID_RATTATA;
        int side=p.is_player?0:1; bt.n_mods[side]=1;
        pr_modifier *m=&bt.mods[side][0];
        *m=(pr_modifier){.cls=PR_MOD_CRIT_BOOSTER,.pokemon_id=p.id,.stack_count=1,.num=1};
        if ((mode>=11 && mode<=18) || mode>=22) {
            m->cls=PR_MOD_SPECIES_CRIT_BOOSTER; m->num=2; m->n_species=3;
            m->species[0]=SPECIES_ID_FARFETCHD; m->species[1]=SPECIES_ID_GALAR_FARFETCHD; m->species[2]=SPECIES_ID_SIRFETCHD;
            p.species=SPECIES_ID_FARFETCHD;
        }
        if (mode==0) m->pokemon_id++;
        if (mode==1) { bt.mods[1][0]=*m;bt.n_mods[1]=1;bt.n_mods[0]=0; }
        if (mode==2) m->unknown_subclass=true;
        if (mode==3) m->stack_count=2;
        if (mode==4) m->virtual_stack_count=1;
        if (mode==5) m->num=2;
        if (mode==6) { m->n_species=1;m->species[0]=SPECIES_ID_FARFETCHD; }
        if (mode==10) m->pokemon_id=-1;
        if (mode==12) p.species=SPECIES_ID_GALAR_FARFETCHD;
        if (mode==13) p.species=SPECIES_ID_SIRFETCHD;
        if (mode==14) p.species=SPECIES_ID_RATTATA;
        if (mode==15) m->num=1;
        if (mode==16) m->species[0]=SPECIES_ID_RATTATA;
        if (mode==17 || mode==19) p.fusion_species=SPECIES_ID_RATTATA;
        if (mode==18) {p.species=SPECIES_ID_RATTATA;p.fusion_species=SPECIES_ID_FARFETCHD;}
        if (mode==20) m->virtual_stack_count=-1;
        if (mode==21) m->stack_count=0;
        if (mode==22) m->n_species=4;
        if (mode==23) {bt.n_mods[side]=2;bt.mods[side][1]=(pr_modifier){.cls=PR_MOD_CRIT_BOOSTER,.pokemon_id=p.id,.stack_count=1,.num=1};}
        int stage=0,want=(mode==9 || mode==10 || mode==19)?1:(mode>=11&&mode<=13)||mode==17||mode==18?2:mode==23?3:0;
        pr_modifier original[2][PR_MAX_MODIFIERS];memcpy(original,bt.mods,sizeof original);
        pr_rng g=bt.rng,b=bt.battle_rng;
        pr_apply_crit_booster_modifiers(&bt,mode==8?NULL:&p,mode==7?NULL:&stage);
        if (stage!=want || memcmp(original,bt.mods,sizeof original)
            || memcmp(&g,&bt.rng,sizeof g) || memcmp(&b,&bt.battle_rng,sizeof b)) return 1;
        if ((mode>=2&&mode<=6 || mode>=15&&mode<=16 || mode>=20&&mode<=22) && !bt.n_unported) return 1;
        checks++;
    }
    printf("Critical item guards: %d checks pass\n",checks);return 0;
}
