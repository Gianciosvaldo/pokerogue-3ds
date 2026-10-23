#include "football_match_view.h"
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
    for (unsigned side=0;side<2;++side) for (unsigned i=0;i<m->teams[side].count;++i) {
        FootballRenderActor *a=&scene.actors[scene.actor_count++];
        a->front_frame=football_render_front_frame(renderer,side*FB_MATCH_TEAM_CAP+i,animation_ms);
        bool offense=side==m->rules.offense;
        float spacing=offense && i==qb && m->play.formation==FB_FORM_SHOTGUN ? 5 : 2+(float)(i/3);
        a->x_yards=ball+direction*(offense ? -spacing : spacing);
        a->y_yards=8+8*(float)i;
        a->away=side==FB_AWAY;
        if (progress>0) {
            float travel=(float)m->resolved.yards*progress;
            if (offense && i!=m->carrier) travel*=.35f;
            a->x_yards+=direction*travel;
            a->movement_x=direction*travel;
        }
        a->x_yards=clamp(a->x_yards,0,120);
    }
    *out=scene; return true;
}
