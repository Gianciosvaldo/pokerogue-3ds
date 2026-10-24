#include "football_screen.h"
#include "football_live_art.h"
#include "football_shop_render.h"
#include "save.h"
#include "ui.h"
#include <3ds.h>
#include <assert.h>
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
bool ui_space(ui_space_id id){(void)id;return true;}
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
void football_live_art_defense_card(FootballRenderer *r,FootballDefenseId d,float x,float y,float w,float h,bool selected){(void)r;(void)d;(void)x;(void)y;(void)w;(void)h;(void)selected;}
bool football_shop_prepare(FootballShopView *v,const FootballSeason *s){(void)v;assert(s->phase==FB_SEASON_SHOP);return true;}
bool football_legacy_screen_open(const football_screen_args *args){(void)args;return false;}
save_result save_write(const char *file,const void *b,size_t n){assert(!strcmp(file,"football_live"));++writes;if(!write_ok)return SAVE_IO_ERROR;assert(n<sizeof bytes);memcpy(bytes,b,n);length=n;return SAVE_OK;}
save_result save_read(const char *file,char **b,size_t *n,bool *backup){assert(!strcmp(file,"football_live"));*backup=false;if(!length)return SAVE_NOT_FOUND;*b=malloc(length);assert(*b);memcpy(*b,bytes,length);*n=length;return SAVE_OK;}
static void exited(void *arg){(void)arg;ui_set(&title);}
static void press(ui_button button){assert(top->input);top->input(top,button);}
static void tick(void){game_screen->update(game_screen,17);if(top==game_screen){top->draw(top,RR_TOP);top->draw(top,RR_BOTTOM);}down=0;}
static void answer(int n){top=game_screen;answer_fn(n,answer_arg);}
int main(void)
{
    football_screen_args args={.entropy=5,.persistence=true,.on_exit=exited};top=&title;
    assert(football_screen_open(&args));game_screen=top;assert(speed==1);
    for(unsigned i=0;i<22;++i){press(BTN_ACTION);if(i<21)press(BTN_DOWN);}
    assert(career()->phase==FB_CAREER_LINEUP && career()->roster.count==22 && career()->roster.cost<=40);tick();assert(writes==1);
    uint8_t old=career()->roster.lineup[0];press(BTN_ACTION);press(BTN_RIGHT);assert(career()->roster.lineup[0]!=(unsigned)old);press(BTN_CANCEL);press(BTN_CANCEL);
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
    write_ok=false;press(BTN_MENU);answer(1);assert(top==game_screen);write_ok=true;press(BTN_MENU);answer(1);assert(top==&title);
    puts("football live screen: real 22-player draft, assignment, playcall/snap separation, held movement, pause, save/resume and failed-write retry passed");
}
