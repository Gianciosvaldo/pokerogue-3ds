/* The real footer must give summary item text the entire message strip. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
const pr_move pr_moves[PR_MOVE_COUNT]={{0}};
static int commands,trays,messages,names,descriptions;
int ui_window_style(void){return 1;}
void ui_window(float x,float y,float w,float h,win_variant v){commands+=x==202;trays+=x==150;}
void ui_cursor(float x,float y){}
const rr_frame* img_frame(const char* key,const char* frame){return NULL;}
const rr_frame* mui_frame(const char* key,const char* frame){return NULL;}
void img_draw(const char* key,const char* frame,float x,float y,float ox,float oy){}
void rr_rect(float x,float y,float w,float h,rr_color color){}
void rr_draw(const rr_frame* frame,float x,float y,const rr_draw_opts* opts){}
text_opts text_defaults(void){return (text_opts){0};}
rr_color text_color(text_style style,bool shadow){return 0;}
float text_width(text_style style,const char* text,const text_opts* opts){return 8*strlen(text);}
float text_draw(text_style style,float x,float y,const char* text,const text_opts* opts){
    names+=!strcmp(text,"Held item");descriptions+=!strcmp(text,"Item effect");return 0;
}
void msgbox_draw(const msgbox* box,float x,float y){messages++;}
void dv_namebox_at(rr_screen s,int ox,int oy,float x,float y,const char* name,int style){}
const char* i18n_t(char* out,int n,const char* key,...){snprintf(out,n,"%s",key);return out;}
const char* i18n_move_name(int move){return "Move";}
const char* i18n_stat_name(int stat){return "Stat";}
const char* gm_enum_lower(gm_enum e,int value,char* out,int n){return "normal";}
const char* gm_enum_camel(gm_enum e,int value,char* out,int n){return "normal";}
static void reset_draws(void){commands=trays=messages=names=descriptions=0;}
int main(void){
    for(int mode=0;mode<2;mode++)for(int top=0;top<2;top++){
        memset(&B,0,sizeof B);B.mode=mode?M_COMMAND:M_IDLE;B.hold_cmd=true;B.has_balls=true;B.bar_sel=-1;
        reset_draws();draw_bottom(top);assert(commands==1&&trays==1&&messages==1&&!names&&!descriptions);
        bv_set_info("Held item","Item effect");reset_draws();draw_bottom(top);
        assert(!commands&&!trays&&!messages&&names==1&&descriptions==1);
        bv_set_info(NULL,NULL);reset_draws();draw_bottom(top);
        assert(commands==1&&trays==1&&messages==1&&!names&&!descriptions);
    }
    puts("Own item description: top/bottom, command/retained footer, restoration; no overlapping commands, tray or prompt");return 0;
}
