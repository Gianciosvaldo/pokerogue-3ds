/* Exercise the actual battle renderer against captured draw calls, without GPU. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"

const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
static rr_frame frame;
static char atlas[80], pose[20];
static int frame_index, draws;
static float px,py;
static rr_draw_opts opts;
const rr_frame* img_anim_frame(const char* key,int n) {
  snprintf(atlas,sizeof atlas,"%s",key);frame_index=n;return &frame;
}
const rr_frame* img_frame(const char* key,const char* name) {
  snprintf(atlas,sizeof atlas,"%s",key);snprintf(pose,sizeof pose,"%s",name);return &frame;
}
void rr_draw(const rr_frame* f,float x,float y,const rr_draw_opts* o) {
  assert(f==&frame);draws++;px=x;py=y;opts=*o;
}
static void near(float x,float y) { assert(fabsf(x-y)<0.001f); }
int main(void) {
  memset(&B,0,sizeof B);
  B.player_trainer_visible=true;B.player_throw_ms=-1;
  B.arena_player_x=300;draw_player_trainer();
  assert(!strcmp(atlas,"images/trainer/trainer_m_back"));near(px,406);near(py,186);
  near(opts.origin_x,.5f);near(opts.origin_y,1);
  B.arena_player_x=0;draw_player_trainer();near(px,106);
  float times[]={0,561,562,625,626,999};int frames[]={0,0,1,1,2,2};
  for(int sex=0;sex<2;sex++) for(int k=0;k<6;k++) {
    B.setup.player_female=sex;B.player_throw_ms=times[k];draw_player_trainer();
    assert(!strcmp(atlas,sex?"images/trainer/trainer_f_back_pb":"images/trainer/trainer_m_back_pb"));
    assert(frame_index==frames[k]);near(px,106-142*times[k]/1000);
  }
  B.player_trainer_visible=false;int before=draws;draw_player_trainer();assert(draws==before);
  B.opening_summon=true;
  const char* balls[]={"pb","gb","ub","rb","mb"};
  for(int ball=0;ball<5;ball++) {
    B.setup.player.pokeball=ball;B.player_throw_ms=750;draw_opening_ball();
    assert(!strcmp(atlas,"images/pb")&&!strcmp(pose,balls[ball]));near(px,36);near(py,80);
    near(opts.origin_x,.5f);near(opts.origin_y,.625f);
  }
  B.player_throw_ms=900;draw_opening_ball();near(py,70);near(opts.angle,0);
  B.player_throw_ms=1150;draw_opening_ball();near(py,77.75f);near(opts.angle,4*3.14159265f);
  B.player_throw_ms=1399;draw_opening_ball();assert(px>99&&py>131);
  before=draws;B.player_throw_ms=1400;draw_opening_ball();assert(draws==before);
  B.player_throw_ms=749;draw_opening_ball();assert(draws==before);
  B.player_throw_ms=800;B.opening_summon=false;draw_opening_ball();assert(draws==before);
  B.trainer_visible=true;strcpy(B.setup.trainer_keys[0],"images/trainer/rival_m");
  B.enemy_slide=0;draw_trainers();near(px,228);near(py,80);
  strcpy(B.setup.trainer_keys[1],"images/trainer/rival_f");before=draws;draw_trainers();
  assert(draws==before+2);near(px,256);
  B.trainer_visible=false;before=draws;draw_trainers();assert(draws==before);
  puts("trainer render: player poses, opening ball flight and trainer visibility passed");
  return 0;
}
