/* The Terastallized look of a battle sprite and of the selected Tera button (upstream's sprite shader with teraColor, and
 * PokemonSpriteTeraSparkleHandler). The real draw_sprite / draw_tera_button run with a recording stand-in for the drawing calls:
 * a Terastallized sprite is drawn through the Tera type's colour (multiply, 70% of the way from white), then again added in that colour over
 * its own shape, pulsing between 0 and 0.3 alpha; a sparkle appears every 400 ms on a random opaque pixel and plays the 13 frames of
 * tera_sparkle at 18 fps; none of it on a Pokemon that is not Terastallized, in its intro silhouette or hidden. */
#include <assert.h>
#include <math.h>
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
bool ba_hidden(int battler){return false;}
const rr_frame* menu_sprite_frame(const menu_sprite* s,uint32_t now){return NULL;}

static float now_ms;
float ui_now(void){return now_ms;}
static rr_frame body,button,sparkle[13],other;
const rr_frame* img_anim_frame(const char* key,int i){return &body;}
const rr_frame* mui_frame(const char* key,const char* frame){return !strcmp(key,"ui/button_tera")?&button:NULL;}
const rr_frame* img_frame(const char* key,const char* frame){
    if(!strcmp(key,"images/effects/tera_sparkle")){int n=atoi(frame);return n>=0&&n<13?&sparkle[n]:NULL;}
    return &other;
}
static int alpha_at;   /* what rr_frame_alpha says */
static int last_px,last_py;
int rr_frame_alpha(const rr_frame* f,int x,int y){last_px=x;last_py=y;return alpha_at;}

typedef struct{const rr_frame* f;float x,y;rr_color tint;rr_blend blend;float scale;}draw_call;
static draw_call calls[32];static int nc;static rr_blend blend_now;
void rr_set_blend(rr_blend b){blend_now=b;}
void rr_set_tone(int r,int g,int b){}
void rr_draw(const rr_frame* f,float x,float y,const rr_draw_opts* o){
    assert(nc<32);calls[nc++]=(draw_call){f,x,y,o?o->tint:0xffffffff,blend_now,o?o->scale_x:1};
}
void rr_draw_cropped(const rr_frame* f,float x,float y,int w,int h,const rr_draw_opts* o){rr_draw(f,x,y,o);}

static int checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"tera sprite FAIL line %d: %s\n",__LINE__,#x);return 1;}checks++;}while(0)
static int sparkles(void){int n=0;for(int i=0;i<nc;i++)for(int k=0;k<13;k++)n+=calls[i].f==&sparkle[k];return n;}
static void fresh(bool tera,int type){
    memset(&B,0,sizeof B);memset(calls,0,sizeof calls);nc=0;blend_now=RR_BLEND_NORMAL;
    side* sd=&B.s[0];sd->present=sd->sprite_on=true;sd->scale=1;sd->mon_alpha=1;sd->mon.species=6;
    sd->mon.terastallized=tera;sd->mon.tera_type=type;snprintf(sd->sprite_key,sizeof sd->sprite_key,"x");
}
/* the colour: 70% of the way from white to the type's colour at full brightness */
static rr_color want_tint(int r,int g,int b,int a){
    int top=r>g?(r>b?r:b):(g>b?g:b);int c[3]={r,g,b};
    for(int i=0;i<3;i++)c[i]=(int)(255-0.7f*(255-c[i]*255.0f/top)+0.5f);
    return RR_RGBA(c[0],c[1],c[2],a);
}

int mui_reclaim_images(void) { return 0; } /* battle_view.c frees the menus' idle icon sheets before giving up on a sprite */
void ba_debug_text(char* out, size_t n) { if (n) out[0] = 0; } /* battle_view.c's debug row asks battle_anim.c */
int main(void){
    body.src_w=64;body.src_h=48;body.w=64;body.h=48;button.src_w=20;button.src_h=18;
    alpha_at=255;
    /* not Terastallized: one plain draw */
    now_ms=700;fresh(false,POKEMON_TYPE_FIRE);
    draw_sprite(&B.s[0],true);
    CHECK(nc==1&&calls[0].f==&body&&calls[0].tint==0xffffffff&&calls[0].blend==RR_BLEND_NORMAL);
    update_tera(1000);draw_sprite(&B.s[0],true);CHECK(nc==2&&sparkles()==0);
    /* Terastallized as Fire: the type's colour through the multiply, then the pulse added over the same shape (at the pulse's peak) */
    now_ms=TERA_PULSE_MS/2;fresh(true,POKEMON_TYPE_FIRE);
    draw_sprite(&B.s[0],true);
    CHECK(calls[0].f==&body&&calls[0].tint==want_tint(240,128,48,255)&&calls[0].blend==RR_BLEND_NORMAL);
    CHECK(calls[1].f==&body&&calls[1].blend==RR_BLEND_ADD&&calls[1].tint==RR_RGBA(240,128,48,77));
    CHECK(calls[1].x==calls[0].x&&calls[1].y==calls[0].y);
    CHECK(blend_now==RR_BLEND_NORMAL);   /* and normal blending for what follows */
    /* the pulse: nothing at the start of a cycle */
    now_ms=0;fresh(true,POKEMON_TYPE_FIRE);draw_sprite(&B.s[0],true);CHECK(calls[1].tint==RR_RGBA(240,128,48,0));
    /* Stellar: no colour to pull toward (plain multiply), a pulse that cycles the hue (green a third of the way round) */
    now_ms=0;fresh(true,POKEMON_TYPE_STELLAR);draw_sprite(&B.s[0],true);CHECK(calls[0].tint==0xffffffff);
    now_ms=TERA_PULSE_MS/2;fresh(true,POKEMON_TYPE_STELLAR);draw_sprite(&B.s[0],true);CHECK(calls[1].tint==RR_RGBA(0,255,25,77));   /* 700 ms: hue 2.1 sextants, green turning to cyan */
    now_ms=2000.0f/3;fresh(true,POKEMON_TYPE_STELLAR);draw_sprite(&B.s[0],true);CHECK(calls[1].tint==RR_RGBA(0,255,0,77));   /* exactly two sextants round: pure green */
    /* the intro silhouette and the catch silhouette keep their own colours */
    now_ms=700;fresh(true,POKEMON_TYPE_FIRE);B.s[0].black=true;draw_sprite(&B.s[0],true);
    CHECK(nc==1&&calls[0].tint==RR_RGBA(0,0,0,255));
    fresh(true,POKEMON_TYPE_FIRE);B.s[0].catch_on=true;draw_sprite(&B.s[0],true);CHECK(nc==1);
    /* behind its Substitute doll it is half transparent as before */
    fresh(true,POKEMON_TYPE_WATER);B.s[0].mon_alpha=0.5f;draw_sprite(&B.s[0],true);CHECK((calls[0].tint&0xff)==128);
    /* hidden (Fly, Dig): nothing at all */
    fresh(true,POKEMON_TYPE_FIRE);B.s[0].mon.hidden=true;draw_sprite(&B.s[0],true);CHECK(nc==0);

    /* sparkles: one at once on an opaque pixel inside the frame, 13 frames at 18 fps, a new one every 400 ms */
    now_ms=0;fresh(true,POKEMON_TYPE_FIRE);
    draw_sprite(&B.s[0],true);CHECK(sparkles()==1);
    CHECK(last_px>=0&&last_px<64&&last_py>=0&&last_py<48);
    for(int i=0;i<nc;i++)if(calls[i].f==&sparkle[0]){CHECK(calls[i].x>=106-32&&calls[i].x<106+32);}
    nc=0;update_tera(399);draw_sprite(&B.s[0],true);CHECK(sparkles()==1);   /* the first is still playing, no second yet */
    nc=0;update_tera(2);draw_sprite(&B.s[0],true);CHECK(sparkles()==2);
    nc=0;update_tera(1000.0f*13/18);draw_sprite(&B.s[0],true);
    CHECK(sparkles()<=1);   /* the first two have played their 13 frames; at most the third remains */
    { /* the age picks the frame */
        side* sd=&B.s[0];memset(sd->tera_sp,0,sizeof sd->tera_sp);sd->tera_sp[0].on=true;sd->tera_sp[0].age=1000.0f*5/18+1;
        nc=0;draw_tera_sparkles(sd,true);CHECK(nc==1&&calls[0].f==&sparkle[5]);
    }
    /* a miss (a transparent pixel) spawns nothing, and the next try is 400 ms later */
    alpha_at=0;fresh(true,POKEMON_TYPE_FIRE);draw_sprite(&B.s[0],true);CHECK(sparkles()==0);
    alpha_at=255;nc=0;update_tera(100);draw_sprite(&B.s[0],true);CHECK(sparkles()==0);
    nc=0;update_tera(300);draw_sprite(&B.s[0],true);CHECK(sparkles()==1);
    /* none off the field or when the Pokemon is not Terastallized, and a Pokemon that stops being so loses its timer */
    fresh(false,POKEMON_TYPE_FIRE);update_tera(1000);draw_sprite(&B.s[0],true);CHECK(sparkles()==0);

    /* the Tera button: plain, and with the cursor on it the Terastallized look */
    fresh(false,POKEMON_TYPE_FIRE);B.opts.can_tera=true;B.opts.tera_type=POKEMON_TYPE_FIRE;B.bar_sel=-1;B.cmd_cursor=0;now_ms=TERA_PULSE_MS/2;
    draw_tera_button();CHECK(nc==1&&calls[0].f==&button&&calls[0].tint==0xffffffff);
    nc=0;B.cmd_cursor=4;draw_tera_button();
    CHECK(nc==2&&calls[0].tint==want_tint(240,128,48,255)&&calls[1].blend==RR_BLEND_ADD&&calls[1].tint==RR_RGBA(240,128,48,77));
    printf("tera sprite PASSED (%d checks)\n",checks);
    return 0;
}
rr_atlas* img_get(const char* k){(void)k;return NULL;}
int rr_atlas_count(const rr_atlas* a){(void)a;return 0;}
const rr_frame* rr_atlas_frame(const rr_atlas* a,int i){(void)a;(void)i;return NULL;}
