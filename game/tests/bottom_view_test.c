/* The battle screen's two hotkeys (lorenzo, 2026-11-16): X cycles what the bottom screen shows (the debug view, which is the screen as it was, and the
 * standard one), Y steps the game speed through the app's hook. Both work in every mode (the events play with them too, and the target pad), neither moves a cursor,
 * a held key's repeats are swallowed until its release, the view outlives the battle (bv_begin clears B, not the view), without a speed hook Y is
 * not taken, and the bottom screen draws the debug rows only in the debug view and the state of both keys in both. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
const pr_move pr_moves[PR_MOVE_COUNT]={{0}};
static int selects,errors,debug_rows;static bool space_ok=true;
static char drawn[16][96];static int n_drawn;static float drawn_origin[16],drawn_x[16],drawn_y[16];
int ui_window_style(void){return 1;}
void ui_window(float x,float y,float w,float h,win_variant v){}
void ui_cursor(float x,float y){}
void ui_play_select(void){selects++;}
void ui_play_error(void){errors++;}
bool ui_space(ui_space_id sp){return space_ok;}
const rr_frame* img_frame(const char* key,const char* frame){return NULL;}
const rr_frame* mui_frame(const char* key,const char* frame){return NULL;}
void img_draw(const char* key,const char* frame,float x,float y,float ox,float oy){}
void rr_rect(float x,float y,float w,float h,rr_color color){}
float ui_now(void){return 0;}
void rr_set_blend(rr_blend b){(void)b;}
void rr_draw(const rr_frame* frame,float x,float y,const rr_draw_opts* opts){}
rr_stats rr_get_stats(void){rr_stats s;memset(&s,0,sizeof s);return s;}
void rr_text_sys(float x,float y,float scale,const char* text,rr_color color){debug_rows++;}
text_opts text_defaults(void){return (text_opts){0};}
rr_color text_color(text_style style,bool shadow){return 0;}
float text_width(text_style style,const char* text,const text_opts* opts){return 8*strlen(text);}
float text_draw(text_style style,float x,float y,const char* text,const text_opts* opts){
    if(n_drawn<16){snprintf(drawn[n_drawn],sizeof drawn[0],"%s",text);drawn_origin[n_drawn]=opts?opts->origin_x:0;drawn_x[n_drawn]=x;drawn_y[n_drawn]=y;n_drawn++;}
    return 0;
}
void msgbox_draw(const msgbox* box,float x,float y){}
void dv_namebox_at(rr_screen s,int ox,int oy,float x,float y,const char* name,int style){}
const char* i18n_t(char* out,int n,const char* key,...){snprintf(out,n,"%s",key);return out;}
const char* i18n_move_name(int move){return "Move";}
const char* i18n_stat_name(int stat){return "Stat";}
const char* gm_enum_lower(gm_enum e,int value,char* out,int n){snprintf(out,n,"type%d",value);return out;}
const char* gm_enum_camel(gm_enum e,int value,char* out,int n){return "normal";}
void tween_start(tween* tw,float from,float to,float ms,ease_fn e){}
void msgbox_show(msgbox* m,const char* text,float delay,bool prompt){}
void msgbox_clear(msgbox* m){}
bool msgbox_input(msgbox* m,ui_button b){return false;}
int mui_reclaim_images(void) { return 0; }
void ba_debug_text(char* out, size_t n) { if (n) out[0] = 0; }

/* the app's hook: the speeds in order, as main.c's cycles them */
static const char* const SPEED_KEYS[4]={"settings:gameSpeedSlow","settings:gameSpeedNormal","settings:gameSpeedFast","settings:gameSpeedTurbo"};
static int speed,steps;
static const char* hook(bool step){if(step){speed=(speed+1)%4;steps++;}return SPEED_KEYS[speed];}

static bv_command got;static int answers;
static void command(bv_command c,void* arg){(void)arg;got=c;answers++;}
static void fresh(mode m){
    memset(&B,0,sizeof B);
    B.mode=m;B.bar_sel=-1;B.on_command=command;
    B.opts.can_ball=B.opts.can_switch=B.opts.can_run=true;B.opts.can_tera=true;B.opts.tera_type=9;
    for(int i=0;i<4;i++)B.opts.move_ok[i]=true;
    memset(&got,0,sizeof got);answers=0;selects=0;
    g_view_held=g_speed_held=false;
}
static bool tap(ui_button b){bool r=bv_input(NULL,b);bv_release(NULL,b);return r;}
static bool has_drawn(const char* text){for(int i=0;i<n_drawn;i++)if(!strcmp(drawn[i],text))return true;return false;}
static void draw_extras(void){n_drawn=0;debug_rows=0;draw_bottom_extras();}

int main(void){
    /* the standard view (lorenzo's Black 2 design, 2026-11-17) is the one the screen starts in: nothing changes until X is pressed */
    assert(g_view==BV_VIEW_STANDARD&&!g_view_held&&!g_speed_held);
    g_view=BV_VIEW_DEBUG; /* the rest of this test is the debug view's strip */

    /* X cycles the views in every mode, takes the key, plays the select sound, and moves nothing else */
    static const mode MODES[5]={M_IDLE,M_EVENTS,M_COMMAND,M_FIGHT,M_TARGET};
    for(int m=0;m<5;m++){
        fresh(MODES[m]);g_view=BV_VIEW_DEBUG;B.cmd_cursor=2;B.move_cursor=1;
        assert(tap(BTN_CYCLE_TERA)&&g_view==BV_VIEW_STANDARD&&selects==1);
        assert(tap(BTN_CYCLE_TERA)&&g_view==BV_VIEW_DEBUG&&selects==2);
        assert(B.mode==MODES[m]&&B.cmd_cursor==2&&B.move_cursor==1&&!answers);
    }
    /* the level-up window (inside the events) does not keep X */
    fresh(M_EVENTS);g_view=BV_VIEW_DEBUG;B.lvl.on=true;
    assert(tap(BTN_CYCLE_TERA)&&g_view==BV_VIEW_STANDARD&&B.lvl.on);
    /* with the item bar focused X still cycles (the bar's own keys are L and R); the cursor leaves the bar, which the standard view does not show
     * (its items are a panel of its own) */
    fresh(M_COMMAND);g_view=BV_VIEW_DEBUG;B.bar_sel=0;B.bar_open=true;
    assert(tap(BTN_CYCLE_TERA)&&g_view==BV_VIEW_STANDARD&&B.bar_sel==-1&&!B.bar_open&&!B.items_on);

    /* a held X repeats: the repeats do nothing (and play nothing) until it is released */
    fresh(M_COMMAND);g_view=BV_VIEW_DEBUG;
    assert(bv_input(NULL,BTN_CYCLE_TERA)&&g_view==BV_VIEW_STANDARD&&selects==1);
    assert(bv_input(NULL,BTN_CYCLE_TERA)&&bv_input(NULL,BTN_CYCLE_TERA)&&g_view==BV_VIEW_STANDARD&&selects==1);
    bv_release(NULL,BTN_CYCLE_NATURE); /* another key's release does not free it */
    assert(bv_input(NULL,BTN_CYCLE_TERA)&&g_view==BV_VIEW_STANDARD&&selects==1);
    bv_release(NULL,BTN_CYCLE_TERA);
    assert(bv_input(NULL,BTN_CYCLE_TERA)&&g_view==BV_VIEW_DEBUG&&selects==2);
    bv_release(NULL,BTN_CYCLE_TERA);

    /* the view is kept when B is cleared for the next battle */
    fresh(M_COMMAND);g_view=BV_VIEW_STANDARD;memset(&B,0,sizeof B);assert(g_view==BV_VIEW_STANDARD);g_view=BV_VIEW_DEBUG;

    /* Y steps the speed through the hook in every mode, once per press, and moves nothing else */
    speed=1;steps=0;speed_hook=hook;
    for(int m=0;m<5;m++){
        fresh(MODES[m]);B.cmd_cursor=3;B.move_cursor=2;const int before=steps,sel=selects;
        assert(tap(BTN_CYCLE_NATURE)&&steps==before+1&&selects==sel+1);
        assert(B.mode==MODES[m]&&B.cmd_cursor==3&&B.move_cursor==2&&!answers);
    }
    assert(steps==5&&speed==2); /* five presses from Normal: Fast, Turbo, Slow, Normal, Fast */
    fresh(M_EVENTS);B.lvl.on=true;assert(tap(BTN_CYCLE_NATURE)&&B.lvl.on&&steps==6&&speed==3);
    /* a held Y repeats: once */
    fresh(M_COMMAND);steps=0;
    assert(bv_input(NULL,BTN_CYCLE_NATURE)&&bv_input(NULL,BTN_CYCLE_NATURE)&&bv_input(NULL,BTN_CYCLE_NATURE)&&steps==1&&selects==1);
    bv_release(NULL,BTN_CYCLE_TERA);
    assert(bv_input(NULL,BTN_CYCLE_NATURE)&&steps==1); /* X's release does not free it */
    bv_release(NULL,BTN_CYCLE_NATURE);
    assert(bv_input(NULL,BTN_CYCLE_NATURE)&&steps==2);
    bv_release(NULL,BTN_CYCLE_NATURE);
    /* X and Y are separate keys: holding one does not stop the other */
    fresh(M_COMMAND);g_view=BV_VIEW_DEBUG;steps=0;
    assert(bv_input(NULL,BTN_CYCLE_NATURE)&&bv_input(NULL,BTN_CYCLE_TERA)&&steps==1&&g_view==BV_VIEW_STANDARD);
    bv_release(NULL,BTN_CYCLE_NATURE);bv_release(NULL,BTN_CYCLE_TERA);g_view=BV_VIEW_DEBUG;

    /* without a hook Y is not taken (it was not before either): nothing is played, nothing changes */
    speed_hook=NULL;fresh(M_COMMAND);
    assert(!tap(BTN_CYCLE_NATURE)&&!selects&&B.mode==M_COMMAND&&B.cmd_cursor==0);
    /* the other keys are what they were: L and R still switch the item bar, A on FIGHT opens the fight menu */
    fresh(M_COMMAND);B.cmd_cursor=0;assert(tap(BTN_ACTION)&&B.mode==M_FIGHT&&g_view==BV_VIEW_DEBUG);
    fresh(M_COMMAND);const int bar=B.bar;assert(tap(BTN_CYCLE_FORM)&&B.bar==(bar^1)&&g_view==BV_VIEW_DEBUG);
    assert(tap(BTN_CYCLE_SHINY)&&B.bar==bar);
    assert(!tap(BTN_CYCLE_GENDER)&&!tap(BTN_CYCLE_ABILITY)&&g_view==BV_VIEW_DEBUG); /* ZL and ZR stay free */

    /* the bottom screen: the debug rows in the debug view only, what X does in both, what Y does when there is a hook */
    speed=2;speed_hook=hook;fresh(M_COMMAND);
    g_view=BV_VIEW_DEBUG;draw_extras();
    assert(debug_rows==3);assert(has_drawn("X  Screen: Debug")&&has_drawn("Y  Speed: settings:gameSpeedFast"));
    g_view=BV_VIEW_STANDARD;draw_extras();
    assert(debug_rows==0);assert(has_drawn("X  Screen: Standard")&&has_drawn("Y  Speed: settings:gameSpeedFast"));
    /* one line from the left edge, X then Y (lorenzo, 2026-11-17: the stacked right-aligned labels were out of place): in the standard view under
     * its buttons while they are up and under its strip otherwise, under the debug rows (physical y 2..31) in the debug one; Y starts after X's text */
    assert(n_drawn==2&&!strcmp(drawn[0],"X  Screen: Standard")&&drawn_x[0]==8&&drawn_y[0]==SV_KEYS_CMD_Y&&drawn_y[1]==SV_KEYS_CMD_Y&&drawn_x[1]==8+8*strlen(drawn[0])+18);
    B.mode=M_EVENTS;draw_extras();assert(n_drawn==2&&drawn_y[0]==SV_KEYS_Y&&drawn_y[1]==SV_KEYS_Y);B.mode=M_COMMAND;
    for(int i=0;i<n_drawn;i++)assert(drawn_origin[i]==0);
    g_view=BV_VIEW_DEBUG;draw_extras();assert(n_drawn==2&&drawn_y[0]==34&&drawn_y[1]==34);g_view=BV_VIEW_STANDARD;
    speed=3;draw_extras();assert(has_drawn("Y  Speed: settings:gameSpeedTurbo")); /* the label follows the speed */
    speed_hook=NULL;draw_extras();
    assert(has_drawn("X  Screen: Standard")&&n_drawn==1); /* no hook: no Y line */
    g_view=BV_VIEW_DEBUG;draw_extras();assert(debug_rows==3&&n_drawn==1&&has_drawn("X  Screen: Debug"));
    /* drawn only where the bottom screen is */
    space_ok=false;speed_hook=hook;draw_extras();assert(!debug_rows&&!n_drawn);space_ok=true;

    puts("Battle hotkeys: X cycles the bottom screen's views and Y the game speed in every mode, repeats swallowed, the view outlives the battle, labels and debug rows drawn by view");
    return 0;
}
