/* Fixed rival party closures (Task 48), ported from ai/rival-team-gen.ts and
 * trainers/rival-party-config.ts at pinned upstream 8555c08. */
#include "pr/run.h"
#include <string.h>

typedef struct { uint16_t species; uint16_t final_species; } rival_choice;

#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define RC(cur, fin) { SPECIES_ID_##cur, SPECIES_ID_##fin }

/* Slot 1: the same static roll follows the starter line through all six fights. */
static const rival_choice s1_f1[] = {
 RC(BULBASAUR,VENUSAUR),RC(CHARMANDER,CHARIZARD),RC(SQUIRTLE,BLASTOISE),
 RC(CHIKORITA,MEGANIUM),RC(CYNDAQUIL,TYPHLOSION),RC(TOTODILE,FERALIGATR),
 RC(TREECKO,SCEPTILE),RC(TORCHIC,BLAZIKEN),RC(MUDKIP,SWAMPERT),
 RC(TURTWIG,TORTERRA),RC(CHIMCHAR,INFERNAPE),RC(PIPLUP,EMPOLEON),
 RC(SNIVY,SERPERIOR),RC(TEPIG,EMBOAR),RC(OSHAWOTT,SAMUROTT),
 RC(CHESPIN,CHESNAUGHT),RC(FENNEKIN,DELPHOX),RC(FROAKIE,GRENINJA),
 RC(ROWLET,DECIDUEYE),RC(LITTEN,INCINEROAR),RC(POPPLIO,PRIMARINA),
 RC(GROOKEY,RILLABOOM),RC(SCORBUNNY,CINDERACE),RC(SOBBLE,INTELEON),
 RC(SPRIGATITO,MEOWSCARADA),RC(FUECOCO,SKELEDIRGE),RC(QUAXLY,QUAQUAVAL)
};
static const rival_choice s1_f2[] = {
 RC(IVYSAUR,VENUSAUR),RC(CHARMELEON,CHARIZARD),RC(WARTORTLE,BLASTOISE),
 RC(BAYLEEF,MEGANIUM),RC(QUILAVA,TYPHLOSION),RC(CROCONAW,FERALIGATR),
 RC(GROVYLE,SCEPTILE),RC(COMBUSKEN,BLAZIKEN),RC(MARSHTOMP,SWAMPERT),
 RC(GROTLE,TORTERRA),RC(MONFERNO,INFERNAPE),RC(PRINPLUP,EMPOLEON),
 RC(SERVINE,SERPERIOR),RC(PIGNITE,EMBOAR),RC(DEWOTT,SAMUROTT),
 RC(QUILLADIN,CHESNAUGHT),RC(BRAIXEN,DELPHOX),RC(FROGADIER,GRENINJA),
 RC(DARTRIX,DECIDUEYE),RC(TORRACAT,INCINEROAR),RC(BRIONNE,PRIMARINA),
 RC(THWACKEY,RILLABOOM),RC(RABOOT,CINDERACE),RC(DRIZZILE,INTELEON),
 RC(FLORAGATO,MEOWSCARADA),RC(CROCALOR,SKELEDIRGE),RC(QUAXWELL,QUAQUAVAL)
};
static const rival_choice s1_final[] = {
 RC(VENUSAUR,VENUSAUR),RC(CHARIZARD,CHARIZARD),RC(BLASTOISE,BLASTOISE),
 RC(MEGANIUM,MEGANIUM),RC(TYPHLOSION,TYPHLOSION),RC(FERALIGATR,FERALIGATR),
 RC(SCEPTILE,SCEPTILE),RC(BLAZIKEN,BLAZIKEN),RC(SWAMPERT,SWAMPERT),
 RC(TORTERRA,TORTERRA),RC(INFERNAPE,INFERNAPE),RC(EMPOLEON,EMPOLEON),
 RC(SERPERIOR,SERPERIOR),RC(EMBOAR,EMBOAR),RC(SAMUROTT,SAMUROTT),
 RC(CHESNAUGHT,CHESNAUGHT),RC(DELPHOX,DELPHOX),RC(GRENINJA,GRENINJA),
 RC(DECIDUEYE,DECIDUEYE),RC(INCINEROAR,INCINEROAR),RC(PRIMARINA,PRIMARINA),
 RC(RILLABOOM,RILLABOOM),RC(CINDERACE,CINDERACE),RC(INTELEON,INTELEON),
 RC(MEOWSCARADA,MEOWSCARADA),RC(SKELEDIRGE,SKELEDIRGE),RC(QUAQUAVAL,QUAQUAVAL)
};

/* Slot 2: the same static roll follows one regional bird line. */
static const rival_choice s2_f1[] = {
 RC(PIDGEY,PIDGEOT),RC(HOOTHOOT,NOCTOWL),RC(TAILLOW,SWELLOW),RC(STARLY,STARAPTOR),
 RC(PIDOVE,UNFEZANT),RC(FLETCHLING,TALONFLAME),RC(PIKIPEK,TOUCANNON),
 RC(ROOKIDEE,CORVIKNIGHT),RC(WATTREL,KILOWATTREL)
};
static const rival_choice s2_f2[] = {
 RC(PIDGEOTTO,PIDGEOT),RC(HOOTHOOT,NOCTOWL),RC(TAILLOW,SWELLOW),RC(STARAVIA,STARAPTOR),
 RC(TRANQUILL,UNFEZANT),RC(FLETCHINDER,TALONFLAME),RC(TRUMBEAK,TOUCANNON),
 RC(CORVISQUIRE,CORVIKNIGHT),RC(WATTREL,KILOWATTREL)
};
static const rival_choice s2_final[] = {
 RC(PIDGEOT,PIDGEOT),RC(NOCTOWL,NOCTOWL),RC(SWELLOW,SWELLOW),RC(STARAPTOR,STARAPTOR),
 RC(UNFEZANT,UNFEZANT),RC(TALONFLAME,TALONFLAME),RC(TOUCANNON,TOUCANNON),
 RC(CORVIKNIGHT,CORVIKNIGHT),RC(KILOWATTREL,KILOWATTREL)
};

/* Slots 3 and 4 are flattened pool coordinates. Split-evolution sub-pools remain separate
 * entries, which gives them exactly the same randSeedItem choice weight as upstream. */
static const rival_choice s3_f2[] = {
 RC(NIDORINA,NIDOQUEEN),RC(NIDORINO,NIDOKING),RC(MANKEY,ANNIHILAPE),RC(GROWLITHE,ARCANINE),
 RC(ABRA,ALAKAZAM),RC(MACHOP,MACHAMP),RC(GASTLY,GENGAR),RC(MAGNEMITE,MAGNEZONE),
 RC(RHYHORN,RHYPERIOR),RC(TANGELA,TANGROWTH),RC(PORYGON,PORYGON_Z),RC(ELEKID,ELECTIVIRE),
 RC(MAGBY,MAGMORTAR),RC(MARILL,AZUMARILL),RC(TEDDIURSA,URSALUNA),RC(SWINUB,MAMOSWINE),
 RC(SLAKOTH,SLAKING),RC(ARON,AGGRON),RC(SPHEAL,WALREIN),RC(MUNCHLAX,SNORLAX),
 RC(ROGGENROLA,GIGALITH),RC(TIMBURR,CONKELDURR),RC(TYMPOLE,SEISMITOAD),RC(SANDILE,KROOKODILE),
 RC(YAMASK,COFAGRIGUS),RC(SOLOSIS,REUNICLUS),RC(VANILLITE,VANILLUXE),RC(TYMPOLE,EELEKTROSS),
 RC(LITWICK,CHANDELURE),RC(MUDBRAY,MUDSDALE),RC(DEWPIDER,ARAQUANID),RC(HATENNA,HATTERENE),
 RC(IMPIDIMP,GRIMMSNARL),RC(SMOLIV,ARBOLIVA),RC(NACLI,GARGANACL),
 RC(CHARCADET,ARMAROUGE),RC(CHARCADET,CERULEDGE),RC(TINKATINK,TINKATON),RC(GLIMMET,GLIMMORA)
};
static const rival_choice s3_f3[] = {
 RC(NIDOQUEEN,NIDOQUEEN),RC(NIDOKING,NIDOKING),RC(PRIMEAPE,ANNIHILAPE),RC(ARCANINE,ARCANINE),
 RC(KADABRA,ALAKAZAM),RC(MACHOKE,MACHAMP),RC(HAUNTER,GENGAR),RC(MAGNETON,MAGNEZONE),
 RC(RHYDON,RHYPERIOR),RC(TANGROWTH,TANGROWTH),RC(PORYGON2,PORYGON_Z),RC(ELECTABUZZ,ELECTIVIRE),
 RC(MAGMAR,MAGMORTAR),RC(AZUMARILL,AZUMARILL),RC(URSARING,URSALUNA),RC(PILOSWINE,MAMOSWINE),
 RC(VIGOROTH,SLAKING),RC(LAIRON,AGGRON),RC(SEALEO,WALREIN),RC(SNORLAX,SNORLAX),
 RC(BOLDORE,GIGALITH),RC(GURDURR,CONKELDURR),RC(PALPITOAD,SEISMITOAD),RC(KROKOROK,KROOKODILE),
 RC(COFAGRIGUS,COFAGRIGUS),RC(DUOSION,REUNICLUS),RC(VANILLISH,VANILLUXE),RC(EELEKTRIK,EELEKTROSS),
 RC(LAMPENT,CHANDELURE),RC(MUDSDALE,MUDSDALE),RC(ARAQUANID,ARAQUANID),RC(HATTREM,HATTERENE),
 RC(MORGREM,GRIMMSNARL),RC(DOLLIV,ARBOLIVA),RC(NACLSTACK,GARGANACL),
 RC(ARMAROUGE,ARMAROUGE),RC(CERULEDGE,CERULEDGE),RC(TINKATUFF,TINKATON),RC(GLIMMORA,GLIMMORA)
};
static const rival_choice s3_final[] = {
 RC(NIDOQUEEN,NIDOQUEEN),RC(NIDOKING,NIDOKING),RC(ANNIHILAPE,ANNIHILAPE),RC(ARCANINE,ARCANINE),
 RC(ALAKAZAM,ALAKAZAM),RC(MACHAMP,MACHAMP),RC(GENGAR,GENGAR),RC(MAGNEZONE,MAGNEZONE),
 RC(RHYPERIOR,RHYPERIOR),RC(TANGROWTH,TANGROWTH),RC(PORYGON_Z,PORYGON_Z),RC(ELECTIVIRE,ELECTIVIRE),
 RC(MAGMORTAR,MAGMORTAR),RC(AZUMARILL,AZUMARILL),RC(URSALUNA,URSALUNA),RC(MAMOSWINE,MAMOSWINE),
 RC(SLAKING,SLAKING),RC(AGGRON,AGGRON),RC(WALREIN,WALREIN),RC(SNORLAX,SNORLAX),
 RC(GIGALITH,GIGALITH),RC(CONKELDURR,CONKELDURR),RC(SEISMITOAD,SEISMITOAD),RC(KROOKODILE,KROOKODILE),
 RC(COFAGRIGUS,COFAGRIGUS),RC(REUNICLUS,REUNICLUS),RC(VANILLUXE,VANILLUXE),RC(EELEKTROSS,EELEKTROSS),
 RC(CHANDELURE,CHANDELURE),RC(MUDSDALE,MUDSDALE),RC(ARAQUANID,ARAQUANID),RC(HATTERENE,HATTERENE),
 RC(GRIMMSNARL,GRIMMSNARL),RC(ARBOLIVA,ARBOLIVA),RC(GARGANACL,GARGANACL),
 RC(ARMAROUGE,ARMAROUGE),RC(CERULEDGE,CERULEDGE),RC(TINKATON,TINKATON),RC(GLIMMORA,GLIMMORA)
};

static const rival_choice s4_f3[] = {
 RC(CLEFABLE,CLEFABLE),RC(SLOWBRO,SLOWBRO),RC(SLOWKING,SLOWKING),RC(PINSIR,PINSIR),RC(LAPRAS,LAPRAS),
 RC(SCIZOR,SCIZOR),RC(HERACROSS,HERACROSS),RC(SNEASEL,WEAVILE),RC(GARDEVOIR,GARDEVOIR),RC(MILOTIC,MILOTIC),
 RC(ROSERADE,ROSERADE),RC(SPIRITOMB,SPIRITOMB),RC(LUCARIO,LUCARIO),RC(DRAPION,DRAPION),RC(GALLADE,GALLADE),
 RC(ROTOM,ROTOM),RC(EXCADRILL,EXCADRILL),RC(ZOROARK,ZOROARK),RC(HISUI_ZOROARK,HISUI_ZOROARK),
 RC(FERROTHORN,FERROTHORN),RC(DURANT,DURANT),RC(FLORGES,FLORGES),RC(DOUBLADE,AEGISLASH),
 RC(VIKAVOLT,VIKAVOLT),RC(GOLISOPOD,GOLISOPOD),RC(MIMIKYU,MIMIKYU),RC(DHELMISE,DHELMISE),
 RC(POLTEAGEIST,POLTEAGEIST),RC(SINISTCHA,SINISTCHA),RC(COPPERAJAH,COPPERAJAH),RC(KLEAVOR,KLEAVOR),
 RC(HISUI_BASCULIN,BASCULEGION),RC(HISUI_SNEASEL,SNEASLER),RC(HISUI_QWILFISH,OVERQWIL),
 RC(PAWMOT,PAWMOT),RC(CETITAN,CETITAN),RC(DONDOZO,DONDOZO),RC(DUDUNSPARCE,DUDUNSPARCE),
 RC(GHOLDENGO,GHOLDENGO),RC(GALAR_SLOWBRO,GALAR_SLOWBRO),RC(GALAR_SLOWKING,GALAR_SLOWKING),
 RC(HISUI_ARCANINE,HISUI_ARCANINE),RC(PALDEA_TAUROS,PALDEA_TAUROS)
};
static const rival_choice s4_final[] = {
 RC(CLEFABLE,CLEFABLE),RC(SLOWBRO,SLOWBRO),RC(SLOWKING,SLOWKING),RC(PINSIR,PINSIR),RC(LAPRAS,LAPRAS),
 RC(SCIZOR,SCIZOR),RC(HERACROSS,HERACROSS),RC(WEAVILE,WEAVILE),RC(GARDEVOIR,GARDEVOIR),RC(MILOTIC,MILOTIC),
 RC(ROSERADE,ROSERADE),RC(SPIRITOMB,SPIRITOMB),RC(LUCARIO,LUCARIO),RC(DRAPION,DRAPION),RC(GALLADE,GALLADE),
 RC(ROTOM,ROTOM),RC(EXCADRILL,EXCADRILL),RC(ZOROARK,ZOROARK),RC(HISUI_ZOROARK,HISUI_ZOROARK),
 RC(FERROTHORN,FERROTHORN),RC(DURANT,DURANT),RC(FLORGES,FLORGES),RC(AEGISLASH,AEGISLASH),
 RC(VIKAVOLT,VIKAVOLT),RC(GOLISOPOD,GOLISOPOD),RC(MIMIKYU,MIMIKYU),RC(DHELMISE,DHELMISE),
 RC(POLTEAGEIST,POLTEAGEIST),RC(SINISTCHA,SINISTCHA),RC(COPPERAJAH,COPPERAJAH),RC(KLEAVOR,KLEAVOR),
 RC(BASCULEGION,BASCULEGION),RC(SNEASLER,SNEASLER),RC(OVERQWIL,OVERQWIL),
 RC(PAWMOT,PAWMOT),RC(CETITAN,CETITAN),RC(DONDOZO,DONDOZO),RC(DUDUNSPARCE,DUDUNSPARCE),
 RC(GHOLDENGO,GHOLDENGO),RC(GALAR_SLOWBRO,GALAR_SLOWBRO),RC(GALAR_SLOWKING,GALAR_SLOWKING),
 RC(HISUI_ARCANINE,HISUI_ARCANINE),RC(PALDEA_TAUROS,PALDEA_TAUROS)
};

static const rival_choice s5_final[] = {
 RC(DRAGONITE,DRAGONITE),RC(KINGDRA,KINGDRA),RC(TYRANITAR,TYRANITAR),RC(SALAMENCE,SALAMENCE),
 RC(METAGROSS,METAGROSS),RC(GARCHOMP,GARCHOMP),RC(HAXORUS,HAXORUS),RC(HYDREIGON,HYDREIGON),
 RC(VOLCARONA,VOLCARONA),RC(GOODRA,GOODRA),RC(KOMMO_O,KOMMO_O),RC(DRAGAPULT,DRAGAPULT),
 RC(KINGAMBIT,KINGAMBIT),RC(BAXCALIBUR,BAXCALIBUR),RC(ARCHALUDON,ARCHALUDON),
 RC(HYDRAPPLE,HYDRAPPLE),RC(HISUI_GOODRA,HISUI_GOODRA)
};
static const rival_choice s6_final[] = { RC(RAYQUAZA,RAYQUAZA) };

static const pr_species *sp(int id)
{
    int lo=0, hi=PR_SPECIES_COUNT-1;
    while(lo<=hi){int m=(lo+hi)/2; if(pr_species_table[m].id==id)return &pr_species_table[m];
        if(pr_species_table[m].id<id)lo=m+1;else hi=m-1;}
    return NULL;
}
static const pr_form *base_form(int id)
{
    const pr_species *s=sp(id); return s ? &pr_forms[s->form0] : NULL;
}
static bool ground_immune(const pr_form *f)
{
    const int a1=f->ability1,a2=f->ability2,ah=f->ability_hidden;
    const bool g1=a1==ABILITY_ID_LEVITATE||a1==ABILITY_ID_EELEVATE||a1==ABILITY_ID_EARTH_EATER;
    const bool g2=a2==ABILITY_ID_LEVITATE||a2==ABILITY_ID_EELEVATE||a2==ABILITY_ID_EARTH_EATER;
    /* Mirror upstream's hidden-ability test, including its ability2 reference. */
    return g1 && (!a2 || g2) && (!ah || g2);
}
static void add_weaknesses(int species, int counts[PR_TYPE_COUNT])
{
    const pr_form *f=base_form(species); if(!f)return;
    for(int atk=0;atk<PR_TYPE_COUNT;atk++){
        if(atk==POKEMON_TYPE_UNKNOWN||atk==POKEMON_TYPE_STELLAR)continue;
        if(atk==POKEMON_TYPE_GROUND&&ground_immune(f))continue;
        double m=pr_type_chart_x2[atk][f->type1]/2.0;
        if(m>=2&&f->type2>=0)m*=pr_type_chart_x2[atk][f->type2]/2.0;
        if(m>=2)counts[atk]++;
    }
}
static bool legal_choice(int species,const int types[PR_TYPE_COUNT],const int weak[PR_TYPE_COUNT],
                         bool balance_types,bool balance_weak)
{
    const pr_form *f=base_form(species); if(!f)return false;
    if(balance_types&&(types[f->type1]>=1||(f->type2>=0&&types[f->type2]>=1)))return false;
    if(balance_weak){
        int w[PR_TYPE_COUNT]={0}; add_weaknesses(species,w);
        for(int i=0;i<PR_TYPE_COUNT;i++)if(w[i]&&weak[i]>=2)return false;
    }
    return true;
}
static void prior_typings(const uint16_t chosen_final[6],int upto,int types[PR_TYPE_COUNT],int weak[PR_TYPE_COUNT])
{
    memset(types,0,sizeof(int)*PR_TYPE_COUNT); memset(weak,0,sizeof(int)*PR_TYPE_COUNT);
    for(int i=0;i<upto;i++){
        const pr_form *f=base_form(chosen_final[i]); if(!f)continue;
        types[f->type1]++; if(f->type2>=0)types[f->type2]++;
        add_weaknesses(chosen_final[i],weak);
    }
}
static int pick(pr_rng *rng,const rival_choice *pool,int n,const uint16_t chosen_final[6],int slot,
                bool bt,bool bw,uint16_t *final)
{
    int types[PR_TYPE_COUNT],weak[PR_TYPE_COUNT],choices[64],nc=0;
    prior_typings(chosen_final,slot,types,weak);
    if(bt||bw)for(int i=0;i<n;i++)if(legal_choice(pool[i].final_species,types,weak,bt,bw))choices[nc++]=i;
    if(!nc)for(int i=0;i<n;i++)choices[nc++]=i;
    int ci=choices[nc==1?0:pr_rng_integer_in_range(rng,0,nc-1)];
    *final=pool[ci].final_species; return pool[ci].species;
}
static const rival_choice *pool_for(int fight,int slot,int *n,bool *bt,bool *bw)
{
    *bt=*bw=false;
    if(slot==0){if(fight==1){*n=N(s1_f1);return s1_f1;}if(fight==2){*n=N(s1_f2);return s1_f2;}*n=N(s1_final);return s1_final;}
    if(slot==1){if(fight==1){*n=N(s2_f1);return s2_f1;}if(fight==2){*n=N(s2_f2);return s2_f2;}*n=N(s2_final);return s2_final;}
    if(slot==2){*bt=*bw=true;if(fight==2){*n=N(s3_f2);return s3_f2;}if(fight==3){*n=N(s3_f3);return s3_f3;}*n=N(s3_final);return s3_final;}
    if(slot==3){*bt=*bw=true;if(fight==3){*n=N(s4_f3);return s4_f3;}*n=N(s4_final);return s4_final;}
    if(slot==4){*n=N(s5_final);return s5_final;}
    if(slot==5){*n=N(s6_final);return s6_final;}
    return NULL;
}

static void bird_ability(int species, pr_enemy_gen *m)
{
    switch(species){
    case SPECIES_ID_PIDGEY:case SPECIES_ID_PIDGEOTTO:case SPECIES_ID_PIDGEOT:
    case SPECIES_ID_TAILLOW:case SPECIES_ID_SWELLOW:
    case SPECIES_ID_STARLY:case SPECIES_ID_STARAVIA:case SPECIES_ID_STARAPTOR:m->ability_index=0;break;
    case SPECIES_ID_PIDOVE:case SPECIES_ID_TRANQUILL:case SPECIES_ID_UNFEZANT:
    case SPECIES_ID_WATTREL:case SPECIES_ID_KILOWATTREL:m->ability_index=1;break;
    case SPECIES_ID_HOOTHOOT:case SPECIES_ID_NOCTOWL:
    case SPECIES_ID_PIKIPEK:case SPECIES_ID_TRUMBEAK:case SPECIES_ID_TOUCANNON:
    case SPECIES_ID_FLETCHLING:case SPECIES_ID_FLETCHINDER:case SPECIES_ID_TALONFLAME:m->ability_index=2;break;
    default:break;
    }
}
static int forced_level(int fight,int slot,int level)
{
    if(slot==2){static const int v[7]={0,0,16,36,71,125,189};return v[fight]?v[fight]:level;}
    if(slot==3){static const int v[7]={0,0,0,38,71,125,189};return v[fight]?v[fight]:level;}
    if(slot==4){static const int v[7]={0,0,0,0,69,127,189};return v[fight]?v[fight]:level;}
    if(slot==5){return fight==5?129:fight==6?200:level;}
    return level;
}
static void postprocess(pr_run *run,int fight,int slot,int species,pr_rng *rng,pr_enemy_gen *m,int level)
{
    if(slot==0){
        m->ability_index=0;
        const pr_form *f=base_form(species); if(f)m->tera_type=f->type1;
        if(fight==5)m->boss_segments=2; else if(fight==6)m->boss_segments=3;
    } else if(slot==1){
        bird_ability(species,m);
        if(fight==6)m->boss_segments=2;
    } else if(slot==3 && fight>=3){
        if(species==SPECIES_ID_ROTOM){
            static const int f[]={1,2,5}; m->form=f[pr_rng_integer_in_range(rng,0,2)];
        } else if(species==SPECIES_ID_PALDEA_TAUROS){
            m->form=pr_rng_integer_in_range(rng,1,2);
        } else if(fight>=4 && species==SPECIES_ID_BASCULEGION){
            m->form=pr_rng_integer_in_range(rng,0,1);
        }
    } else if(slot==5 && fight>=5){
        if(fight==6)m->form=1;
        m->shiny=true; m->variant=1;
        m->boss_segments=fight==5?3:pr_run_get_encounter_boss_segments(run,195,level,species,true);
    }
}

int pr_run_gen_rival_party(pr_run *run,const pr_trainer_party_ctx *ctx,int fight,pr_trainer_party *out)
{
    memset(out,0,sizeof *out);
    if(fight<1||fight>6)return -1;
    uint16_t chosen_final[6]={0};
    const pr_rng saved=run->rng;
    for(int slot=0;slot<ctx->n_levels&&slot<6;slot++){
        int n=0;bool bt,bw;const rival_choice *pool=pool_for(fight,slot,&n,&bt,&bw);
        if(!pool||!n){run->rng=saved;return -1;}
        /* Static rivals all derive to TrainerType.RIVAL for their per-member seed. */
        pr_rng rng=pr_run_seed_offset_rng(run,TRAINER_TYPE_RIVAL+((slot+1)<<8),NULL);
        int species=pick(&rng,pool,n,chosen_final,slot,bt,bw,&chosen_final[slot]);
        int initial_level=ctx->levels[slot];

        pr_enemy_gen_ctx ec=ctx->enemy;
        ec.species=species;ec.level=initial_level;ec.trainer_slot=1;ec.boss=false;ec.shiny_lock=false;
        if(pr_run_enemy_pokemon_base_rng(&rng,run,&ec,&out->mons[slot])<0){run->rng=saved;return -1;}
        int level=forced_level(fight,slot,initial_level);
        postprocess(run,fight,slot,species,&rng,&out->mons[slot],level);
        out->species[slot]=(uint16_t)species;
        out->levels[slot]=level;
        out->n=slot+1;
    }
    run->rng=saved;
    return 0;
}
