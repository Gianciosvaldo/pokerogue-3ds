/* Actual changeForm API under supplied fusion and actual Transform state. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
int main(int argc,char **argv){
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input||!getenv("TURNTEST_LOCALES"))return 2;pr_closures_init();double h[3];read_vector("FORMFUSION ",h,3);if(h[0]!=2)bad();int n=integer(h[1],64,100000),fail=0;
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.game_mode=GAME_MODES_CLASSIC;bt.ignoring_effect_source=-1;
  double c[8];read_vector("C ",c,8);if(c[0]!=i)bad();int b=integer(c[1],0,3),to=integer(c[2],0,255);if(b!=(i%2?2:0))bad();rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  double g[2];read_vector("G ",g,2);bt.game_mode=integer(g[0],GAME_MODES_CLASSIC,GAME_MODES_CHALLENGE);int caps[8];int nm=integer(g[1],0,8);bt.n_mods[b/2]=nm;
  for(int k=0;k<nm;k++){char *row=line_read("M "),cls[128],extra;long long owner;int stack,virt,num,stat,cap,transfer;if(sscanf(row,"%127s %lld %d %d %d %d %d %d %c",cls,&owner,&stack,&virt,&num,&stat,&cap,&transfer,&extra)!=8||stack<0||stack>100||virt<0||virt>100||stat<0||stat>5)bad();pr_modifier *m=&bt.mods[b/2][k];m->cls=!strcmp(cls,"BaseStatModifier")?PR_MOD_BASE_STAT:!strcmp(cls,"PokemonBaseStatTotalModifier")?PR_MOD_BASE_STAT_TOTAL:!strcmp(cls,"PokemonBaseStatFlatModifier")?PR_MOD_BASE_STAT_FLAT:0;if(!m->cls)bad();m->pokemon_id=owner;m->stack_count=stack;m->virtual_stack_count=virt;caps[k]=cap;m->num=num;m->stat=stat;m->is_transferable=transfer;if(transfer||cap<0||cap>31)bad();if(m->cls==PR_MOD_BASE_STAT_TOTAL&&cap!=2)bad();if(m->cls==PR_MOD_BASE_STAT_FLAT&&cap!=1)bad();}
  double v[WIDTH];read_vector("I ",v,WIDTH);pr_pokemon p;load(&p,v,b);p.ability_lookup_known=true;p.ability_index=integer(c[3],0,2);p.custom_ability=integer(c[4],-1,PR_ABILITY_COUNT-1);p.fusion_form=integer(c[5],0,255);p.fusion_ability_index=integer(c[6],0,2);p.fusion_custom_ability=integer(c[7],-1,PR_ABILITY_COUNT-1);p.lookup_ability_override=p.daily_boss_ability=-1;bt.field[b]=bt.party[b/2][0]=&p;bt.n_party[b/2]=1;
  for(int k=0;k<nm;k++)if(bt.mods[b/2][k].cls==PR_MOD_BASE_STAT&&caps[k]!=p.ivs[bt.mods[b/2][k].stat])bad();
  bool ok=compare(&p,v,i,b,'I');pr_change_form_state(&bt,&p,to);
  read_vector("O ",v,WIDTH);ok&=compare(&p,v,i,b,'O');double x[2];read_vector("X ",x,2);ok&=integer(x[0],0,2)==p.ability_index&&integer(x[1],0,2)==p.fusion_ability_index;pr_rng r;rng_read("RO ",&r);ok&=rng_equal(r,bt.rng);rng_read("BO ",&r);ok&=rng_equal(r,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||e.overflow)ok=false;if(!ok){if(fail<24)fprintf(stderr,"formfusion case%d species%d tf%d fusion%d form%d abilityidx%d unported%d %s\n",i,p.species,p.transformed,p.fusion_species,p.form,p.ability_index,bt.n_unported,bt.n_unported?pr_unported_name(bt.unported[0]):"none");fail++;}
 }
 double end[1];read_vector("FORMFUSION_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Form/fusion/Transform: %d complete, %d exact, %d failures\n",n,n-fail,fail);return fail?1:0;
}
