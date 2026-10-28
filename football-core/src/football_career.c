#include "football_career.h"
#include "gc/data.h"
#include <stdio.h>
#include <string.h>
static void prepare(FootballCareer *c,unsigned member)
{
    char seed[20];snprintf(seed,sizeof seed,"%u-%u",c->seed,member);
    gc_football_new(c->scratch,c->profile,seed,c->seed+member);
    c->scratch->party[0]=c->roster.mon[member];c->scratch->n_party=1;
    c->scratch->party_order[0]=0;c->scratch->bt.party[0][0]=&c->scratch->party[0].b;c->scratch->bt.n_party[0]=1;c->scratch->screen.kind=GC_SCREEN_NONE;
}
bool football_career_new(FootballCareer *c,gc_run *scratch,gc_system *profile,unsigned seed)
{
    if(!c || !scratch)return false;
    memset(c,0,sizeof *c);c->scratch=scratch;c->profile=profile;c->seed=seed?seed:1;c->round=1;c->mega=-1;c->stone_target=-1;
    gc_football_new(scratch,profile,"football-live",c->seed);
    return football_roster_init(&c->roster);
}
bool football_career_start(FootballCareer *c)
{
    if(!c || (c->phase!=FB_CAREER_READY && c->phase!=FB_CAREER_LINEUP) || c->roster.count!=22)return false;
    FootballPlayer mega_player;
    bool mega=c->mega>=0;
    if(mega){
        if(c->mega>=22 || !c->roster.mega_item[c->mega])return false;
        prepare(c,(unsigned)c->mega);
        const pr_form *form=gc_football_mega_form(c->scratch,0,c->roster.mega_item[c->mega]);
        if(!form || !football_stats_generate(&mega_player,c->roster.mon[c->mega].b.id,form,&c->roster.maxima))return false;
        unsigned spd=mega_player.attributes.spd+c->roster.speed[c->mega],str=mega_player.attributes.str+c->roster.strength[c->mega];
        mega_player.attributes.spd=(uint8_t)(spd>99?99:spd);mega_player.attributes.str=(uint8_t)(str>99?99:str);
    }
    if(!football_live_begin(&c->live,&c->roster,c->seed+c->round*104729u,c->round))return false;
    if(mega){
        c->live.units[FB_HOME][c->mega]=mega_player.attributes;
        /* begin() already copied the first formation. Update the active actor too,
         * including a selected defensive player and a rearranged lineup. */
        for(unsigned i=0;i<22;++i)if(c->live.actors[i].home && c->live.actors[i].roster==c->mega)
            c->live.actors[i].attributes=mega_player.attributes;
    }
    c->candy_plays=c->candy_team=c->seen_snaps=c->candy_plays_milli=c->candy_team_milli=0;
    c->phase=FB_CAREER_MATCH;return true;
}
/* The candy bar of the species a member started as is as long as its friendship cap in the classic game; 0 for none. */
static int candy_bar(const FootballCareer *c,unsigned member,uint16_t *root)
{
    uint16_t species=member<22?gc_species_root_for_starter(c->roster.mon[member].b.species):0;
    if(root)*root=species;
    if(!species)return 0;
    int cost=gc_starter_cost(species);
    return getStarterValueFriendshipCap(cost<1?1:cost);
}
unsigned football_candy_milli(const FootballCareer *c,unsigned member,unsigned points)
{
    int bar=c?candy_bar(c,member,NULL):0;
    if(bar<=0)return 0;
    return (unsigned)(((unsigned long)points*FB_CANDY_FRIENDSHIP_PER_POINT*1000u+(unsigned)bar/2)/(unsigned)bar);
}
unsigned football_career_team_candy_milli(const FootballCareer *c,bool won)
{
    unsigned total=0;
    for(unsigned i=0;i<22;++i)total+=football_candy_milli(c,i,FB_CANDY_PLAYED+(won?FB_CANDY_WIN:0));
    return total;
}
/* Starter candy is earned by the species the member started as. A career with no profile earns nothing. */
static unsigned give_candy(FootballCareer *c,unsigned member,unsigned points)
{
    if(!c->profile || member>=22 || !points)return 0;
    c->candy_pending[member]+=points;c->candy_plays+=points;
    return football_candy_milli(c,member,points);
}
unsigned football_career_flush_candy(FootballCareer *c)
{
    unsigned given=0;
    if(!c || !c->profile)return 0;
    for(unsigned i=0;i<22;++i){
        unsigned points=c->candy_pending[i];c->candy_pending[i]=0;
        uint16_t species=0;
        if(points && candy_bar(c,i,&species) && species)given+=(unsigned)gc_system_add_starter_friendship(c->profile,species,(int)(points*FB_CANDY_FRIENDSHIP_PER_POINT));
    }
    return given;
}
/* The dead ball has just made its credits: the player behind each of the human team's plays is paid at once. */
static void pay_credits(FootballCareer *c)
{
    const FootballLive *m=&c->live;
    for(unsigned i=0;i<m->credit_count && i<4;++i)if(m->credits[i].side==FB_HOME)c->candy_plays_milli+=give_candy(c,m->credits[i].member,football_credit_candy(m->credits[i].kind));
}
/* Everyone on the team earns candy for the match, once, when it is over. */
static void pay_match(FootballCareer *c,bool won)
{
    unsigned before=c->candy_plays;
    for(unsigned i=0;i<22;++i)c->candy_team_milli+=give_candy(c,i,FB_CANDY_PLAYED+(won?FB_CANDY_WIN:0));
    c->candy_team=c->candy_plays-before;
}
/* What an evolution item costs: the common ones climb with the round, the rare ones (id above 50, the classic game's rare tier) cost more. */
static unsigned evo_price(unsigned round,unsigned item){return item>50?800+75*round:400+50*round;}
bool football_career_evo_valid(FootballCareer *c,const FootballEvoOffer *o)
{
    if(!c || !o || o->member>=22 || o->member>=c->roster.count || !o->item)return false;
    if(c->roster.mega_item[o->member] || c->stone_target==(int)o->member)return false; /* evolving would lose the stone */
    prepare(c,o->member);
    gc_football_item_evolution found[GC_FOOTBALL_MAX_ITEM_EVOLUTIONS];int n=gc_football_item_evolutions(c->scratch,0,found);
    for(int i=0;i<n;++i)if(found[i].item==o->item)return found[i].species==o->species;
    return false;
}
/* The evolution items the roster can use now, for the shop. Each member is offered the items that evolve it; when more are possible than
 * the shop holds, the first item of every member comes before anyone's second, and the member the list starts from moves with the round,
 * so nobody is left out for good. A member who holds a mega stone, or is about to be offered one, is left alone: evolving would lose it. */
static void roll_evolutions(FootballCareer *c)
{
    gc_football_item_evolution found[22][GC_FOOTBALL_MAX_ITEM_EVOLUTIONS];int n[22];
    memset(found,0,sizeof found);memset(c->evo,0,sizeof c->evo);c->evo_count=0;
    for(unsigned i=0;i<22;++i){
        n[i]=0;
        if(c->roster.mega_item[i] || c->stone_target==(int)i)continue;
        prepare(c,i);n[i]=gc_football_item_evolutions(c->scratch,0,found[i]);
    }
    const unsigned start=(c->seed+c->round*5)%22;
    for(unsigned pass=0;pass<GC_FOOTBALL_MAX_ITEM_EVOLUTIONS;++pass)for(unsigned k=0;k<22;++k){
        unsigned i=(start+k)%22;
        if((int)pass>=n[i] || c->evo_count>=FB_SHOP_EVO_MAX)continue;
        c->evo[c->evo_count++]=(FootballEvoOffer){.member=(uint8_t)i,.item=found[i][pass].item,.state=FB_EVO_OFFER,.species=found[i][pass].species,.price=(uint16_t)evo_price(c->round,found[i][pass].item)};
    }
    /* Listed by roster member, so a member's items sit together. */
    for(unsigned i=1;i<c->evo_count;++i){FootballEvoOffer o=c->evo[i];unsigned j=i;
        while(j>0 && (c->evo[j-1].member>o.member || (c->evo[j-1].member==o.member && c->evo[j-1].item>o.item))){c->evo[j]=c->evo[j-1];--j;}
        c->evo[j]=o;}
}
static void shop(FootballCareer *c)
{
    pay_match(c,true);
    c->phase=FB_CAREER_SHOP;memset(c->sold,0,sizeof c->sold);c->prices[0]=125+25*c->round;c->prices[1]=c->prices[0];c->prices[2]=1000;c->stone_target=-1;c->stone_item=0;c->message[0]=0;
    bool owns_mega=false;
    for(unsigned i=0;i<22;++i)owns_mega|=c->roster.mega_item[i]!=0;
    /* Keep the first usable stone available until it can be afforded. Otherwise
     * an unaffordable first offer (500 winnings, 1000 price) can vanish for four rounds. */
    if(!owns_mega || (c->seed+c->round*7)%4==0)for(unsigned k=0;k<22;++k){
        unsigned i=(c->seed+c->round*7+k)%22;
        if(c->roster.mega_item[i])continue;
        prepare(c,i);int item=gc_football_mega_item(c->scratch,0);
        if(item>0){c->stone_target=(int)i;c->stone_item=item;break;}
    }
    roll_evolutions(c);
}
/* An evolution of the member's species that asks for this move (or for a move of its type, when it is the only one known). */
static bool evolution_needs(const pr_pokemon *p,int slot)
{
    const unsigned move=PR_MOVES(p)[slot];
    if(move>=PR_MOVE_COUNT)return false;
    for(int i=0;i<gc_species_evo_count;++i){
        if(gc_species_evos[i].id!=p->species)continue;
        for(unsigned k=0;k<gc_species_evos[i].n;++k){
            const gc_evo *e=&gc_evo_data[gc_species_evos[i].start+k];
            if(e->item)continue;
            for(unsigned c=0;c<e->n_cond;++c){
                const gc_evo_cond *cond=&gc_evo_conds[e->cond_start+c];
                if(cond->key==GC_EVO_MOVE && cond->v[0]==(int)move)return true;
                if(cond->key==GC_EVO_MOVE_TYPE && pr_moves[move].type==cond->v[0]){
                    unsigned same=0;
                    for(int m=0;m<PR_N_MOVES(p) && m<4;++m){const unsigned other=PR_MOVES(p)[m];if(other<PR_MOVE_COUNT && pr_moves[other].type==cond->v[0])++same;}
                    if(same<=1)return true;
                }
            }
        }
    }
    return false;
}
int football_career_forget_slot(const pr_pokemon *p)
{
    int slot=-1,weakest=0;
    if(!p)return -1;
    /* The weakest move goes, but not one its evolution needs (Tangela keeps Ancient Power for Tangrowth), unless all of them are. */
    for(int pass=0;pass<2 && slot<0;++pass)
        for(int i=0;i<PR_N_MOVES(p) && i<4;++i){
            if(!pass && evolution_needs(p,i))continue;
            unsigned id=PR_MOVES(p)[i];int power=id<PR_MOVE_COUNT && pr_moves[id].power>0?pr_moves[id].power:0;
            if(slot<0 || power<weakest){slot=i;weakest=power;}
        }
    return slot;
}
static bool award(FootballCareer *c)
{
    prepare(c,c->exp_member);return gc_football_award_levels(c->scratch,c->round,GC_FOOTBALL_LEVELS_PER_WIN,NULL);
}
void football_career_tick(FootballCareer *c,unsigned ms,FootballLiveInput input)
{
    if(!c)return;
    if(c->phase==FB_CAREER_MATCH){
        football_live_tick(&c->live,ms,input);
        if(c->live.snaps!=c->seen_snaps){c->seen_snaps=c->live.snaps;pay_credits(c);}
        if(c->live.phase==FB_LIVE_END)c->phase=FB_CAREER_RESULT;
        return;
    }
    if(c->phase!=FB_CAREER_EXP)return;
    gc_event event;while(gc_run_next_event(c->scratch,&event)){}
    gc_football_progress progress=gc_football_tick(c->scratch);
    while(gc_run_next_event(c->scratch,&event)){}
    if(progress==GC_FB_ERROR){c->phase=FB_CAREER_ERROR;snprintf(c->message,sizeof c->message,"Team progression failed; checkpoint preserved.");}
    else if(progress==GC_FB_WAIT){
        if(c->scratch->screen.kind==GC_SCREEN_EVOLVE){c->evolution_count=(unsigned)gc_football_evolution_choices(c->scratch,c->evolutions);c->phase=FB_CAREER_EVOLVE;}
        else if(c->scratch->screen.kind==GC_SCREEN_LEARN_MOVE){
            /* Moves play no part in a match, so a level-up move is learned without asking. */
            gc_choice choice={.kind=GC_CH_FORGET,.a=football_career_forget_slot(&c->scratch->party[c->scratch->screen.party].b)};
            if(!gc_football_choose(c->scratch,&choice,0,NULL))c->phase=FB_CAREER_ERROR;
        }
        else {gc_choice choice={.kind=GC_CH_CONFIRM};if(!gc_football_choose(c->scratch,&choice,0,NULL))c->phase=FB_CAREER_ERROR;}
    }else if(progress==GC_FB_DONE){
        c->roster.mon[c->exp_member]=c->scratch->party[0];football_roster_refresh(&c->roster,c->exp_member);
        if(++c->exp_member<22){if(!award(c))c->phase=FB_CAREER_ERROR;}else shop(c);
    }
}
bool football_career_continue(FootballCareer *c)
{
    if(!c)return false;
    if(c->phase==FB_CAREER_RESULT){
        if(c->live.rules.winner!=FB_HOME){pay_match(c,false);c->phase=FB_CAREER_END;return true;}
        ++c->wins;c->wallet+=400+100*c->round;c->exp_member=0;c->phase=FB_CAREER_EXP;
        if(!award(c)){c->phase=FB_CAREER_ERROR;return false;}return true;
    }
    if(c->phase==FB_CAREER_SHOP){memset(c->evo,0,sizeof c->evo);c->evo_count=0;c->message[0]=0;if(c->round==7)c->phase=FB_CAREER_END;else {++c->round;c->mega=-1;c->phase=FB_CAREER_READY;}return true;}
    return false;
}
bool football_career_evolve(FootballCareer *c,int pick)
{
    if(!c || c->phase!=FB_CAREER_EVOLVE || pick>=(int)c->evolution_count)return false;
    gc_choice choice={.kind=pick<0?GC_CH_CANCEL:GC_CH_EVOLVE};
    if(!gc_football_choose(c->scratch,&choice,pick<0?0:(unsigned)pick,NULL))return false;
    c->phase=FB_CAREER_EXP;return true;
}
bool football_career_evo_preview(FootballCareer *c,unsigned index,FootballPlayer *out)
{
    if(!c || !out || index>=c->evo_count || !football_career_evo_valid(c,&c->evo[index]))return false;
    const unsigned member=c->evo[index].member;
    prepare(c,member);
    if(!gc_football_item_evolve(c->scratch,0,c->evo[index].item,football_career_forget_slot,NULL))return false;
    /* Run the roster's own refresh on the evolved member, then put the member back exactly. */
    const gc_mon old=c->roster.mon[member];const FootballPlayer old_player=c->roster.player[member];
    c->roster.mon[member]=c->scratch->party[0];
    bool ok=football_roster_refresh(&c->roster,member);
    if(ok)*out=c->roster.player[member];
    c->roster.mon[member]=old;c->roster.player[member]=old_player;
    return ok;
}
static bool buy_evolution(FootballCareer *c,unsigned index)
{
    FootballEvoOffer *o=&c->evo[index];
    if(o->state!=FB_EVO_OFFER || c->wallet<o->price || !football_career_evo_valid(c,o))return false;
    const unsigned member=o->member;
    prepare(c,member);
    const gc_mon old=c->roster.mon[member];const FootballPlayer old_player=c->roster.player[member];
    const uint16_t before=old.b.species;
    if(!gc_football_item_evolve(c->scratch,0,o->item,football_career_forget_slot,NULL) || c->scratch->party[0].b.species!=o->species)return false;
    c->roster.mon[member]=c->scratch->party[0];
    if(!football_roster_refresh(&c->roster,member)){c->roster.mon[member]=old;c->roster.player[member]=old_player;return false;}
    c->wallet-=o->price;o->state=FB_EVO_SOLD;
    /* The member has evolved: its other items no longer fit. */
    for(unsigned i=0;i<c->evo_count;++i)if(i!=index && c->evo[i].member==member && c->evo[i].state==FB_EVO_OFFER)c->evo[i].state=FB_EVO_VOID;
    const char *from=NULL,*to=NULL;
    for(unsigned i=0;i<PR_SPECIES_COUNT;++i){if(pr_species_table[i].id==before)from=pr_species_table[i].name;if(pr_species_table[i].id==o->species)to=pr_species_table[i].name;}
    snprintf(c->message,sizeof c->message,"%s evolved into %s!",from?from:"Pokemon",to?to:"a new form");
    return true;
}
bool football_career_buy(FootballCareer *c,unsigned offer,unsigned member)
{
    if(!c || c->phase!=FB_CAREER_SHOP)return false;
    if(offer>=FB_SHOP_FIXED)return offer-FB_SHOP_FIXED<c->evo_count && buy_evolution(c,offer-FB_SHOP_FIXED);
    if(member>=22 || c->sold[offer] || c->wallet<c->prices[offer])return false;
    if(offer==2){
        if(c->stone_target<0 || c->stone_target>=c->roster.count || c->roster.mega_item[c->stone_target])return false;
        prepare(c,(unsigned)c->stone_target);
        if(!gc_football_mega_form(c->scratch,0,c->stone_item))return false;
        c->roster.mega_item[c->stone_target]=(int16_t)c->stone_item;
    }
    else {uint8_t *boost=offer==0?&c->roster.speed[member]:&c->roster.strength[member];if(*boost>=20)return false;*boost=(uint8_t)(*boost+2>20?20:*boost+2);football_roster_refresh(&c->roster,member);}
    c->wallet-=c->prices[offer];c->sold[offer]=true;return true;
}
