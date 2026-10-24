#include "football_screen.h"
#include "football_saves.h"
#include "football_match_view.h"
#include "football_shop_render.h"
#include "starter_host.h"
#include "ui.h"
#include "ui_widgets.h"
#include <3ds.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FootballFileStatus file_status=FB_FILE_EMPTY;
static bool file_write_ok=true;
static unsigned file_writes,file_clears;
static unsigned char file_bytes[FB_SAVE_MAX];static size_t file_length;
FootballFileStatus football_saves_load(FootballMode *m,gc_run *r,gc_system *p,bool *b){*b=false;if(file_status==FB_FILE_OK)return football_save_decode(m,r,p,file_bytes,file_length)?FB_FILE_OK:FB_FILE_BAD;return file_status;}
bool football_saves_write(const FootballMode *m){++file_writes;if(!file_write_ok)return false;file_length=football_save_encode(m,file_bytes,sizeof file_bytes);assert(file_length);file_status=FB_FILE_OK;return true;}
bool football_saves_clear(void){++file_clears;file_status=FB_FILE_EMPTY;return true;}
static ui_screen *top,*playoff;
static ui_screen starter={.name="fake-starter"},title={.name="fake-title"},overlay={.name="options"};
static sh_events draft;
static uint32_t frame;
static float speed=2;
static bool busy,repeating,save_ok=true,render_ok=true;
static unsigned exits,saves,syncs,loads,clears,prepared,top_draws,bottom_draws;
static void (*options_done)(int,void*);
static void *options_arg;
static char option_label[96],notice[180];
static football_screen_args config;
u32 hidKeysDown(void){return 0;}
void hidTouchRead(touchPosition *p){p->px=p->py=0;}
void C3D_FrameSync(void){++syncs;}
uint32_t ui_frame(void){return frame;}
float ui_game_speed(void){return speed;}
void ui_set_game_speed(float v){speed=v;}
bool ui_busy(void){return busy;}
bool ui_input_repeating(void){return repeating;}
ui_screen *ui_top(void){return top;}
void ui_set(ui_screen *s){ui_screen *old=top;top=s;if(old&&old->leave)old->leave(old);if(s->enter)s->enter(s);}
void ui_push_options(const char *const *labels,int n,bool cancel,void(*done)(int,void*),void *arg)
{assert(n>=1&&cancel);snprintf(option_label,sizeof option_label,"%s",labels[n-1]);options_done=done;options_arg=arg;top=&overlay;}
static void answer(int option){top=playoff;void(*done)(int,void*)=options_done;options_done=NULL;if(done)done(option,options_arg);}
bool ui_space(ui_space_id s){(void)s;return true;}
void rr_rect(float x,float y,float w,float h,rr_color c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void rr_text_sys(float x,float y,float scale,const char *s,rr_color c){(void)x;(void)y;(void)scale;(void)c;assert(s);}
bool sh_open(gc_system *profile,const char *text,const sh_events *ev)
{(void)profile;draft=*ev;snprintf(notice,sizeof notice,"%s",text);top=&starter;return true;}
bool football_render_init(FootballRenderer *r){if(!render_ok)return false;r->text=r;return true;}
void football_render_clear_sprites(FootballRenderer *r){(void)r;++clears;}
void football_render_free(FootballRenderer *r){r->text=NULL;}
bool football_render_load_front(FootballRenderer *r,unsigned slot,const menu_mon *mon)
{assert(r->text&&slot<12&&mon->species);++loads;return true;}
void football_render_top(FootballRenderer *r,const FootballRenderScene *s){(void)r;assert(s);++top_draws;}
void football_render_bottom(FootballRenderer *r,const FootballPlaycall *ui){(void)r;assert(ui);++bottom_draws;}
void football_render_defense_bottom(FootballRenderer *r,const FootballDefenseMenu *m){(void)r;assert(m);++bottom_draws;}
void football_render_evolution_bottom(FootballRenderer *r,const FootballEvolutionMenu *m){(void)r;assert(m->active);++bottom_draws;}
bool football_shop_prepare(FootballShopView *v,const FootballSeason *s){(void)v;assert(s->phase==FB_SEASON_SHOP);++prepared;return true;}
void football_shop_draw(FootballRenderer *r,const FootballShopView *v,const FootballSeason *s,const FootballShopMenu *m)
{(void)r;(void)v;(void)m;assert(s->phase==FB_SEASON_SHOP);++bottom_draws;}
bool football_match_scene(FootballRenderScene *out,const FootballMatch *m,const FootballRenderer *r,uint32_t ms)
{(void)r;(void)ms;out->home_score=m->rules.score[0];out->away_score=m->rules.score[1];return true;}
static bool save(void *arg){assert(arg==&config);++saves;return save_ok;}
static void exit_mode(void *arg){assert(arg==&config);++exits;ui_set(&title);}
static const FootballMode *mode(void){const FootballMode *m=football_screen_test_mode();assert(m);return m;}
static void press(ui_button b){++frame;assert(top->input);top->input(top,b);}
static void tap(unsigned x,unsigned y){++frame;assert(top->touch);top->touch(top,(float)x,(float)y);}
static void tick(void){++frame;if(playoff&&playoff->update)playoff->update(playoff,50);if(football_screen_test_mode()){playoff->draw(playoff,RR_TOP);playoff->draw(playoff,RR_BOTTOM);}}
static void begin(void)
{
    config=(football_screen_args){.entropy=123,.on_exit=exit_mode,.arg=&config,.save_profile=save};
    top=&title;assert(football_legacy_screen_open(&config));assert(top==&starter&&speed==1);
    assert(strstr(notice,"cannot be saved"));
    gc_starter bad={.species=SPECIES_ID_IVYSAUR,.tera_type=-1};const char *why=NULL;
    assert(draft.validate_team&&!draft.validate_team(&bad,1,draft.arg,&why)&&why);
    gc_starter team={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};
    assert(draft.validate_team(&team,1,draft.arg,&why));draft.on_start(&team,1,draft.arg);
    playoff=top;assert(!strcmp(top->name,"football-playoff"));
}
static void win(void)
{
    press(BTN_ACTION);assert(mode()->phase==FB_MODE_PLAY);
    FootballMatch *m=(FootballMatch*)&mode()->session.match;
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<m->teams[s].count;++i){uint8_t r=s==FB_HOME?99:1;m->teams[s].players[i].attributes=(FootballAttributes){r,r,r,r,r,r,r,r};}
    unsigned guard=0;
    while(mode()->phase!=FB_MODE_SHOP && ++guard<10000){
        FootballModePhase phase=mode()->phase;
        if(phase==FB_MODE_PLAY&&m->phase==FB_MATCH_CALL)press(BTN_ACTION);
        else if(phase==FB_MODE_RESULT||phase==FB_MODE_MESSAGE)press(BTN_ACTION);
        else if(phase==FB_MODE_EVOLVE||phase==FB_MODE_LEARN)press(BTN_CANCEL);
        tick();assert(mode()->phase!=FB_MODE_ERROR);
    }
    assert(guard<10000&&mode()->phase==FB_MODE_SHOP);
}
bool football_legacy_screen_open(const football_screen_args *args);
int main(void)
{
    begin();assert(mode()->phase==FB_MODE_ROUND);
    /* Same-frame input cannot cascade from round entry through playcalling. */
    press(BTN_ACTION);assert(mode()->phase==FB_MODE_PLAY);
    top->input(top,BTN_ACTION);assert(mode()->playcall.state==FB_CALL_MAIN);
    unsigned before=loads;tick();assert(loads==before+7&&clears==1);
    uint32_t tactic_rng=mode()->session.match.rng;
    tap(25,196);assert(mode()->session.match.home_tactic==FB_CONTACT_JUKE && mode()->playcall.state==FB_CALL_MAIN);
    press(BTN_LEFT);assert(mode()->session.match.home_tactic==FB_CONTACT_BALANCED && mode()->session.match.rng==tactic_rng);
    repeating=true;press(BTN_ACTION);assert(mode()->playcall.state==FB_CALL_MAIN);repeating=false;
    busy=true;uint16_t plays=mode()->session.match.plays;tick();assert(mode()->session.match.plays==plays);busy=false;
    press(BTN_MENU);assert(top==&overlay);tick();assert(mode()->session.match.plays==plays);answer(0);
    /* Quit is confirmed and returns through the normal screen leave callback. */
    press(BTN_MENU);answer(1);assert(top==&title&&exits==1&&speed==2&&!football_screen_test_mode());
    assert(syncs>=2 && top_draws>0 && bottom_draws>0);

    begin();save_ok=false;unsigned save_before=saves;win();assert(saves==save_before+1&&prepared==1);
    uint32_t wallet=mode()->season.wallet;
    tap(20,70);assert(mode()->season.wallet==wallet); /* Save error blocks purchases. */
    press(BTN_CANCEL);tap(20,70);assert(mode()->season.shop[0].sold);
    press(BTN_MENU);answer(1);assert(top==playoff); /* Exit attempts to save again and fails. */
    press(BTN_CANCEL);assert(top==&overlay&&strstr(option_label,"without saving"));answer(0);
    save_ok=true;press(BTN_MENU);answer(1);assert(top==&title&&exits==2&&!football_screen_test_mode());

    render_ok=false;begin();assert(mode()->phase==FB_MODE_ERROR);press(BTN_ACTION);assert(top==&title&&exits==3);render_ok=true;
    /* Cancelling the draft also releases the once-per-entry allocation. */
    assert(football_legacy_screen_open(&config));assert(top==&starter);draft.on_exit(draft.arg);
    assert(top==&title&&exits==4&&speed==2&&!football_screen_test_mode());
    /* Production persistence path: autosave, exact resume, retry and explicit deletion. */
    config.persistence=true;assert(football_legacy_screen_open(&config));assert(top==&starter);
    gc_starter team={.species=SPECIES_ID_MAWILE,.tera_type=-1};draft.on_start(&team,1,draft.arg);playoff=top;
    tick();assert(file_writes==1 && file_status==FB_FILE_OK);
    uint32_t id=mode()->season.roster->party[0].b.id;
    press(BTN_MENU);answer(1);assert(top==&title && exits==5);
    assert(football_legacy_screen_open(&config));assert(top==&overlay);answer(0);
    assert(mode()->phase==FB_MODE_ROUND && mode()->season.roster->party[0].b.id==id);
    unsigned writes=file_writes;tick();assert(file_writes==writes); /* Resume alone does not rewrite the file. */
    file_write_ok=false;press(BTN_MENU);answer(1);assert(top==playoff);
    file_write_ok=true;press(BTN_ACTION);assert(top==&title && exits==6);
    file_status=FB_FILE_BAD;writes=file_writes;
    assert(football_legacy_screen_open(&config));assert(mode()->phase==FB_MODE_ERROR);tick();assert(file_writes==writes);
    press(BTN_MENU);answer(2);assert(top==&overlay);answer(1);
    assert(top==&title && file_clears==1 && exits==7);
    puts("football screen: draft routing, one-action input, overlay pause, sprite lifecycle, save failure/retry and exit passed");
}
