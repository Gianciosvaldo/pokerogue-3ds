/* Actual renderer loading path: one atlas per Pokemon and none for empty slots. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
static int loads,probes;static rr_frame frame;
bool menu_sprite_load(menu_sprite *s,const menu_mon *m,bool back){assert(m->species);loads++;s->n=1;return true;}
void menu_sprite_free(menu_sprite *s){memset(s,0,sizeof *s);}
bool img_has(const char *key){probes++;return false;}
const rr_frame *menu_sprite_frame(const menu_sprite *s,uint32_t ms){assert(s->n);return &frame;}
const rr_frame *img_anim_frame(const char *key,int n){assert(0);return NULL;}
float ui_now(void){return 100;}
void ui_play_select(void){}
static int cancelled;
static void command(bv_command c,void *arg){assert(c.command==4&&arg==&cancelled);cancelled++;}
int main(void){
 B.mode=M_COMMAND;B.opts.can_undo=false;B.on_command=command;B.command_arg=&cancelled;
 assert(!undo_command(BTN_CANCEL)&&cancelled==0);
 B.opts.can_undo=true;assert(!undo_command(BTN_ACTION));assert(undo_command(BTN_CANCEL)&&cancelled==1&&B.mode==M_IDLE);
 assert(!undo_command(BTN_CANCEL)&&cancelled==1);
 B.setup.doubles=true;assert(info_base_x(side_of(0),true)+40<=400&&info_base_x(side_of(0),true)-130+40>=0);
 assert(info_base_x(side_of(2),false)-130+40>=0);
 side_of(2)->mon.boss=true;assert(info_base_x(side_of(2),false)-178+40>=0);

 for(int i=0;i<4;i++){side *s=side_of(i);s->mon.species=25+i;load_mon_sprite(s,i<2);assert(sprite_frame(s)==&frame);}
 assert(loads==4&&probes==0);
 side empty={0};load_mon_sprite(&empty,true);assert(loads==4&&probes==0&&!sprite_frame(&empty));
 puts("Battle loading: one shared atlas per battler; empty slots never load or animate");return 0;
}
