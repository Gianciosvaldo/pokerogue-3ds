/* Actual renderer loading path: one atlas per Pokemon and none for empty slots; a sheet that finds no texture memory is tried once more
 * after the idle animation images are freed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
static int loads,probes,releases,refuse;static bool can_release;static rr_frame frame;
bool menu_sprite_load(menu_sprite *s,const menu_mon *m,bool back){assert(m->species);loads++;if(refuse>0){refuse--;return false;}s->n=1;return true;}
bool ba_release_graphics(void){releases++;return can_release;}
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
 assert(releases==0); /* a sheet that loads never frees the animation images */

 /* no room at first: the idle animation images go and the second try succeeds, so the sprite is the atlas and not the stand-in box */
 side *s=side_of(1);s->mon.species=73;can_release=true;refuse=1;
 load_mon_sprite(s,false);assert(loads==6&&releases==1&&refuse==0&&sprite_frame(s)==&frame&&!s->sprite_key[0]&&probes==0);
 /* an animation is playing: nothing is freed and there is no second try; the stand-in path takes over (img's key, not the atlas) */
 can_release=false;refuse=1;
 load_mon_sprite(s,false);assert(loads==7&&releases==2&&refuse==0&&s->sprite.n==0&&s->sprite_key[0]&&probes>0);
 /* freed, but still no room: one second try and no more */
 probes=0;can_release=true;refuse=5;
 load_mon_sprite(s,false);assert(loads==9&&releases==3&&refuse==3&&s->sprite.n==0&&s->sprite_key[0]);
 refuse=0;
 puts("Battle loading: one shared atlas per battler; empty slots never load or animate");return 0;
}
