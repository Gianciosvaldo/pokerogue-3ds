/* Strict transport/replay for the actual pinned Transform/Imposter boundary. */
#include "pr/engine.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH = 104 };
static FILE *input;
static int lineno;
static char line[8192];
static void bad(void) { fprintf(stderr, "invalid Transform fixture at line %d\n", lineno); exit(2); }
static void read_vector(const char *key, double *v, int n)
{
    if (!fgets(line, sizeof line, input)) bad();
    lineno++;
    if (strncmp(line, key, strlen(key))) bad();
    char *p = line + strlen(key), *end;
    for (int i = 0; i < n; i++) {
        v[i] = strtod(p, &end);
        if (end == p || !isfinite(v[i])) bad();
        p = end;
    }
    while (isspace((unsigned char)*p)) p++;
    if (*p) bad();
}
static int integer(double v, double low, double high)
{
    if (v < low || v > high || v != floor(v)) bad();
    return (int)v;
}
static void load(pr_pokemon *p, const double *v, int b)
{
    memset(p, 0, sizeof *p);
    p->species = integer(v[0], 1, 65535); p->form = integer(v[1], 0, 255); p->gender = integer(v[2], -1, 1);
    pr_pokemon id = {0}; id.species = p->species; const pr_species *sp = pr_pokemon_species(&id);
    if (!sp || p->form >= sp->nforms || v[3] < 0 || v[3] > UINT32_MAX || v[3] != floor(v[3])) bad();
    p->id = (uint32_t)v[3]; p->level = integer(v[4], 1, 255); p->nature = integer(v[5], 0, 24);
    p->hp = integer(v[6], 0, INT32_MAX); p->status = integer(v[7], 0, STATUS_EFFECT_FAINT);
    p->shiny = integer(v[8], 0, 1); p->base_ability = integer(v[9], 0, PR_ABILITY_COUNT - 1);
    p->ability = p->ability_override = integer(v[10], 0, PR_ABILITY_COUNT - 1);
    p->passive = integer(v[11], 0, PR_ABILITY_COUNT - 1); p->passive_flag = integer(v[12], 0, 1);
    p->custom_passive = integer(v[13], -1, PR_ABILITY_COUNT - 1); p->fusion_species = integer(v[14], 0, 65535);
    p->fusion_ability_index = -1; p->illusion.species = integer(v[15], 0, 65535); p->illusion.active = v[15] != 0;
    p->on_field = integer(v[16], 0, 1); p->switch_out_status = integer(v[17], 0, 1); p->is_player = b < 2;
    for (int i = 0; i < 6; i++) { p->stats[i] = integer(v[18+i], 1, INT32_MAX); p->summon_stats[i] = v[24+i]; }
    for (int i = 0; i < 7; i++) p->stat_stages[i] = integer(v[30+i], -6, 6);
    p->n_moves = integer(v[37], 0, 4);
    for (int i = 0; i < 4; i++) {
        p->moves[i] = integer(v[38+i], 0, PR_MOVE_COUNT-1); p->pp_used[i] = integer(v[42+i], 0, 255);
        p->pp_up[i] = integer(v[46+i], 0, 3); p->max_pp_override[i] = integer(v[50+i], -1, 32767);
    }
    p->n_summon_types = integer(v[54], 0, PR_MAX_SUMMON_TYPES);
    for (int i = 0; i < 4; i++) p->summon_types[i] = integer(v[55+i], POKEMON_TYPE_UNKNOWN, POKEMON_TYPE_STELLAR);
    p->added_type = integer(v[59], 0, 19); p->terastallized = integer(v[60], 0, 1); p->tera_type = integer(v[61], -1, 19);
    p->transformed = integer(v[62], 0, 1); p->transform_species = integer(v[63], 0, 65535);
    p->transform_form = integer(v[64], 0, 255); p->transform_gender = integer(v[65], -1, 1);
    if (p->transformed) {
        pr_pokemon identity={0};identity.species=p->transform_species;
        const pr_species *species=pr_pokemon_species(&identity);
        if(!species||p->transform_form>=species->nforms)bad();
        p->transform_n_moves = p->n_moves;
        memcpy(p->transform_moves, p->moves, sizeof p->moves);
        memcpy(p->transform_pp_used, p->pp_used, sizeof p->pp_used);
        memcpy(p->transform_pp_up, p->pp_up, sizeof p->pp_up);
        memcpy(p->transform_max_pp_override, p->max_pp_override, sizeof p->max_pp_override);
        p->n_moves = integer(v[66], 0, 4);
        for (int i = 0; i < 4; i++) {
            p->moves[i] = integer(v[67+i], 0, PR_MOVE_COUNT-1); p->pp_used[i] = integer(v[71+i], 0, 255);
            p->pp_up[i] = integer(v[75+i], 0, 3); p->max_pp_override[i] = integer(v[79+i], -1, 32767);
        }
    }
    for (int i = 0; i < 2; i++) if (integer(v[83+i], 0, 1)) {
        p->tags[p->n_tags++].type = i ? BATTLER_TAG_TYPE_SUBSTITUTE : BATTLER_TAG_TYPE_POWER_TRICK;
    }
    p->variant = integer(v[100], 0, 2);
    integer(v[85],1,65535);integer(v[86],0,255);integer(v[87],-1,1);
    int n_types=integer(v[94],1,PR_MAX_TYPES);
    for(int i=0;i<n_types;i++)integer(v[95+i],POKEMON_TYPE_UNKNOWN,POKEMON_TYPE_STELLAR);
    integer(v[102],0,1);integer(v[103],0,1);
}
static void capture(const pr_pokemon *p, double *v)
{
    memset(v, 0, WIDTH * sizeof *v);
    v[0]=p->species; v[1]=p->form; v[2]=p->gender; v[3]=p->id; v[4]=p->level; v[5]=p->nature;
    v[6]=p->hp; v[7]=p->status; v[8]=p->shiny; v[9]=p->base_ability; v[10]=p->ability;
    v[11]=p->passive; v[12]=p->passive_flag; v[13]=p->custom_passive; v[14]=p->fusion_species;
    v[15]=p->illusion.active ? p->illusion.species : 0; v[16]=p->on_field && !p->switch_out_status; v[17]=p->switch_out_status;
    for (int i=0;i<6;i++){v[18+i]=p->stats[i];v[24+i]=p->summon_stats[i];}
    for (int i=0;i<7;i++)v[30+i]=p->stat_stages[i];
    v[37]=PR_N_MOVES(p);
    for(int i=0;i<4;i++){v[38+i]=i<PR_N_MOVES(p)?PR_MOVES(p)[i]:0;v[42+i]=i<PR_N_MOVES(p)?PR_PP_USED(p)[i]:0;v[46+i]=i<PR_N_MOVES(p)?PR_PP_UP(p)[i]:0;v[50+i]=i<PR_N_MOVES(p)?PR_MAX_PP(p)[i]:-1;}
    v[54]=p->n_summon_types;for(int i=0;i<4;i++)v[55+i]=i<p->n_summon_types?p->summon_types[i]:0;
    v[59]=p->added_type;v[60]=p->terastallized;v[61]=p->tera_type;v[62]=p->transformed;
    if(p->transformed){v[63]=p->transform_species;v[64]=p->transform_form;v[65]=p->transform_gender;v[66]=p->n_moves;
        for(int i=0;i<4;i++){v[67+i]=i<p->n_moves?p->moves[i]:0;v[71+i]=i<p->n_moves?p->pp_used[i]:0;v[75+i]=i<p->n_moves?p->pp_up[i]:0;v[79+i]=i<p->n_moves?p->max_pp_override[i]:-1;}}
    v[83]=!!pr_get_battler_tag(p,BATTLER_TAG_TYPE_POWER_TRICK);v[84]=!!pr_get_battler_tag(p,BATTLER_TAG_TYPE_SUBSTITUTE);
    const pr_form *form=pr_pokemon_form(p);v[85]=form->species;v[86]=form->form_index;v[87]=pr_get_gender(p);
    for(int i=0;i<6;i++)v[88+i]=pr_get_stat(p,i,false);
    int8_t types[PR_MAX_TYPES];int n=pr_get_types(p,types,0);v[94]=n;for(int i=0;i<n;i++)v[95+i]=types[i];
    v[100]=p->variant;v[101]=pr_get_sprite_scale(p);v[102]=pr_is_mega(p);v[103]=pr_is_max(p);
}
static bool compare(const pr_pokemon *p, const double *want, int c, int b, char stage)
{
    double got[WIDTH];capture(p,got);bool ok=true;
    for(int i=0;i<WIDTH;i++)if(got[i]!=want[i]){if(c<100)fprintf(stderr,"case%d %c%d field%d C:%.17g JS:%.17g\n",c,stage,b,i,got[i],want[i]);ok=false;}
    return ok;
}
static void rng_read(const char *key, pr_rng *r)
{
    double v[4];read_vector(key,v,4);r->c=integer(v[0],0,INT32_MAX);
    if(v[1]<0||v[1]>=1||v[2]<0||v[2]>=1||v[3]<0||v[3]>=1)bad();
    r->s0=v[1];r->s1=v[2];r->s2=v[3];
}
static bool rng_equal(pr_rng a, pr_rng b){return a.c==b.c&&a.s0==b.s0&&a.s1==b.s1&&a.s2==b.s2;}
int main(int argc,char **argv)
{
    if(argc!=2||(input=fopen(argv[1],"r"))==NULL)return 2;
    pr_closures_init();double header[3];read_vector("TRANSFORM ",header,3);if(header[0]!=1)bad();int n=integer(header[1],48,100000);
    int seed=integer(header[2],0,INT32_MAX);
    int failures=0,transformed=0,imposter=0;
    for(int c=0;c<n;c++){
        static pr_battle bt;static pr_engine e;static pr_pokemon mons[4];memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);
        bt.engine=&e;e.n_levels=1;e.current=-1;bt.double_battle=true;bt.game_mode=GAME_MODES_CLASSIC;bt.ignoring_effect_source=-1;bt.wave_index=3;
        char wave[64];snprintf(wave,sizeof wave,"transform-wave-%d",seed);pr_seed_from_ascii(&bt.wave_seed,wave);bt.turn=1;
        double h[6];read_vector("C ",h,6);if(h[0]!=c)bad();int kind=integer(h[1],0,2),missing=integer(h[2],0,1);bool eligible=integer(h[3],0,1),ok=true;
        int target_slot=integer(h[5],0,3);
        rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
        for(int b=0;b<4;b++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=b)bad();load(&mons[b],v+1,b);bt.field[b]=&mons[b];bt.party[b<2?0:1][b%2]=&mons[b];bt.n_party[b<2?0:1]++;ok &=compare(&mons[b],v+1,c,b,'I');}
        mons[0].ability_suppressed=integer(h[4],0,1);
        if(pr_can_transform_into(&mons[0],&mons[target_slot])!=eligible){fprintf(stderr,"case%d eligibility differs\n",c);ok=false;}
        if(kind==0){
            const pr_fnlist *conditions=&pr_moves[MOVE_ID_TRANSFORM].conditions;
            if(!conditions->n)bad();
            bool condition=pr_closure_call(&bt,conditions->v[0],PR_AC_TransformAttr,0,&mons[0],&mons[target_slot],MOVE_ID_TRANSFORM)!=0;
            if(condition!=eligible)ok=false;
            if(condition){int ph=pr_phase_new(&e,PH_MOVE_EFFECT,0);e.pool[ph].move_id=MOVE_ID_TRANSFORM;
            int ai=-1;for(int j=0;j<pr_moves[MOVE_ID_TRANSFORM].nattr;j++){int a=pr_moves[MOVE_ID_TRANSFORM].attr0+j;if(pr_attrs[a].cls==PR_AC_TransformAttr)ai=a;}if(ai<0)bad();
            pr_apply_move_effect(&bt,&e,ph,ai,&mons[0],&mons[target_slot]);e.used[ph]=false;
            }
        }else if(kind==1){pr_ab_params params={.pokemon=&mons[0],.move=-1};pr_apply_ab_attrs(&bt,PR_AC_PostSummonTransformAbAttr,&params);}
        else if(kind==2){int ph=pr_phase_new(&e,PH_TRANSFORM,0);e.pool[ph].transform_target=target_slot;pr_phase_unshift(&bt,&e,ph);}
        int phase=-1;for(int j=0;j<PR_MAX_PHASES;j++)if(e.used[j]&&e.pool[j].type==PH_TRANSFORM){if(phase!=-1)ok=false;phase=j;}
        double chosen;read_vector("CHOSEN ",&chosen,1);integer(chosen,-1,3);if((phase<0?-1:e.pool[phase].transform_target)!=chosen){fprintf(stderr,"case%d target C%d JS%.0f\n",c,phase<0?-1:e.pool[phase].transform_target,chosen);ok=false;}
        pr_rng want;rng_read("RB ",&want);if(!rng_equal(want,bt.battle_rng))ok=false;
        if(phase>=0){if(missing)bt.field[2]=NULL;pr_phase_run(&bt,&e,phase);if(missing)bt.field[2]=&mons[2];transformed+=!missing;imposter+=kind==1;}
        for(int b=0;b<4;b++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=b)bad();ok &=compare(&mons[b],v+1,c,b,'O');}
        rng_read("RO ",&want);if(!rng_equal(want,bt.rng)){fprintf(stderr,"case%d global RNG differs\n",c);ok=false;}
        rng_read("BO ",&want);if(!rng_equal(want,bt.battle_rng)){fprintf(stderr,"case%d battle RNG differs\n",c);ok=false;}
        double sound;read_vector("SOUND ",&sound,1);integer(sound,0,1);int ns=0;for(int j=0;j<e.n_events;j++)if(e.events[j].type==PR_EV_TRANSFORM&&e.events[j].value)ns++;if(ns!=sound){fprintf(stderr,"case%d sound differs\n",c);ok=false;}
        for(int b=0;b<4;b++){pr_reset_summon_data(&mons[b]);double v[WIDTH+1];read_vector("Z ",v,WIDTH+1);if(v[0]!=b)bad();ok &=compare(&mons[b],v+1,c,b,'Z');}
        read_vector("E",NULL,0);if(bt.n_unported||bt.n_deferred||e.overflow||e.events_overflow){fprintf(stderr,"case%d unported%d deferred%d overflow%d events%d\n",c,bt.n_unported,bt.n_deferred,e.overflow,e.events_overflow);for(int k=0;k<bt.n_unported;k++)fprintf(stderr,"  %s\n",pr_unported_name(bt.unported[k]));ok=false;}
        if(!ok){if(failures<20)fprintf(stderr,"Transform case%d failed\n",c);failures++;}
    }
    double footer;read_vector("TRANSFORM_COMPLETE ",&footer,1);if(footer!=n||fgetc(input)!=EOF)bad();
    printf("Transform cases%d exact%d failures%d transformed%d imposter%d\n",n,n-failures,failures,transformed,imposter);
    return failures?1:0;
}
