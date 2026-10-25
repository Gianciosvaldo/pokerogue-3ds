#include "playoff_season.h"
#include <string.h>

static uint32_t random_next(FootballSeason *s)
{
    uint32_t x=s->rng;
    x^=x<<13; x^=x>>17; x^=x<<5;
    return s->rng=x;
}
static uint8_t boost(uint8_t value, uint8_t bonus)
{
    unsigned result=value+bonus;
    return result>99 ? 99 : (uint8_t)result;
}
static bool refresh(FootballSeason *s)
{
    for (int i=0;i<s->roster->n_party;++i) {
        const gc_mon *mon=&s->roster->party[i];
        const pr_form *f=NULL;
        for (unsigned k=0;k<PR_FORM_COUNT;++k)
            if (pr_forms[k].species==mon->b.species && pr_forms[k].form_index==mon->b.form) {
                f=&pr_forms[k]; break;
            }
        if (!f || !football_stats_generate(&s->players[i],mon->b.id,f,&s->maxima)) return false;
        s->players[i].attributes.spd=boost(s->players[i].attributes.spd,s->speed_boost[i]);
        s->players[i].attributes.str=boost(s->players[i].attributes.str,s->strength_boost[i]);
        s->players[i].position=football_stats_best_position(&s->players[i]);
    }
    return true;
}
static void open_shop(FootballSeason *s)
{
    memset(s->shop,0,sizeof s->shop);
    s->shop[0]=(FootballShopOffer){FB_ITEM_CARBOS,100+25*s->round,STAT_SPD,-1,false};
    s->shop[1]=(FootballShopOffer){FB_ITEM_PROTEIN,100+25*s->round,STAT_ATK,-1,false};
    s->shop_count=2;
    /* One 10% rare roll per shop, stable for this season's independent seed. */
    if (random_next(s)%10==0) {
        for (int i=0;i<s->roster->n_party;++i) {
            int item=gc_football_mega_item(s->roster,(unsigned)i);
            if (item<0) continue;
            s->shop[s->shop_count++]=(FootballShopOffer){
                FB_ITEM_MEGA_STONE,1000,(int16_t)item,(int8_t)i,false};
            break;
        }
    }
    s->phase=FB_SEASON_SHOP;
}
bool football_season_new(FootballSeason *s, gc_run *roster, gc_system *profile,
                         const char *seed, uint32_t entropy)
{
    if (!s || !roster) return false;
    memset(s,0,sizeof *s);
    s->roster=roster; s->rng=entropy ? entropy : 0x6d2b79f5u;
    s->round=1; s->mega_slot=-1; s->phase=FB_SEASON_DRAFT;
    if (!football_stats_build_maxima(&s->maxima,pr_forms,PR_FORM_COUNT)) {
        s->phase=FB_SEASON_ERROR; return false;
    }
    gc_football_new(roster,profile,seed,entropy);
    return true;
}
bool football_season_draft(FootballSeason *s, const gc_starter *team, int count,
                           const char **why)
{
    if (!s || s->phase!=FB_SEASON_DRAFT) return false;
    if (!gc_football_draft(s->roster,team,count,why)) return false;
    if (!refresh(s)) { s->phase=FB_SEASON_ERROR; return false; }
    s->phase=FB_SEASON_READY;
    return true;
}
bool football_season_select_mega(FootballSeason *s,int slot)
{
    if (!s || !s->roster || s->phase!=FB_SEASON_READY || slot< -1 ||
        slot>=s->roster->n_party || (slot>=0 && !gc_football_owned_mega(s->roster,(unsigned)slot))) return false;
    s->mega_slot=(int8_t)slot;return true;
}
bool football_season_match_player(const FootballSeason *s,unsigned slot,FootballPlayer *out)
{
    if (!s || !s->roster || !out || slot>=(unsigned)s->roster->n_party || slot>=FB_TEAM_CAP) return false;
    FootballPlayer player=s->players[slot];
    if ((int)slot==s->mega_slot) {
        const pr_form *form=gc_football_owned_mega(s->roster,slot);
        if (!form || !football_stats_generate(&player,player.roster_id,form,&s->maxima)) return false;
        player.attributes.spd=boost(player.attributes.spd,s->speed_boost[slot]);
        player.attributes.str=boost(player.attributes.str,s->strength_boost[slot]);
        player.position=football_stats_best_position(&player);
    }
    *out=player;return true;
}
bool football_season_start_match(FootballSeason *s, FootballMatchSpec *out)
{
    if (!s || !out || s->phase!=FB_SEASON_READY) return false;
    *out=(FootballMatchSpec){s->round,(uint16_t)(256+FB_DIFFICULTY_STEP_Q8*(s->round-1)),random_next(s)};
    s->phase=FB_SEASON_MATCH;
    return true;
}
bool football_season_finish_match(FootballSeason *s, bool won, const char **why)
{
    if (!s || s->phase!=FB_SEASON_MATCH) return false;
    s->mega_slot=-1; /* Match-only copy expires before EXP/evolution. */
    if (!won) { s->phase=FB_SEASON_ELIMINATED; return true; }
    if (!gc_football_award_exp(s->roster,s->round,why)) {
        s->phase=FB_SEASON_ERROR; return false;
    }
    ++s->wins;
    s->wallet+=400+100*s->round;
    s->phase=FB_SEASON_EXP;
    return true;
}
gc_football_progress football_season_tick(FootballSeason *s)
{
    if (!s || s->phase!=FB_SEASON_EXP) return GC_FB_ERROR;
    gc_football_progress p=gc_football_tick(s->roster);
    if (p==GC_FB_ERROR) s->phase=FB_SEASON_ERROR;
    if (p==GC_FB_DONE) {
        if (!refresh(s)) { s->phase=FB_SEASON_ERROR; return GC_FB_ERROR; }
        open_shop(s);
    }
    return p;
}
bool football_season_choose(FootballSeason *s, const gc_choice *c,
                            unsigned evolution_choice, const char **why)
{
    return s && s->phase==FB_SEASON_EXP &&
        gc_football_choose(s->roster,c,evolution_choice,why);
}
bool football_season_buy(FootballSeason *s, unsigned offer, unsigned party)
{
    if (!s || s->phase!=FB_SEASON_SHOP || offer>=s->shop_count ||
        party>=(unsigned)s->roster->n_party) return false;
    FootballShopOffer *o=&s->shop[offer];
    if (o->sold || s->wallet<o->price) return false;
    if (o->item==FB_ITEM_MEGA_STONE) {
        if (o->target!=(int)party || !gc_football_give_mega(s->roster,party,o->classic_arg)) return false;
    } else {
        uint8_t *bonus=o->item==FB_ITEM_CARBOS ? &s->speed_boost[party] : &s->strength_boost[party];
        uint8_t value=o->item==FB_ITEM_CARBOS ? s->players[party].attributes.spd : s->players[party].attributes.str;
        if (*bonus>=20 || value>=99) return false;
        *bonus=(uint8_t)(*bonus+2);
        if (!refresh(s)) { s->phase=FB_SEASON_ERROR; return false; }
    }
    s->wallet-=o->price; o->sold=true;
    return true;
}
bool football_season_leave_shop(FootballSeason *s)
{
    if (!s || s->phase!=FB_SEASON_SHOP) return false;
    if (s->round==FB_PLAYOFF_ROUNDS) s->phase=FB_SEASON_CLEAR;
    else { ++s->round; s->phase=FB_SEASON_READY; }
    return true;
}
const char *football_shop_asset_key(const FootballShopOffer *o)
{
    if (!o) return NULL;
    switch (o->item) {
    case FB_ITEM_CARBOS: return "BASE_STAT_BOOSTER";
    case FB_ITEM_PROTEIN: return "BASE_STAT_BOOSTER";
    case FB_ITEM_MEGA_STONE: return "FORM_CHANGE_ITEM";
    }
    return NULL;
}
