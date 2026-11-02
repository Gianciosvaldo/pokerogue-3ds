/* Actual dialogue state machine plus the real message-box typing/input code. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
static unsigned selects,portraits,hides,resets,variants;
static char first_pose[32];
text_opts text_defaults(void){text_opts o={0};o.chars=-1;return o;}
int text_utf8_len(const char *s){int n=0;for(;*s;++s)n+=((unsigned char)*s&0xc0)!=0x80;return n;}
void text_wrap(text_style st,const char *s,const text_opts *o,char *out,int n){(void)st;(void)o;snprintf(out,(size_t)n,"%s",s);}
void ui_play_sound(const char *key){(void)key;}
void ui_play_select(void){selects++;}
void chs_show(const char *atlas,const char *variant){assert(atlas);snprintf(first_pose,sizeof first_pose,"%s",variant);portraits++;}
bool chs_busy(void){return false;}
void chs_set_variant(const char *variant){assert(variant);variants++;}
void chs_hide(void){hides++;}
void chs_reset(void){resets++;}
static void dialogue(const char *text,unsigned pages)
{
 memset(&B,0,sizeof B);msgbox_init(&B.msg,TS_MESSAGE,280,2);B.s[1].present=true;B.s[1].mon.species=SPECIES_ID_ETERNATUS;
 bv_event e={.type=BEV_DIALOGUE,.battler=2,.text=text,.name="Eternatus"};
 assert(!step_dialogue(&e,0));assert(B.step==4&&!strcmp(B.speaker,"Eternatus")&&B.s[1].present);
 /* Neither animation time nor an early A skips a line that has not typed yet. */
 assert(!msgbox_input(&B.msg,BTN_ACTION));
 for(unsigned page=0;page<pages;++page){
  for(unsigned ms=0;ms<4000;++ms){msgbox_update(&B.msg,1);assert(!step_dialogue(&e,1));assert(B.s[1].present&&!strcmp(B.speaker,"Eternatus"));}
  assert(B.msg.waiting&&!msgbox_done(&B.msg));assert(msgbox_input(&B.msg,BTN_ACTION));
  if(page+1<pages){assert(!step_dialogue(&e,0));assert(B.speaker&&!strcmp(B.speaker,"Eternatus"));}
 }
 assert(step_dialogue(&e,0));assert(!B.speaker&&portraits==0); /* the owner may now advance to BEV_FAINT */
}
static void portrait_dialogue(void)
{
 memset(&B,0,sizeof B);msgbox_init(&B.msg,TS_MESSAGE,280,2);
 bv_event e={.type=BEV_DIALOGUE,.text="@c{happy}First page.$@c{angry}Last page.",.name="Trainer",.atlas="images/character/rival_m"};
 assert(!step_dialogue(&e,250)&&!B.speaker&&!portraits);
 assert(!step_dialogue(&e,250)&&B.step==4&&portraits==1&&!strcmp(first_pose,"happy"));
 assert(!strcmp(B.speaker,"Trainer")&&fabsf(B.overlay-.5f)<.001f);
 for(unsigned page=0;page<2;++page){
  for(unsigned ms=0;ms<2000;++ms){msgbox_update(&B.msg,1);assert(!step_dialogue(&e,1));}
  assert(B.msg.waiting&&B.speaker);assert(msgbox_input(&B.msg,BTN_ACTION));
  if(!page)assert(!step_dialogue(&e,0));
 }
 assert(!step_dialogue(&e,0)&&!B.speaker&&hides==1&&resets==1&&variants==2);
 assert(!step_dialogue(&e,125));assert(step_dialogue(&e,125));assert(fabsf(B.overlay)<.001f);
}
int main(void){dialogue("…Magnificent.",1);dialogue("A first page.$The final page.",2);portrait_dialogue();assert(selects==5);puts("Final boss dialogue playback: typing, A per page, persistent speaker/sprite no auto-dismiss and retained portrait transitions passed");}
