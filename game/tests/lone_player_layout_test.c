/* The player's lone Pokemon in a double battle on the battle screen (lorenzo, 2026-11-15: "during a double battle, if only one pokemon is on your
 * field the sprite should be placed in the centre of the platform and not to the side and its hp bar should be the single battle one"; a Pokemon
 * whose partner fainted is moved at the end of that turn, not at the faint).
 * Upstream: Pokemon.setFieldPosition(CENTER) sets the sprite's offset to (0, 0) and the box to the single battle's (setMini(false),
 * setOffset(false)); LEFT is (-32, -8), RIGHT (+32, 0). SummonPhase.summon puts a player's Pokemon at CENTER when it is the only one of the party
 * that can fight, ToggleDoublePositionPhase moves it over 500 ms (Sine.easeOut). The real renderer is run with a recording stand-in for the drawing
 * calls:
 *   1  a double that begins with a lone Pokemon (bv_setup.player_centred): the sprite at the centre mark, the full pbinfo_player box with the HP
 *      numbers' room (310, -72), the foes in their two columns with their own boxes, nothing else moved
 *   2  BEV_PLAYER_CENTRE after a faint, the survivor in slot 0 (stood LEFT): the box is the single one at once, the sprite slides from (-32, -8)
 *      to the centre over 500 ms with Sine.easeOut and stays there; the foes do not move
 *   3  the survivor in slot 1 (stood RIGHT): BEV_FIELD_SWAP (the engine's party swap, in the middle of the turn) changes nothing on the screen,
 *      neither the sprite nor its mini box (320, -57); BEV_PLAYER_CENTRE then slides it from (+32, 0), not from the left column, to the centre
 *   4  BEV_PLAYER_CENTRE a = 0 (a partner is back): the boxes are the doubles ones at once and the sprite slides back to its column; b = 1 does
 *      either at once; a screen that is already there does nothing
 *   5  a doubles encounter that never lost anybody (nothing centred), a single battle and the final boss' second phase are what they were
 *   6  a centred final boss second phase: the lead that slid to the left (BEV_DOUBLES) goes to the centre the same way
 * The numbers are the app's own and upstream's constants (getFieldPositionOffset, battle-info setMini/setOffset), not an upstream output; no
 * upstream run. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
const char *i18n_language(void){return "en";}
rr_atlas *img_get(const char *k){(void)k;return NULL;}
int rr_atlas_count(const rr_atlas *a){(void)a;return 0;}
const rr_frame *rr_atlas_frame(const rr_atlas *a,int i){(void)a;(void)i;return NULL;}

static rr_frame frames[64];static char keys[64][160],names[64][20];static int nf;
const rr_frame *img_frame(const char *key,const char *name){
 for(int i=0;i<nf;i++)if(!strcmp(keys[i],key)&&!strcmp(names[i],name?name:""))return &frames[i];
 int i=nf++;assert(i<64);snprintf(keys[i],sizeof keys[i],"%s",key);snprintf(names[i],sizeof names[i],"%s",name?name:"");
 frames[i].src_w=strstr(key,"overlay_hp_boss")?86:strstr(key,"overlay_hp")?48:8;frames[i].src_h=strstr(key,"overlay_hp")?4:8;return &frames[i];
}
typedef struct{char key[160];float x,y;}img_call;
static img_call imgs[64];static int ni;
void img_draw(const char *key,const char *frame,float x,float y,float ox,float oy){
 (void)frame;(void)ox;(void)oy;
 if(ni<64){snprintf(imgs[ni].key,sizeof imgs[ni].key,"%s",key);imgs[ni].x=x;imgs[ni].y=y;ni++;}
}
void img_draw_opts(const char *key,const char *frame,float x,float y,const rr_draw_opts *o){(void)o;img_draw(key,frame,x,y,0,0);}
void rr_rect(float x,float y,float w,float h,rr_color c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void rr_draw_cropped(const rr_frame *f,float x,float y,int w,int h,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)w;(void)h;(void)o;}
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)o;}
void rr_set_tone(int r,int g,int b){(void)r;(void)g;(void)b;}
void rr_set_grey(int a){(void)a;}
void rr_circle(float x,float y,float r,rr_color c){(void)x;(void)y;(void)r;(void)c;}
float text_draw(text_style st,float x,float y,const char *s,const text_opts *o){(void)st;(void)x;(void)y;(void)o;return (float)strlen(s)*4;}
float text_width(text_style st,const char *s,const text_opts *o){(void)st;(void)o;return (float)strlen(s)*4;}
text_opts text_defaults(void){text_opts o;memset(&o,0,sizeof o);return o;}
rr_color text_color(text_style s,bool shadow){(void)s;(void)shadow;return 0;}

static int checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"lone player layout FAIL line %d: %s\n",__LINE__,#x);return 1;}checks++;}while(0)
static bool near(float a,float b){return fabsf(a-b)<=.001f;}
static void reset(void){ni=0;}
static const img_call *find(const char *key){for(int i=0;i<ni;i++)if(!strcmp(imgs[i].key,key))return &imgs[i];return NULL;}

static void put(side *sd,int species,bool boss){
 memset(sd,0,sizeof *sd);sd->present=sd->info_on=true;sd->mon.species=species;sd->mon.level=50;sd->mon.max_hp=300;sd->mon.hp=300;sd->hp_shown=300;
 sd->mon.boss=boss;sd->mon.boss_segments=boss?4:0;sd->scale=1;snprintf(sd->mon.name,sizeof sd->mon.name,"Mon%d",species);
}
/* the first player box a call draws, and which one it was */
static bool player_box(side *sd,float *x,float *y,bool *mini){
 reset();draw_player_info(sd);
 const img_call *c=find("images/ui/pbinfo_player");*mini=false;
 if(!c){c=find("images/ui/pbinfo_player_mini");*mini=true;}
 if(!c)return false;
 *x=c->x;*y=c->y;return true;
}
/* a double battle begun as bv_begin sets it, with the player's side as given */
static void begin_double(bool lone){
 memset(&B,0,sizeof B);B.setup.doubles=true;B.centred=lone;B.centre=lone?1.0f:0.0f;
}
/* one event played to its end; the number of frames it took */
static int play(bev_type t,int a,int b){
 B.step=0;int frames_n=0;
 for(;;){frames_n++;
  bool done=false;
  switch(t){
   case BEV_PLAYER_CENTRE:done=step_centre(16.0f,a!=0,b!=0);break;
   case BEV_DOUBLES:done=step_doubles(16.0f);break;
   default:assert(0);
  }
  if(done)break;
  if(frames_n>200)break;}
 return frames_n;
}

int main(void){
 /* ---- 1: a double that begins with a lone Pokemon */
 begin_double(true);
 put(side_of(0),6,false);side_of(1)->present=side_of(1)->info_on=false;put(side_of(2),25,false);put(side_of(3),26,false);
 float x,y,bx,by;bool mini;
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,106)&&near(y,PLAYER_Y));
 CHECK(!player_doubles()&&near(player_split(),0)&&!player_mini(side_of(0)));
 CHECK(player_box(side_of(0),&bx,&by,&mini)&&!mini&&near(bx,310)&&near(by,-72));
 {float a,b;mon_pos(side_of(2),false,&a,&b);CHECK(near(a,236+0-32)&&near(b,84-8-FOE_RAISE));   /* the foes: two columns, as in any double */
  mon_pos(side_of(3),false,&a,&b);CHECK(near(a,236+32)&&near(b,84-FOE_RAISE));
  CHECK(near(info_base_x(side_of(2),false),140)&&near(info_base_x(side_of(3),false),130));
  reset();draw_enemy_info(side_of(2));const img_call *c=find("images/ui/pbinfo_enemy_mini");CHECK(c&&near(c->x,140)&&near(c->y,-141));}
 /* with the other slot's struct empty, nothing else about it changed: the swap hold is off */
 CHECK(!B.flipped&&B.centred&&near(B.centre,1));

 /* ---- 2: the survivor of slot 0 goes to the centre at the end of the turn */
 begin_double(false);
 put(side_of(0),6,false);put(side_of(1),9,false);put(side_of(2),25,false);put(side_of(3),26,false);
 float lx,ly;mon_pos(side_of(0),true,&lx,&ly);
 CHECK(near(lx,106-32)&&near(ly,PLAYER_Y-8));                                   /* LEFT */
 CHECK(player_box(side_of(0),&bx,&by,&mini)&&mini&&near(bx,310)&&near(by,-84));
 side_of(1)->present=side_of(1)->info_on=false;                               /* the partner fainted: its slot is empty, the survivor has not moved */
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,lx)&&near(y,ly));
 CHECK(player_box(side_of(0),&bx,&by,&mini)&&mini&&near(bx,310)&&near(by,-84));
 B.step=0;
 CHECK(!step_centre(0,true,false)&&B.centred&&near(B.centre,0));             /* the boxes at once, the sprite not yet */
 CHECK(player_box(side_of(0),&bx,&by,&mini)&&!mini&&near(bx,310)&&near(by,-72));
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,lx)&&near(y,ly));
 CHECK(!step_centre(250,true,false));
 {float k=ease(EASE_SINE_OUT,.5f);mon_pos(side_of(0),true,&x,&y);CHECK(near(x,lx+32*k)&&near(y,ly+8*k));CHECK(x>lx&&x<106);}
 CHECK(step_centre(250,true,false));
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,106)&&near(y,PLAYER_Y)&&near(B.centre,1));
 {float a,b;mon_pos(side_of(2),false,&a,&b);CHECK(near(a,236-32)&&near(b,84-8-FOE_RAISE));}   /* the foes did not move */
 CHECK(player_box(side_of(0),&bx,&by,&mini)&&!mini&&near(bx,310)&&near(by,-72));

 /* ---- 3: the survivor of slot 1: the swap is not seen, the slide starts from the right column */
 begin_double(false);
 put(side_of(0),6,false);put(side_of(1),9,false);put(side_of(2),25,false);put(side_of(3),26,false);
 float rx,ry,rbx,rby;mon_pos(side_of(1),true,&rx,&ry);CHECK(near(rx,106+32)&&near(ry,PLAYER_Y));   /* RIGHT */
 CHECK(player_box(side_of(1),&rbx,&rby,&mini)&&mini&&near(rbx,320)&&near(rby,-57));
 side_of(0)->present=side_of(0)->info_on=false;                               /* the lead fainted */
 {/* BEV_FIELD_SWAP as run_event plays it: the survivor becomes slot 0 */
  side tmp=B.s[0];B.s[0]=B.s[2];B.s[2]=tmp;if(player_doubles())B.flipped=!B.flipped;}
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,rx)&&near(y,ry));                 /* still on the right mark */
 CHECK(player_box(side_of(0),&bx,&by,&mini)&&mini&&near(bx,rbx)&&near(by,rby));
 B.step=0;
 CHECK(!step_centre(0,true,false));
 CHECK(player_box(side_of(0),&bx,&by,&mini)&&!mini&&near(bx,310)&&near(by,-72));
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,rx)&&near(y,ry));                 /* it starts from where it stood */
 CHECK(!step_centre(250,true,false));
 {float k=ease(EASE_SINE_OUT,.5f);mon_pos(side_of(0),true,&x,&y);CHECK(near(x,rx-32*k)&&near(y,ry));CHECK(x<rx&&x>106);}
 CHECK(step_centre(250,true,false));
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,106)&&near(y,PLAYER_Y)&&!B.flipped);

 /* ---- 4: back to the columns, at once, and the screens that are already there */
 begin_double(true);
 put(side_of(0),6,false);side_of(1)->present=side_of(1)->info_on=false;put(side_of(2),25,false);put(side_of(3),26,false);
 B.step=0;
 CHECK(!step_centre(0,false,false)&&!B.centred);
 CHECK(player_box(side_of(0),&bx,&by,&mini)&&mini&&near(bx,310)&&near(by,-84));   /* the boxes first */
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,106));                            /* the sprite still in the centre */
 CHECK(step_centre(500,false,false));
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,106-32)&&near(y,PLAYER_Y-8));
 put(side_of(1),9,false);mon_pos(side_of(1),true,&x,&y);CHECK(near(x,106+32)&&near(y,PLAYER_Y));
 CHECK(player_box(side_of(1),&bx,&by,&mini)&&mini&&near(bx,320)&&near(by,-57));
 /* b = 1: at once, either way, with no tween to wait for */
 B.step=0;CHECK(step_centre(0,true,true)&&B.centred&&near(B.centre,1));
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,106)&&near(y,PLAYER_Y));
 B.step=0;CHECK(step_centre(0,false,true)&&!B.centred&&near(B.centre,0));
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,106-32));
 /* already there: no tween, no change */
 B.step=0;CHECK(step_centre(0,false,false)&&!B.centred&&near(B.centre,0));
 B.step=0;B.centred=true;B.centre=1;CHECK(step_centre(0,true,false)&&B.centred&&near(B.centre,1));
 CHECK(play(BEV_PLAYER_CENTRE,1,0)>=1);

 /* ---- 5: a double nobody has left, a single battle: what they were */
 begin_double(false);
 put(side_of(0),6,false);put(side_of(1),9,false);
 CHECK(player_doubles()&&near(player_split(),1)&&player_mini(side_of(0)));
 CHECK(player_box(side_of(0),&bx,&by,&mini)&&mini&&near(bx,310)&&near(by,-84));
 CHECK(player_box(side_of(1),&bx,&by,&mini)&&mini&&near(bx,320)&&near(by,-57));
 memset(&B,0,sizeof B);put(side_of(0),6,false);
 CHECK(!player_doubles()&&near(player_split(),0));
 CHECK(player_box(side_of(0),&bx,&by,&mini)&&!mini&&near(bx,310)&&near(by,-72));
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,106)&&near(y,PLAYER_Y));
 
 /* ---- 6: the final boss' second phase: the lead stepped left (BEV_DOUBLES), the ally fell, the lead goes back to the centre */
 memset(&B,0,sizeof B);put(side_of(0),6,false);put(side_of(1),890,true);side_of(2)->present=side_of(2)->info_on=false;
 B.step=0;while(!step_doubles(16.0f)){}
 CHECK(B.late_double&&near(B.split,1));
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,106-32)&&near(y,PLAYER_Y-8));
 put(side_of(2),9,false);side_of(2)->present=side_of(2)->info_on=false;
 B.step=0;CHECK(!step_centre(0,true,false));
 CHECK(player_box(side_of(0),&bx,&by,&mini)&&!mini&&near(bx,310)&&near(by,-72));
 CHECK(step_centre(500,true,false));
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,106)&&near(y,PLAYER_Y));
 /* BEV_DOUBLES on a screen that has been through it (centred or not) does not slide the lead again */
 B.step=0;CHECK(step_doubles(100));

 printf("Lone player layout: %d checks (centred from the start; end-of-turn slide from either column; the swap unseen; back to the columns; doubles, single and final boss unchanged)\n",checks);
 return 0;
}
