#include "football_screen.h"
#include "football_live_art.h"
#include "football_shop_render.h"
#include "save.h"
#include "ui.h"
#include "menu_fakes.h"
#include <3ds.h>
#include <assert.h>
#include "football_profile_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern const FootballCareer *football_live_screen_test_career(void);
static ui_screen *top,*game_screen,title={.name="title"},overlay={.name="overlay"};
static float speed=2;static u32 down,held;static circlePosition circle;
static void (*answer_fn)(int,void*);static void *answer_arg;
static unsigned writes,loads,draws;static bool write_ok=true;static size_t length;static unsigned char bytes[FB_CAREER_SAVE_MAX];
static const FootballCareer *career(void){const FootballCareer *c=football_live_screen_test_career();assert(c);return c;}
u32 hidKeysDown(void){return down;}u32 hidKeysHeld(void){return held;}
void hidCircleRead(circlePosition *p){*p=circle;}
bool ui_busy(void){return false;}bool ui_input_repeating(void){return false;}float ui_game_speed(void){return speed;}void ui_set_game_speed(float v){speed=v;}
ui_screen *ui_top(void){return top;}
void ui_set(ui_screen *s){ui_screen *old=top;top=s;if(old && old->leave)old->leave(old);if(s->enter)s->enter(s);}
void ui_push_options(const char *const *o,int count,bool cancel,void(*done)(int,void*),void *arg){assert(o && count>0 && cancel);top=&overlay;answer_fn=done;answer_arg=arg;}
void C3D_FrameSync(void){}
bool C2D_DrawRectSolid(float x,float y,float z,float w,float h,u32 c){(void)x;(void)y;(void)z;(void)c;assert(w>0 && h>0);return true;}
void C2D_TextBufClear(C2D_TextBuf b){assert(b);}
void C2D_TextParse(C2D_Text *t,C2D_TextBuf b,const char *s){(void)t;assert(b && s);}
void C2D_TextOptimize(C2D_Text *t){(void)t;}
void C2D_DrawText(const C2D_Text *t,unsigned flags,float x,float y,float z,float sx,float sy,u32 c){(void)t;(void)flags;(void)x;(void)y;(void)z;(void)sx;(void)sy;(void)c;}
bool football_render_init(FootballRenderer *r){r->text=r;return true;}
void football_render_free(FootballRenderer *r){r->text=NULL;}
void football_render_clear_sprites(FootballRenderer *r){assert(r->text);}
bool football_render_load_front(FootballRenderer *r,unsigned slot,const menu_mon *m){assert(r->text && slot<22 && m->species);return true;}
bool football_render_load_compact(FootballRenderer *r,unsigned slot,const menu_mon *m){assert(r->text && slot<22 && m->species);++loads;return true;}
const rr_frame *football_render_front_frame(const FootballRenderer *r,unsigned slot,uint32_t ms){(void)r;(void)slot;(void)ms;return NULL;}
bool football_render_draw_sprite(const rr_frame *f,float x,float y,float scale,bool left){(void)f;(void)x;(void)y;(void)scale;(void)left;return false;}
void football_live_art_top(FootballRenderer *r,const FootballCareer *c,const FootballCamera *cam,unsigned ms){(void)r;(void)c;(void)cam;(void)ms;++draws;}
void football_live_art_card(FootballRenderer *r,FootballPlayId p,float x,float y,float w,float h,bool selected){(void)r;(void)p;(void)x;(void)y;(void)w;(void)h;(void)selected;}
static unsigned special_cards;
const char *football_live_art_special_name(FootballSpecial k){return k==FB_SPECIAL_PUNT?"PUNT":"KICK";}
void football_live_art_special_card(FootballRenderer *r,FootballSpecial k,const char *detail,float x,float y,float w,float h,bool selected,bool enabled){(void)r;(void)k;(void)x;(void)y;(void)w;(void)h;(void)selected;(void)enabled;assert(detail && detail[0]);++special_cards;}
void football_live_art_defense_card(FootballRenderer *r,FootballDefenseId d,float x,float y,float w,float h,bool selected){(void)r;(void)d;(void)x;(void)y;(void)w;(void)h;(void)selected;}
bool football_shop_prepare(FootballShopView *v,const FootballSeason *s){(void)v;assert(s->phase==FB_SEASON_SHOP);return true;}
bool football_legacy_screen_open(const football_screen_args *args){(void)args;return false;}
save_result save_write(const char *file,const void *b,size_t n){assert(!strcmp(file,"football_live"));++writes;if(!write_ok)return SAVE_IO_ERROR;assert(n<sizeof bytes);memcpy(bytes,b,n);length=n;return SAVE_OK;}
save_result save_read(const char *file,char **b,size_t *n,bool *backup){assert(!strcmp(file,"football_live"));*backup=false;if(!length)return SAVE_NOT_FOUND;*b=malloc(length);assert(*b);memcpy(*b,bytes,length);*n=length;return SAVE_OK;}
static void exited(void *arg){(void)arg;ui_set(&title);}
static void press(ui_button button){assert(top->input);top->input(top,button);}
/* Both screens drawn onto the recording fakes: nothing runs off a screen, every text sits in a window and clear of the others. */
static void draw_both(void){fake_begin(RR_TOP);game_screen->draw(game_screen,RR_TOP);fake_check();fake_begin(RR_BOTTOM);game_screen->draw(game_screen,RR_BOTTOM);fake_check();}
static void tick(void){game_screen->update(game_screen,17);if(top==game_screen)draw_both();down=0;}
static void answer(int n){top=game_screen;answer_fn(n,answer_arg);}
static gc_system profile,locked_profile;
int main(void)
{
    football_test_unlock_all(&profile);
    football_screen_args args={.profile=&profile,.entropy=5,.persistence=true,.on_exit=exited};top=&title;
    gc_system_init_defaults(&locked_profile);args.profile=&locked_profile;
    assert(football_screen_open(&args));game_screen=top;draw_both();
    for(unsigned i=0;i<25;++i){press(BTN_ACTION);press(BTN_DOWN);}
    assert(career()->phase==FB_CAREER_DRAFT && career()->roster.count<22 && writes==0);
    press(BTN_MENU);answer(4);assert(top==&title);args.profile=&profile;
    assert(football_screen_open(&args));game_screen=top;assert(speed==1);
    /* The draft list pages with Left/Right and draws on every page; paging back and forth returns to the first row. */
    game_screen->update(game_screen,17);draw_both();for(unsigned i=0;i<3;++i){press(BTN_RIGHT);draw_both();}for(unsigned i=0;i<3;++i)press(BTN_LEFT);draw_both();
    for(unsigned i=0;i<22;++i){press(BTN_ACTION);if(i<21)press(BTN_DOWN);}
    assert(career()->phase==FB_CAREER_LINEUP && career()->roster.count==22 && career()->roster.cost<=55);tick();assert(writes==1);
    uint8_t old=career()->roster.lineup[0];press(BTN_ACTION);draw_both();press(BTN_RIGHT);draw_both();assert(career()->roster.lineup[0]!=(unsigned)old);press(BTN_CANCEL);press(BTN_CANCEL);
    assert(career()->phase==FB_CAREER_READY);press(BTN_ACTION);tick();assert(career()->phase==FB_CAREER_MATCH && career()->live.phase==FB_LIVE_CALL && loads==22);
    down=KEY_A;press(BTN_ACTION);tick();assert(career()->live.phase==FB_LIVE_SET); /* play selection must not also snap */
    for(unsigned i=0;i<30;++i){tick();}
    assert(career()->live.phase==FB_LIVE_SET);
    down=KEY_A;tick();assert(career()->live.phase==FB_LIVE_PLAY);
    float y=career()->live.actors[career()->live.controlled].y;circle.dy=156;
    for(unsigned i=0;i<10;++i){tick();}
    assert(career()->live.actors[career()->live.controlled].y<y);circle.dy=0;
    unsigned steps=career()->live.steps;press(BTN_MENU);tick();assert(career()->live.steps==steps);answer(0);
    /* Returning mid-play preserves the last safe checkpoint; resume opens a playcall. */
    press(BTN_MENU);answer(4);assert(top==&title && speed==2);
    assert(football_screen_open(&args));game_screen=top;assert(career()->phase==FB_CAREER_MATCH && career()->live.phase==FB_LIVE_CALL);tick();assert(draws>0);
    /* Species candy waits for a checkpoint: a failed write leaves it pending, the saved one hands it to the profile. */
    {
        FootballCareer *w=(FootballCareer *)career();uint16_t root=gc_species_root_for_starter(w->roster.mon[3].b.species);int32_t had=profile.starters[root].candy_count;
        w->candy_pending[3]=7;
        write_ok=false;press(BTN_MENU);answer(1);assert(top==game_screen && w->candy_pending[3]==7 && profile.starters[root].candy_count==had);
        write_ok=true;press(BTN_MENU);answer(1);assert(top==&title && !w->candy_pending[3] && profile.starters[root].candy_count==had+7);
    }
    /* Special teams are the third page of the playcall: a punt is always there, a field goal only from inside range. */
    assert(football_screen_open(&args));game_screen=top;assert(career()->live.phase==FB_LIVE_CALL && career()->live.rules.offense==FB_HOME);
    tick();press(BTN_RIGHT);press(BTN_RIGHT);tick();assert(special_cards==2);
    press(BTN_DOWN);press(BTN_ACTION);tick();assert(career()->live.phase==FB_LIVE_CALL && !career()->live.special); /* out of range from the 25 */
    press(BTN_UP);down=KEY_A;press(BTN_ACTION);tick();
    assert(career()->live.phase==FB_LIVE_SET && career()->live.special==FB_SPECIAL_PUNT && career()->live.controlled==0); /* the same press does not also snap */
    for(unsigned i=0;i<30;++i){tick();}
    assert(career()->live.phase==FB_LIVE_SET);
    down=KEY_A;tick();assert(career()->live.phase==FB_LIVE_PLAY);
    for(unsigned i=0;i<3000 && career()->live.phase==FB_LIVE_PLAY;++i)tick();
    assert(career()->live.phase==FB_LIVE_DEAD && career()->live.rules.offense==FB_AWAY && career()->live.event!=FB_EVENT_NONE);
    /* The try after a touchdown: two cards, no page flipping, and either choice leaves the call without also snapping. */
    {
        FootballCareer *w=(FootballCareer *)career();
        w->live.phase=FB_LIVE_CALL;w->live.special=FB_SPECIAL_NONE;w->live.rules.offense=FB_HOME;w->live.rules.ball=96;w->live.rules.down=1;w->live.rules.first_down_at=100;
        w->live.conversion=1;w->live.next_offense=FB_AWAY;w->live.after_side=FB_HOME;
        unsigned cards=special_cards;tick();assert(special_cards==cards+2);
        press(BTN_RIGHT);tick();assert(career()->live.conversion==1 && special_cards==cards+4); /* no second page */
        press(BTN_DOWN);press(BTN_ACTION);tick();
        assert(career()->live.conversion==2 && career()->live.phase==FB_LIVE_CALL && !career()->live.special); /* go for two: pick a play next */
        press(BTN_RIGHT);press(BTN_RIGHT);press(BTN_RIGHT);tick();assert(special_cards==cards+4); /* only the two pages of plays */
        press(BTN_ACTION);tick();assert(career()->live.phase==FB_LIVE_SET && career()->live.conversion==2);
        w->live.phase=FB_LIVE_CALL;w->live.special=FB_SPECIAL_NONE;w->live.conversion=1;
        down=KEY_A;press(BTN_ACTION);tick();
        assert(career()->live.phase==FB_LIVE_SET && career()->live.special==FB_SPECIAL_EXTRA_POINT && career()->live.rules.ball==85);
    }
    /* Every other menu, drawn: the result, the experience, evolution, shop and end screens, the checkpoint error, and a failed save on top. */
    {
        FootballCareer *w=(FootballCareer *)career();
        static const FootballCareerPhase phases[]={FB_CAREER_DRAFT,FB_CAREER_LINEUP,FB_CAREER_READY,FB_CAREER_RESULT,FB_CAREER_EXP,FB_CAREER_EVOLVE,FB_CAREER_SHOP,FB_CAREER_END,FB_CAREER_ERROR};
        w->evolution_count=3;w->evolutions[0]=2;w->evolutions[1]=3;w->evolutions[2]=6;w->wallet=1234;w->round=5;w->wins=4;w->mega=2;
        for(unsigned i=0;i<sizeof phases/sizeof phases[0];++i){
            w->phase=phases[i];
            if(phases[i]==FB_CAREER_DRAFT)snprintf(w->message,sizeof w->message,"Over budget: 55.00 points at most.");
            if(phases[i]==FB_CAREER_ERROR)snprintf(w->message,sizeof w->message,"The checkpoint could not be read: it is kept as it is, and a new playoff can be started from the title screen.");
            draw_both();
            if(phases[i]==FB_CAREER_RESULT){w->live.rules.winner=w->live.rules.winner==FB_HOME?FB_AWAY:FB_HOME;draw_both();} /* a win and a loss */
        }
        w->phase=FB_CAREER_SHOP;press(BTN_RIGHT);press(BTN_DOWN);draw_both();
        w->message[0]=0;w->mega=-1;w->phase=FB_CAREER_READY;draw_both();
    }
    {
        static const char *const seen[]={"Draft  ","Lineup","Pick a member","Playoffs","Round 5 / 7","Choose your play","Special teams","Go for two","Touchdown!","Live play","Match won","Playoff elimination","Experience","Choose an evolution","Shop","Cash 1234","Playoff complete","Checkpoint preserved","Species candy","Mega evolution","Tap the map","Circle Pad"};
        for(unsigned i=0;i<sizeof seen/sizeof seen[0];++i)if(!fake_saw(seen[i]))fprintf(stderr,"never drawn: %s\n",seen[i]);
        for(unsigned i=0;i<sizeof seen/sizeof seen[0];++i)assert(fake_saw(seen[i]));
    }
    press(BTN_MENU);answer(4);assert(top==&title);
    puts("football live screen: real 22-player draft, assignment, playcall/snap separation, held movement, pause, save/resume and failed-write retry passed");
}
