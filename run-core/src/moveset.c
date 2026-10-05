/* Task 63 bounded port at 8555c08: level-only pools and fixed dispatch.
 * Battle-core tables are read-only. Broader pools return an explicit reason. */
#include "pr/moveset.h"
#include <math.h>
#include <string.h>

const char *pr_moveset_reason(pr_moveset_result r)
{
    static const char *const names[] = {"supported", "species", "form", "level-pool",
        "tm-egg-pool", "ability-state", "fusion", "tera", "challenge", "invalid"};
    return r >= 0 && r < PR_MOVESET_REASON_COUNT ? names[r] : "invalid";
}
static const pr_species *species_data(int id)
{
    for (int i=0;i<PR_SPECIES_COUNT;i++) if (pr_species_table[i].id==id) return &pr_species_table[i];
    return NULL;
}
static void append(pr_generated_moveset *out, int id, int up)
{
    int i=out->count++, step=pr_moves[id].pp/5;
    out->moves[i]=id; out->pp_up[i]=up;
    out->pp[i]=pr_moves[id].pp + up*(step<1 ? 1 : step);
}
typedef struct { int id; double weight; } weighted;
static int choose(pr_rng *rng, const weighted *pool, int n)
{
    int total=0;
    for (int i=0;i<n;i++) total+=(int)pool[i].weight;
    int roll=pr_rng_seed_int(rng,total,0), i=0;
    /* Pinned upstream uses >, rather than >=, at weight boundaries. */
    while (i+1<n && roll>pool[i].weight) roll-=(int)pool[i++].weight;
    return pool[i].id;
}
pr_moveset_result pr_run_enemy_moveset(pr_rng *rng, const pr_moveset_ctx *c, pr_generated_moveset *out)
{
    if (!rng || !c || !out || c->level<1 || c->attack<1 || c->special_attack<1) return PR_MOVESET_INVALID;
    bool fixed=c->species==SPECIES_ID_SMEARGLE || c->species==SPECIES_ID_ETERNATUS;
    if (!fixed && c->species!=SPECIES_ID_CATERPIE && c->species!=SPECIES_ID_WEEDLE
        && c->species!=SPECIES_ID_MAGIKARP && c->species!=SPECIES_ID_DITTO) return PR_MOVESET_SPECIES;
    if (c->form<0 || c->form>(c->species==SPECIES_ID_ETERNATUS ? 1 : 0)) return PR_MOVESET_FORM;
    if (c->fused) return PR_MOVESET_FUSION;
    if (c->other_challenges || (c->inverse && !fixed)) return PR_MOVESET_CHALLENGE;
    if (c->will_tera && !fixed) return PR_MOVESET_TERA;
    const pr_species *sp=species_data(c->species);
    const pr_form *form=&pr_forms[sp->form0+c->form];
    if (!fixed && (c->passive || (c->ability!=form->ability1
        && !(form->ability2 && c->ability==form->ability2)
        && !(form->ability_hidden && c->ability==form->ability_hidden)))) return PR_MOVESET_ABILITY_STATE;
    if (!fixed && c->trainer && c->level>=(c->species==SPECIES_ID_DITTO ? 80 : 25)) return PR_MOVESET_TM_EGG_POOL;
    if (c->species==SPECIES_ID_MAGIKARP && c->level>=25) return PR_MOVESET_LEVEL_POOL;
    pr_generated_moveset result={0};
    if (fixed) {
        if (c->species==SPECIES_ID_SMEARGLE) for (int i=0;i<4;i++) append(&result,MOVE_ID_SKETCH,0);
        else {
            append(&result,c->form ? MOVE_ID_DYNAMAX_CANNON : MOVE_ID_ETERNABEAM,0);
            append(&result,c->form ? MOVE_ID_CROSS_POISON : MOVE_ID_SLUDGE_BOMB,0);
            append(&result,c->inverse ? MOVE_ID_EARTH_POWER : MOVE_ID_FLAMETHROWER,0);
            append(&result,c->form ? MOVE_ID_RECOVER : MOVE_ID_COSMIC_POWER,c->form ? -4 : 0);
        }
        *out=result; return PR_MOVESET_OK;
    }
    weighted pool[4]; int n=0;
    /* Supported roots have no prevolution/form/relearn/evolution moves, forced
     * signatures or superseding pairs. Their native abilities do not change
     * these moves' effective power. Status moves have no self-boost conditions. */
    for (int i=0;i<sp->level_moves.n;i++) {
        pr_level_move e=sp->level_moves.v[i];
        if (e.level<1 || e.level>c->level || pr_moves[e.move].unimplemented) continue;
        bool duplicate=false;
        for (int j=0;j<n;j++) duplicate|=pool[j].id==e.move;
        if (duplicate) continue;
        if ((c->boss || c->trainer) && c->level>=70 &&
            (e.move==MOVE_ID_TACKLE || e.move==MOVE_ID_POISON_STING || e.move==MOVE_ID_SPLASH)) {
            result.soft_rejection=true; continue;
        }
        if (n==4) return PR_MOVESET_LEVEL_POOL;
        pool[n++]=(weighted){e.move,e.level+20};
    }
    double max_power=40;
    for (int i=0;i<n;i++) {
        const pr_move *m=&pr_moves[pool[i].id];
        if (m->category!=MOVE_CATEGORY_STATUS) max_power=fmax(max_power,m->power*(m->accuracy<0 ? 1 : m->accuracy/100.0));
    }
    double ratio=(double)(c->attack<c->special_attack ? c->attack : c->special_attack)
        /(c->attack>c->special_attack ? c->attack : c->special_attack);
    double adjustment=fmin(js_pow(ratio,3)*2,1);
    for (int i=0;i<n;i++) {
        const pr_move *m=&pr_moves[pool[i].id];
        if (m->category!=MOVE_CATEGORY_STATUS) {
            pool[i].weight*=fmax(0.25,fmin(1,m->power*(m->accuracy<0 ? 1 : m->accuracy/100.0)/max_power));
            if (c->attack<=c->special_attack) pool[i].weight*=adjustment;
        }
        pool[i].weight=ceil(js_pow(pool[i].weight,c->boss ? 2.0 : 1.6)*100);
    }
    weighted stab[4]; int ns=0;
    for (int i=0;i<n;i++) {
        const pr_move *m=&pr_moves[pool[i].id];
        if (m->category!=MOVE_CATEGORY_STATUS && (m->type==form->type1 || m->type==form->type2)) stab[ns++]=pool[i];
    }
    if (ns) { append(&result,choose(rng,stab,ns),0); result.forced_stab=true; }
    while (result.count<4) {
        weighted remaining[4]; int nr=0, damaging=0;
        for (int j=0;j<result.count;j++) damaging+=pr_moves[result.moves[j]].power>1;
        for (int i=0;i<n;i++) {
            const pr_move *m=&pr_moves[pool[i].id];
            bool selected=false, overlap=false;
            for (int j=0;j<result.count;j++) {
                const pr_move *chosen=&pr_moves[result.moves[j]];
                selected|=result.moves[j]==pool[i].id;
                overlap|=chosen->category!=MOVE_CATEGORY_STATUS && chosen->type==m->type;
            }
            if (selected) continue;
            double weight=pool[i].weight;
            if (c->trainer && m->category!=MOVE_CATEGORY_STATUS) {
                if (overlap) weight=sqrt(weight);
                else {
                    weight/=fmax(js_pow(4,damaging)/8,0.5);
                    if (m->type==form->type1 || m->type==form->type2) weight*=20;
                }
                weight=ceil(weight);
            }
            remaining[nr++]=(weighted){pool[i].id,weight};
        }
        if (!nr) break;
        append(&result,choose(rng,remaining,nr),0);
    }
    *out=result; return PR_MOVESET_OK;
}
