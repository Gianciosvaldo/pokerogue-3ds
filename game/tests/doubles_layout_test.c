/* The foes' side of a double battle on the top screen (lorenzo, 2026-11-09: "the enemy pokemon are way too high up and the platform is still in
 * the singles placement, making it look like they are floating out of the screen").
 * The foes stand FOE_RAISE higher than in a single battle so they clear the player's two full-sized boxes. The platform was raised with the
 * wrong sign (y grows downwards: it went down 36 px while the Pokemon went up 36), so they floated over a platform left far behind, and the
 * field was zoomed 1.1x, which cut the tallest off at the top. The real renderer is run with a recording stand-in for the drawing calls:
 *   1  a single battle is what it was: nothing raised, the platform at y 0, the doll and the trainer on their old marks, zoom 1.2
 *   2  a doubles encounter: the foe platform (and the old one sliding out) is drawn FOE_RAISE UP (negative y), under the foes that stand there
 *   3  the trainer's sprite goes up with the platform it stands on; the Substitute doll stands on its Pokemon's own column (it was drawn on
 *      the singles mark whatever the column)
 *   4  the field is drawn smaller (FIELD_ZOOM_DOUBLES < the single zoom) and, at that zoom, the foes' feet clear the player's top box (field y
 *      43) and the top of an 80 px sprite stays inside the screen's 30 px top band; the G-Max zoom-out comes on top
 * The numbers (platform images drawn at (0, y), the zoom about (160, 132), the box places) are the app's own constants, not an upstream
 * output; no upstream run. */
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

static rr_frame frame96;
const rr_frame *img_frame(const char *key,const char *name){(void)key;(void)name;return &frame96;}
const rr_frame *img_anim_frame(const char *key,int frame){(void)key;(void)frame;return &frame96;}
bool img_has(const char *key){(void)key;return true;}
typedef struct{char key[96];float x,y,zoom;}img_call;
static img_call imgs[64];static int ni;
static float cur_zoom=1;
void rr_set_zoom(float z,float cx,float cy){(void)cx;(void)cy;cur_zoom=z;}
void img_draw(const char *key,const char *frame,float x,float y,float ox,float oy){
 (void)frame;(void)ox;(void)oy;
 if(ni<64){snprintf(imgs[ni].key,sizeof imgs[ni].key,"%s",key);imgs[ni].x=x;imgs[ni].y=y;imgs[ni].zoom=cur_zoom;ni++;}
}
void img_draw_opts(const char *key,const char *frame,float x,float y,const rr_draw_opts *o){(void)o;img_draw(key,frame,x,y,0,0);}
void rr_rect(float x,float y,float w,float h,rr_color c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void rr_draw_cropped(const rr_frame *f,float x,float y,int w,int h,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)w;(void)h;(void)o;}
typedef struct{float x,y;}rr_call;
static rr_call draws[64];static int nd;
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){(void)f;(void)o;if(nd<64){draws[nd].x=x;draws[nd].y=y;nd++;}}
void rr_set_tone(int r,int g,int b){(void)r;(void)g;(void)b;}
void rr_set_blend(rr_blend b){(void)b;}
int rr_frame_alpha(const rr_frame *f,int x,int y){(void)f;(void)x;(void)y;return 255;}
float ui_now(void){return 0;}
const rr_frame *menu_sprite_frame(const menu_sprite *s,uint32_t ms){(void)s;(void)ms;return &frame96;}
void ba_draw(ba_layer layer,const ba_mon field[4]){(void)layer;(void)field;}
bool ba_hidden(int battler){(void)battler;return false;}
float text_draw(text_style st,float x,float y,const char *s,const text_opts *o){(void)st;(void)x;(void)y;(void)o;return (float)strlen(s)*4;}
float text_width(text_style st,const char *s,const text_opts *o){(void)st;(void)o;return (float)strlen(s)*4;}
text_opts text_defaults(void){text_opts o;memset(&o,0,sizeof o);return o;}
rr_color text_color(text_style s,bool shadow){(void)s;(void)shadow;return 0;}

static int checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"doubles layout FAIL line %d: %s\n",__LINE__,#x);return 1;}checks++;}while(0)
static bool near(float a,float b){return fabsf(a-b)<=.001f;}
static void reset(void){ni=0;nd=0;cur_zoom=1;}
static const img_call *find(const char *key){for(int i=0;i<ni;i++)if(!strcmp(imgs[i].key,key))return &imgs[i];return NULL;}

static void put(side *sd,int species){
 memset(sd,0,sizeof *sd);sd->present=sd->info_on=true;sd->sprite_on=true;sd->mon.species=species;sd->mon.level=10;sd->mon.max_hp=30;sd->mon.hp=30;sd->hp_shown=30;
 sd->scale=1;snprintf(sd->mon.name,sizeof sd->mon.name,"Mon%d",species);
}

/* where the field's zoom about (160, 132) puts a field y */
static float zoomed_y(float y,float zoom){return 132+(y-132)*zoom;}
static float field_zoom_now(void){reset();field_zoom(true);return cur_zoom;}

int main(void){
 frame96.src_w=frame96.w=96;frame96.src_h=frame96.h=96;
 /* ---- 1: a single battle is what it was */
 memset(&B,0,sizeof B);B.field_scale=1;snprintf(B.biome,sizeof B.biome,"plains");
 put(side_of(0),6);put(side_of(2),25);
 CHECK(near(foe_raise(),0));
 CHECK(near(field_zoom_now(),FIELD_ZOOM));
 reset();draw_field();
 {const img_call *b=find("images/arenas/plains_b"),*a=find("images/arenas/plains_a");CHECK(b&&a&&near(b->y,0)&&near(a->y,0)&&near(b->zoom,FIELD_ZOOM));}
 {float x,y,fx,fy;field_pos(side_of(2),false,&x,&y);mon_pos(side_of(2),false,&fx,&fy);CHECK(near(x,236+B.enemy_slide)&&near(y,84)&&near(fx,x)&&near(fy,y));}

 /* ---- 2: a doubles encounter: the platform goes UP with the foes (y grows downwards: a negative y) */
 memset(&B,0,sizeof B);B.field_scale=1;B.setup.doubles=true;snprintf(B.biome,sizeof B.biome,"plains");
 put(side_of(0),6);put(side_of(1),7);put(side_of(2),25);put(side_of(3),26);
 B.arena_old_on=true;B.arena_old_x=20;B.arena_enemy_x=0;
 CHECK(near(foe_raise(),FOE_RAISE)&&FOE_RAISE>0);
 reset();draw_field();
 {
  const img_call *b=NULL;int nb=0;
  for(int i=0;i<ni;i++)if(!strcmp(imgs[i].key,"images/arenas/plains_b")){nb++;b=&imgs[i];CHECK(near(imgs[i].y,-FOE_RAISE));}
  CHECK(nb==2&&b);                                   /* the platform sliding out and the one in */
  const img_call *a=find("images/arenas/plains_a");CHECK(a&&near(a->y,0));  /* the player's platform stays */
 }
 {float x,y,fx,fy;
  field_pos(side_of(2),false,&x,&y);CHECK(near(y,84-8-FOE_RAISE));       /* LEFT: (-32, -8), then the raise */
  field_pos(side_of(3),false,&x,&y);CHECK(near(y,84-FOE_RAISE)&&near(x,236+B.enemy_slide+32));
  mon_pos(side_of(3),false,&fx,&fy);CHECK(near(fx,x)&&near(fy,y)); /* no sprite gap, no catch, no doll: the field position */
 }

 /* ---- 3: the trainer goes up with the platform; the doll stands on its Pokemon's own column */
 B.trainer_visible=true;snprintf(B.setup.trainer_keys[0],sizeof B.setup.trainer_keys[0],"images/trainer/test");
 reset();draw_trainers();CHECK(nd==1&&near(draws[0].y,80-FOE_RAISE));
 for(int col=0;col<4;col++){
  side *sd=side_of(col);const bool player=col%2==0;
  sd->sub_on=true;sd->sub_alpha=1;
  float x,y;field_pos(sd,player,&x,&y);
  reset();draw_sub(sd,player,false);CHECK(nd==1&&near(draws[0].x,x)&&near(draws[0].y,y));
 }
 {float x2,y2,x3,y3;field_pos(side_of(2),false,&x2,&y2);field_pos(side_of(3),false,&x3,&y3);CHECK(!near(x2,x3));} /* the two foes' dolls are apart */
 B.setup.doubles=false;reset();B.trainer_visible=true;draw_trainers();CHECK(near(draws[0].y,80));B.setup.doubles=true;

 /* ---- 4: smaller field; at that zoom the foes clear the player's top box and fit under the top of the screen */
 {
  const float z=field_zoom_now();
  CHECK(near(z,FIELD_ZOOM_DOUBLES)&&z<FIELD_ZOOM);
  float x,y;field_pos(side_of(2),false,&x,&y);              /* the foe that stands higher (LEFT) */
  const float feet2=zoomed_y(y,z);
  field_pos(side_of(3),false,&x,&y);
  const float feet3=zoomed_y(y,z);
  const float box_top=180+player_info_y(side_of(0))-21;      /* the player's lead box: centre -116 under the field's bottom, 42 tall */
  CHECK(near(box_top,43));
  CHECK(feet3<=box_top+3&&feet2<=feet3);                     /* standing on or above the top of the boxes (the platform's rim is a few px under the feet) */
  CHECK(feet2-80*z>=-30);                                    /* a sprite 80 px tall (most are shorter; a full 96 px cell is the exception) fits in the 30 px band above the field */
  /* the zoomed-out G-Max field only makes it smaller, so it still fits */
  B.field_scale=.75f;{const float z2=field_zoom_now();CHECK(near(z2,.75f*FIELD_ZOOM_DOUBLES));CHECK(zoomed_y(y,z2)-80*z2>=-30);}
 }
 printf("Doubles layout: %d checks (single battle unchanged; foe platform raised with the foes, not lowered; trainer and doll on their marks; foes clear the player's boxes and fit the screen)\n",checks);
 return 0;
}
