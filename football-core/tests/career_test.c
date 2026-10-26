#include "football_career.h"
#include "gc/data.h"
#include <assert.h>
#include "football_profile_fixture.h"
#include <stdio.h>
#include <string.h>
static FootballCareer career,loaded;
static gc_run scratch,other;
static unsigned char bytes[FB_CAREER_SAVE_MAX];
static uint32_t crc(const unsigned char *p,size_t n){uint32_t v=~0u;while(n--){v^=*p++;for(unsigned i=0;i<8;++i)v=(v>>1)^(0xedb88320u & (0u-(v&1)));}return ~v;}
static void put32(unsigned offset,uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=(unsigned char)(value>>(8*i));}
static gc_system profile,locked_profile;
int main(void)
{
    football_test_unlock_all(&profile);
    assert(football_career_new(&career,&scratch,&profile,7));
    for(int i=0;i<gc_starter_cost_count && career.roster.count<22;++i){if(gc_starter_costs[i].cost>1)continue;gc_starter s={.species=gc_starter_costs[i].id,.tera_type=-1};football_roster_add(&career.roster,&scratch,&s,&profile,NULL);}
    assert(career.roster.count==22);football_roster_autofill(&career.roster);career.phase=FB_CAREER_LINEUP;
    size_t n=football_career_save(&career,bytes,sizeof bytes);assert(n);
    assert(football_career_load(&loaded,&other,&profile,bytes,n));assert(loaded.roster.count==22 && loaded.seed==career.seed);
    assert(!memcmp(career.roster.lineup,loaded.roster.lineup,22));
    gc_system_init_defaults(&locked_profile);
    assert(!football_career_load(&loaded,&other,&locked_profile,bytes,n));
    assert(football_career_load(&loaded,&other,&profile,bytes,n));
    /* Valid-CRC out-of-range count/index values must fail before indexing. */
    put32(44,255);put32(16,crc(bytes+20,n-20));assert(!football_career_load(&loaded,&other,&profile,bytes,n));
    n=football_career_save(&career,bytes,sizeof bytes);assert(n);
    put32(52,255);put32(16,crc(bytes+20,n-20));assert(!football_career_load(&loaded,&other,&profile,bytes,n));
    n=football_career_save(&career,bytes,sizeof bytes);assert(n);
    bytes[n-1]^=1;assert(!football_career_load(&loaded,&other,&profile,bytes,n));bytes[n-1]^=1;
    assert(!football_career_load(&loaded,&other,&profile,bytes,n-1));
    assert(football_career_start(&career));n=football_career_save(&career,bytes,sizeof bytes);assert(n && football_career_load(&loaded,&other,&profile,bytes,n));
    assert(loaded.live.rules.ball==career.live.rules.ball && loaded.live.rng==career.live.rng && loaded.live.actors[0].x==career.live.actors[0].x);
    assert(loaded.live.rules.td_points==6 && career.live.rules.td_points==6); /* a touchdown is six wherever the match resumes */
    /* The try after a touchdown is not a safe checkpoint: a resumed match always opens a playcall, never a try. */
    career.live.conversion=1;assert(!football_career_save_ready(&career));career.live.conversion=2;assert(!football_career_save_ready(&career));career.live.conversion=0;assert(football_career_save_ready(&career));
    assert(football_live_call(&career.live,FB_RUN_DIVE));assert(!football_career_save_ready(&career));
    /* Reach real shared EXP/evolution phases for all 22 members. */
    /* Full movesets everywhere, so every level-up move needs a slot replaced. */
    static const uint16_t filler[4]={MOVE_ID_TACKLE,MOVE_ID_GROWL,MOVE_ID_TAIL_WHIP,MOVE_ID_LEER};
    for(unsigned i=0;i<22;++i){pr_pokemon *p=&career.roster.mon[i].b;p->n_moves=4;for(unsigned k=0;k<4;++k){p->moves[k]=filler[k];p->pp_used[k]=0;}}
    career.phase=FB_CAREER_RESULT;career.live.rules.winner=FB_HOME;assert(football_career_continue(&career));
    unsigned guard=0,choices=0,changed=0;uint16_t before[22][4];unsigned known[22];
    for(unsigned i=0;i<22;++i){known[i]=(unsigned)PR_N_MOVES(&career.roster.mon[i].b);for(unsigned k=0;k<4;++k)before[i][k]=k<known[i]?PR_MOVES(&career.roster.mon[i].b)[k]:0;}
    while(career.phase!=FB_CAREER_SHOP && ++guard<10000){
        /* A level-up move is learned without a prompt: the career never waits on one. */
        assert(career.phase!=FB_CAREER_LEARN);
        if(career.phase==FB_CAREER_EVOLVE){assert(football_career_evolve(&career,0));choices++;}
        else {assert(career.phase==FB_CAREER_EXP);football_career_tick(&career,16,(FootballLiveInput){.receiver=-1});}
    }
    for(unsigned i=0;i<22;++i){const pr_pokemon *p=&career.roster.mon[i].b;unsigned n=(unsigned)PR_N_MOVES(p);assert(n>=known[i] && n<=4);
        for(unsigned a=0;a<n;++a)for(unsigned b=a+1;b<n;++b)assert(PR_MOVES(p)[a]!=PR_MOVES(p)[b] || PR_MOVES(p)[a]==MOVE_ID_SKETCH);
        for(unsigned k=0;k<n;++k)if(k>=known[i] || PR_MOVES(p)[k]!=before[i][k]){++changed;break;}}
    assert(changed>0); /* some member replaced a full slot with a level-up move */
    for(unsigned i=0;i<22 && changed;++i){const pr_pokemon *p=&career.roster.mon[i].b;assert(PR_N_MOVES(p)==4);
        for(unsigned k=0;k<4;++k)if(PR_MOVES(p)[k]!=filler[0] && PR_MOVES(p)[k]!=filler[1] && PR_MOVES(p)[k]!=filler[2] && PR_MOVES(p)[k]!=filler[3])changed=0;}
    assert(!changed);
    assert(career.phase==FB_CAREER_SHOP && guard<10000 && career.wins==1 && career.wallet==500);
    for(unsigned i=0;i<22;++i)assert(career.roster.mon[i].b.level>5);
    assert(football_career_buy(&career,0,21));assert(!football_career_buy(&career,0,21));
    n=football_career_save(&career,bytes,sizeof bytes);assert(n && football_career_load(&loaded,&other,&profile,bytes,n));assert(loaded.roster.speed[21]==2 && loaded.sold[0]);
    assert(football_career_continue(&loaded) && loaded.round==2 && loaded.phase==FB_CAREER_READY);
    {   /* Replacing a full moveset drops the weakest move; status moves count as power 0 and the earliest slot wins a tie. */
        pr_pokemon p;memset(&p,0,sizeof p);unsigned strong=0,weak=0,status=0;
        for(unsigned id=1;id<PR_MOVE_COUNT;++id){if(pr_moves[id].unimplemented)continue;
            if(pr_moves[id].power>=100 && !strong)strong=id;
            if(pr_moves[id].power>0 && pr_moves[id].power<=20 && !weak)weak=id;
            if(pr_moves[id].power<=0 && !status)status=id;}
        assert(strong && weak && status);
        p.n_moves=4;p.moves[0]=(uint16_t)strong;p.moves[1]=(uint16_t)weak;p.moves[2]=(uint16_t)strong;p.moves[3]=(uint16_t)status;
        assert(football_career_forget_slot(&p)==3);
        p.moves[3]=(uint16_t)strong;assert(football_career_forget_slot(&p)==1);
        p.moves[1]=(uint16_t)strong;assert(football_career_forget_slot(&p)==0);
        p.n_moves=0;assert(football_career_forget_slot(&p)==-1 && football_career_forget_slot(NULL)==-1);
    }
    printf("football career: 22-member persistence, CRC/truncation rejection, safe checkpoints, shared EXP/evolution (%u choices), prompt-free level-up moves and shop conservation passed\n",choices);
}
