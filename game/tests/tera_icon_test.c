/* The Tera icon beside a Pokemon's name in its battle info box (BattleInfo.teraIcon, updateTeraType, updateIconDisplay): icon_tera at half size,
 * filled with the Tera type's colour (getTypeRgb; setTintFill), one pixel after the name and the gender sign; absent when the Pokemon is not
 * Terastallized. The real draw_name is run with a recording stand-in for the drawing calls. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
const pr_move pr_moves[PR_MOVE_COUNT]={{0}};
int ui_window_style(void){return 1;}
void ui_window(float x,float y,float w,float h,win_variant v){}
void ui_cursor(float x,float y){}
void ui_play_select(void){}
void ui_play_error(void){}
void img_draw(const char* key,const char* frame,float x,float y,float ox,float oy){}
void rr_rect(float x,float y,float w,float h,rr_color color){}
rr_color text_color(text_style style,bool shadow){return 0;}
text_opts text_defaults(void){return (text_opts){0};}
float text_width(text_style style,const char* text,const text_opts* opts){return 4*strlen(text);}
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

/* name 4 px a letter, the gender sign 4 px */
float text_draw(text_style style,float x,float y,const char* text,const text_opts* opts){return 4*strlen(text)/ (text[0]&0x80 ? 3 : 1);}
static rr_frame icon;static bool icon_missing;
const rr_frame* img_frame(const char* key,const char* frame){return !icon_missing&&!strcmp(key,"images/ui/icon_tera")?&icon:NULL;}
static int tone[3],draws;static float dx,dy,dscale;static int tone_at_draw[3];static rr_color dtint;
void rr_set_tone(int r,int g,int b){tone[0]=r;tone[1]=g;tone[2]=b;}
void rr_draw(const rr_frame* f,float x,float y,const rr_draw_opts* o){
    if(f!=&icon)return;
    draws++;dx=x;dy=y;dscale=o->scale_x;dtint=o->tint;memcpy(tone_at_draw,tone,sizeof tone);
}

static int checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"tera icon FAIL line %d: %s\n",__LINE__,#x);return 1;}checks++;}while(0)
static void near(float a,float b){if(a-b>.001f||b-a>.001f){fprintf(stderr,"%g != %g\n",a,b);assert(0);}}

static bv_mon mon(int gender,bool tera,int type,const char* name){
    bv_mon m;memset(&m,0,sizeof m);m.species=6;m.level=50;m.gender=gender;m.terastallized=tera;m.tera_type=type;snprintf(m.name,sizeof m.name,"%s",name);return m;
}
int main(void){
    icon.src_w=12;icon.src_h=15;
    /* not Terastallized: no icon, however it is typed */
    bv_mon m=mon(0,false,POKEMON_TYPE_FIRE,"Char");
    draws=0;draw_name(&m,true,100,50);CHECK(draws==0);
    /* Terastallized: the icon at half size after the name (4 letters, 16 px) and the gender sign (4 px), 1 px on, filled with the type's colour */
    m=mon(0,true,POKEMON_TYPE_FIRE,"Char");
    draws=0;draw_name(&m,true,100,50);CHECK(draws==1);
    near(dx,100+16+4+1);near(dy,50-0.5f);near(dscale,.5f);
    CHECK(dtint==RR_RGBA(0,0,0,255));   /* a flat fill: black through the multiply, the colour added as the tone */
    CHECK(tone_at_draw[0]==240&&tone_at_draw[1]==128&&tone_at_draw[2]==48);
    CHECK(tone[0]==0&&tone[1]==0&&tone[2]==0);   /* and the tone is back to none for what follows */
    /* genderless: the icon follows the name directly */
    m=mon(-1,true,POKEMON_TYPE_WATER,"Char");
    draws=0;draw_name(&m,false,20,10);CHECK(draws==1);
    near(dx,20+16+1);CHECK(tone_at_draw[0]==104&&tone_at_draw[1]==144&&tone_at_draw[2]==240);
    /* every type has upstream's colour (getTypeRgb), Stellar is white */
    static const int RGB[19][3]={{168,168,120},{192,48,40},{168,144,240},{160,64,160},{224,192,104},{184,160,56},{168,184,32},{112,88,152},{184,184,208},
        {240,128,48},{104,144,240},{120,200,80},{248,208,48},{248,88,136},{152,216,216},{112,56,248},{112,88,72},{232,136,200},{255,255,255}};
    for(int t=0;t<19;t++){
        m=mon(1,true,t,"Char");draws=0;draw_name(&m,true,0,0);CHECK(draws==1);
        CHECK(tone_at_draw[0]==RGB[t][0]&&tone_at_draw[1]==RGB[t][1]&&tone_at_draw[2]==RGB[t][2]);
    }
    /* a pack without the icon draws nothing, and leaves the tone alone */
    icon_missing=true;tone[0]=7;
    m=mon(0,true,POKEMON_TYPE_FIRE,"Char");
    draws=0;draw_name(&m,true,0,0);CHECK(draws==0&&tone[0]==7);
    printf("tera icon PASSED (%d checks)\n",checks);
    return 0;
}
