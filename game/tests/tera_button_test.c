/* The Terastallize button of the battle command menu (upstream CommandUiHandler): it is reached with LEFT from FIGHT or POKEMON, RIGHT goes
 * back to FIGHT, UP and DOWN do not leave it, A on it opens the fight menu and a move picked there is the command {FIGHT, slot, tera};
 * without the button offered none of it applies; a tap on the button is A on it; it is drawn only when offered. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
const pr_move pr_moves[PR_MOVE_COUNT]={{0}};
static int selects,errors,buttons,button_frames;static char button_type[24];
int ui_window_style(void){return 1;}
void ui_window(float x,float y,float w,float h,win_variant v){}
void ui_cursor(float x,float y){}
void ui_play_select(void){selects++;}
void ui_play_error(void){errors++;}
const rr_frame* img_frame(const char* key,const char* frame){return NULL;}
const rr_frame* mui_frame(const char* key,const char* frame){
    static rr_frame f;
    if(!strcmp(key,"ui/button_tera")){button_frames++;snprintf(button_type,sizeof button_type,"%s",frame?frame:"");return &f;}
    return NULL;
}
void img_draw(const char* key,const char* frame,float x,float y,float ox,float oy){}
void rr_rect(float x,float y,float w,float h,rr_color color){}
float ui_now(void){return 0;}
void rr_set_blend(rr_blend b){(void)b;}
void rr_draw(const rr_frame* frame,float x,float y,const rr_draw_opts* opts){buttons++;}
text_opts text_defaults(void){return (text_opts){0};}
rr_color text_color(text_style style,bool shadow){return 0;}
float text_width(text_style style,const char* text,const text_opts* opts){return 8*strlen(text);}
float text_draw(text_style style,float x,float y,const char* text,const text_opts* opts){return 0;}
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

static bv_command got;static int answers;
static void command(bv_command c,void* arg){(void)arg;got=c;answers++;}
static void fresh(bool tera,bool struggle){
    memset(&B,0,sizeof B);
    B.mode=M_COMMAND;B.bar_sel=-1;B.on_command=command;
    B.opts.can_ball=B.opts.can_switch=B.opts.can_run=true;
    B.opts.can_tera=tera;B.opts.tera_type=9;B.opts.struggle=struggle;
    for(int i=0;i<4;i++)B.opts.move_ok[i]=true;
    memset(&got,0,sizeof got);answers=0;
}
static void in(int b){bv_input(NULL,b);}
int mui_reclaim_images(void) { return 0; } /* battle_view.c frees the menus' idle icon sheets before giving up on a sprite */
void ba_debug_text(char* out, size_t n) { if (n) out[0] = 0; } /* battle_view.c's debug row asks battle_anim.c */
int main(void){
    /* reaching the button: LEFT from FIGHT and from POKEMON, RIGHT back to FIGHT, UP and DOWN stay on it */
    fresh(true,false);
    in(BTN_LEFT);assert(B.cmd_cursor==4);
    in(BTN_UP);assert(B.cmd_cursor==4);in(BTN_DOWN);assert(B.cmd_cursor==4);in(BTN_LEFT);assert(B.cmd_cursor==4);
    in(BTN_RIGHT);assert(B.cmd_cursor==0);
    B.cmd_cursor=2;in(BTN_LEFT);assert(B.cmd_cursor==4);in(BTN_RIGHT);assert(B.cmd_cursor==0);
    B.cmd_cursor=1;in(BTN_LEFT);assert(B.cmd_cursor==0);in(BTN_LEFT);assert(B.cmd_cursor==4); /* BALL's left is FIGHT, not the button */
    B.cmd_cursor=3;in(BTN_LEFT);assert(B.cmd_cursor==2);
    /* not offered: LEFT stays on the left column; a stale cursor on the button is put back on FIGHT */
    fresh(false,false);
    in(BTN_LEFT);assert(B.cmd_cursor==0);B.cmd_cursor=2;in(BTN_LEFT);assert(B.cmd_cursor==2);
    B.cmd_cursor=4;in(BTN_ACTION);assert(B.mode==M_FIGHT&&!B.tera_fight);
    /* A on the button: the fight menu in Tera mode; a move there answers {FIGHT, slot, tera}; B goes back to the button */
    fresh(true,false);
    B.cmd_cursor=4;B.opts.move_ok[2]=true;in(BTN_ACTION);assert(B.mode==M_FIGHT&&B.tera_fight&&!answers);
    in(BTN_CANCEL);assert(B.mode==M_COMMAND&&B.cmd_cursor==4&&!answers);
    in(BTN_ACTION);B.move_cursor=0;
    {
        static bv_mon m;memset(&m,0,sizeof m);
        /* the fight menu reads the acting Pokemon's moves */
        side*s=command_side();s->mon.n_moves=2;s->mon.pp_left[0]=5;s->mon.pp_left[1]=5;
        in(BTN_RIGHT);assert(B.move_cursor==1);in(BTN_ACTION);
        assert(answers==1&&got.command==0&&got.move_slot==1&&got.tera==1&&B.mode==M_IDLE);
        /* the plain FIGHT afterwards is no Tera */
        fresh(true,false);s=command_side();s->mon.n_moves=2;s->mon.pp_left[0]=5;s->mon.pp_left[1]=5;
        B.cmd_cursor=0;in(BTN_ACTION);assert(B.mode==M_FIGHT&&!B.tera_fight);in(BTN_ACTION);
        assert(answers==1&&got.command==0&&got.move_slot==0&&got.tera==0);
    }
    /* every move unusable: FIGHT goes straight to Struggle, and from the button it Terastallizes with it */
    fresh(true,true);B.cmd_cursor=4;in(BTN_ACTION);assert(answers==1&&got.command==0&&got.tera==1&&B.mode==M_IDLE);
    fresh(true,true);B.cmd_cursor=0;in(BTN_ACTION);assert(answers==1&&got.command==0&&got.tera==0);
    /* a tap on the button is A on it; taps beside it do not */
    fresh(true,false);
    assert(bv_touch(NULL,TERA_BTN_X,240+TERA_BTN_Y));assert(B.cmd_cursor==4&&B.mode==M_FIGHT&&B.tera_fight);
    fresh(false,false);
    assert(!bv_touch(NULL,TERA_BTN_X,240+TERA_BTN_Y)&&B.mode==M_COMMAND);
    /* the button is drawn only when offered, with the Pokemon's Tera type as its frame; the message gets shorter */
    fresh(true,false);buttons=button_frames=0;draw_bottom(false);assert(buttons==1&&button_frames==1&&!strcmp(button_type,"type9"));
    buttons=button_frames=0;draw_bottom(true);assert(buttons==1&&button_frames==1);
    fresh(false,false);buttons=button_frames=0;draw_bottom(false);assert(!buttons&&!button_frames);
    {
        bv_command_opts o;memset(&o,0,sizeof o);o.can_tera=true;o.tera_type=3;o.has_party_balls=true;
        B.cmd_cursor=4;bv_ask_command(&o,command,NULL);assert(B.cmd_cursor==4&&B.msg.opts.wrap_w==128-200/6.0f);
        o.can_tera=false;bv_ask_command(&o,command,NULL);assert(B.cmd_cursor==0&&B.msg.opts.wrap_w==128);
    }
    puts("Tera button: cursor, fight menu in Tera mode, command {FIGHT, slot, tera}, Struggle, touch, drawn only when offered");return 0;
}
