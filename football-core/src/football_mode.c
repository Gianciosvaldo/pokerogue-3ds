#include "football_mode.h"
#include <stdio.h>
#include <string.h>
static const char *species_name(unsigned species)
{
    for (unsigned i=0;i<PR_SPECIES_COUNT;++i) if (pr_species_table[i].id==species) return pr_species_table[i].name;
    return "Pokemon";
}
static const char *move_name(unsigned move)
{
    return move<PR_MOVE_COUNT ? pr_moves[move].name : "Unknown move";
}
static void error(FootballMode *m,const char *why)
{
    m->phase=FB_MODE_ERROR;
    snprintf(m->message,sizeof m->message,"%s",why && *why ? why : "The playoff could not continue.");
}
bool football_mode_new(FootballMode *m,gc_run *roster,gc_system *profile,uint32_t entropy)
{
    if (!m || !roster) return false;
    memset(m,0,sizeof *m);
    if (!football_season_new(&m->season,roster,profile,NULL,entropy)) return false;
    m->phase=FB_MODE_DRAFT; return true;
}
bool football_mode_draft(FootballMode *m,const gc_starter *team,int count,const char **why)
{
    if (!m || m->phase!=FB_MODE_DRAFT || !football_season_draft(&m->season,team,count,why)) return false;
    m->phase=FB_MODE_ROUND; return true;
}
static bool show_event(FootballMode *m,const gc_event *event)
{
    const gc_run *run=m->season.roster;
    const char *name=event->a>=0 && event->a<run->n_party ? species_name(run->party[event->a].b.species) : "Pokemon";
    switch (event->kind) {
    case GC_EV_EXP:
        snprintf(m->message,sizeof m->message,"%s gained %.0f EXP.",name,event->exp_amount);break;
    case GC_EV_LEVEL_UP:
        snprintf(m->message,sizeof m->message,"%s reached level %d.",name,event->b);break;
    case GC_EV_EVOLVED:
        m->profile_dirty=true;
        snprintf(m->message,sizeof m->message,"%s evolved into %s.",species_name((unsigned)event->b),species_name((unsigned)event->c));break;
    case GC_EV_MOVE_LEARNED:
        snprintf(m->message,sizeof m->message,"%s learned %s.",name,move_name((unsigned)event->c));break;
    default: return false; /* Typed events already present the corresponding generic text. */
    }
    m->phase=FB_MODE_MESSAGE;return true;
}
void football_mode_tick(FootballMode *m,uint32_t elapsed_ms)
{
    if (!m || m->done) return;
    if (m->phase==FB_MODE_PLAY) {
        const char *why=NULL;
        if (!football_playoff_match_tick(&m->session,&m->season,elapsed_ms,&why)) {
            error(m,why);return;
        }
        if (!m->session.active) {
            m->phase=FB_MODE_RESULT;
            snprintf(m->message,sizeof m->message,"%s  Home %u - Away %u",
                m->session.match.rules.winner==FB_HOME ? "Match won!" : "Match lost.",
                (unsigned)m->session.match.rules.score[0],(unsigned)m->session.match.rules.score[1]);
        }
    } else if (m->phase==FB_MODE_EVENTS) {
        gc_event event;
        /* Keep the bounded gc event queue drained BEFORE showing its next prompt. */
        for (unsigned i=0;i<GC_MAX_EVENTS && gc_run_next_event(m->season.roster,&event);++i)
            if (show_event(m,&event)) return;
        if (m->season.phase==FB_SEASON_SHOP) {
            memset(&m->shop,0,sizeof m->shop);m->phase=FB_MODE_SHOP;return;
        }
        const gc_screen *screen=gc_run_screen(m->season.roster);
        if (screen->kind==GC_SCREEN_EVOLVE) {
            uint16_t options[GC_FOOTBALL_MAX_EVOLUTIONS];
            int n=gc_football_evolution_choices(m->season.roster,options);
            if (n<1 || !football_evolution_open(&m->evolution,options,(unsigned)n)) {
                error(m,"No valid evolution choices are available.");return;
            }
            m->phase=FB_MODE_EVOLVE;return;
        }
        if (screen->kind==GC_SCREEN_LEARN_MOVE) { m->move_choice=0;m->phase=FB_MODE_LEARN;return; }
        m->profile_dirty=true; /* Shared evolution/form phases can update the profile. */
        if (football_season_tick(&m->season)==GC_FB_ERROR) error(m,NULL);
        /* New events/prompts are handled on the next tick, in their original order. */
    }
}
unsigned football_mode_move_count(const FootballMode *m)
{
    if (!m || m->phase!=FB_MODE_LEARN) return 0;
    const gc_run *run=m->season.roster;
    int party=run->screen.party;
    if (party<0 || party>=run->n_party || PR_N_MOVES(&run->party[party].b)>4) return 0;
    return (unsigned)PR_N_MOVES(&run->party[party].b)+1;
}
const char *football_mode_move_name(const FootballMode *m,unsigned option)
{
    unsigned count=football_mode_move_count(m);
    if (!count || option>=count) return "";
    if (option+1==count) return "Do not learn";
    const gc_run *r=m->season.roster;
    return move_name(PR_MOVES(&r->party[r->screen.party].b)[option]);
}
const char *football_mode_new_move_name(const FootballMode *m)
{
    return m && m->phase==FB_MODE_LEARN ? move_name(m->season.roster->screen.move) : "";
}
FootballTouchRect football_mode_move_rect(unsigned option)
{
    return option<5 ? (FootballTouchRect){12,60+(int)option*28,296,24} : (FootballTouchRect){0,0,0,0};
}
static bool hit(FootballTouchRect r,FootballCallInput in)
{
    return in.touch_down && (int)in.touch_x>=r.x && (int)in.touch_x<r.x+r.w &&
        (int)in.touch_y>=r.y && (int)in.touch_y<r.y+r.h;
}
static void choose(FootballMode *m,gc_choice c,unsigned evolution)
{
    const char *why=NULL;
    if (!football_season_choose(&m->season,&c,evolution,&why)) { error(m,why);return; }
    m->profile_dirty=true;m->phase=FB_MODE_EVENTS;
}
void football_mode_input(FootballMode *m,FootballCallInput in,int target_step)
{
    if (!m || m->done) return;
    bool accept=(in.pressed&FB_INPUT_ACCEPT)!=0 || hit((FootballTouchRect){12,164,296,52},in);
    switch (m->phase) {
    case FB_MODE_ROUND:
        if (target_step || hit((FootballTouchRect){12,132,296,26},in)) {
            int step=target_step<0 ? -1 : 1,slot=m->season.mega_slot;
            int count=m->season.roster->n_party+1;
            for (int tries=0;tries<count;++tries) {
                slot=(slot+1+step+count)%count-1;
                if (football_season_select_mega(&m->season,slot)) break;
            }
            break;
        }
        if (accept) {
            if (!football_playoff_match_begin(&m->session,&m->season)) error(m,"The next match could not be loaded.");
            else { football_playcall_reset(&m->playcall);m->phase=FB_MODE_PLAY; }
        } break;
    case FB_MODE_PLAY:
        if (football_match_waiting_for_home(&m->session.match)) {
            football_playcall_update(&m->playcall,in);
            PlayInfo play;
            if (football_playcall_take(&m->playcall,&play) && !football_match_call(&m->session.match,play)) error(m,"Playcall refused.");
        } break;
    case FB_MODE_RESULT:
        if (accept) m->phase=m->season.phase==FB_SEASON_ELIMINATED ? FB_MODE_END : FB_MODE_EVENTS;
        break;
    case FB_MODE_MESSAGE:
        if (accept || (in.pressed&FB_INPUT_BACK)) m->phase=FB_MODE_EVENTS;
        break;
    case FB_MODE_EVOLVE: {
        football_evolution_update(&m->evolution,in);
        unsigned pick;bool cancelled;
        if (football_evolution_take(&m->evolution,&pick,&cancelled))
            choose(m,(gc_choice){.kind=cancelled ? GC_CH_CANCEL : GC_CH_EVOLVE},cancelled ? 0 : pick);
        break;
    }
    case FB_MODE_LEARN: {
        unsigned count=football_mode_move_count(m);
        if (!count) { error(m,"No valid move choices are available.");break; }
        if (in.pressed&FB_INPUT_UP) m->move_choice=(m->move_choice+count-1)%count;
        else if (in.pressed&FB_INPUT_DOWN) m->move_choice=(m->move_choice+1)%count;
        bool picked=(in.pressed&FB_INPUT_ACCEPT)!=0;
        for (unsigned i=0;i<count;++i) if (hit(football_mode_move_rect(i),in)) { m->move_choice=i;picked=true;break; }
        if (in.pressed&FB_INPUT_BACK) { m->move_choice=count-1;picked=true; }
        if (picked) choose(m,(gc_choice){.kind=GC_CH_FORGET,.a=(int16_t)(m->move_choice+1==count ? -1 : (int)m->move_choice)},0);
        break;
    }
    case FB_MODE_SHOP:
        football_shop_update(&m->shop,&m->season,in,target_step);
        if (m->season.phase==FB_SEASON_READY) m->phase=FB_MODE_ROUND;
        else if (m->season.phase==FB_SEASON_CLEAR) m->phase=FB_MODE_END;
        break;
    case FB_MODE_END: case FB_MODE_ERROR:
        if (accept || (in.pressed&FB_INPUT_BACK)) m->done=true;
        break;
    default: break;
    }
}
