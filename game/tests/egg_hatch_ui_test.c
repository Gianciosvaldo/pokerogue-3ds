/* Actual hatch screen, with draw calls recorded instead of a GPU. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/egg_hatch_screen.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
static ui_screen *top;static menu_mon loaded;static rr_frame frame;
static int eggs,cracks,mons,pops,frees,done_count;static bool repeating;
void ui_push(ui_screen *s){assert(!top);top=s;}
void ui_pop(void){ui_screen *s=top;assert(s);top=NULL;pops++;s->leave(s);}
bool ui_input_repeating(void){return repeating;}
void ui_play_sound(const char *s){assert(s);}
void mui_set_origin(rr_screen s,int x,int y){(void)s;(void)x;(void)y;}
void rr_rect(float x,float y,float w,float h,rr_color c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void mui_window(float x,float y,float w,float h){(void)x;(void)y;(void)w;(void)h;}
int mui_text(mui_style s,float x,float y,const char *text){assert(text);(void)s;(void)x;(void)y;return 0;}
bool mui_image(const char *key,const char *f,float x,float y,float ox,float oy){
 assert(f);if(!strcmp(key,"egg/egg"))eggs++;else if(!strcmp(key,"egg/egg_crack"))cracks++;else assert(0);
 (void)x;(void)y;(void)ox;(void)oy;return true;
}
const char* i18n_species_name(int id){assert(id>0);return "Hatched Pokemon";}
bool menu_sprite_load(menu_sprite *s,const menu_mon *m,bool back){assert(!back);loaded=*m;s->n=1;return true;}
void menu_sprite_free(menu_sprite *s){frees++;memset(s,0,sizeof *s);}
const rr_frame *menu_sprite_frame(const menu_sprite *s,uint32_t ms){assert(s->n==1);(void)ms;return &frame;}
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){assert(f==&frame&&x==160&&y==90&&o->origin_x==.5f);mons++;}
static void done(void *arg){assert(arg==&done_count&&!top);done_count++;}
static void draw_top(void){top->draw(top,RR_TOP);top->draw(top,RR_BOTTOM);}
int main(void){
 for(int variant=0;variant<3;variant++){
  gc_event e={.kind=GC_EV_EGG_HATCH,.a=25,.b=3,.c=1,.egg_tier=2,.egg_shiny=true,.egg_variant=variant};
  egg_hatch_open(&e,done,&done_count);
  assert(loaded.species==25&&loaded.form==3&&loaded.gender==1&&loaded.shiny&&loaded.variant==variant);
  int before=mons;draw_top();assert(mons==before&&eggs>0);
  top->update(top,2100);int old=cracks;draw_top();assert(cracks==old+1&&mons==before);
  repeating=true;top->input(top,BTN_ACTION);assert(!E.revealed);repeating=false;
  if(variant==0)top->input(top,BTN_CANCEL);else top->update(top,3000);
  assert(E.revealed);draw_top();assert(mons==before+1);
  top->input(top,BTN_ACTION);assert(top); /* reveal must remain visible */
  top->update(top,500);top->input(top,BTN_ACTION);assert(!top);
 }
 assert(done_count==3&&pops==3&&frees==3);
 puts("Egg hatch UI: egg/cracks/reveal, exact appearance, skip/dwell and continuation passed");
 return 0;
}
