/* Compare the actual renderer to native BattleInfo factory coordinates. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
static rr_frame frames[100];static char keys[100][140],names[100][20];static int nf;
static struct{int frame;float x,y;rr_draw_opts opts;} draws[100];static int nd;
const char *i18n_language(void){return "en";}
const rr_frame *img_frame(const char *key,const char *name){
 for(int i=0;i<nf;i++)if(!strcmp(keys[i],key)&&!strcmp(names[i],name?name:""))return &frames[i];
 int i=nf++;assert(i<100);snprintf(keys[i],sizeof keys[i],"%s",key);snprintf(names[i],sizeof names[i],"%s",name?name:"");
 int w=0,h=8;
 if(strstr(key,"player_stats")){w=130;h=42;}
 else if(strstr(key,"boss_stats")){w=178;h=31;}
 else if(strstr(key,"mini_stats")){w=130;h=31;}
 else if(strstr(key,"stat_numbers"))w=!strcmp(name,"empty")?1:9;
 else if(strstr(key,"pbinfo_stat"))w=!strcmp(name,"HP")?9:(!strcmp(name,"SPATK")||!strcmp(name,"SPDEF"))?19:13;
 assert(w>0);frames[i].src_w=w;frames[i].src_h=h;return &frames[i];
}
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){
 assert(nd<100);draws[nd].frame=(int)(f-frames);draws[nd].x=x;draws[nd].y=y;draws[nd++].opts=*o;
}
static void near(float a,float b){assert(fabsf(a-b)<.001f);}
int main(int argc,char **argv){
 assert(argc==2);FILE *fp=fopen(argv[1],"r");assert(fp);char tag[30];int version,total;
 assert(fscanf(fp,"%29s%d%d",tag,&version,&total)==3&&!strcmp(tag,"BATTLE_STATS")&&version==1&&total==3);
 for(int mode=0;mode<3;mode++){
  int m,w,h;assert(fscanf(fp,"%29s%d%d%d",tag,&m,&w,&h)==4&&!strcmp(tag,"B")&&m==mode);
  char labels[7][20],values[7][20];float lx[7],ly[7],nx[7],ny[7];
  for(int k=0;k<7;k++)assert(fscanf(fp,"%29s%19s%19s%f%f%f%f",tag,labels[k],values[k],&lx[k],&ly[k],&nx[k],&ny[k])==7&&!strcmp(tag,"L"));
  for(int field=mode==0?0:2;field<(mode==0?2:4);field++){
   memset(&B,0,sizeof B);side *sd=side_of(field);sd->mon.boss=mode==2;
   const int8_t stages[]={-6,-4,-2,0,2,4,6};memcpy(sd->mon.stat_stages,stages,sizeof stages);
   nd=0;draw_stat_panel(sd,field<2,0,0);assert(nd==0);
   assert(stats_input(BTN_STATS)&&B.stats_shown);tween_update(&B.stats_tw,125);B.stats_alpha=B.stats_tw.value;
   draw_stat_panel(sd,field<2,0,0);assert(nd==15);
   assert(frames[draws[0].frame].src_w==w&&frames[draws[0].frame].src_h==h);
   for(int k=0;k<7;k++){
    int a=1+2*k,b=a+1;
    assert(!strcmp(names[draws[a].frame],labels[k])&&!strcmp(names[draws[b].frame],values[k]));
    near(draws[a].x,lx[k]);near(draws[a].y,ly[k]);near(draws[b].x,nx[k]);near(draws[b].y,ny[k]);
    assert(draws[a].opts.tint==0xffffffff&&draws[b].opts.tint==0xffffffff);
   }
   B.mode=M_EVENTS;B.cur=7;bv_release(NULL,BTN_STATS);tween_update(&B.stats_tw,125);B.stats_alpha=B.stats_tw.value;
   assert(!B.stats_shown&&B.stats_alpha==0&&B.mode==M_EVENTS&&B.cur==7);
   int before=nd;draw_stat_panel(sd,field<2,0,0);assert(nd==before);
  }
 }
 int modes,entries;assert(fscanf(fp,"%29s%d%d",tag,&modes,&entries)==3&&!strcmp(tag,"COMPLETE")&&modes==3&&entries==21);
 assert(fscanf(fp,"%29s",tag)==EOF);fclose(fp);
 puts("Native battle stat panels: player/enemy/boss geometry, seven signed stages, four battlers and held/released SELECT passed");return 0;
}
