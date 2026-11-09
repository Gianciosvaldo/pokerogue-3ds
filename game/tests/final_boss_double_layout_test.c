/* The final boss' second phase on the battle screen (initFinalBossPhaseTwo): Eternamax Eternatus turns the fight into a double, the player's
 * lead steps to the left (ToggleDoublePositionPhase -> setFieldPosition(LEFT, 500)) and the second Pokemon is sent out to its right. The one boss
 * is not moved. The screen drew the player's side from setup.doubles alone, which is fixed when the wave begins, so the ally was sent out onto
 * the lead's place with its info box on top of the lead's: two Pokemon overlapping and one HP bar (lorenzo, 2026-11-01).
 * The real renderer is run with a recording stand-in for the drawing calls:
 *   1  the single layout is what it was (lead at 106, box 310 / -72, boss box 188 / -141, nothing raised)
 *   2  BEV_DOUBLES: the lead slides to FieldPosition.LEFT (-32, -8) over 500 ms, Sine.easeOut; the player's two boxes take their doubles places
 *      (310, -84 for the lead and 320, -57 for the ally, the mini boxes: upstream's setMini and setOffset); the ally stands at RIGHT (+32, 0); two different Pokemon, two different boxes
 *   3  the foe stays where it was: same position, same box, no FOE_RAISE, no doubles box shift (102 / 150), the zoom is the single one
 *   4  a doubles encounter (setup.doubles) is exactly what it was, and BEV_DOUBLES does nothing there
 * Positions are transcribed from the pinned source (pokemon.ts getFieldPositionOffset / setFieldPosition, battle-info.ts setOffset): the layout
 * numbers are constants there, not the output of a ported function. No upstream run. */
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
float text_draw(text_style st,float x,float y,const char *s,const text_opts *o){(void)st;(void)x;(void)y;(void)o;return (float)strlen(s)*4;}
float text_width(text_style st,const char *s,const text_opts *o){(void)st;(void)o;return (float)strlen(s)*4;}
text_opts text_defaults(void){text_opts o;memset(&o,0,sizeof o);return o;}
rr_color text_color(text_style s,bool shadow){(void)s;(void)shadow;return 0;}

static int checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"final boss doubles layout FAIL line %d: %s\n",__LINE__,#x);return 1;}checks++;}while(0)
static bool near(float a,float b){return fabsf(a-b)<=.001f;}
static void reset(void){ni=0;}
static const img_call *find(const char *key){for(int i=0;i<ni;i++)if(!strcmp(imgs[i].key,key))return &imgs[i];return NULL;}

static void put(side *sd,int species,bool boss){
 memset(sd,0,sizeof *sd);sd->present=sd->info_on=true;sd->mon.species=species;sd->mon.level=100;sd->mon.max_hp=300;sd->mon.hp=300;sd->hp_shown=300;
 sd->mon.boss=boss;sd->mon.boss_segments=boss?4:0;sd->scale=1;snprintf(sd->mon.name,sizeof sd->mon.name,"Mon%d",species);
}

/* one frame's drawn position of a player's info box: the first pbinfo_player the call draws */
static bool player_box(side *sd,float *x,float *y){reset();draw_player_info(sd);const img_call *c=find("images/ui/pbinfo_player");if(!c)c=find("images/ui/pbinfo_player_mini");if(!c)return false;*x=c->x;*y=c->y;return true;}

int main(void){
 /* ---- 1: the single fight, as it was: the lead and the one boss */
 memset(&B,0,sizeof B);
 put(side_of(0),6,false);put(side_of(1),9,false);side_of(1)->present=side_of(1)->info_on=false;put(side_of(2),890,true);
 float lx0,ly0,fx0,fy0;mon_pos(side_of(0),true,&lx0,&ly0);mon_pos(side_of(2),false,&fx0,&fy0);
 CHECK(near(lx0,106)&&near(ly0,PLAYER_Y));
 CHECK(near(fx0,236+B.enemy_slide)&&near(fy0,84));
 CHECK(!player_doubles()&&near(player_split(),0));
 {float bx,by;CHECK(player_box(side_of(0),&bx,&by)&&near(bx,310)&&near(by,-72));}
 CHECK(near(info_base_x(side_of(2),false),188));
 reset();draw_enemy_info(side_of(2));{const img_call *c=find("images/ui/pbinfo_enemy_boss");CHECK(c&&near(c->x,188)&&near(c->y,-141));}

 /* ---- 2: BEV_DOUBLES: nothing moves before it, the lead slides left over 500 ms, the ally is then sent out to its right */
 memset(&B,0,sizeof B);
 put(side_of(0),6,false);side_of(1)->present=side_of(1)->info_on=false;put(side_of(2),890,true);
 mon_pos(side_of(0),true,&lx0,&ly0);mon_pos(side_of(2),false,&fx0,&fy0);
 B.step=0;
 bool done=step_doubles(0);
 CHECK(!done&&B.late_double&&player_doubles()&&near(B.split,0));
 float x,y;
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,lx0)&&near(y,ly0)); /* no sprite has moved yet */
 /* the boxes change at once (setMini, setOffset), the sprite after it */
 {float bx,by;CHECK(player_box(side_of(0),&bx,&by)&&near(bx,310)&&near(by,-84));}
 done=step_doubles(250);
 CHECK(!done);
 {float k=ease(EASE_SINE_OUT,.5f);mon_pos(side_of(0),true,&x,&y);CHECK(near(x,lx0-32*k)&&near(y,ly0-8*k));CHECK(x<lx0&&x>lx0-32);}
 done=step_doubles(250);
 CHECK(done);
 mon_pos(side_of(0),true,&x,&y);CHECK(near(x,lx0-32)&&near(y,ly0-8)); /* FieldPosition.LEFT: (-32, -8) */
 /* the ally: sent out after it (BEV_SUMMON sets the data on the second player side) */
 put(side_of(1),9,false);
 {float ax,ay;mon_pos(side_of(1),true,&ax,&ay);CHECK(near(ax,lx0+32)&&near(ay,ly0));CHECK(ax-x==64); /* FieldPosition.RIGHT: (+32, 0) */}
 {
  float bx0,by0,bx1,by1;
  CHECK(player_box(side_of(0),&bx0,&by0)&&player_box(side_of(1),&bx1,&by1));
  CHECK(near(bx0,310)&&near(bx1,320)&&near(by0,-84)&&near(by1,-57));   /* the doubles places, two mini boxes: one HP bar for each Pokemon */
  CHECK(by0-21+24<by1-21+2);                                           /* the mini art is rows 2..24 of 42: the two do not overlap */
  CHECK(by1-21+24<=-48&&by0-21>=-180);                                 /* both above the command strip (its art's last row) */
 }

 /* ---- 3: the boss stays where it was */
 {
  float fx,fy;mon_pos(side_of(2),false,&fx,&fy);CHECK(near(fx,fx0)&&near(fy,fy0));
  CHECK(near(info_base_x(side_of(2),false),188));
  reset();draw_enemy_info(side_of(2));const img_call *c=find("images/ui/pbinfo_enemy_boss");CHECK(c&&near(c->x,188)&&near(c->y,-141));
  /* (the platform is raised only for two foes; the zoom is the single one) */
  CHECK(near(B.setup.doubles?FOE_RAISE:0,0));
 }
 /* a second BEV_DOUBLES (it never comes, but a restart of the events must not slide the lead again) does nothing */
 B.step=0;{float s0=B.split;CHECK(step_doubles(100)&&near(B.split,s0));}

 /* ---- 4: a doubles encounter is what it was (both sides in two columns from the start), and BEV_DOUBLES does nothing there */
 memset(&B,0,sizeof B);B.setup.doubles=true;
 put(side_of(0),6,false);put(side_of(1),9,false);put(side_of(2),25,false);put(side_of(3),26,false);
 {
  float x0,y0,x2,y2;mon_pos(side_of(0),true,&x0,&y0);mon_pos(side_of(1),true,&x2,&y2);
  CHECK(near(x0,106-32)&&near(y0,PLAYER_Y-8)&&near(x2,106+32)&&near(y2,PLAYER_Y));
  float a,b;mon_pos(side_of(2),false,&a,&b);CHECK(near(a,236+0-32)&&near(b,84-8-FOE_RAISE));
  mon_pos(side_of(3),false,&a,&b);CHECK(near(a,236+32)&&near(b,84-FOE_RAISE));
  CHECK(near(info_base_x(side_of(2),false),140)&&near(info_base_x(side_of(3),false),130)&&near(info_base_x(side_of(0),true),310)&&near(info_base_x(side_of(1),true),320));
  CHECK(near(player_info_y(side_of(0)),-84)&&near(player_info_y(side_of(1)),-57));
  B.step=0;CHECK(step_doubles(0)&&!B.late_double&&near(B.split,0));
  mon_pos(side_of(0),true,&a,&b);CHECK(near(a,x0)&&near(b,y0));
 }
 printf("Final boss doubles layout: %d checks (single layout unchanged; BEV_DOUBLES slide, two player boxes, ally place; boss not moved; doubles encounter unchanged)\n",checks);
 return 0;
}
