/* EggHatchPhase presentation on the two 3DS screens. The generated Pokemon is
 * carried by the event, independently of the battle party and remaining eggs. */
#include "egg_hatch_screen.h"
#include "menus.h"
#include "i18n.h"
#include "mui.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct {
  menu_mon mon;
  menu_sprite sprite;
  int tier;uint8_t previous_ivs[6];uint16_t egg_move;bool rare;
  float time;
  bool revealed,closed;
  void (*done)(void *);
  void *arg;
} E;
static void reveal(void){
  if(E.revealed)return;
  E.revealed=true;E.time=0;ui_play_sound("se/egg_hatch");
}
static bool input(ui_screen *s,ui_button b){
  (void)s;
  if(b!=BTN_ACTION&&b!=BTN_CANCEL&&b!=BTN_SUBMIT)return false;
  if(ui_input_repeating())return true;
  if(!E.revealed){reveal();return true;}
  if(E.time<500||E.closed)return true;
  E.closed=true;void (*done)(void *)=E.done;void *arg=E.arg;
  ui_pop();if(done)done(arg);return true;
}
static void update(ui_screen *s,float dt){
  (void)s;float before=E.time;E.time+=dt;
  if(!E.revealed){
    if(before<2000&&E.time>=2000)ui_play_sound("se/egg_crack");
    if(before<3500&&E.time>=3500)ui_play_sound("se/egg_crack");
    if(E.time>=5000)reveal();
  }
}
static void leave(ui_screen *s){(void)s;menu_sprite_free(&E.sprite);}
/* Pinned StatsContainer uses this axis order and the 0.825 horizontal factor. */
static const int chart_order[6]={0,1,2,5,4,3};
static const float chart_axis[6][2]={{0,-1},{.825f,-.5f},{.825f,.5f},{-.825f,-.5f},{-.825f,.5f},{0,1}};
static void polygon(const float pts[6][2],rr_color color){
  for(int y=-40;y<=40;y++){
    float cuts[6];int n=0;
    for(int i=0;i<6;i++){
      int j=(i+1)%6;float a=pts[i][1],b=pts[j][1];
      if((a<=y&&b>y)||(b<=y&&a>y))cuts[n++]=pts[i][0]+(y-a)*(pts[j][0]-pts[i][0])/(b-a);
    }
    if(n==2){float left=fminf(cuts[0],cuts[1]),right=fmaxf(cuts[0],cuts[1]);
      rr_rect(160+left,85+y,right-left,1,color);}
  }
}
/* Fixed label columns leave room for the longer special-stat names and IV gains.
 * Use a dark opaque panel so every window theme has the same readable contrast. */
static void hatch_text(float x,float y,const char *text,bool gold){
  int w=mui_text_width(TS_WINDOW,text);
  mui_text_c(TS_WINDOW,x-w*.5f,y,text,gold?RR_RGBA(255,216,112,255):RR_RGBA(248,248,248,255),RR_RGBA(16,24,32,255));
}
static void hatch_info(void){
  extern const char* const menu_nature_keys[25];
  mui_window(8,-20,304,228);
  rr_rect(16,-12,288,212,RR_RGBA(32,43,56,255));
  char key[96],text[120];
  snprintf(key,sizeof key,"nature:%s",menu_nature_keys[E.mon.nature<25?E.mon.nature:0]);
  snprintf(text,sizeof text,"Nature: %s",mui_t(key));hatch_text(160,-10,text,false);
  float outer[6][2],inner[6][2];
  for(int i=0;i<6;i++){int st=chart_order[i];for(int c=0;c<2;c++){
    outer[i][c]=40*chart_axis[st][c];inner[i][c]=outer[i][c]*E.mon.ivs[st]/31.0f;
  }}
  polygon(outer,RR_RGBA(22,38,55,255));
  polygon(inner,RR_RGBA(62,155,114,255));
  static const char* const names[]={"HP","ATK","DEF","SP.ATK","SP.DEF","SPEED"};
  static const float label_xy[6][2]={{160,12},{258,52},{258,102},{62,52},{62,102},{160,130}};
  for(int st=0;st<6;st++){
    float x=label_xy[st][0],y=label_xy[st][1];
    hatch_text(x,y,names[st],false);
    int value=E.mon.ivs[st],diff=value-E.previous_ivs[st];
    if(diff>0)snprintf(text,sizeof text,"%d (+%d)",value,diff);else snprintf(text,sizeof text,"%d",value);
    hatch_text(x,y+16,text,value==31);
  }
  if(E.egg_move){snprintf(text,sizeof text,"%s egg move: %s",E.rare?"Rare":"New",i18n_move_name(E.egg_move));
    hatch_text(160,174,text,false);}
  mui_text_c(TS_INSTRUCTIONS_TEXT,18,197,"IVs / improvement    A / B: Continue",RR_RGBA(248,248,248,255),RR_RGBA(16,24,32,255));
}

static void draw(ui_screen *s,rr_screen which){
  (void)s;mui_set_origin(which,which==RR_TOP?40:0,30);
  rr_rect(which==RR_TOP?-40:0,-30,which==RR_TOP?400:320,240,RR_RGBA(0x60,0x6c,0x78,255));
  if(which==RR_BOTTOM){
    if(E.revealed)hatch_info();
    else {
      mui_window(12,44,296,88);
      mui_text(TS_WINDOW,24,56,"Something is moving inside!");
      mui_text(TS_WINDOW,24,82,"Your egg is hatching...");
      mui_text(TS_WINDOW,24,108,"A / B: Reveal");
    }
    return;
  }
  if(!E.revealed){
    char frame[24];snprintf(frame,sizeof frame,"egg_%d",E.tier);
    float shake=E.time<2000?0:sinf(E.time*.04f)*(E.time<3500?2:4);
    mui_image("egg/egg",frame,160+shake,90,.5f,.5f);
    if(E.time>=2000){snprintf(frame,sizeof frame,"%d",E.time<3500?0:E.time<4500?2:4);
      mui_image("egg/egg_crack",frame,160+shake,90,.5f,.5f);}
  }else{
    const rr_frame *f=menu_sprite_frame(&E.sprite,(uint32_t)E.time);
    if(f){rr_draw_opts o=RR_DEFAULT_OPTS;o.origin_x=o.origin_y=.5f;rr_draw(f,160,90,&o);}
    else mui_text(TS_WINDOW,100,90,i18n_species_name(E.mon.species));
    if(E.time<250)rr_rect(-40,-30,400,240,RR_RGBA(255,255,255,(int)(255*(1-E.time/250))));
  }
}
static ui_screen screen={.name="egg-hatch",.input=input,.update=update,.draw=draw,.leave=leave};
void egg_hatch_open(const gc_event *e,void (*done)(void *),void *arg){
  memset(&E,0,sizeof E);E.mon.species=e->a;E.mon.form=e->b;E.mon.gender=e->c;
  E.mon.nature=e->egg_nature;memcpy(E.mon.ivs,e->egg_ivs,6);memcpy(E.previous_ivs,e->egg_previous_ivs,6);
  E.egg_move=e->egg_move;E.rare=e->egg_move_rare;
  E.mon.shiny=e->egg_shiny;E.mon.variant=e->egg_variant;E.tier=e->egg_tier;
  E.done=done;E.arg=arg;menu_sprite_load(&E.sprite,&E.mon,false);ui_push(&screen);
}
