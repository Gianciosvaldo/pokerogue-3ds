#include "football_match.h"
#include "football_playbook.h"
#include <string.h>

static uint32_t random_next(uint32_t *state)
{
    uint32_t x=*state; x^=x<<13; x^=x>>17; x^=x<<5; return *state=x;
}
static int clamp(int v,int low,int high) { return v<low ? low : v>high ? high : v; }
static bool valid_team(const FootballTeam *team)
{
    if (!team || !team->count || team->count>FB_MATCH_TEAM_CAP) return false;
    for (unsigned i=0;i<team->count;++i) {
        const FootballAttributes *a=&team->players[i].attributes;
        const unsigned values[]={a->spd,a->agi,a->str,a->tak,a->sta,a->thp,a->tha,a->cth};
        for (unsigned j=0;j<8;++j) if (!values[j] || values[j]>99) return false;
    }
    return true;
}
static int attribute(const FootballMatch *m,unsigned side,unsigned slot,unsigned stat)
{
    const FootballAttributes *a=&m->teams[side].players[slot].attributes;
    const int v[]={a->spd,a->agi,a->str,a->tak,a->sta,a->thp,a->tha,a->cth};
    int raw=side==FB_AWAY ? v[stat]*m->difficulty_q8/256 : v[stat];
    raw=clamp(raw,1,99);
    return clamp(raw*(70+30*m->energy[side][slot]/100)/100,1,99);
}
/* Stat indexes: speed, agility, strength, tackle, stamina, throw power,
 * throw accuracy, catching. No struct-byte aliasing or compiler padding assumed. */
static float role_affinity(const FootballPlayer *p,FootballPosition role)
{
    float value=football_stats_type_affinity(p->primary_type,role);
    if (p->secondary_type>=0 && p->secondary_type<PR_TYPE_COUNT && p->secondary_type!=p->primary_type)
        value=(value+football_stats_type_affinity(p->secondary_type,role))*.5f;
    return value;
}
static unsigned best(const FootballMatch *m,unsigned side,unsigned a,unsigned b)
{
    unsigned result=0; float highest=-1;
    FootballPosition role=a==5 ? FB_QB : a==3 ? FB_LB : a==7 ? FB_WR : FB_RB;
    for (unsigned i=0;i<m->teams[side].count;++i) {
        float score=(attribute(m,side,i,a)+attribute(m,side,i,b))*role_affinity(&m->teams[side].players[i],role);
        if (score>highest) { highest=score; result=i; }
    }
    return result;
}
unsigned football_match_quarterback(const FootballMatch *m,FootballSide side)
{
    if (!m || (unsigned)side>FB_AWAY || !m->teams[side].count || m->teams[side].count>FB_MATCH_TEAM_CAP) return 0;
    return best(m,(unsigned)side,5,6);
}
static int average(const FootballMatch *m,unsigned side,unsigned stat)
{
    int sum=0;
    for (unsigned i=0;i<m->teams[side].count;++i) sum+=attribute(m,side,i,stat);
    return sum/m->teams[side].count;
}
static FootballAttributes effective(const FootballMatch *m,unsigned side,unsigned slot)
{
    return (FootballAttributes){attribute(m,side,slot,0),attribute(m,side,slot,1),
        attribute(m,side,slot,2),attribute(m,side,slot,3),attribute(m,side,slot,4),
        attribute(m,side,slot,5),attribute(m,side,slot,6),attribute(m,side,slot,7)};
}
static unsigned lineman(const FootballMatch *m,unsigned side,unsigned exclude_a,unsigned exclude_b)
{
    unsigned result=255;float highest=-1;
    for (unsigned i=0;i<m->teams[side].count;++i) if (i!=exclude_a && i!=exclude_b) {
        float score=(attribute(m,side,i,2)+attribute(m,side,i,3))*role_affinity(&m->teams[side].players[i],FB_OL_DL);
        if (score>highest) { highest=score;result=i; }
    }
    return result;
}
static void snap(FootballMatch *m,PlayInfo play,FootballDefenseId defense_call)
{
    const FootballPlayDefinition *definition=football_playbook_get(play.id);
    const FootballDefenseDefinition *coverage=football_defense_get(defense_call);
    m->defense=defense_call;
    unsigned side=m->rules.offense,defense=1-side;
    unsigned qb=best(m,side,5,6);
    FootballPosition wanted=play.type==FB_PLAY_PASS ? FB_WR : FB_RB;
    bool hinted=play.target_slot<m->teams[side].count &&
        (m->teams[side].players[play.target_slot].position==wanted ||
         (play.type==FB_PLAY_PASS && m->teams[side].players[play.target_slot].position==FB_TE_S));
    unsigned target=hinted ? play.target_slot : best(m,side,play.type==FB_PLAY_PASS ? 7 : 0,1);
    if (play.type==FB_PLAY_PASS && target==qb && m->teams[side].count>1) {
        float highest=-1;
        for (unsigned i=0;i<m->teams[side].count;++i) if (i!=qb) {
            float score=(attribute(m,side,i,7)+attribute(m,side,i,0))*role_affinity(&m->teams[side].players[i],FB_WR);
            if (score>highest) { highest=score; target=i; }
        }
    }
    unsigned tackler=best(m,defense,3,0);
    int tackling=attribute(m,defense,tackler,3), speed=attribute(m,side,target,0);
    int agility=attribute(m,side,target,1), strength=attribute(m,side,target,2);
    m->blocker=(uint8_t)lineman(m,side,qb,target);
    m->rusher=(uint8_t)lineman(m,defense,255,255);
    m->tackler=(uint8_t)tackler;
    memset(&m->blocking,0,sizeof m->blocking);
    memset(&m->contact,0,sizeof m->contact);
    if (m->blocker!=255) {
        FootballAttributes b=effective(m,side,m->blocker),d=effective(m,defense,m->rusher);
        football_contact_block(&m->blocking,&b,&d,random_next(&m->rng)%100);
    }
    int roll=(int)(random_next(&m->rng)%100);
    FootballResolvedPlay result={FB_RESULT_RUN,0,10};
    if (play.type==FB_PLAY_RUN) {
        int edge=(speed+agility+strength+average(m,side,2))/4-
                 (tackling+average(m,defense,2))/2;
        edge+=m->blocking.edge;
        edge+=definition->speed_weight*((speed+agility)/2-strength)/2;
        result.yards=(int16_t)clamp(3+coverage->run_gain+definition->gain+edge/10+(play.formation==FB_FORM_UNDER_CENTER ? 1 : 0)+(int)(random_next(&m->rng)%11)-5,-8,30);
        if (roll>94 && edge>0) result.yards=(int16_t)(result.yards+15);
        result.seconds=(uint8_t)clamp(12+(result.yards>0 ? result.yards/3 : 0),8,25);
    } else {
        int power=attribute(m,side,qb,5), accuracy=attribute(m,side,qb,6);
        int catching=attribute(m,side,target,7);
        int pressure=average(m,defense,2)-average(m,side,2)-m->blocking.edge;
        int sack=clamp(7+coverage->sack+definition->sack+pressure/10+(play.formation==FB_FORM_UNDER_CENTER ? 3 : 0),2,20);
        int intercept=clamp(7+coverage->interception+definition->interception+(tackling-accuracy)/12,2,18);
        int completion=clamp(55+coverage->completion+definition->completion+(accuracy+catching-2*tackling)/5+(play.formation==FB_FORM_SHOTGUN ? 5 : 0),20,90);
        result.seconds=5;
        if (roll<sack) { result.kind=FB_RESULT_SACK; result.yards=-5; result.seconds=14; }
        else if (roll<sack+intercept) { result.kind=FB_RESULT_INTERCEPTION; result.yards=8; }
        else if ((int)(random_next(&m->rng)%100)<completion) {
            result.kind=FB_RESULT_COMPLETE;
            result.yards=(int16_t)clamp(5+definition->gain+power/10+(speed-tackling)/15+(play.formation==FB_FORM_UNDER_CENTER ? 5 : 0)+
                                     (int)(random_next(&m->rng)%13),1,40);
            result.seconds=12;
        } else result.kind=FB_RESULT_INCOMPLETE;
    }
    /* Resolve at most one carrier contact per snap. A completed pass can be
     * fumbled after the catch; an incompletion/interception has no carrier hit. */
    if (result.kind==FB_RESULT_RUN || result.kind==FB_RESULT_COMPLETE) {
        unsigned carrier=target;
        FootballAttributes c=effective(m,side,carrier),d=effective(m,defense,tackler);
        bool caught=result.kind==FB_RESULT_COMPLETE;
        unsigned tackle_roll=random_next(&m->rng)%100;
        unsigned fumble_roll=random_next(&m->rng)%100;
        football_contact_tackle(&m->contact,&c,&d,caught,tackle_roll,fumble_roll);
        if (m->contact.broken) result.yards=(int16_t)clamp(result.yards+m->contact.extra_yards,-8,45);
        if (m->contact.fumble) result.kind=FB_RESULT_FUMBLE;
    }
    m->play=play; m->play.target_slot=(uint8_t)target; m->carrier=(uint8_t)(result.kind==FB_RESULT_SACK ? qb : target);
    m->resolved=result; m->animation_ms=0; m->phase=FB_MATCH_ANIMATION;
}
bool football_match_begin(FootballMatch *out,const FootballTeam *home,
                           const FootballTeam *away,uint32_t seed,uint16_t difficulty)
{
    if (!out || !valid_team(home) || !valid_team(away) || difficulty<256 || difficulty>640) return false;
    FootballMatch m={0};
    m.teams[FB_HOME]=*home; m.teams[FB_AWAY]=*away;
    memset(m.energy,100,sizeof m.energy);
    m.rng=seed ? seed : 0xa341316cu; m.difficulty_q8=difficulty;
    m.phase=FB_MATCH_CALL;
    /* Final bounded overtime decider is strength-weighted and reproducible. */
    unsigned home_strength=(unsigned)(average(&m,0,6)+average(&m,0,7));
    unsigned away_strength=(unsigned)(average(&m,1,6)+average(&m,1,7));
    FootballSide tie=random_next(&m.rng)%(home_strength+away_strength)<home_strength ? FB_HOME : FB_AWAY;
    football_rules_begin(&m.rules,tie); *out=m; return true;
}
bool football_match_waiting_for_home(const FootballMatch *m)
{
    return m && m->phase==FB_MATCH_CALL && m->rules.offense==FB_HOME && !m->rules.finished;
}
bool football_match_waiting_for_defense(const FootballMatch *m)
{
    return m && m->manual_defense && m->phase==FB_MATCH_CALL && m->rules.offense==FB_AWAY && !m->rules.finished;
}
static PlayInfo opponent_call(FootballMatch *m)
{
    int pass_bias=football_rules_distance(&m->rules)>6 || m->rules.down>=3 ? 70 : 35;
    bool pass=(int)(random_next(&m->rng)%100)<pass_bias;
    FootballFormation formation=(FootballFormation)(random_next(&m->rng)%2);
    FootballPlayType type=pass ? FB_PLAY_PASS : FB_PLAY_RUN;
    unsigned count=football_playbook_count(formation,type);
    const FootballPlayDefinition *choice=football_playbook_option(formation,type,random_next(&m->rng)%count);
    return (PlayInfo){formation,type,choice->id,255};
}
static FootballDefenseId opponent_defense(FootballMatch *m)
{
    unsigned roll=random_next(&m->rng)%100;
    if(football_rules_distance(&m->rules)>6 && roll<55)return FB_DEF_ZONE;
    return roll>=75 ? FB_DEF_BLITZ : FB_DEF_MAN;
}
bool football_match_defend(FootballMatch *m,FootballDefenseId defense)
{
    if(!football_match_waiting_for_defense(m) || !football_defense_get(defense))return false;
    PlayInfo play=opponent_call(m);snap(m,play,defense);return true;
}
bool football_match_call(FootballMatch *m,PlayInfo play)
{
    if (!football_match_waiting_for_home(m) || !football_playbook_valid(play)) return false;
    FootballDefenseId defense=opponent_defense(m);
    snap(m,play,defense); return true;
}
bool football_match_tick(FootballMatch *m,uint32_t elapsed_ms)
{
    if (!m || !elapsed_ms || m->phase==FB_MATCH_FINISHED || m->phase==FB_MATCH_ERROR) return false;
    if (m->phase==FB_MATCH_CALL) {
        if (m->rules.offense==FB_HOME) return false;
        if(m->manual_defense)return false;
        PlayInfo play=opponent_call(m);
        snap(m,play,opponent_defense(m));return true;
    }
    m->animation_ms=(uint16_t)(m->animation_ms+(elapsed_ms>250 ? 250 : elapsed_ms));
    if (m->animation_ms<FB_PLAY_ANIMATION_MS) return true;
    m->animation_ms=FB_PLAY_ANIMATION_MS;
    if (!football_rules_advance(&m->rules,m->resolved)) { m->phase=FB_MATCH_ERROR; return false; }
    ++m->plays;
    for (unsigned side=0;side<2;++side) for (unsigned i=0;i<m->teams[side].count;++i) {
        unsigned stamina=m->teams[side].players[i].attributes.sta;
        int energy=m->energy[side][i]-(int)(2+(99-stamina)/20);
        if (m->rules.last.turnover || m->rules.last.touchdown || m->rules.last.safety || m->rules.last.halftime)
            energy+=8;
        m->energy[side][i]=(uint8_t)clamp(energy,0,100);
    }
    m->phase=m->rules.finished ? FB_MATCH_FINISHED : FB_MATCH_CALL;
    return true;
}
bool football_match_opponent(FootballTeam *out,const FootballDexMaxima *maxima,
                              uint32_t seed,unsigned round)
{
    if (!out || !maxima || round<1 || round>7) return false;
    FootballTeam team={0}; uint32_t rng=seed ? seed : 0x9e3779b9u;
    for (unsigned slot=0;slot<FB_MATCH_TEAM_CAP;++slot) {
        const pr_form *chosen=NULL; unsigned eligible=0;
        /* Reservoir sampling uses fixed memory, excludes special/mega forms and
         * permits progressively higher base-stat totals as rounds advance. */
        for (unsigned i=0;i<PR_FORM_COUNT;++i) {
            const pr_form *f=&pr_forms[i];
            if (f->form_index || f->unobtainable || f->base_total>300+50*round) continue;
            bool duplicate=false;
            for (unsigned k=0;k<slot;++k) duplicate|=team.players[k].species_id==f->species;
            if (duplicate) continue;
            if (random_next(&rng)%++eligible==0) chosen=f;
        }
        if (!chosen || !football_stats_generate(&team.players[slot],slot+1,chosen,maxima)) return false;
        ++team.count;
    }
    *out=team; return true;
}
const char *football_match_result_name(FootballResultKind result)
{
    switch (result) {
    case FB_RESULT_RUN: return "Run";
    case FB_RESULT_COMPLETE: return "Pass complete";
    case FB_RESULT_INCOMPLETE: return "Incomplete pass";
    case FB_RESULT_INTERCEPTION: return "Interception";
    case FB_RESULT_FUMBLE: return "Fumble lost";
    case FB_RESULT_SACK: return "Sack";
    }
    return "";
}
