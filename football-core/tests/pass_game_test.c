#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
/* The pass game: every concept sends five different players somewhere different (the back is an outlet, not a fifth vertical), starts from
 * alignments that vary and mirror, play action fakes before it throws and pulls the second level up, the screen builds a wall, and the routes
 * that settle stop where they should. */
static FootballRoster roster;
static FootballLive m;
static const FootballPlayId passes[12]={FB_PASS_SLANTS,FB_PASS_PLAY_ACTION,FB_PASS_VERTICALS,FB_PASS_POST,FB_PASS_STICK,FB_PASS_MESH,FB_PASS_FLOOD,
    FB_PASS_CURLS,FB_PASS_SMASH,FB_PASS_HB_SCREEN,FB_PASS_BUBBLE,FB_PASS_SHALLOW_CROSS};
static unsigned checks;
#define CHECK(c) do{++checks;assert(c);}while(0)
static void setup(uint32_t seed,unsigned away,FootballPlayId play,FootballDefenseId defense)
{
    assert(football_live_begin(&m,&roster,seed,1));
    for(unsigned i=0;i<22;++i)m.units[0][i]=m.units[1][i];
    m.rules.offense=away?FB_AWAY:FB_HOME;m.rules.down=1;m.rules.ball=40;m.rules.first_down_at=50;m.rules.quarter=2;m.rules.seconds_left=900;
    m.autoplay=1;
    assert(football_live_call(&m,play));
    m.defense=defense;football_live_defense_setup(&m);
}
/* Run the CPU's snap (1.2 s in the set) and stop on the first frame of the play. */
static void snap(void){for(unsigned g=0;m.phase!=FB_LIVE_PLAY;++g){assert(g<400);football_live_tick(&m,16,(FootballLiveInput){.receiver=-1});}}
static bool same_path(FootballRouteView a,FootballRouteView b)
{
    if(a.count!=b.count)return false;
    for(unsigned k=0;k<a.count;++k)if(a.point[k].x!=b.point[k].x || a.point[k].y!=b.point[k].y)return false;
    return true;
}
/* Where a receiver stands in the unmirrored picture, depth past the line and y, from the alignment table alone. */
static void spot(const FootballPlayDefinition *p,const FootballAlignment *al,unsigned rank,float *d,float *y)
{
    if(rank==1){*d=-7+al->rb_back+(p->formation==FB_FORM_UNDER_CENTER?2:0);*y=26.667f+al->rb_dy;return;}
    *d=al->wr[rank-2].depth;*y=al->wr[rank-2].y;
}
/* No two receivers on one coordinate, nobody out of bounds, the back never deep, in both alignments each concept is run from. */
static void spacing(void)
{
    for(unsigned n=0;n<12;++n){
        const FootballPlayDefinition *p=football_playbook_get(passes[n]);
        for(unsigned option=0;option<2;++option){
            const FootballAlignment *al=football_playbook_alignment(football_playbook_alignment_for(p,option));
            float px[6][17],py[6][17];
            for(unsigned r=1;r<=5;++r){
                float d0,y0;spot(p,al,r,&d0,&y0);float inside=y0<26.67f?1.f:-1.f;
                FootballRouteView path=football_playbook_receiver_path(p,r);CHECK(path.count==FB_ROUTE_POINTS);
                for(unsigned k=0;k<=16;++k){FootballRoutePoint q=football_playbook_sample_path(path,k/16.0f);px[r][k]=d0+q.x;py[r][k]=y0+q.y*inside;
                    CHECK(py[r][k]>=2 && py[r][k]<=51.34f);}
                if(r==1)CHECK(px[1][16]<=8); /* the back is the outlet: a checkdown or a flat, never a fifth vertical */
            }
            for(unsigned r=1;r<=5;++r)for(unsigned s=r+1;s<=5;++s)for(unsigned k=0;k<=16;++k)
                CHECK(hypotf(px[r][k]-px[s][k],py[r][k]-py[s][k])>=1.5f);
        }
    }
    /* The table the test uses is the one the engine uses: same spots, mirrored or not. */
    for(unsigned n=0;n<12;++n)for(uint32_t seed=1;seed<=20;++seed){
        setup(seed,0,passes[n],FB_DEF_MAN);const FootballPlayDefinition *p=football_playbook_get(passes[n]);
        const FootballAlignment *al=football_playbook_alignment((FootballAlignmentId)m.align);
        for(unsigned r=1;r<=5;++r){
            float d,y;spot(p,al,r,&d,&y);if(m.flip)y=53.333f-y;
            CHECK(fabsf(m.snap_x[r]-m.los-d)<.001f && fabsf(m.snap_y[r]-y)<.001f);
        }
    }
}
/* Each concept's five routes are their own: the back, the tight end and the slot never repeat the outside receiver's route. */
static void distinct(void)
{
    for(unsigned n=0;n<12;++n){
        const FootballPlayDefinition *p=football_playbook_get(passes[n]);
        FootballRouteView path[6];for(unsigned r=1;r<=5;++r)path[r]=football_playbook_receiver_path(p,r);
        for(unsigned r=2;r<=5;++r)CHECK(!same_path(path[1],path[r]));  /* the back runs nobody else's route */
        CHECK(!same_path(path[5],path[2]) && !same_path(path[5],path[4])); /* nor does the TE */
        CHECK(!same_path(path[4],path[2]));                               /* nor the slot */
    }
    /* Concepts that used to share routes do not any more. */
    FootballRouteView a,b;
    const FootballPlayDefinition *mesh=football_playbook_get(FB_PASS_MESH),*shallow=football_playbook_get(FB_PASS_SHALLOW_CROSS);
    for(unsigned r=1;r<=5;++r)if(r!=5 && r!=1){a=football_playbook_receiver_path(mesh,r);b=football_playbook_receiver_path(shallow,r);CHECK(!same_path(a,b));}
    const FootballPlayDefinition *screen=football_playbook_get(FB_PASS_HB_SCREEN),*bubble=football_playbook_get(FB_PASS_BUBBLE);
    for(unsigned r=1;r<=5;++r){a=football_playbook_receiver_path(screen,r);b=football_playbook_receiver_path(bubble,r);CHECK(!same_path(a,b));}
    CHECK(football_playbook_primary_receiver(screen)==4 && football_playbook_primary_receiver(bubble)==2); /* the back, and WR3 */
}
/* Passes are lined up in different pictures and mirrored, the outside receivers always on opposite sides, nobody on top of anybody. */
static void alignments(void)
{
    for(unsigned n=0;n<12;++n){
        unsigned seen[FB_ALIGN_COUNT]={0},flips[2]={0};
        for(unsigned away=0;away<2;++away)for(uint32_t seed=1;seed<=100;++seed){
            setup(seed,away,passes[n],FB_DEF_MAN);unsigned off=away?11:0;
            seen[m.align]++;flips[m.flip]++;
            CHECK((m.actors[off+2].y<26.67f)!=(m.actors[off+3].y<26.67f)); /* WR1 and WR2 on opposite sides */
            CHECK(m.actors[off+2].y<12 || m.actors[off+2].y>41.5f);        /* and out wide */
            for(unsigned i=0;i<11;++i)for(unsigned j=i+1;j<11;++j)CHECK(hypotf(m.actors[off+i].x-m.actors[off+j].x,m.actors[off+i].y-m.actors[off+j].y)>FB_BODY_DIAMETER+.05f);
        }
        const FootballPlayDefinition *p=football_playbook_get(passes[n]);
        unsigned usual=football_playbook_alignment_for(p,0),alt=football_playbook_alignment_for(p,1);
        CHECK(usual!=alt && seen[usual]>seen[alt] && seen[alt]>30); /* its usual picture most of the time, the other a third of it */
        CHECK(flips[0]>60 && flips[1]>60);
        for(unsigned a=0;a<FB_ALIGN_COUNT;++a)if(a!=usual && a!=alt)CHECK(seen[a]==0);
    }
}
/* Play action: the back goes to the QB, the linebackers (zone) and safeties step up to meet it, then they recover, and the throw waits for the
 * routes. Concepts without a fake leave the second level alone. */
static void play_action(void)
{
    CHECK(football_playbook_fake(football_playbook_get(FB_PASS_PLAY_ACTION)) && football_playbook_fake(football_playbook_get(FB_PASS_POST)));
    CHECK(!football_playbook_fake(football_playbook_get(FB_PASS_SLANTS)) && !football_playbook_fake(football_playbook_get(FB_RUN_DIVE)));
    for(unsigned away=0;away<2;++away)for(uint32_t seed=1;seed<=6;++seed){
        for(unsigned n=0;n<3;++n){
            /* Two fakes, and a concept without one (Slants) as the control. The same zone defense reads the same field at the fake (.3 s)
             * and after it (.7 s); only the fake can make the difference. */
            FootballPlayId id=n==0?FB_PASS_PLAY_ACTION:n==1?FB_PASS_POST:FB_PASS_SLANTS;bool fakes=n<2;
            for(unsigned deep_receivers=0;deep_receivers<2;++deep_receivers){
                setup(seed,away,id,FB_DEF_ZONE);unsigned off=away?11:0,def=11-off;float dir=away?-1:1;
                snap();
                /* Receivers 20 yards deep give the linebackers no one to cover in their zones; at the line they are all that the safeties are
                 * not looking at. Either way the only thing left to move a defender is the fake. */
                for(unsigned r=1;r<=5;++r)m.actors[off+r].x=m.los+dir*(deep_receivers?20.f:-1.f);
                float fx[11],fy;
                m.elapsed=.3f;CHECK(football_live_fake_active(&m)==fakes);
                for(unsigned r=0;r<11;++r){football_live_defense_intent(&m,def+r,&fx[r],&fy);}
                m.elapsed=FB_LIVE_FAKE+.05f;CHECK(!football_live_fake_active(&m));
                for(unsigned r=0;r<11;++r){
                    float x,y;football_live_defense_intent(&m,def+r,&x,&y);
                    float step=(fx[r]-x)*dir; /* how much further toward the line the fake sends him */
                    if(!fakes || r<4 || r==7 || r==8)CHECK(fabsf(step)<.001f);          /* line, corners, and every defender on a concept with no fake */
                    else if(r<=6 && deep_receivers)CHECK(step<-3.f);                   /* linebackers: the whole step up when nothing else moves them */
                    else if(r>=9 && !deep_receivers)CHECK(step<-1.5f);                      /* safeties: half as far */
                }
            }
            /* Once the ball is in the air, or the QB runs, there is no fake. */
            setup(seed,away,id,FB_DEF_ZONE);snap();
            m.elapsed=.3f;m.flying=true;CHECK(!football_live_fake_active(&m));m.flying=false;
            m.scramble=true;CHECK(!football_live_fake_active(&m));m.scramble=false;
        }
        /* The back's route is the fake: through the mesh point beside the QB, then out to his outlet. */
        setup(seed,away,FB_PASS_PLAY_ACTION,FB_DEF_MAN);unsigned off=away?11:0,def=11-off;
        for(unsigned d=def;d<def+11;++d){m.actors[d].x=away?3:117;m.actors[d].stun=30;}
        snap();float best=99,fx,fy,after=0;
        for(unsigned f=0;f<110 && m.phase==FB_LIVE_PLAY;++f){
            football_live_tick(&m,17,(FootballLiveInput){.receiver=-1});
            football_live_fake_point(&m,&fx,&fy);
            if(m.elapsed<FB_LIVE_FAKE+.05f)best=fminf(best,hypotf(m.actors[off+1].x-fx,m.actors[off+1].y-fy));
            if(m.elapsed>1.5f && after==0)after=hypotf(m.actors[off+1].x-m.actors[off].x,m.actors[off+1].y-m.actors[off].y);
        }
        CHECK(best<1.f);        /* his route takes him through the mesh point beside the QB */
        CHECK(after>3.f);       /* and then left it for his route */
    }
    /* The QB gives the fake time before he throws; a screen waits for the wall, a bubble does not. */
    CHECK(football_playbook_window(football_playbook_get(FB_PASS_BUBBLE))<football_playbook_window(football_playbook_get(FB_PASS_SLANTS)) + .5f);
    CHECK(football_playbook_window(football_playbook_get(FB_PASS_HB_SCREEN))>football_playbook_window(football_playbook_get(FB_PASS_BUBBLE))+.5f);
}
/* HB Screen: the rush is let in, then three linemen leave their men and set a wall ahead of the back, on his side. Every other pass keeps
 * its linemen in the pocket. */
static void screen_wall(void)
{
    unsigned measured=0;
    for(unsigned away=0;away<2;++away)for(uint32_t seed=1;seed<=10;++seed){
        for(unsigned control=0;control<2;++control){
            setup(seed,away,control?FB_PASS_SLANTS:FB_PASS_HB_SCREEN,FB_DEF_MAN);unsigned off=away?11:0,def=11-off;float dir=away?-1:1;
            for(unsigned d=def;d<def+11;++d){m.actors[d].x=away?3:117;m.actors[d].stun=30;}
            snap();
            unsigned wall=0;float wx,wy;
            for(unsigned r=6;r<=10;++r){m.elapsed=FB_LIVE_SCREEN_RELEASE-.1f;CHECK(!football_live_screen_wall(&m,r,&wx,&wy));}
            m.elapsed=FB_LIVE_SCREEN_RELEASE+.1f;for(unsigned r=6;r<=10;++r)wall+=football_live_screen_wall(&m,r,&wx,&wy);
            CHECK(wall==(control?0u:3u));
            m.elapsed=.0f;
            /* Played out with no one rushing: where are the linemen when the ball is thrown? */
            unsigned out=0,toward=0;
            for(unsigned f=0;f<82 && m.phase==FB_LIVE_PLAY;++f)football_live_tick(&m,17,(FootballLiveInput){.receiver=-1}); /* 1.4 s: the screen is not thrown before 1.45 */
            if(m.phase!=FB_LIVE_PLAY)continue;
            float end=m.snap_y[1]+football_playbook_receiver_path(football_playbook_get(m.play),1).point[3].y*(m.snap_y[1]<26.67f?1.f:-1.f);
            for(unsigned r=6;r<=10;++r){
                const FootballLiveActor *a=&m.actors[off+r];
                if(hypotf(a->x-m.snap_x[r],a->y-m.snap_y[r])>1.5f)++out;
                if(fabsf(a->y-end)<fabsf(m.snap_y[r]-end)-1.f && (a->x-m.los)*dir>1.f)++toward;
            }
            ++measured;
            if(control)CHECK(out==0);
            else CHECK(out>=3 && toward>=3);
        }
    }
    CHECK(measured>=30);
}
/* Routes that settle stop on their last point, or at the back of the end zone from a goal-line snap; nobody runs out of it. */
static void settles(void)
{
    unsigned settlers=0;
    for(unsigned n=0;n<12;++n){
        const FootballPlayDefinition *p=football_playbook_get(passes[n]);if(!football_playbook_settles(p))continue;++settlers;
        for(unsigned away=0;away<2;++away)for(uint32_t seed=1;seed<=4;++seed){
            assert(football_live_begin(&m,&roster,seed,1));
            for(unsigned i=0;i<22;++i)m.units[0][i]=m.units[1][i];
            m.rules.offense=away?FB_AWAY:FB_HOME;m.rules.down=1;m.rules.ball=97;m.rules.first_down_at=100;m.autoplay=1;
            assert(football_live_call(&m,passes[n]));m.defense=FB_DEF_MAN;football_live_defense_setup(&m);
            unsigned off=away?11:0,def=11-off;float dir=away?-1:1;
            for(unsigned d=def;d<def+11;++d){m.actors[d].x=away?117:3;m.actors[d].y=2+(d-def)*4;m.actors[d].stun=30;}
            snap();m.thrown=true; /* the QB keeps the ball, so the routes run out */
            float farthest=0;
            for(unsigned f=0;f<500 && m.phase==FB_LIVE_PLAY;++f){
                football_live_tick(&m,17,(FootballLiveInput){.receiver=-1});
                for(unsigned r=1;r<=5;++r)farthest=fmaxf(farthest,(m.actors[off+r].x-m.los)*dir);
            }
            CHECK(m.elapsed>6.f);
            CHECK(farthest>=3.f && farthest<=10.f); /* some route ran into the end zone, none past the back line (116.5 or 3.5: 9.5 yards from the 3) */
            for(unsigned r=1;r<=5;++r)CHECK(fabsf(m.actors[off+r].vx)<.3f && fabsf(m.actors[off+r].vy)<.3f);
        }
    }
    CHECK(settlers>=6);
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){70,70,70,70,70,70,70,70};
    spacing();distinct();alignments();play_action();screen_wall();settles();
    printf("pass game: %u checks (routes spaced and in bounds in both alignments, five different routes per concept, mirrored alignments, play-action fake and bite, screen wall, settling routes) passed\n",checks);
}
