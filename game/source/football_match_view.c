#include "football_match_view.h"
#include "football_playbook.h"
#include <string.h>
static float clamp(float v,float low,float high) { return v<low ? low : v>high ? high : v; }
bool football_match_scene(FootballRenderScene *out,const FootballMatch *m,
                           const FootballRenderer *renderer,uint32_t animation_ms)
{
    if (!out || !m || !renderer || m->rules.offense>FB_AWAY ||
        !m->teams[0].count || m->teams[0].count>FB_MATCH_TEAM_CAP ||
        !m->teams[1].count || m->teams[1].count>FB_MATCH_TEAM_CAP) return false;
    FootballRenderScene scene={0};
    scene.home_score=m->rules.score[0]; scene.away_score=m->rules.score[1];
    scene.seconds_left=m->rules.seconds_left;
    scene.down=m->rules.down; scene.distance=(uint8_t)football_rules_distance(&m->rules);
    unsigned drives=m->rules.overtime_drives;
    if (m->rules.finished && drives) --drives;
    scene.quarter=m->rules.quarter<5 ? m->rules.quarter : (uint8_t)(5+drives/2);
    float ball=m->rules.offense==FB_HOME ? 10+m->rules.ball : 110-m->rules.ball;
    scene.ball_x_yards=ball;
    float direction=m->rules.offense==FB_HOME ? 1 : -1;
    float progress=m->phase==FB_MATCH_ANIMATION ? clamp(m->animation_ms/(float)FB_PLAY_ANIMATION_MS,0,1) : 0;
    unsigned qb=football_match_quarterback(m,(FootballSide)m->rules.offense);
    unsigned carrier=m->carrier<m->teams[m->rules.offense].count ? m->carrier : qb;
    const FootballPlayDefinition *play=football_playbook_get(m->play.id);
    bool passing=play && play->type==FB_PLAY_PASS;
    bool sack=m->phase==FB_MATCH_ANIMATION && m->resolved.kind==FB_RESULT_SACK;
    FootballRoutePoint route=football_playbook_route(play,progress);
    FootballRoutePoint end=football_playbook_route(play,1);
    FootballRoutePoint previous=football_playbook_route(play,progress-.01f);
    float carrier_x=ball,carrier_y=26.666667f,qb_x=ball,qb_y=26.666667f;
    for (unsigned side=0;side<2;++side) {
        unsigned rank=0;
        for (unsigned i=0;i<m->teams[side].count;++i) {
            FootballRenderActor *a=&scene.actors[scene.actor_count++];
            a->front_frame=football_render_front_frame(renderer,side*FB_MATCH_TEAM_CAP+i,animation_ms);
            bool offense=side==m->rules.offense;
            float depth,lateral;
            if (offense && i==qb) {
                depth=m->play.formation==FB_FORM_SHOTGUN ? -5 : -1.5f;
                lateral=26.666667f;
            } else if (offense && i==carrier) {
                depth=passing ? -1 : -6;
                lateral=passing ? 9 : 28;
            } else {
                unsigned lane=rank++;
                depth=offense ? -1 : 2+(float)(lane/3)*3;
                lateral=10+8*(float)lane;
            }
            a->x_yards=ball+direction*depth;
            a->y_yards=lateral;
            a->away=side==FB_AWAY;
            float travel=0;
            if (progress>0) {
                if (offense && i==carrier) {
                    if (sack) travel=(m->resolved.yards-depth)*progress;
                    else {
                        float fraction=end.x!=0 ? route.x/end.x : progress;
                        travel=(m->resolved.yards-depth)*fraction;
                        a->y_yards+=route.y*(lateral>26.666667f ? -1 : 1);
                    }
                } else if (offense && i==qb) travel=passing ? -2*progress : progress;
                else if (offense) {
                    travel=(passing ? 10 : 3)*progress;
                    a->y_yards+=(26.666667f-lateral)*.15f*progress;
                } else {
                    /* Defenders close toward the resolved spot, never the RNG. */
                    travel=(m->resolved.yards-depth)*progress;
                    float target_y=passing ? 9+end.y : 28-end.y;
                    a->y_yards+=(clamp(target_y,1,52)-lateral)*progress;
                }
            }
            a->x_yards=clamp(a->x_yards+direction*travel,0,120);
            a->y_yards=clamp(a->y_yards,1,52);
            a->movement_x=direction*travel;
            if (progress>0 && offense && i==carrier && !sack && end.x!=0)
                a->movement_x=direction*(m->resolved.yards-depth)*(route.x-previous.x)/end.x;
            if (offense && i==carrier) { carrier_x=a->x_yards;carrier_y=a->y_yards; }
            if (offense && i==qb) { qb_x=a->x_yards;qb_y=a->y_yards; }
        }
    }
    scene.football_visible=true;
    scene.football_x_yards=qb_x;scene.football_y_yards=qb_y;
    if (progress>0 && !sack) {
        float flight=passing ? clamp((progress-.3f)/.5f,0,1) : clamp(progress/.25f,0,1);
        scene.football_x_yards=qb_x+(carrier_x-qb_x)*flight;
        scene.football_y_yards=qb_y+(carrier_y-qb_y)*flight;
        scene.football_height=passing ? 4*flight*(1-flight) : 0;
    }
    *out=scene; return true;
}
