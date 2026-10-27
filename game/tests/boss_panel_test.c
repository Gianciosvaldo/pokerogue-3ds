/* The boss's info box (EnemyBattleInfo.updateBossSegments, updateBossSegmentDividers; Pokemon.showInfo's slide): the real renderer is run
 * with a recording stand-in for the drawing calls and compared with the layout of upstream's pinned source:
 *   enemy-battle-info.ts   box pbinfo_enemy_boss (178 px), name and its icons -48, level -2, HP bar -38 / -2 up on overlay_hp_boss (86 px),
 *                          label -38 / -1 up on overlay_hp_label_boss, one divider per segment but the first at
 *                          round(maxHp / segments * s) / maxHp of the bar, white while bossSegmentIndex >= s, else 0x404040,
 *                          1 px wide, from 1 px under the bar's top, bar height less 1 tall;
 *   pokemon.ts showInfo    the box rests 48 px right of a mini one in singles (140 -> 188) and slides in by 246, not 150.
 * Transcribed from the pinned source (no upstream run: the layout numbers are constants there, not the output of a ported function). The
 * mini box is drawn as it was before, so everything else on the screen stays where it was. */
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
 /* the pinned sizes: overlay_hp 48x4, overlay_hp_boss 86x4 (the frames of both are high, medium, low) */
 frames[i].src_w=strstr(key,"overlay_hp_boss")?86:strstr(key,"overlay_hp")?48:8;frames[i].src_h=strstr(key,"overlay_hp")?4:8;return &frames[i];
}
typedef struct{char key[160],frame[20];float x,y,ox,oy;}img_call;
static img_call imgs[64];static int ni;
void img_draw(const char *key,const char *frame,float x,float y,float ox,float oy){
 assert(ni<64);snprintf(imgs[ni].key,sizeof imgs[ni].key,"%s",key);snprintf(imgs[ni].frame,sizeof imgs[ni].frame,"%s",frame?frame:"");
 imgs[ni].x=x;imgs[ni].y=y;imgs[ni].ox=ox;imgs[ni].oy=oy;ni++;
}
void img_draw_opts(const char *key,const char *frame,float x,float y,const rr_draw_opts *o){(void)o;img_draw(key,frame,x,y,0,0);}
typedef struct{float x,y,w,h;rr_color c;}rect_call;
static rect_call rects[64];static int nr;
void rr_rect(float x,float y,float w,float h,rr_color c){assert(nr<64);rects[nr++]=(rect_call){x,y,w,h,c};}
typedef struct{const rr_frame *f;float x,y;int w,h;}crop_call;
static crop_call crops[8];static int nc;
void rr_draw_cropped(const rr_frame *f,float x,float y,int w,int h,const rr_draw_opts *o){(void)o;assert(nc<8);crops[nc++]=(crop_call){f,x,y,w,h};}
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)o;}
static float name_x=-1e9f;
float text_draw(text_style st,float x,float y,const char *s,const text_opts *o){(void)st;(void)y;(void)o;if(name_x<-1e8f)name_x=x;return (float)strlen(s)*4;}
float text_width(text_style st,const char *s,const text_opts *o){(void)st;(void)o;return (float)strlen(s)*4;}
text_opts text_defaults(void){text_opts o;memset(&o,0,sizeof o);return o;}
rr_color text_color(text_style s,bool shadow){(void)s;(void)shadow;return 0;}

static void near(float a,float b){if(fabsf(a-b)>.001f){fprintf(stderr,"%g != %g\n",a,b);assert(0);}}
static void reset(void){ni=nr=nc=0;name_x=-1e9f;}
static const img_call *find(const char *key){for(int i=0;i<ni;i++)if(!strcmp(imgs[i].key,key))return &imgs[i];return NULL;}
static int checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"boss panel FAIL line %d: %s\n",__LINE__,#x);return 1;}checks++;}while(0)

static void foe(side *sd,bool boss,int segments,int index,int hp,int max_hp){
 memset(sd,0,sizeof *sd);sd->present=sd->info_on=true;sd->mon.species=25;sd->mon.level=40;sd->mon.max_hp=max_hp;sd->mon.hp=hp;
 sd->hp_shown=(float)hp;sd->mon.boss=boss;sd->mon.boss_segments=segments;sd->mon.boss_segment_index=index;snprintf(sd->mon.name,sizeof sd->mon.name,"Pikachu");
}

/* upstream's divider position for segment s, in canvas px from the bar's left edge (86 wide): round(maxHp / segments * s) / maxHp * 86 */
static double up_divider(int max_hp,int segments,int s){return floor(max_hp/(double)segments*s+0.5)/max_hp*86;}

int main(void){
 /* ---- the mini box, unchanged: pbinfo_enemy_mini, name at -124, bar at (-71, 4.5), label at (-72, 1.5), no dividers */
 {
  memset(&B,0,sizeof B);side *sd=side_of(1);foe(sd,false,0,0,30,60);reset();
  const float bx=info_base_x(sd,false),by=-141;near(bx,140);
  draw_enemy_info(sd);
  CHECK(find("images/ui/pbinfo_enemy_mini")&&!find("images/ui/pbinfo_enemy_boss"));
  const img_call *box=find("images/ui/pbinfo_enemy_mini");near(box->x,bx);near(box->y,by);near(box->ox,1);near(box->oy,.5f);
  CHECK(nc==1&&!strcmp(names[crops[0].f-frames],"medium")&&strstr(keys[crops[0].f-frames],"images/ui/overlay_hp")&&!strstr(keys[crops[0].f-frames],"boss"));
  near(crops[0].x,bx-71);near(crops[0].y,by+4.5f);CHECK(crops[0].w==24);
  const img_call *lab=find("images/ui/text_images/en/battle_ui/overlay_hp_label");CHECK(lab&&!find("images/ui/text_images/en/battle_ui/overlay_hp_label_boss"));near(lab->x,bx-72);near(lab->y,by+1.5f);
  near(name_x,bx-124);CHECK(nr==0);
 }
 /* ---- a boss, full shields: the wide box 48 px to the right, name -172, bar (-109, 2.5) 86 wide, label (-110, 0.5), dividers all white */
 static const int max_hps[]={23,47,61,150,1000};
 for(int segments=2;segments<=7;segments++)for(int hi=0;hi<5;hi++){
  /* (a small maximum makes one HP several pixels, where rounding the threshold and flooring it land on different ones) */
  memset(&B,0,sizeof B);side *sd=side_of(1);const int max_hp=max_hps[hi]+(hi==3?segments*37:0);foe(sd,true,segments,segments-1,max_hp,max_hp);reset();
  const float bx=info_base_x(sd,false),by=-141;near(bx,188);
  draw_enemy_info(sd);
  const img_call *box=find("images/ui/pbinfo_enemy_boss");CHECK(box&&!find("images/ui/pbinfo_enemy_mini"));near(box->x,bx);near(box->y,by);near(box->ox,1);near(box->oy,.5f);
  CHECK(nc==1&&!strcmp(names[crops[0].f-frames],"high")&&!strcmp(keys[crops[0].f-frames],"images/ui/overlay_hp_boss"));
  near(crops[0].x,bx-109);near(crops[0].y,by+2.5f);CHECK(crops[0].w==86&&crops[0].h==4);
  const img_call *lab=find("images/ui/text_images/en/battle_ui/overlay_hp_label_boss");CHECK(lab&&!find("images/ui/text_images/en/battle_ui/overlay_hp_label"));
  near(lab->x,bx-110);near(lab->y,by+.5f);near(lab->ox,1);near(lab->oy,0);
  near(name_x,bx-172);
  const img_call *lv=find("images/ui/text_images/en/battle_ui/overlay_lv");CHECK(lv);near(lv->x,bx-52+5.5f);
  CHECK(nr==segments-1);
  for(int s=1;s<segments;s++){
   const rect_call *r=&rects[s-1];
   near(r->x,floorf((float)(bx-109+up_divider(max_hp,segments,s))));near(r->y,by+2.5f+1);near(r->w,1);near(r->h,3);
   CHECK(r->c==RR_RGBA(0xff,0xff,0xff,255));
  }
 }
 /* ---- broken shields go dark, from the bottom up: index k leaves dividers 1..k white and k+1.. dark */
 {
  const int segments=5,max_hp=1000;
  for(int index=0;index<segments;index++){
   memset(&B,0,sizeof B);side *sd=side_of(1);foe(sd,true,segments,index,max_hp*(index+1)/segments,max_hp);reset();draw_enemy_info(sd);
   CHECK(nr==segments-1);
   for(int s=1;s<segments;s++)CHECK(rects[s-1].c==(s<=index?RR_RGBA(0xff,0xff,0xff,255):RR_RGBA(0x40,0x40,0x40,255)));
  }
  /* the bar's length follows the HP on the 86 px bar, rounded like the mini bar's */
  memset(&B,0,sizeof B);side *sd=side_of(1);foe(sd,true,segments,1,500,max_hp);reset();draw_enemy_info(sd);CHECK(crops[0].w==43&&!strcmp(names[crops[0].f-frames],"medium"));
  foe(sd,true,segments,0,1,max_hp);reset();draw_enemy_info(sd);CHECK(crops[0].w==1&&!strcmp(names[crops[0].f-frames],"low"));
  /* a boss that is not segmented (an old caller's is_boss alone) has the box and bar, no dividers */
  foe(sd,true,0,0,max_hp,max_hp);reset();draw_enemy_info(sd);CHECK(find("images/ui/pbinfo_enemy_boss")&&nr==0);
 }
 /* ---- doubles keep their own boss shift (102 -> 150) and the slide is 246 for a boss, 150 otherwise, both ways */
 {
  memset(&B,0,sizeof B);B.setup.doubles=true;side *sd=side_of(1);foe(sd,false,0,0,10,10);near(info_base_x(sd,false),102);near(info_slide(sd,false),150);
  foe(sd,true,3,2,10,10);near(info_base_x(sd,false),150);near(info_slide(sd,false),246);near(info_slide(sd,true),150);
  B.setup.doubles=false;near(info_base_x(sd,false),188);near(info_base_x(sd,true),310);
 }
 printf("Boss info box: %d checks (mini box unchanged; boss box, bar, label, name, level, dividers for 2..7 segments, shield colours, slide)\n",checks);
 return 0;
}
