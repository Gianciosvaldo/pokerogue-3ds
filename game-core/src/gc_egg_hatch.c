/* EggLapsePhase / Egg.generatePlayerPokemon: isolated egg seed and profile rewards. */
#include "internal.h"

bool gc_egg_generate(gc_run *run, const gc_egg *egg, gc_mon *out)
{
    if (!run || !egg || !out || !egg->species || egg->species>GC_MAX_SPECIES_ID
        || egg->source_type>4 || egg->variant_tier>2 || egg->egg_move_index>3) return false;
    pr_seedstr base,shift;
    pr_seed_from_ascii(&base,"1073741824");
    /* String.fromCharCode wraps each shifted code unit to 16 bits. */
    pr_seed_shift(&shift,&base,(int)(egg->id&0xffffu));
    pr_rng rng;
    pr_rng_sow(&rng,&shift);
    int species=egg->species;
    if (species==SPECIES_ID_PHIONE && egg->source_type==3)
        species=pr_rng_seed_int(&rng,8,0)?SPECIES_ID_PHIONE:SPECIES_ID_MANAPHY;
    const pr_species_gen *sp=gc_species_gen(species);
    if (!sp) return false;
    bool egg_hidden=!pr_rng_seed_int(&rng,egg->source_type==3?8:192,0);
    int ability;
    if (sp->has_hidden && (egg->override_hidden_ability || egg_hidden)) ability=2;
    else {
        int regular=sp->same_abilities?0:pr_rng_seed_int(&rng,2,0);
        bool hidden=sp->has_hidden && !pr_rng_seed_int(&rng,256,0);
        ability=hidden?2:regular;
    }
    /* The constructor receives shiny=false; the Egg applies shiny/variant after
     * construction, so it must not trigger a constructor shiny-variant roll. */
    if (!gc_mon_roll_player(run,&rng,out,species,1,ability,-2,0)) return false;
    uint32_t iv=(uint32_t)floor(pr_rng_frac(&rng)*4294967295.0);
    static const int shifts[]={25,20,15,10,5,0};
    for (int s=0;s<6;s++) {
        int value=(iv>>shifts[s])&31;
        if (value>out->b.ivs[s]) out->b.ivs[s]=(uint8_t)value;
    }
    out->b.shiny=egg->is_shiny;
    out->variant=egg->variant_tier;
    out->b.is_player=true;
    return true;
}

int gc_system_hatch_ready_eggs(gc_run *run)
{
    if (!run || !run->sys) return 0;
    gc_system *sys=run->sys;
    int hatched=0;
    for (int i=0;i<sys->n_eggs;) {
        gc_egg egg=sys->eggs[i];
        if (egg.hatch_waves>0) { i++; continue; }
        gc_mon mon;
        if (!gc_egg_generate(run,&egg,&mon)) { i++; continue; }
        /* Remove only after generation succeeds; already-zero eggs from older
         * versions hatch here too. The profile and run share the normal save. */
        memmove(&sys->eggs[i],&sys->eggs[i+1],(sys->n_eggs-i-1)*sizeof sys->eggs[0]);
        sys->n_eggs--;
        gc_system_set_pokemon_caught_ex(sys,&mon,true,true,false,false,NULL);
        gc_system_update_species_dex_ivs(sys,mon.b.species,mon.b.ivs);
        sys->stats.total_hatches++;
        gc_ev_msg(run,"egg:hatchFromTheEgg",1,pr_arg_pokemon_name(&run->bt,"pokemonName",&mon.b));
        for (int k=0;k<gc_starter_egg_moves_count;k++) {
            const gc_starter_egg_moves *moves=&gc_starter_egg_moves_data[k];
            if (moves->species!=mon.b.species || !moves->egg_moves[egg.egg_move_index]) continue;
            unsigned bit=1u<<egg.egg_move_index;
            if (!(sys->starters[mon.b.species].egg_moves&bit)) {
                sys->starters[mon.b.species].egg_moves|=bit;
                gc_ev_msg(run,egg.egg_move_index==3?"egg:rareEggMoveUnlock":"egg:eggMoveUnlock",1,
                    pr_arg_move("moveName",moves->egg_moves[egg.egg_move_index]));
            }
            break;
        }
        hatched++;
    }
    return hatched;
}
