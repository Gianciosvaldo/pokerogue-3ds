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
  int tier;
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
static void draw(ui_screen *s,rr_screen which){
  (void)s;mui_set_origin(which,which==RR_TOP?40:0,30);
  rr_rect(which==RR_TOP?-40:0,-30,which==RR_TOP?400:320,240,RR_RGBA(0xe8,0xf0,0xe0,255));
  if(which==RR_BOTTOM){
    mui_window(12,44,296,88);
    mui_text(TS_WINDOW,24,56,E.revealed?"A Pokemon hatched!":"Something is moving inside!");
    mui_text(TS_WINDOW,24,82,E.revealed?i18n_species_name(E.mon.species):"Your egg is hatching...");
    mui_text(TS_WINDOW,24,108,E.revealed?"A / B: Continue":"A / B: Reveal");
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
  E.mon.shiny=e->egg_shiny;E.mon.variant=e->egg_variant;E.tier=e->egg_tier;
  E.done=done;E.arg=arg;menu_sprite_load(&E.sprite,&E.mon,false);ui_push(&screen);
}
