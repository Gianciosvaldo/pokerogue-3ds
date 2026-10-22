/* Real top-bar draw path remains visible during events and between turns. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
static rr_frame icon;static int icons,rects;static char title[40];
const rr_frame *mui_frame(const char *atlas,const char *key){assert(!strcmp(atlas,"items")&&key);return &icon;}
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){assert(f==&icon);icons++;}
void rr_rect(float x,float y,float w,float h,rr_color color){rects++;}
text_opts text_defaults(void){return (text_opts){0};}
float text_draw(text_style style,float x,float y,const char *text,const text_opts *o){if(!strcmp(text,"Key items")||!strcmp(text,"Foe items"))snprintf(title,sizeof title,"%s",text);return 0;}
int main(void){
 bv_item foe={"foe",2,"Enemy item",""},key={"key",1,"Key item",""};
 memset(&B,0,sizeof B);bv_set_item_bars(&foe,1,&key,1,7);draw_top_bar();assert(!rects&&!icons);
 B.begun=true;
 for(int mode=M_IDLE;mode<=M_FIGHT;mode++)for(int bar=0;bar<2;bar++){
  B.mode=mode;B.bar=bar;icons=rects=0;title[0]=0;draw_top_bar();
  assert(icons==1&&rects>=2&&!strcmp(title,bar?"Key items":"Foe items"));
 }
 B.mode=M_IDLE;B.bar=1;B.bar_sel=0;B.bar_open=true;
 bv_set_item_bars(NULL,0,&key,1,8);icons=rects=0;draw_top_bar();assert(icons==1&&B.n_foe==0&&B.n_key==1&&B.opts.party_luck==8);
 bv_set_item_bars(NULL,0,NULL,0,0);assert(B.bar_sel==-1&&!B.bar_open);
 puts("Persistent key/foe item bars: event, idle, command, fight and stock refresh passed");return 0;
}
