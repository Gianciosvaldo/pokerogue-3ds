/* Actual controller-owned Eternamax form/HP/stat/PP execution. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
int main(int argc,char **argv){
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input||!getenv("TURNTEST_LOCALES"))return 2;pr_closures_init();double h[3];read_vector("FINALBOSSFORM ",h,3);if(h[0]!=1)bad();int n=integer(h[1],512,100000),failed=0;
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=200;
  pr_pokemon p={0};double c[4];read_vector("C ",c,4);if(c[0]!=i)bad();p.id=9;p.species=SPECIES_ID_ETERNATUS;p.level=integer(c[1],1,255);p.nature=integer(c[2],0,24);p.hp=integer(c[3],1,100000);p.boss_segments=4;p.is_boss=true;p.on_field=true;p.ability_lookup_known=true;p.custom_ability=p.lookup_ability_override=p.daily_boss_ability=p.custom_passive=p.ability_override=-1;p.ability=p.base_ability=pr_get_ability(&p,false);bt.party[1][0]=bt.field[2]=&p;bt.n_party[1]=1;
  double iv[6],stats[6];read_vector("IV ",iv,6);read_vector("I ",stats,6);for(int k=0;k<6;k++){p.ivs[k]=integer(iv[k],0,31);p.stats[k]=integer(stats[k],1,1000000);}rng_read("R ",&bt.rng);bt.rng_live=true;
  pr_init_final_boss_phase_two(&bt,&e,&p);int form=-1;for(int k=0;k<PR_MAX_PHASES;k++)if(e.used[k]&&e.pool[k].type==PH_QUIET_FORM_CHANGE){if(form>=0)bad();form=k;}if(form<0)bad();e.current=form;pr_phase_run(&bt,&e,form);
  double o[12];read_vector("O ",o,12);for(int k=0;k<12;k++)integer(o[k],0,1000000);bool exact=p.form==o[0]&&p.hp==o[1]&&p.ability_index==o[2]&&pr_get_ability(&p,false)==o[3]&&pr_get_passive_ability(&p)==o[4]&&pr_has_passive(&bt,&p)==o[5];for(int k=0;k<6;k++)exact&=p.stats[k]==o[6+k];
  double f[2];read_vector("F ",f,2);for(int k=0;k<2;k++)integer(f[k],0,PR_ABILITY_COUNT-1);exact&=p.n_form_change_abilities==2&&p.form_change_abilities[0]==f[0]&&p.form_change_abilities[1]==f[1];
  char expected_hex[4096];char *hex=line_read("T ");hex[strcspn(hex,"\n")]=0;if(strlen(hex)>=sizeof expected_hex)bad();strcpy(expected_hex,hex);hex=expected_hex;if(strlen(hex)%2||strspn(hex,"0123456789abcdef")!=strlen(hex))bad();char got[4096]="",text[2048];pr_text_source src={locale_get,NULL,"en"};int nt=0;for(int k=0;k<e.n_events;k++)if(e.events[k].type==PR_EV_TEXT){nt++;size_t len=pr_msg_format(&e.events[k].msg,&src,text,sizeof text);if(len>=sizeof text)bad();for(size_t j=0;j<len;j++)snprintf(got+j*2,3,"%02x",(unsigned char)text[j]);}exact&=nt==1&&!strcmp(got,hex);
  for(int k=0;k<4;k++){double m[5];read_vector("M ",m,5);int count=integer(m[0],1,10);for(int j=1;j<5;j++)integer(m[j],j==2?-4:0,PR_MOVE_COUNT);pr_use_moveset_pp(&p,k,false,count);int max=pr_moves[p.moves[k]].pp+p.pp_up[k]*(int)fmax(1,floor(pr_moves[p.moves[k]].pp/5.0));exact&=p.moves[k]==m[1]&&p.pp_up[k]==m[2]&&p.pp_used[k]==m[3]&&max==m[4];}
  pr_rng after;rng_read("RO ",&after);exact&=rng_equal(bt.rng,after)&&!bt.battle_rng_live&&!bt.n_unported&&!e.overflow;if(strcmp(line_read("E"),"\n"))bad();if(!exact){if(failed<12)fprintf(stderr,"Eternamax%d form%d hp%d/%g passive%d/%g applied%d text%d unported%d\n",i,p.form,p.hp,o[1],pr_get_passive_ability(&p),o[4],p.n_form_change_abilities,nt,bt.n_unported);if(failed<1){fprintf(stderr,"state %d/%g %d/%g %d/%g %d/%g %d/%g %d/%g\n",p.form,o[0],p.hp,o[1],p.ability_index,o[2],pr_get_ability(&p,false),o[3],pr_get_passive_ability(&p),o[4],pr_has_passive(&bt,&p),o[5]);for(int z=0;z<6;z++)fprintf(stderr,"stat%d %d/%g\n",z,p.stats[z],o[6+z]);fprintf(stderr,"sets %d,%d/%g,%g texts %s / %s\n",p.form_change_abilities[0],p.form_change_abilities[1],f[0],f[1],got,hex);}
failed++;}
 }
 double footer[1];read_vector("FINALBOSSFORM_COMPLETE ",footer,1);if(footer[0]!=n||fgetc(input)!=EOF)bad();printf("Eternamax form/PP: %d complete, %d failures\n",n,failed);return failed?1:0;
}
