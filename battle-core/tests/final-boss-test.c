/* Actual scene controller boundary; phase execution and item theft are separate. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
static bool match_phase(pr_phase *ph,pr_pokemon *p,const char *row){
 char name[80],marker[80],form[80],extra;int b,target,field,player,party,activate;
 if(sscanf(row,"%79s %d %d %d %d %d %79s %79s %d %c",name,&b,&target,&field,&player,&party,marker,form,&activate,&extra)!=9)bad();
 if(b < -1||b>3||target<0||target>1||field < -1||field>1||player < -1||player>1||party < -1||party>5||activate<0||activate>1)bad();
 bool ok=!strcmp(name,pr_phase_name[ph->type]);
 if(target)ok &= ph->pokemon==p;
 if(ph->type==PH_SUMMON)ok &= field==ph->sw_field_index&&player==ph->sw_player&&party==ph->sw_field_index;
 else if(ph->type==PH_POST_SUMMON)ok &= b==ph->battler&&field==ph->battler%2&&player==(ph->battler<2)&&activate==ph->ps_activate;
 if(ph->type==PH_MARKER)ok &= !strcmp(marker,pr_phase_name[ph->marker_of]);
 if(ph->type==PH_QUIET_FORM_CHANGE)ok &= !strcmp(form,"eternamax")&&ph->move_id==1;
 return ok;
}
int main(int argc,char **argv){
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input)return 2;pr_closures_init();double h[2];read_vector("FINALBOSS ",h,2);if(h[0]!=1)bad();int n=integer(h[1],512,100000),fail=0,transitions=0,refused=0;
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;
  double c[9];read_vector("C ",c,9);if(c[0]!=i)bad();int enemy=integer(c[1],0,1),boss=integer(c[2],0,1),form=integer(c[3],0,1),seg=integer(c[4],0,2),expected=integer(c[5],0,1),method=integer(c[6],0,2);bt.game_mode=integer(c[7],0,4);bt.wave_index=integer(c[8],1,100000);
  pr_pokemon party[3]={0},foe={0};double pv[6];read_vector("PARTY ",pv,6);bt.n_party[0]=3;bt.n_party[1]=1;
  for(int k=0;k<3;k++){party[k].id=k+1;party[k].species=SPECIES_ID_BULBASAUR;party[k].is_player=true;party[k].hp=integer(pv[k*2],0,100000);party[k].on_field=integer(pv[k*2+1],0,1);party[k].ability=party[k].base_ability=ABILITY_ID_BALL_FETCH;party[k].ability_override=-1;party[k].custom_passive=-1;bt.party[0][k]=&party[k];if(k<2&&party[k].on_field)bt.field[k]=&party[k];}
  foe.id=9;foe.species=SPECIES_ID_ETERNATUS;foe.hp=1;foe.on_field=true;foe.boss_segments=boss?4:0;foe.boss_segment_index=seg;bt.party[1][0]=bt.field[2]=&foe;pr_pokemon *p=enemy?&foe:&party[0];p->form=form;
  rng_read("R ",&bt.rng);bt.rng_live=true;
  double a[1];read_vector("I ",a,1);p->n_moves=integer(a[0],0,4);for(int k=0;k<p->n_moves;k++){double m[4];read_vector("M ",m,4);p->moves[k]=integer(m[0],1,PR_MOVE_COUNT-1);p->pp_used[k]=integer(m[1],0,255);p->pp_up[k]=integer(m[2],-4,3);p->max_pp_override[k]=-1;}
  if(method==0)pr_init_final_boss_phase_two(&bt,&e,p);
  else {int ph=pr_phase_new(&e,method==1?PH_DAMAGE_ANIM:PH_POST_TURN_STATUS_EFFECT,enemy?2:0);e.current=ph;pr_phase_run(&bt,&e,ph);}
  /* Challenge passive-access evaluation remains explicitly unsupported. It
   * is outside this controller's Classic/Endless/Daily proof boundary. */
  bool challenge_refusal=bt.game_mode==GAME_MODES_CHALLENGE&&e.n_dyn[PH_POST_SUMMON]>0;
  bool accounting=challenge_refusal?(bt.n_unported==1&&bt.unported[0]==PR_FEAT_WAVE):!bt.n_unported;
  refused+=challenge_refusal;
  bool exact=bt.double_battle==expected&&accounting&&!e.overflow;transitions+=bt.double_battle;
  read_vector("O ",a,1);int nm=integer(a[0],0,4);exact&=p->n_moves==nm;for(int k=0;k<nm;k++){double m[4];read_vector("M ",m,4);for(int j=0;j<4;j++)integer(m[j],j==2?-4:0,10000);int pp=pr_moves[p->moves[k]].pp+p->pp_up[k]*(int)fmax(1,floor(pr_moves[p->moves[k]].pp/5.0));exact&=p->moves[k]==m[0]&&p->pp_used[k]==m[1]&&p->pp_up[k]==m[2]&&pp==m[3];}
  read_vector("MODS ",a,1);int mods=integer(a[0],0,1);exact&=bt.n_mods[1]==mods;for(int k=0;k<mods;k++){double m[3];read_vector("MOD ",m,3);exact&=m[0]==1&&bt.mods[1][k].pokemon_id==p->id&&bt.mods[1][k].stack_count==m[1]&&bt.mods[1][k].is_transferable==m[2];}
  {char *row=line_read("LEVELS "),*end;long levels=strtol(row,&end,10);if(end==row||levels<1||levels>PR_MAX_LEVELS)bad();bool levels_equal=levels==e.n_levels;row=end;for(int l=0;l<levels;l++){long size=strtol(row,&end,10);if(end==row||size<0||size>PR_LEVEL_CAP)bad();if(l>=e.n_levels||size!=e.level_len[l])levels_equal=false;row=end;}if(strcmp(row,"\n"))bad();exact&=levels_equal;}
  read_vector("Q ",a,1);int nq=integer(a[0],0,20),got=0;for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){
   if(got++>=nq){exact=false;continue;}pr_phase *ph=&e.pool[e.levels[l][k]];exact&=match_phase(ph,p,line_read("P "));
  }
  while(got<nq){line_read("P ");got++;exact=false;}
  read_vector("DYNAMIC ",a,1);int nd=integer(a[0],0,20);exact&=nd==e.n_dyn[PH_POST_SUMMON];for(int k=0;k<nd;k++){
   char *d=line_read("D ");if(strncmp(d,"PostSummonPhase ",16))bad();
   if(k>=e.n_dyn[PH_POST_SUMMON])exact=false;else exact&=match_phase(&e.pool[e.dyn[PH_POST_SUMMON][k]],p,d+16);
  }
  pr_rng after;rng_read("RO ",&after);exact&=rng_equal(bt.rng,after)&&!bt.battle_rng_live;if(strcmp(line_read("E"),"\n"))bad();if(!exact){if(fail<12)fprintf(stderr,"finalboss%d enemy%d boss%d form%d seg%d expected%d queue%d/%d unported%d\n",i,enemy,boss,form,seg,expected,got,nq,bt.n_unported);fail++;}
 }
 double end[1];read_vector("FINALBOSS_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Final boss controller: %d complete, %d transitions, %d Challenge-passive refusals, %d failures\n",n,transitions,refused,fail);return fail?1:0;
}
