#include "football_career.h"
#include "gc/data.h"
#include <assert.h>
#include "football_profile_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* The shop's evolution items: the game-core call that evolves a member with an item, and the career's offers, purchase and save. */
static FootballCareer career,loaded,again;
static gc_run scratch,other;
static gc_system profile;
static unsigned char bytes[FB_CAREER_SAVE_MAX];
static uint32_t crc(const unsigned char *p,size_t n){uint32_t v=~0u;while(n--){v^=*p++;for(unsigned i=0;i<8;++i)v=(v>>1)^(0xedb88320u & (0u-(v&1)));}return ~v;}
static void put32(unsigned offset,uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=(unsigned char)(value>>(8*i));}
static const pr_species *species_of(unsigned id){for(unsigned i=0;i<PR_SPECIES_COUNT;++i)if(pr_species_table[i].id==id)return &pr_species_table[i];return NULL;}
static bool member_of(gc_run *run,unsigned species)
{
    gc_starter s;
    if(!football_roster_starter(&profile,species,&s))return false;
    gc_football_new(run,&profile,"shop-evo",species); /* a fresh isolated run, as the career prepares one for each member */
    return gc_football_create_member(run,&s,NULL);
}
/* A member of this species at any stage: made as the base form it started as, then standing where levels would have taken it. */
static bool make_member(gc_run *run,unsigned species)
{
    unsigned root=species;
    for(const pr_species *sp=species_of(root);sp && sp->prevolution>0;sp=species_of(root))root=(unsigned)sp->prevolution; /* up to the form a football draft starts from */
    if(!root || !member_of(run,root))return false;
    if(root!=species){pr_pokemon *p=&run->party[0].b;p->species=(int)species;p->form=0;pr_recalculate_stats(&run->bt,p);}
    return true;
}
static bool has_item_evolution(unsigned species)
{
    for(int i=0;i<gc_species_evo_count;++i)if(gc_species_evos[i].id==species)
        for(unsigned k=0;k<gc_species_evos[i].n;++k)if(gc_evo_data[gc_species_evos[i].start+k].item)return true;
    return false;
}
static unsigned forgot;
static int forget_first(const pr_pokemon *p){(void)p;++forgot;return 0;}
/* The first cap species (any stage) that an item evolves in the table, from table index "from" on, that a member can be made of now. */
static unsigned species_with_items(unsigned from,unsigned *list,unsigned cap)
{
    unsigned n=0;
    for(unsigned i=from;i<PR_SPECIES_COUNT && n<cap;++i){
        unsigned id=pr_species_table[i].id;gc_football_item_evolution found[GC_FOOTBALL_MAX_ITEM_EVOLUTIONS];
        if(!has_item_evolution(id) || !make_member(&scratch,id))continue;
        if(gc_football_item_evolutions(&scratch,0,found)>0)list[n++]=id;
    }
    return n;
}
/* A career at its shop after a won round, with the roster set to these species (22 of them, repeating the list). */
static void shop_with_mega(FootballCareer *c,unsigned seed,unsigned round,const unsigned *roster,unsigned roster_count,int mega_member)
{
    memset(c,0,sizeof *c);
    assert(football_career_new(c,&scratch,&profile,seed));
    for(unsigned i=0;i<22;++i){
        assert(make_member(&scratch,roster[i%roster_count]));
        c->roster.mon[i]=scratch.party[0];c->roster.lineup[i]=(uint8_t)i;
    }
    c->roster.count=22;
    for(unsigned i=0;i<22;++i)assert(football_roster_refresh(&c->roster,i));
    football_roster_autofill(&c->roster);
    if(mega_member>=0)c->roster.mega_item[mega_member]=1; /* holds a stone: evolving it would lose it */
    c->round=round;c->wins=round-1;c->phase=FB_CAREER_RESULT;c->live.rules.winner=FB_HOME;
    assert(football_career_continue(c));
    unsigned guard=0;
    while(c->phase!=FB_CAREER_SHOP && ++guard<10000){
        if(c->phase==FB_CAREER_EVOLVE)assert(football_career_evolve(c,0));
        else {assert(c->phase==FB_CAREER_EXP);football_career_tick(c,16,(FootballLiveInput){.receiver=-1});}
    }
    assert(c->phase==FB_CAREER_SHOP && c->wins==round);
}
static void shop_with(FootballCareer *c,unsigned seed,unsigned round,const unsigned *roster,unsigned roster_count){shop_with_mega(c,seed,round,roster,roster_count,-1);}
int main(void)
{
    football_test_unlock_all(&profile);

    /* ---- game-core: which items evolve a member, and evolving with one ---- */
    unsigned items_species[64];unsigned item_species_count=species_with_items(0,items_species,64);
    assert(item_species_count>=40); /* plenty of species evolve with an item */
    gc_football_item_evolution found[GC_FOOTBALL_MAX_ITEM_EVOLUTIONS];
    assert(make_member(&scratch,25)); /* Pikachu, grown from Pichu: a stone makes it Raichu */
    int count=gc_football_item_evolutions(&scratch,0,found);assert(count>=1 && count<=GC_FOOTBALL_MAX_ITEM_EVOLUTIONS);
    bool raichu=false;for(int i=0;i<count;++i){raichu|=found[i].species==26;for(int j=i+1;j<count;++j)assert(found[i].item!=found[j].item);}
    assert(raichu);
    assert(make_member(&scratch,4) && gc_football_item_evolutions(&scratch,0,found)==0); /* Charmander only evolves with levels */
    assert(gc_football_item_evolutions(&scratch,1,found)==0 && gc_football_item_evolutions(NULL,0,found)==0 && gc_football_item_evolutions(&scratch,0,NULL)==0);

    /* The item that does not fit changes nothing and leaves the run usable. */
    assert(make_member(&scratch,25));gc_football_item_evolutions(&scratch,0,found);
    const char *why=NULL;int wrong=1;
    for(;;++wrong){bool listed=false;for(int i=0;i<count;++i)listed|=found[i].item==wrong;if(!listed)break;}
    unsigned before=scratch.party[0].b.species,level=(unsigned)scratch.party[0].b.level;
    assert(!gc_football_item_evolve(&scratch,0,wrong,NULL,&why) && why && scratch.party[0].b.species==before);
    assert(!gc_football_item_evolve(&scratch,0,0,NULL,&why) && !gc_football_item_evolve(&scratch,0,-3,NULL,&why) && !gc_football_item_evolve(&scratch,1,7,NULL,&why) && !gc_football_item_evolve(NULL,0,7,NULL,&why));
    assert(scratch.party[0].b.species==before && scratch.screen.kind==GC_SCREEN_NONE);
    unsigned stone=0;for(int i=0;i<count;++i)if(found[i].species==26)stone=found[i].item;
    assert(stone && gc_football_item_evolve(&scratch,0,(int)stone,forget_first,&why));
    assert(scratch.party[0].b.species==26 && (unsigned)scratch.party[0].b.level==level && scratch.screen.kind==GC_SCREEN_NONE);
    assert(!gc_football_item_evolve(&scratch,0,(int)stone,forget_first,&why) && scratch.party[0].b.species==26); /* Raichu is not evolved by it again */

    /* Nothing is evolved while the run has other work queued, and a member whose evolutions are paused is left alone. */
    {
        assert(make_member(&scratch,25) && gc_football_award_exp(&scratch,1,NULL));
        assert(!gc_football_item_evolve(&scratch,0,(int)stone,forget_first,&why) && scratch.party[0].b.species==25);
        gc_event event;unsigned guard=0;gc_football_progress progress;
        {   /* ... or while it waits on a prompt (a full moveset makes the level-up moves ask): the prompt is left open */
            pr_pokemon *p=&scratch.party[0].b;static const uint16_t full[4]={MOVE_ID_TACKLE,MOVE_ID_GROWL,MOVE_ID_TAIL_WHIP,MOVE_ID_LEER};
            p->n_moves=4;for(unsigned k=0;k<4;++k){p->moves[k]=full[k];p->pp_used[k]=0;}
            do{while(gc_run_next_event(&scratch,&event)){}progress=gc_football_tick(&scratch);}while(progress==GC_FB_BUSY && ++guard<1000);
            assert(progress==GC_FB_WAIT && scratch.screen.kind!=GC_SCREEN_NONE);
            int screen=scratch.screen.kind;
            assert(!gc_football_item_evolve(&scratch,0,(int)stone,forget_first,&why) && scratch.screen.kind==screen && scratch.party[0].b.species==25);
        }
        do{while(gc_run_next_event(&scratch,&event)){}progress=gc_football_tick(&scratch);
            if(progress==GC_FB_WAIT){gc_choice answer={.kind=scratch.screen.kind==GC_SCREEN_LEARN_MOVE?GC_CH_FORGET:GC_CH_CONFIRM,.a=-1};assert(gc_football_choose(&scratch,&answer,0,NULL));}}
        while(progress!=GC_FB_DONE && progress!=GC_FB_ERROR && ++guard<1000);
        assert(progress==GC_FB_DONE && scratch.party[0].b.species==25);
        assert(gc_football_item_evolve(&scratch,0,(int)stone,forget_first,&why) && scratch.party[0].b.species==26); /* once the queue is empty it works */
        assert(make_member(&scratch,25));scratch.party[0].pause_evolutions=true;
        assert(gc_football_item_evolutions(&scratch,0,found)==0 && !gc_football_item_evolve(&scratch,0,(int)stone,forget_first,&why) && scratch.party[0].b.species==25);
    }

    /* A full moveset: a move the evolution brings is placed by the callback, or left unlearned when none is given. */
    {
        static const uint16_t filler[4]={MOVE_ID_TACKLE,MOVE_ID_GROWL,MOVE_ID_TAIL_WHIP,MOVE_ID_LEER};
        unsigned replaced=0,kept=0;
        for(unsigned i=0;i<PR_SPECIES_COUNT;++i){
            unsigned id=pr_species_table[i].id;
            if(!has_item_evolution(id) || !make_member(&scratch,id))continue;
            gc_football_item_evolution list[GC_FOOTBALL_MAX_ITEM_EVOLUTIONS];int n=gc_football_item_evolutions(&scratch,0,list);
            for(int k=0;k<n;++k)for(unsigned pass=0;pass<2;++pass){
                assert(make_member(&scratch,id));pr_pokemon *p=&scratch.party[0].b;p->n_moves=4;for(unsigned m=0;m<4;++m){p->moves[m]=filler[m];p->pp_used[m]=0;}
                forgot=0;assert(gc_football_item_evolve(&scratch,0,list[k].item,pass?NULL:forget_first,&why));
                p=&scratch.party[0].b;assert(p->species==list[k].species && PR_N_MOVES(p)==4 && scratch.screen.kind==GC_SCREEN_NONE);
                if(pass){for(unsigned m=0;m<4;++m)assert(PR_MOVES(p)[m]==filler[m]);assert(forgot==0);kept+=1;}
                else replaced+=forgot;
            }
        }
        assert(kept>0 && replaced>0); /* some evolution brings a move that needs a slot */
    }

    /* Every item a species lists really evolves a member of it, into the species listed, without a prompt left open. */
    unsigned evolved=0,rare=0,multi=0;
    for(unsigned i=0;i<PR_SPECIES_COUNT;++i){
        unsigned id=pr_species_table[i].id;
        if(!has_item_evolution(id) || !make_member(&scratch,id))continue;
        int n=gc_football_item_evolutions(&scratch,0,found);multi+=n>1;
        gc_football_item_evolution list[GC_FOOTBALL_MAX_ITEM_EVOLUTIONS];memcpy(list,found,sizeof list);
        for(int a=0;a<n;++a)for(int b=a+1;b<n;++b)assert(list[a].item!=list[b].item); /* one entry for each item */
        for(int k=0;k<n;++k){
            assert(make_member(&scratch,id));
            assert(gc_football_item_evolve(&scratch,0,list[k].item,football_career_forget_slot,&why));
            assert(scratch.party[0].b.species==list[k].species && scratch.screen.kind==GC_SCREEN_NONE);
            ++evolved;rare+=list[k].item>50;
        }
    }
    assert(evolved>=60 && rare>=3 && multi>=3); /* Eevee and the like list several */

    /* ---- the career's shop ---- */
    unsigned roster[22];unsigned roster_count=species_with_items(0,roster,22);assert(roster_count==22);
    shop_with(&career,7,1,roster,roster_count);
    assert(career.wallet==500 && career.evo_count>0 && career.evo_count<=FB_SHOP_EVO_MAX);
    for(unsigned i=0;i<career.evo_count;++i){
        const FootballEvoOffer *o=&career.evo[i];
        assert(o->state==FB_EVO_OFFER && o->member<22 && o->item && o->species && species_of(o->species));
        assert(o->price==(o->item>50?800+75*1:400+50*1)); /* round 1 */
        assert(football_career_evo_valid(&career,o));
        if(i)assert(career.evo[i-1].member<o->member || (career.evo[i-1].member==o->member && career.evo[i-1].item<o->item)); /* by member, no repeats */
    }
    assert(career.evo_count==FB_SHOP_EVO_MAX); /* 22 members who can evolve, more than the shop holds */
    {   /* A different round starts from a different member, so the shop does not offer the same eight forever. */
        FootballCareer *other_round=calloc(1,sizeof *other_round);assert(other_round);
        shop_with(other_round,7,2,roster,roster_count);
        bool same=other_round->evo_count==career.evo_count;
        for(unsigned i=0;same && i<career.evo_count;++i)same=other_round->evo[i].member==career.evo[i].member;
        assert(!same);
        for(unsigned i=0;i<other_round->evo_count;++i)assert(other_round->evo[i].price==(other_round->evo[i].item>50?800+75*2:400+50*2));
        for(unsigned i=1;i<other_round->evo_count;++i)assert(other_round->evo[i-1].member<other_round->evo[i].member || (other_round->evo[i-1].member==other_round->evo[i].member && other_round->evo[i-1].item<other_round->evo[i].item)); /* listed by member even when the pick wrapped round the roster */
        free(other_round);
    }

    {   /* A member holding a mega stone is never offered an evolution, and the next member's offer takes its place. */
        unsigned held=career.evo[0].member;
        FootballCareer *with_stone=calloc(1,sizeof *with_stone);assert(with_stone);
        shop_with_mega(with_stone,7,1,roster,roster_count,(int)held);
        assert(with_stone->evo_count==FB_SHOP_EVO_MAX);
        for(unsigned i=0;i<with_stone->evo_count;++i)assert(with_stone->evo[i].member!=held);
        free(with_stone);
    }
    {   /* More items than room (22 Eevees, five stones each): the first stone of eight different members, not all of one member's first. */
        unsigned eevees[1]={133};FootballCareer *many=calloc(1,sizeof *many);assert(many);
        shop_with(many,7,1,eevees,1);
        assert(many->evo_count==FB_SHOP_EVO_MAX);
        for(unsigned i=0;i<many->evo_count;++i)for(unsigned j=i+1;j<many->evo_count;++j)assert(many->evo[i].member!=many->evo[j].member);
        free(many);
    }

    {   /* One Eevee among Charmanders: all five of its stones are on sale, and buying one makes the others useless. */
        unsigned eevee_line[22];eevee_line[0]=133;for(unsigned i=1;i<22;++i)eevee_line[i]=4;
        FootballCareer *one=calloc(1,sizeof *one);assert(one);
        shop_with(one,7,1,eevee_line,22);
        assert(one->evo_count==5);
        for(unsigned i=0;i<one->evo_count;++i)assert(one->evo[i].member==0 && one->evo[i].state==FB_EVO_OFFER);
        FootballEvoOffer bought=one->evo[2];
        one->roster.mega_item[0]=1;assert(!football_career_buy(one,FB_SHOP_FIXED+2,0) && one->wallet==500 && one->evo[2].state==FB_EVO_OFFER); /* a held stone would be lost */
        one->roster.mega_item[0]=0;
        assert(football_career_buy(one,FB_SHOP_FIXED+2,0) && (unsigned)one->roster.mon[0].b.species==bought.species && one->wallet==500u-bought.price);
        for(unsigned i=0;i<one->evo_count;++i)assert(one->evo[i].state==(i==2?FB_EVO_SOLD:FB_EVO_VOID));
        for(unsigned i=0;i<one->evo_count;++i)assert(!football_career_buy(one,FB_SHOP_FIXED+i,0));
        assert(one->wallet==500u-bought.price);
        size_t saved=football_career_save(one,bytes,sizeof bytes);assert(saved && football_career_load(&loaded,&other,&profile,bytes,saved)); /* voided offers need not hold, so the checkpoint loads */
        assert(loaded.evo_count==5 && loaded.evo[2].state==FB_EVO_SOLD && loaded.evo[0].state==FB_EVO_VOID);
        free(one);
    }

    /* Buying: the member really evolves, the cash goes, the other items for it no longer fit, the roster is refreshed. */
    FootballEvoOffer first=career.evo[0];unsigned member=first.member;
    const gc_mon old_mon=career.roster.mon[member];
    unsigned other_for_member=0;for(unsigned i=1;i<career.evo_count;++i)other_for_member+=career.evo[i].member==member;
    assert(first.price<=career.wallet);
    unsigned wallet=career.wallet;
    /* The preview is what the purchase will make of the member, and looking at it changes nothing. */
    FootballPlayer preview,seen_before=career.roster.player[member];
    assert(football_career_evo_preview(&career,0,&preview) && !football_career_evo_preview(&career,career.evo_count,&preview) && !football_career_evo_preview(&career,0,NULL) && !football_career_evo_preview(NULL,0,&preview));
    assert(!memcmp(&career.roster.mon[member],&old_mon,sizeof old_mon) && career.roster.player[member].species_id==seen_before.species_id && !memcmp(&career.roster.player[member].attributes,&seen_before.attributes,sizeof seen_before.attributes));
    assert(preview.species_id==first.species && seen_before.species_id!=first.species && preview.roster_id==seen_before.roster_id); /* the same mon, as the new species */
    assert(!football_career_buy(&career,FB_SHOP_FIXED+career.evo_count,0)); /* no such offer */
    assert(football_career_buy(&career,FB_SHOP_FIXED+0,0));
    assert(career.wallet==wallet-first.price && career.evo[0].state==FB_EVO_SOLD);
    assert(career.roster.mon[member].b.species==first.species && career.roster.mon[member].b.id==old_mon.b.id && career.roster.mon[member].b.level==old_mon.b.level);
    assert(career.roster.mon[member].met_species==old_mon.met_species); /* candy still goes to the species it started as */
    assert(strstr(career.message,"evolved into"));
    assert(!memcmp(&preview.attributes,&career.roster.player[member].attributes,sizeof preview.attributes)); /* what the preview showed is what it became */
    assert(!football_career_evo_preview(&career,0,&preview)); /* bought: nothing left to preview */
    for(unsigned i=1;i<career.evo_count;++i)if(career.evo[i].member==member)assert(career.evo[i].state==FB_EVO_VOID);
    assert(!football_career_buy(&career,FB_SHOP_FIXED+0,0)); /* sold */
    for(unsigned i=1;i<career.evo_count;++i)if(career.evo[i].member==member)assert(!football_career_buy(&career,FB_SHOP_FIXED+i,0) && career.wallet==wallet-first.price);
    (void)other_for_member;
    {   /* Not enough cash: nothing changes. */
        unsigned i=1;while(i<career.evo_count && career.evo[i].state!=FB_EVO_OFFER)++i;
        assert(i<career.evo_count);
        unsigned poor=(unsigned)career.evo[i].price-1u;career.wallet=poor;int species=career.roster.mon[career.evo[i].member].b.species;
        assert(!football_career_buy(&career,FB_SHOP_FIXED+i,0) && career.wallet==poor && career.roster.mon[career.evo[i].member].b.species==species && career.evo[i].state==FB_EVO_OFFER);
        career.wallet=wallet-first.price;
    }

    /* The fixed offers still work beside them, and a purchase made in the shop is saved. */
    career.wallet+=500;wallet=career.wallet;
    assert(football_career_buy(&career,0,21) && !football_career_buy(&career,0,21) && career.roster.speed[21]==2 && career.wallet==wallet-career.prices[0]);
    size_t n=football_career_save(&career,bytes,sizeof bytes);assert(n);
    assert(football_career_load(&loaded,&other,&profile,bytes,n));
    assert(loaded.evo_count==career.evo_count && !memcmp(loaded.evo,career.evo,sizeof career.evo) && loaded.wallet==career.wallet && loaded.roster.mon[member].b.species==first.species);
    for(unsigned i=0;i<loaded.evo_count;++i)if(loaded.evo[i].state==FB_EVO_OFFER)assert(football_career_buy(&loaded,FB_SHOP_FIXED+i,0) || loaded.wallet<loaded.evo[i].price);

    /* A saved offer that no longer fits is refused, as is one outside the shop, one of an unknown version, and a stone the member holds. */
    n=football_career_save(&career,bytes,sizeof bytes);assert(n);
    unsigned open=0;while(open<career.evo_count && career.evo[open].state!=FB_EVO_OFFER)++open;assert(open<career.evo_count);
    FootballEvoOffer saved=career.evo[open];
    career.evo[open].species=(uint16_t)(saved.species==1?2:1);n=football_career_save(&career,bytes,sizeof bytes);assert(n && !football_career_load(&loaded,&other,&profile,bytes,n));
    career.evo[open]=saved;career.evo[open].item=(uint8_t)(saved.item==1?2:1);n=football_career_save(&career,bytes,sizeof bytes);
    {   /* item 1 or 2 can fit some member by chance: only count it as a refusal when the offer really does not hold */
        bool holds=football_career_evo_valid(&career,&career.evo[open]);assert(holds==(n && football_career_load(&loaded,&other,&profile,bytes,n)));
    }
    career.evo[open]=saved;career.evo[open].member=(uint8_t)((saved.member+1)%22);
    {   bool holds=football_career_evo_valid(&career,&career.evo[open]);n=football_career_save(&career,bytes,sizeof bytes);assert(n && holds==football_career_load(&loaded,&other,&profile,bytes,n));}
    {   /* A save that fails (an offer that costs nothing is not one) leaves every offer as it was. */
        FootballEvoOffer snapshot[FB_SHOP_EVO_MAX];memcpy(snapshot,career.evo,sizeof snapshot);
        career.evo[open].price=0;snapshot[open].price=0;n=football_career_save(&career,bytes,sizeof bytes);assert(!n);
        assert(!memcmp(snapshot,career.evo,sizeof snapshot));
    }
    career.evo[open]=saved;career.evo[open].member=22;n=football_career_save(&career,bytes,sizeof bytes);assert(!n);
    career.evo[open]=saved;career.evo[open].state=3;n=football_career_save(&career,bytes,sizeof bytes);assert(!n);
    career.evo[open]=saved;
    career.roster.mega_item[saved.member]=1;n=football_career_save(&career,bytes,sizeof bytes);
    assert(!football_career_evo_valid(&career,&saved) && (!n || !football_career_load(&loaded,&other,&profile,bytes,n)));
    career.roster.mega_item[saved.member]=0;
    {   int stone_before=career.stone_target;career.stone_target=(int)saved.member;
        assert(!football_career_evo_valid(&career,&saved));career.stone_target=stone_before;}
    n=football_career_save(&career,bytes,sizeof bytes);assert(n && football_career_load(&loaded,&other,&profile,bytes,n));
    put32(8,3);put32(16,crc(bytes+20,n-20));assert(!football_career_load(&loaded,&other,&profile,bytes,n)); /* a newer format is refused */
    put32(8,0);put32(16,crc(bytes+20,n-20));assert(!football_career_load(&loaded,&other,&profile,bytes,n));
    {   /* Offers outside the shop are refused. */
        FootballCareer *lineup=calloc(1,sizeof *lineup);assert(lineup);*lineup=career;lineup->phase=FB_CAREER_LINEUP;lineup->wins=lineup->round-1; /* a consistent lineup checkpoint, but for the offers */
        n=football_career_save(lineup,bytes,sizeof bytes);assert(n && !football_career_load(&loaded,&other,&profile,bytes,n));
        lineup->evo_count=0;memset(lineup->evo,0,sizeof lineup->evo);n=football_career_save(lineup,bytes,sizeof bytes);assert(n && football_career_load(&loaded,&other,&profile,bytes,n)); /* the same checkpoint without them loads */
        free(lineup);
    }

    /* A version 1 checkpoint (no offers) still loads, with an empty shelf of evolution items. */
    {
        FootballCareer *v1=calloc(1,sizeof *v1);assert(v1);*v1=career;v1->wallet=1000;v1->evo_count=0;memset(v1->evo,0,sizeof v1->evo);
        n=football_career_save(v1,bytes,sizeof bytes);assert(n>24);
        n-=4;put32(8,1);put32(12,(uint32_t)n);put32(16,crc(bytes+20,n-20)); /* the offer count is the last word of a shop checkpoint */
        assert(football_career_load(&loaded,&other,&profile,bytes,n) && loaded.evo_count==0 && loaded.phase==FB_CAREER_SHOP && loaded.wallet==v1->wallet);
        assert(football_career_buy(&loaded,1,3) && loaded.roster.strength[3]==2); /* the rest of the shop works as before */
        free(v1);
    }

    /* Leaving the shop clears the shelf, and the message with it. */
    assert(career.evo_count>0 && football_career_continue(&career) && career.evo_count==0 && career.round==2 && career.message[0]==0);
    for(unsigned i=0;i<FB_SHOP_EVO_MAX;++i)assert(career.evo[i].item==0);

    /* The same career rolls the same shelf. */
    shop_with(&career,7,1,roster,roster_count);shop_with(&again,7,1,roster,roster_count);
    assert(career.evo_count==again.evo_count && !memcmp(career.evo,again.evo,sizeof career.evo));

    /* A member who cannot evolve with an item is never offered one; a roster of such members has an empty shelf. */
    {
        unsigned plain[1]={4};/* Charmander */
        shop_with(&career,9,1,plain,1);assert(career.evo_count==0 && career.wallet==500);
        assert(!football_career_buy(&career,FB_SHOP_FIXED,0));
    }
    /* A rare item (Black Augurite, Metal Alloy, a scroll) against Growlithe's Fire Stone is dearer than a common one, and the price climbs with the round. */
    {
        unsigned rare_roster[4]={123,884,891,58}; /* Scyther, Duraludon, Kubfu, Growlithe */
        shop_with(&career,9,3,rare_roster,4);
        unsigned cheap=0,dear=0;
        for(unsigned i=0;i<career.evo_count;++i){const FootballEvoOffer *o=&career.evo[i];
            assert(o->price==(o->item>50?800+75*3:400+50*3));
            if(o->item>50)dear=o->price;else cheap=o->price;}
        assert(cheap && dear>cheap);
        assert(career.wallet==400+100*3 && (cheap>career.wallet?football_career_buy(&career,FB_SHOP_FIXED,0)==false:true));
    }
    printf("football shop evolution: %u item evolutions (%u rare, %u species with several items), offers, purchase, round rotation, version 1/2 saves and refusals passed\n",evolved,rare,multi);
    return 0;
}
