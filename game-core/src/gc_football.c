#include "internal.h"
#include "gc/football.h"

static bool fail(const char **why, const char *text)
{
    if (why) *why=text;
    return false;
}
void gc_football_new(gc_run *r, gc_system *profile, const char *seed, uint32_t entropy)
{
    gc_run_new(r,GAME_MODES_CLASSIC,seed,entropy);
    r->sys=profile;
    r->money=0;
    r->bt.engine=&r->eng;
    r->bt.game_mode=GAME_MODES_CLASSIC;
    r->bt.challenge_flags_known=true;
}
bool gc_football_create_member(gc_run *r,const gc_starter *s,const char **why)
{
    if(!r || !gc_starter_is_available(NULL,s,NULL,why))return false;
    const pr_species *sp=gc_species(s->species);
    if(!sp || sp->prevolution>0 || s->form)return fail(why,"base forms only");
    gc_mon_new_player(r,&r->party[0],s,5);
    r->n_party=1;r->party_order[0]=0;r->bt.party[0][0]=&r->party[0].b;r->bt.n_party[0]=1;
    r->screen.kind=GC_SCREEN_NONE;return true;
}
bool gc_football_validate_draft(const gc_system *profile, const gc_starter *team, int count, const char **why)
{
    if (!team || count<1 || count>6)
        return fail(why,"invalid football draft");
    double cost=0;
    for (int i=0;i<count;++i) {
        double value;
        const pr_species *sp=gc_species(team[i].species);
        if (!sp || sp->prevolution>0 || team[i].form!=0)
            return fail(why,"draft base-form Pokemon only");
        if (!gc_starter_is_available(profile,&team[i],&value,why)) return false;
        for (int j=0;j<i;++j) if (team[i].species==team[j].species)
            return fail(why,"duplicate species");
        cost+=value;
    }
    if (cost>gc_starter_budget(GAME_MODES_CLASSIC)+1e-9)
        return fail(why,"starterSelectUiHandler:cost");
    return true;
}
bool gc_football_draft(gc_run *r, const gc_starter *team, int count, const char **why)
{
    if (!r || r->screen.kind!=GC_SCREEN_STARTERS) return fail(why,"invalid football draft");
    if (!gc_football_validate_draft(r->sys,team,count,why)) return false;
    for (int i=0;i<count;++i) {
        gc_mon_new_player(r,&r->party[i],&team[i],5);
        r->party_order[i]=(int8_t)i;
        r->bt.party[0][i]=&r->party[i].b;
    }
    r->n_party=count; r->bt.n_party[0]=count;
    r->screen.kind=GC_SCREEN_NONE;
    return true;
}
bool gc_football_award_exp(gc_run *r, unsigned round, const char **why)
{
    if (!r || !r->n_party || round<1 || round>7 || r->screen.kind!=GC_SCREEN_NONE)
        return fail(why,"invalid football EXP phase");
    gc_rqueue *q=&gc_int(r)->rq;
    if (q->waiting || q->current.type!=RP_NONE) return fail(why,"pending phase");
    for (int i=0;i<q->n_levels;++i) if (q->n[i]) return fail(why,"pending phase");
    /* Increasing classic caps/EXP without loading a classic battle. */
    r->wave=(int)round*20;
    r->bt.wave_index=r->wave;
    for (int i=0;i<r->n_party;++i) {
        gc_rphase p=gc_rp(RP_EXP,i,0,0);
        p.d=(double)GC_FOOTBALL_EXP_FACTOR*round*round;
        gc_rq_push(r,p);
    }
    return true;
}
static bool allowed_phase(int type)
{
    return type==RP_EXP || type==RP_LEVEL_UP || type==RP_LEARN_MOVE ||
           type==RP_EVOLUTION || type==RP_REWARD_FORM;
}
gc_football_progress gc_football_tick(gc_run *r)
{
    if (!r) return GC_FB_ERROR;
    gc_rqueue *q=&gc_int(r)->rq;
    if (q->overflow || r->ev_overflow) return GC_FB_ERROR;
    if (q->waiting) return GC_FB_WAIT;
    for (unsigned step=0;step<8;++step) {
        /* A level-up phase can emit several move/evolution events. */
        if (r->ev_count>GC_MAX_EVENTS-16) return GC_FB_BUSY;
        gc_rphase p;
        if (!gc_rq_next(q,&p)) { r->screen.kind=GC_SCREEN_NONE; return GC_FB_DONE; }
        if (!allowed_phase(p.type)) return GC_FB_ERROR;
        q->current=p;
        if (!gc_phase_start(r,&q->current)) { q->waiting=true; return GC_FB_WAIT; }
        q->current.type=RP_NONE;
        if (q->overflow || r->ev_overflow) return GC_FB_ERROR;
    }
    return GC_FB_BUSY;
}
int gc_football_evolution_choices(gc_run *r, uint16_t out[GC_FOOTBALL_MAX_EVOLUTIONS])
{
    if (!r || !out || r->screen.kind!=GC_SCREEN_EVOLVE) return 0;
    int choices[GC_FOOTBALL_MAX_EVOLUTIONS];
    int n=gc_mon_football_evolutions(r,&r->party[r->screen.party],choices,
                                   GC_FOOTBALL_MAX_EVOLUTIONS);
    for (int i=0;i<n;++i) out[i]=gc_evo_data[choices[i]].species;
    return n;
}
bool gc_football_choose(gc_run *r, const gc_choice *c, unsigned pick, const char **why)
{
    if (!r || !c) return fail(why,"missing choice");
    gc_rqueue *q=&gc_int(r)->rq;
    if (!q->waiting || !allowed_phase(q->current.type)) return fail(why,"no pending choice");
    if (q->current.type==RP_EVOLUTION && c->kind==GC_CH_EVOLVE) {
        int choices[GC_FOOTBALL_MAX_EVOLUTIONS];
        int n=gc_mon_football_evolutions(r,&r->party[q->current.a],choices,
                                       GC_FOOTBALL_MAX_EVOLUTIONS);
        if (n<0 || pick>=(unsigned)n) return fail(why,"invalid evolution choice");
        q->current.b=choices[pick];
    }
    bool ok=false;
    const char *dummy=NULL;
    if (!why) why=&dummy;
    bool done=gc_phase_choose(r,&q->current,c,why,&ok);
    if (!ok) return false;
    if (done) {
        q->waiting=false; q->current.type=RP_NONE;
        memset(&r->screen,0,sizeof r->screen);
    }
    return true;
}
const pr_form *gc_football_mega_form(const gc_run *r,unsigned party,int item)
{
    if (!r || party>=(unsigned)r->n_party || r->n_party>6 || item<0) return NULL;
    const gc_mon *m=&r->party[party];
    int key=pr_reward_form_key(m->b.species,m->b.form);
    for (int i=0;i<pr_item_fc_species_count;++i) {
        const pr_species_slice *s=&pr_item_fc_species[i];
        if (s->species!=m->b.species) continue;
        bool applicable=false;
        for (unsigned k=0;k<s->n;++k) {
            const pr_item_fc *f=&pr_item_fcs[s->start+k];
            if (f->item==item && (f->flags&5)==5 && !f->n_cond && f->pre_form_key==key) applicable=true;
        }
        if (!applicable) return NULL;
        /* The generated inactive transition identifies the held item's mega
         * target by its pre-form key. Do not assume form 1 or mega-X. */
        for (unsigned k=0;k<s->n;++k) {
            const pr_item_fc *f=&pr_item_fcs[s->start+k];
            if (f->item!=item || (f->flags&4)) continue;
            for (unsigned j=0;j<PR_FORM_COUNT;++j) {
                const pr_form *form=&pr_forms[j];
                if (form->species==m->b.species && !strncmp(form->form_key,"mega",4) &&
                    pr_reward_form_key(form->species,form->form_index)==f->pre_form_key) return form;
            }
        }
    }
    return NULL;
}
const pr_form *gc_football_owned_mega(const gc_run *r,unsigned party)
{
    if (!r || party>=(unsigned)r->n_party || r->n_party>6) return NULL;
    for (int i=0;i<r->n_items;++i) {
        const gc_item *item=&r->items[i];
        if (item->pokemon_id==r->party[party].b.id && item->stack>0 && !item->detached && item->key &&
            !strcmp(item->key,"FORM_CHANGE_ITEM")) {
            const pr_form *form=gc_football_mega_form(r,party,item->arg);
            if (form) return form;
        }
    }
    return NULL;
}
int gc_football_mega_item(const gc_run *r,unsigned party)
{
    if (!r || r->n_party<1 || r->n_party>6 || party>=(unsigned)r->n_party || gc_football_owned_mega(r,party)) return -1;
    for (int i=0;i<pr_item_fc_species_count;++i) {
        const pr_species_slice *s=&pr_item_fc_species[i];
        if (s->species!=r->party[party].b.species) continue;
        for (unsigned k=0;k<s->n;++k) {
            const pr_item_fc *f=&pr_item_fcs[s->start+k];
            if (gc_football_mega_form(r,party,f->item)) return f->item;
        }
    }
    return -1;
}
bool gc_football_give_mega(gc_run *r,unsigned party,int item)
{
    if (!gc_football_mega_form(r,party,item) || r->n_items>=GC_MAX_ITEMS) return false;
    uint32_t id=r->party[party].b.id;
    for (int i=0;i<r->n_items;++i)
        if (r->items[i].pokemon_id==id && !strcmp(r->items[i].key,"FORM_CHANGE_ITEM")) return false;
    gc_item_add(r,"FORM_CHANGE_ITEM",(int16_t)item,id,1);
    gc_items_to_battle(r);
    return true;
}
