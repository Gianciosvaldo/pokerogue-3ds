/* Kars's Perfect Being on the battle screen (lorenzo, 2026-11-16: "it would slide in like dio's secondary sprite after his 5th pokemon is koed,
 * with a custom message at the bottom"; the lines are the ones he picked). The real renderer is run with recording stand-ins:
 *   1  BEV_PERFECT a = 1: his second sprite (kars_perfect; his own without the file; nothing for a trainer that is not a JoJo one) slides in
 *      from 140 px right of DIO's place over 750 ms, then "Kars has become the Ultimate Life Form!" as a plain message; it stays drawn after
 *   2  a = 2: "<the opposing X>'s flesh flowed aside like water!", then "The attack did nothing!" (two messages, so neither runs past the box)
 *   3  a = 3: "<the opposing X>'s perfect body softened the blow!"
 *   4  the defeated trainer comes back with the Perfect Being sprite, and the second one is no longer drawn
 * Each step is checked through the screen's own event loop. No upstream counterpart (custom content). */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
const char *i18n_language(void){return "en";}
rr_atlas *img_get(const char *k){(void)k;return NULL;}
int rr_atlas_count(const rr_atlas *a){(void)a;return 0;}
const rr_frame *rr_atlas_frame(const rr_atlas *a,int i){(void)a;(void)i;return NULL;}

/* the boss's two sheets: Eternatus 100 px cell with its 66 rows from y 28; Eternamax 112 px cell with its 98 rows from y 9 */
static rr_frame frame_base={.src_w=100,.src_h=100,.off_x=2,.off_y=28,.w=97,.h=66},frame_max={.src_w=112,.src_h=112,.off_x=8,.off_y=9,.w=100,.h=98};
static rr_frame frame_plain={.src_w=96,.src_h=96,.w=96,.h=96};
static const rr_frame *answer(void){return B.s[1].mon.species==890?(B.s[1].mon.form>0&&B.s[1].sprite_key[0]=='M'?&frame_max:&frame_base):&frame_plain;}
const rr_frame *img_frame(const char *key,const char *name){(void)key;(void)name;return &frame_plain;}
static char drawn_key[80];static int n_drawn;static float drawn_x;
const rr_frame *img_anim_frame(const char *key,int frame){(void)frame;snprintf(drawn_key,sizeof drawn_key,"%s",key);return answer();}
static bool has_perfect=true;
bool img_has(const char *key){return has_perfect||!strstr(key,"_perfect");}
static int n_load,n_sound;
bool menu_sprite_load(menu_sprite *s,const menu_mon *m,bool player){(void)s;(void)m;(void)player;n_load++;return false;}
void menu_sprite_free(menu_sprite *s){(void)s;}
const rr_frame *menu_sprite_frame(const menu_sprite *s,uint32_t ms){(void)s;(void)ms;return &frame_plain;}
bool ba_release_graphics(void){return false;}
void ba_show(int battler){(void)battler;}
void ba_draw(ba_layer layer,const ba_mon field[4]){(void)layer;(void)field;}
bool ba_hidden(int battler){(void)battler;return false;}
void ui_play_sound(const char *key){if(strstr(key,"Transform"))n_sound++;}
static float cur_zoom=1;
void rr_set_zoom(float z,float cx,float cy){(void)cx;(void)cy;cur_zoom=z;}
void rr_rect(float x,float y,float w,float h,rr_color c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void rr_draw_cropped(const rr_frame *f,float x,float y,int w,int h,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)w;(void)h;(void)o;}
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){(void)f;(void)y;(void)o;n_drawn++;drawn_x=x;}
void rr_set_tone(int r,int g,int b){(void)r;(void)g;(void)b;}
void rr_set_grey(int a){(void)a;}
void rr_circle(float x,float y,float r,rr_color c){(void)x;(void)y;(void)r;(void)c;}
int rr_frame_alpha(const rr_frame *f,int x,int y){(void)f;(void)x;(void)y;return 255;}
float ui_now(void){return 0;}
void chs_show(const char *a,const char *v){(void)a;(void)v;}
void chs_set_variant(const char *v){(void)v;}
void chs_hide(void){}
void chs_reset(void){}
const char *i18n_t(char *out,int n,const char *key,...){
 va_list ap;va_start(ap,key);(void)va_arg(ap,const char*);const char *v=va_arg(ap,const char*);va_end(ap);
 snprintf(out,n,"%s %s",strcmp(key,"battle:foePokemonWithAffix")?"Wild":"The opposing",v);return out;}
const char *i18n_ability_name(int id){(void)id;return "";}
bool ba_play(ba_kind kind,int id,int user,int target,const ba_mon field[4]){(void)kind;(void)id;(void)user;(void)target;(void)field;return false;}
bool ba_active(void){return false;}
void ba_update(float dt,const ba_mon field[4]){(void)dt;(void)field;}
bool chs_busy(void){return false;}
static char shown[8][160];static int n_shown;static bool shown_prompt;
void msgbox_show(msgbox *m,const char *t,float d,bool p){(void)m;(void)d;shown_prompt|=p;if(n_shown<8)snprintf(shown[n_shown],160,"%s",t);n_shown++;}
void msgbox_clear(msgbox *m){(void)m;}
static int done_after,done_calls;
bool msgbox_done(const msgbox *m){(void)m;return ++done_calls>=done_after;}
const char *msgbox_take_variant(msgbox *m){(void)m;return NULL;}
float text_draw(text_style st,float x,float y,const char *s,const text_opts *o){(void)st;(void)x;(void)y;(void)o;return (float)strlen(s)*4;}
float text_width(text_style st,const char *s,const text_opts *o){(void)st;(void)o;return (float)strlen(s)*4;}
text_opts text_defaults(void){text_opts o;memset(&o,0,sizeof o);return o;}
rr_color text_color(text_style s,bool shadow){(void)s;(void)shadow;return 0;}

static int checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"perfect being view FAIL line %d: %s\n",__LINE__,#x);return 1;}checks++;}while(0)
static bool near(float a,float b){return fabsf(a-b)<=.01f;}
static void put(side *sd,int species){
 memset(sd,0,sizeof *sd);sd->present=sd->info_on=true;sd->sprite_on=true;sd->mon.species=species;sd->mon.level=50;sd->mon.max_hp=200;sd->mon.hp=200;sd->hp_shown=200;
 sd->scale=1;snprintf(sd->mon.name,sizeof sd->mon.name,"Minior");
}
static void start(bv_event ev){B.ev[0]=ev;B.n_ev=1;B.cur=0;B.step=0;B.t=0;n_shown=0;done_calls=0;shown_prompt=false;}
static float place(float slide){float x,y;stand_pos(slide,&x,&y);return x;}
int mui_reclaim_images(void) { return 0; }
void ba_debug_text(char* out, size_t n) { if (n) out[0] = 0; }
float ui_sound_seconds(const char *key){(void)key;return 0;}
int main(void){
 memset(&B,0,sizeof B);B.field_scale=1;
 put(side_of(0),445);put(side_of(2),774);
 B.setup.trainer=true;
 snprintf(B.setup.trainer_keys[0],sizeof B.setup.trainer_keys[0],"images/trainer/custom/kars");
 snprintf(B.setup.trainer_name,sizeof B.setup.trainer_name,"Kars");
 field_zoom_on=1;field_zoom_cur=1;
 /* ---- 1 */
 done_after=1;
 start((bv_event){.type=BEV_PERFECT,.battler=2,.a=1});
 run_event(10);
 CHECK(B.perfect.on&&!strcmp(B.perfect.key,"images/trainer/custom/kars_perfect")&&n_shown==0);
 n_drawn=0;draw_perfect_kars();CHECK(n_drawn==1&&!strcmp(drawn_key,"images/trainer/custom/kars_perfect"));
 CHECK(drawn_x>place(1)+100);                                      /* still off to the right */
 int frames=1;
 while(B.cur<1&&frames<1000){run_event(10);frames++;if(B.perfect.t<740)CHECK(n_shown==0);}
 CHECK(B.cur==1&&frames>=75&&frames<=77);                          /* 750 ms of slide, then the line */
 CHECK(n_shown==1&&!strcmp(shown[0],"Kars has become the Ultimate Life Form!")&&!shown_prompt);
 CHECK(B.perfect.on&&near(B.perfect.slide,1));
 n_drawn=0;draw_perfect_kars();CHECK(n_drawn==1&&near(drawn_x,place(1)));   /* it stays, beside his Pokemon */
 /* the line waits for the box: 30 polls before it is done */
 B.perfect.on=false;done_after=30;
 start((bv_event){.type=BEV_PERFECT,.battler=2,.a=1});
 frames=0;while(B.cur<1&&frames<1000){run_event(10);frames++;}
 CHECK(B.cur==1&&n_shown==1&&frames>=75+29);
 /* without the file his own sprite; another trainer gets none */
 B.perfect.on=false;has_perfect=false;done_after=1;
 start((bv_event){.type=BEV_PERFECT,.battler=2,.a=1});run_event(10);
 CHECK(!strcmp(B.perfect.key,"images/trainer/custom/kars"));
 has_perfect=true;B.perfect.on=false;
 snprintf(B.setup.trainer_keys[0],sizeof B.setup.trainer_keys[0],"images/trainer/rival_m");
 start((bv_event){.type=BEV_PERFECT,.battler=2,.a=1});run_event(10);
 CHECK(!B.perfect.key[0]);n_drawn=0;draw_perfect_kars();CHECK(n_drawn==0);
 snprintf(B.setup.trainer_keys[0],sizeof B.setup.trainer_keys[0],"images/trainer/custom/kars");
 B.perfect.on=false;
 start((bv_event){.type=BEV_PERFECT,.battler=2,.a=1});
 frames=0;while(B.cur<1&&frames<1000){run_event(10);frames++;}
 CHECK(B.perfect.on&&B.perfect.stage==0);
 /* ---- 2 */
 start((bv_event){.type=BEV_PERFECT,.battler=2,.a=2});
 frames=0;while(B.cur<1&&frames<1000){run_event(10);frames++;}
 CHECK(B.cur==1&&frames==1&&n_shown==2);                           /* no slide: the two lines only */
 CHECK(!strcmp(shown[0],"The opposing Minior's flesh flowed aside like water!"));
 CHECK(!strcmp(shown[1],"The attack did nothing!"));
 CHECK(B.perfect.stage==0&&B.perfect.on&&near(B.perfect.slide,1));
 done_after=20;
 start((bv_event){.type=BEV_PERFECT,.battler=2,.a=2});
 frames=0;while(B.cur<1&&frames<1000){run_event(10);frames++;if(frames<19)CHECK(n_shown==1);}
 CHECK(B.cur==1&&n_shown==2);                                      /* the second line only once the first is done */
 done_after=1;
 /* ---- 3 */
 start((bv_event){.type=BEV_PERFECT,.battler=2,.a=3});
 frames=0;while(B.cur<1&&frames<1000){run_event(10);frames++;}
 CHECK(B.cur==1&&n_shown==1&&!strcmp(shown[0],"The opposing Minior's perfect body softened the blow!"));
 /* ---- 4 */
 start((bv_event){.type=BEV_TRAINER_SHOW,.battler=-1});
 frames=0;while(B.cur<1&&frames<1000){run_event(10);frames++;}
 CHECK(B.cur==1&&!strcmp(B.setup.trainer_keys[0],"images/trainer/custom/kars_perfect")&&!B.perfect.on);
 n_drawn=0;draw_perfect_kars();CHECK(n_drawn==0);
 printf("perfect being view: %d checks, 0 failed\n",checks);
 return 0;
}
