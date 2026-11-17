/* The standard bottom screen (lorenzo's Black 2 design, 2026-11-17: a strip with "Wave biome money [battery]", Items, the party's balls and
 * Settings ("alternative to pressing start") under it, FIGHT in the centre with Ball left, Pokemon right and Run below; "no more item bar" on the
 * top screen). The real battle_view.c is run with recording stand-ins for the drawing calls:
 *   1  it is the default view, X still reaches the debug view (and back), and the debug view keeps the strip's command window
 *   2  the strip shows what bv_set_header gave and the battery the hook answers (5 s between asks; none without a hook)
 *   3  the buttons are drawn as hexagons (rects and triangles), the pointed one framed; the top screen has no item bar and no command window
 *   4  the D-pad goes between the buttons by where they stand; UP from FIGHT, L or R open the item panel
 *   5  a tap on a button is A on it: FIGHT opens the moves, Ball / Pokemon / Run answer, a command that is off answers nothing, Settings is
 *      START (the run menu), the Tera button fights with Tera; a tap on a point's empty corner is no button
 *   6  the item panel: the D-pad by cell (11 a row), L / R switch the bar, B / Items / "B: close" close it, a tap points at an item */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
const char *i18n_language(void){return "en";}
rr_atlas *img_get(const char *k){(void)k;return NULL;}
int rr_atlas_count(const rr_atlas *a){(void)a;return 0;}
const rr_frame *rr_atlas_frame(const rr_atlas *a,int i){(void)a;(void)i;return NULL;}
static rr_frame frame96;
const rr_frame *img_frame(const char *key,const char *name){(void)key;(void)name;return &frame96;}
const rr_frame *mui_frame(const char *key,const char *frame){(void)key;(void)frame;return &frame96;}
const rr_frame *img_anim_frame(const char *key,int frame){(void)key;(void)frame;return &frame96;}
bool img_has(const char *key){(void)key;return true;}
void rr_set_zoom(float z,float cx,float cy){(void)z;(void)cx;(void)cy;}
void img_draw(const char *key,const char *frame,float x,float y,float ox,float oy){(void)key;(void)frame;(void)x;(void)y;(void)ox;(void)oy;}
void img_draw_opts(const char *key,const char *frame,float x,float y,const rr_draw_opts *o){(void)key;(void)frame;(void)x;(void)y;(void)o;}
static int rects,tris,balls_drawn;
static rr_color rect_c[64];static float rect_x[64],rect_y[64],rect_w[64];static int nrc;
void rr_rect(float x,float y,float w,float h,rr_color c){(void)h;if(nrc<64){rect_c[nrc]=c;rect_x[nrc]=x;rect_y[nrc]=y;rect_w[nrc]=w;nrc++;}rects++;}
void rr_tri(float x0,float y0,float x1,float y1,float x2,float y2,rr_color c){(void)x0;(void)y0;(void)x1;(void)y1;(void)x2;(void)y2;(void)c;tris++;}
void rr_draw_cropped(const rr_frame *f,float x,float y,int w,int h,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)w;(void)h;(void)o;}
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)o;balls_drawn++;}
void rr_set_tone(int r,int g,int b){(void)r;(void)g;(void)b;}
void rr_set_grey(int a){(void)a;}
void rr_set_blend(rr_blend b){(void)b;}
int rr_frame_alpha(const rr_frame *f,int x,int y){(void)f;(void)x;(void)y;return 255;}
float ui_now(void){return 0;}
const rr_frame *menu_sprite_frame(const menu_sprite *s,uint32_t ms){(void)s;(void)ms;return &frame96;}
bool ba_hidden(int battler){(void)battler;return false;}
/* the texts drawn, to look for */
static char texts[64][96];static float text_x[64],text_y[64];static int nt;
float text_draw(text_style st,float x,float y,const char *s,const text_opts *o){
 (void)st;(void)o;if(nt<64){snprintf(texts[nt],sizeof texts[nt],"%s",s);text_x[nt]=x;text_y[nt]=y;nt++;}return (float)strlen(s)*4;
}
float text_width(text_style st,const char *s,const text_opts *o){(void)st;(void)o;return (float)strlen(s)*4;}
text_opts text_defaults(void){text_opts o;memset(&o,0,sizeof o);return o;}
rr_color text_color(text_style s,bool shadow){(void)s;(void)shadow;return 0;}
static int selects,errors;
void ui_play_select(void){selects++;}
void ui_play_error(void){errors++;}
static int windows;
void ui_window(float x,float y,float w,float h,win_variant v){(void)x;(void)y;(void)w;(void)h;(void)v;windows++;}
void ui_cursor(float x,float y){(void)x;(void)y;}
void msgbox_clear(msgbox* m){(void)m;}
bool msgbox_input(msgbox* m,ui_button b){(void)m;(void)b;return false;}
float hint_x(text_style st,float x0,const char* const* texts_,int i){(void)st;(void)texts_;return x0+50*i;}
int hint_at(text_style st,float x0,const char* const* texts_,int n,float x){(void)st;(void)texts_;for(int i=0;i<n;i++)if(x>=x0-6&&x<x0+50*(i+1))return i;return -1;}
int mui_reclaim_images(void){return 0;}
const pr_move pr_moves[PR_MOVE_COUNT]={{0}};
int ui_window_style(void){return 1;}
void msgbox_draw(const msgbox* m,float x,float y){(void)m;(void)x;(void)y;}
void msgbox_show(msgbox* m,const char* t,float d,bool p){(void)m;(void)t;(void)d;(void)p;}
void dv_namebox_at(rr_screen sc,int ox,int oy,float x,float y,const char* name,int style){(void)sc;(void)ox;(void)oy;(void)x;(void)y;(void)name;(void)style;}
const char* i18n_move_name(int move){(void)move;return "Move";}
const char* i18n_stat_name(int stat){(void)stat;return "Stat";}
void ba_debug_text(char* out,size_t n){if(n)out[0]=0;}
/* the current space: everything the test draws is asked in one space at a time */
static ui_space_id cur_space=UI_PHYS_BOTTOM;
bool ui_space(ui_space_id s){return s==cur_space;}
const char* i18n_t(char* out,int n,const char* key,...){
 const char* v=!strcmp(key,"commandUiHandler:fight")?"Fight":!strcmp(key,"commandUiHandler:ball")?"Ball":!strcmp(key,"commandUiHandler:pokemon")?"Pokemon":
   !strcmp(key,"commandUiHandler:run")?"Run":key;
 snprintf(out,(size_t)n,"%s",v);return out;
}

static int checks,failed;
#define CHECK(x) do{checks++;if(!(x)){failed++;fprintf(stderr,"standard view FAIL line %d: %s\n",__LINE__,#x);}}while(0)

static int answered,answers,menus,tera_answer;
static void on_command(bv_command c,void* arg){(void)arg;answered=c.command;tera_answer=c.tera;answers++;}
static void on_menu(void* arg){(void)arg;menus++;}
static bv_item foes[30],keys[2];
static bv_command_opts opts(void){
 bv_command_opts o;memset(&o,0,sizeof o);
 o.can_ball=o.can_switch=o.can_run=true;for(int i=0;i<4;i++)o.move_ok[i]=true;
 o.on_menu=on_menu;o.foe_items=foes;o.n_foe_items=30;o.key_items=keys;o.n_key_items=2;
 o.has_party_balls=true;for(int i=0;i<6;i++)o.party_balls[i]=i<4?BV_BALL_ALIVE:BV_BALL_EMPTY;
 return o;
}
static void put(int field,int species){
 side *sd=side_of(field);memset(sd,0,sizeof *sd);sd->present=sd->info_on=true;sd->mon.species=species;sd->mon.level=30;
 sd->mon.max_hp=sd->mon.hp=60;sd->hp_shown=60;snprintf(sd->mon.name,sizeof sd->mon.name,"Mon%d",species);sd->mon.n_moves=4;
 for(int i=0;i<4;i++){sd->mon.moves[i]=33;sd->mon.pp_left[i]=10;sd->mon.pp_max[i]=10;}
}
static void start(const bv_command_opts* o){
 memset(&B,0,sizeof B);B.begun=true;B.bar_sel=-1;snprintf(B.biome,sizeof B.biome,"plains");put(0,1);put(2,7);
 answers=0;answered=-9;menus=0;errors=0;bv_ask_command(o,on_command,NULL);
}
static bool in(ui_button b){return bv_input(NULL,b);}
static bool tap(float x,float y){return bv_touch(NULL,x,y);}
static bool drawn(const char* s){for(int i=0;i<nt;i++)if(!strcmp(texts[i],s))return true;return false;}
static void draw_bottom_screen(void){nt=0;nrc=0;rects=tris=balls_drawn=windows=0;cur_space=UI_PHYS_BOTTOM;sv_draw();}
static int batt_level=3;static bool batt_charging;static int batt_asks;
static int battery(bool* charging){batt_asks++;*charging=batt_charging;return batt_level;}
static int count_rects_w(float w){int n=0;for(int i=0;i<nrc;i++)if(fabsf(rect_w[i]-w)<.01f)n++;return n;}

int main(void){
 frame96.src_w=frame96.w=8;frame96.src_h=frame96.h=8;
 for(int i=0;i<30;i++)foes[i]=(bv_item){"leftovers",1,"Leftovers","Heals."};
 keys[0]=(bv_item){"mega_bracelet",1,"Mega Bracelet","Mega."};keys[1]=(bv_item){"tera_orb",1,"Tera Orb","Tera."};
 bv_command_opts o=opts();

 /* ---- 1: the default view; X goes to the debug view and back */
 CHECK(g_view==BV_VIEW_STANDARD);
 start(&o);
 in(BTN_CYCLE_TERA);bv_release(NULL,BTN_CYCLE_TERA);CHECK(g_view==BV_VIEW_DEBUG);
 draw_bottom_screen();CHECK(!drawn("Items")&&!drawn("Settings"));   /* the debug view: the strip as it was */
 cur_space=UI_BOTTOM_UI;nt=0;draw_bottom(false);CHECK(drawn("Fight")&&drawn("Run"));     /* (its command window) */
 in(BTN_CYCLE_TERA);bv_release(NULL,BTN_CYCLE_TERA);CHECK(g_view==BV_VIEW_STANDARD);
 cur_space=UI_BOTTOM_UI;nt=0;draw_bottom(false);CHECK(!drawn("Fight")&&sv_cmd_shown());                  /* the strip's command window is gone */
 cur_space=UI_TOP_CANVAS_UI;nt=0;draw_bottom(true);CHECK(!drawn("Fight")&&!drawn("Run"));  /* and the top strip has "What will X do?" alone */

 /* ---- 2: the strip */
 bv_set_header(12,"Plains","\xe2\x82\xbd" "1,234");
 bv_set_battery_hook(NULL);sv_battery_update(6000); /* (no hook: nothing is asked) */
 draw_bottom_screen();
 CHECK(drawn("Wave 12")&&drawn("Plains")&&drawn("\xe2\x82\xbd" "1,234"));
 CHECK(count_rects_w(2.6f)==0&&count_rects_w(20)==0);                                  /* no hook, no battery (not even its outline) */
 bv_set_battery_hook(battery);batt_asks=0;sv_battery_update(16);CHECK(batt_asks==1);
 sv_battery_update(4000);CHECK(batt_asks==1);sv_battery_update(1001);CHECK(batt_asks==2);      /* every 5 s */
 draw_bottom_screen();CHECK(count_rects_w(2.6f)==3&&count_rects_w(20)==1);                                     /* level 3: three bars */
 batt_level=-1;bv_set_battery_hook(battery);sv_battery_update(16);draw_bottom_screen();CHECK(count_rects_w(2.6f)==0&&count_rects_w(20)==0);
 batt_level=5;bv_set_battery_hook(battery);sv_battery_update(16);draw_bottom_screen();CHECK(count_rects_w(2.6f)==5);
 bv_set_header(0,"",NULL);draw_bottom_screen();CHECK(!drawn("Wave 12")&&!drawn("Wave 0"));
 bv_set_header(12,"Plains","\xe2\x82\xbd" "1,234");

 /* ---- 3: the buttons */
 draw_bottom_screen();
 CHECK(drawn("Fight")&&drawn("Ball")&&drawn("Pokemon")&&drawn("Run")&&drawn("Items")&&drawn("Settings"));
 CHECK(tris==2*2*(4+2)+2*2);         /* each hexagon is an edge and a fill, each two triangles; the pointed one also its white frame */
 CHECK(balls_drawn==6);              /* the party's six balls */
 cur_space=UI_PHYS_TOP;nt=0;draw_top_bar();CHECK(!drawn("Foe items")&&nt==0); /* no item bar (nor its luck) on the top screen */
 g_view=BV_VIEW_DEBUG;nt=0;draw_top_bar();CHECK(drawn("Foe items"));g_view=BV_VIEW_STANDARD;
 /* the X / Y line goes under the buttons while they are up, under the strip otherwise */
 CHECK(bv_keys_y()==SV_KEYS_CMD_Y);B.mode=M_EVENTS;CHECK(bv_keys_y()==SV_KEYS_Y);B.mode=M_COMMAND;
 CHECK(SV_KEYS_CMD_Y>=SV_CMD[3].y+SV_CMD[3].h&&SV_KEYS_Y>SV_HDR_H);
 /* a command that is off is greyed */
 o.can_run=false;start(&o);draw_bottom_screen();
 { bool grey=false;for(int i=0;i<nrc;i++)if(rect_c[i]==RR_RGBA(0x60,0x60,0x68,255)&&rect_y[i]==SV_CMD[3].y)grey=true;CHECK(grey); }
 o=opts();

 /* ---- 4: the D-pad */
 start(&o);CHECK(B.cmd_cursor==0);
 in(BTN_DOWN);CHECK(B.cmd_cursor==3);
 in(BTN_LEFT);CHECK(B.cmd_cursor==1);
 in(BTN_RIGHT);CHECK(B.cmd_cursor==3);
 in(BTN_RIGHT);CHECK(B.cmd_cursor==2);
 in(BTN_LEFT);CHECK(B.cmd_cursor==3);
 in(BTN_UP);CHECK(B.cmd_cursor==0);
 in(BTN_RIGHT);CHECK(B.cmd_cursor==2);
 in(BTN_UP);CHECK(B.cmd_cursor==0);
 in(BTN_LEFT);CHECK(B.cmd_cursor==1);                       /* no Tera: FIGHT's left is Ball */
 in(BTN_UP);CHECK(B.cmd_cursor==0);
 B.cmd_cursor=2;CHECK(!in(BTN_RIGHT)&&B.cmd_cursor==2);     /* nothing right of Pokemon */
 B.cmd_cursor=1;CHECK(!in(BTN_LEFT)&&B.cmd_cursor==1);
 o.can_tera=true;start(&o);
 in(BTN_LEFT);CHECK(B.cmd_cursor==4);                       /* the Tera button left of FIGHT */
 in(BTN_DOWN);CHECK(B.cmd_cursor==1);
 in(BTN_UP);CHECK(B.cmd_cursor==4);
 in(BTN_RIGHT);CHECK(B.cmd_cursor==0);
 in(BTN_UP);CHECK(B.items_on&&B.bar_sel==0);               /* above FIGHT: the items */
 in(BTN_CANCEL);CHECK(!B.items_on&&B.bar_sel<0&&B.mode==M_COMMAND&&answers==0);
 in(BTN_CYCLE_FORM);CHECK(B.items_on);in(BTN_CANCEL);
 in(BTN_CYCLE_SHINY);CHECK(B.items_on&&B.bar==0);in(BTN_CANCEL); /* opening keeps the bar */
 o=opts();

 /* ---- 5: taps */
 for(int c=0;c<4;c++){
   start(&o);const sv_hex* h=&SV_CMD[c];
   CHECK(tap(h->x+h->w/2,h->y+h->h/2));
   if(c==0)CHECK(B.mode==M_FIGHT&&answers==0);
   else CHECK(answers==1&&answered==c);
 }
 start(&o);CHECK(!tap(SV_CMD[0].x+2,SV_CMD[0].y+2)&&B.mode==M_COMMAND);     /* the corner beside FIGHT's left point */
 CHECK(tap(SV_CMD[0].x+2,SV_CMD[0].y+SV_CMD[0].h/2)&&B.mode==M_FIGHT);        /* its point */
 o.can_ball=false;start(&o);CHECK(!tap(SV_CMD[1].x+50,SV_CMD[1].y+20)&&answers==0&&errors==1);
 o=opts();
 start(&o);CHECK(tap(SV_SETTINGS.x+38,SV_SETTINGS.y+10)&&menus==1);
 o.on_menu=NULL;start(&o);CHECK(!tap(SV_SETTINGS.x+38,SV_SETTINGS.y+10)&&menus==0);o=opts();
 o.can_tera=true;start(&o);CHECK(tap(SV_TERA_X,SV_TERA_Y)&&B.mode==M_FIGHT&&B.tera_fight);
 o.can_tera=false;start(&o);CHECK(!tap(SV_TERA_X,SV_TERA_Y)&&B.mode==M_COMMAND);
 o=opts();

 /* ---- 6: the item panel */
 start(&o);CHECK(tap(SV_ITEMS.x+38,SV_ITEMS.y+10)&&B.items_on&&B.bar==0&&B.bar_sel==0);
 draw_bottom_screen();CHECK(drawn("Foe items")&&drawn("Leftovers")&&drawn("Heals."));
 in(BTN_RIGHT);CHECK(B.bar_sel==1);
 in(BTN_DOWN);CHECK(B.bar_sel==12);
 in(BTN_UP);in(BTN_LEFT);CHECK(B.bar_sel==0);
 CHECK(!in(BTN_LEFT)&&!in(BTN_UP)&&B.bar_sel==0);
 B.bar_sel=27;CHECK(!in(BTN_DOWN)&&B.bar_sel==27);          /* no cell under it (30 items) */
 CHECK(!in(BTN_ACTION)&&B.mode==M_COMMAND&&answers==0);     /* A does nothing here */
 in(BTN_CYCLE_SHINY);CHECK(B.bar==1&&B.bar_sel==0);
 draw_bottom_screen();CHECK(drawn("Key items")&&drawn("Mega Bracelet"));
 in(BTN_CYCLE_FORM);CHECK(B.bar==0);
 CHECK(tap(SV_GRID_X+26*2+5,SV_GRID_Y+26+5)&&B.bar_sel==13);  /* a tap points at a cell */
 CHECK(!tap(SV_GRID_X+26*8+5,SV_GRID_Y+26*2+5)&&B.bar_sel==13);/* no item there (30 items: cell 30) */
 CHECK(!tap(SV_CMD[3].x+50,SV_CMD[3].y+18)&&answers==0);     /* the buttons under the panel do nothing */
 B.opts.can_tera=true;tap(SV_TERA_X,SV_TERA_Y);CHECK(B.cmd_cursor==0&&B.mode==M_COMMAND);B.opts.can_tera=false; /* nor the Tera button */
 CHECK(tap(SV_HINT_X+60,SV_TITLE_Y+2)&&!B.items_on);          /* "B: close" */
 in(BTN_CYCLE_FORM);CHECK(tap(SV_HINT_X+10,SV_TITLE_Y+2)&&B.bar==1&&B.items_on); /* "L/R: switch" */
 CHECK(tap(SV_ITEMS.x+38,SV_ITEMS.y+10)&&!B.items_on);        /* Items again */
 in(BTN_UP);in(BTN_MENU);CHECK(menus==1);                     /* START still opens the menu */
 in(BTN_CYCLE_TERA);bv_release(NULL,BTN_CYCLE_TERA);CHECK(!B.items_on&&B.bar_sel<0); /* X leaves the panel */
 in(BTN_CYCLE_TERA);bv_release(NULL,BTN_CYCLE_TERA);
 /* an empty bar: the panel opens with no cursor and says so */
 o.n_foe_items=0;start(&o);in(BTN_UP);CHECK(B.items_on&&B.bar_sel<0);draw_bottom_screen();CHECK(drawn("none"));
 CHECK(!in(BTN_RIGHT));in(BTN_CANCEL);
 /* the fight menu: L / R do nothing (no bar on screen) */
 o=opts();start(&o);in(BTN_ACTION);CHECK(B.mode==M_FIGHT);CHECK(!in(BTN_CYCLE_FORM)&&B.bar==0);

 printf("standard view: %d checks, %d failed\n",checks,failed);
 return failed?1:0;
}
