/* Actual native ability callback eligibility and queued form phases; API boundary. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
static const char *locales_dir(void)
{
    const char *d = getenv("TURNTEST_LOCALES");
    return d ? d : "/home/claude/pokerogue/locales/en";
}

static const char *locale_get(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    static struct { char name[64]; char *text; size_t len; } cache[96];
    static int n_cache;
    if (fallback) return NULL;
    for (int i = 0; i < n_cache; i++)
        if (!strcmp(cache[i].name, file)) { *len = cache[i].len; return cache[i].text; }
    if (n_cache >= 96) return NULL;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.json", locales_dir(), file);
    FILE *f = fopen(path, "rb");
    char *text = NULL;
    size_t n = 0;
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        text = malloc((size_t)sz + 1);
        n = fread(text, 1, (size_t)sz, f);
        text[n] = 0;
        fclose(f);
        /* upstream's tests load each locale file as a module namespace (import.meta.glob, eager),
         * whose named exports can't be reserved words: there move "return" and ability "static"
         * are missing, and i18next shows "return.name". Hide them the same way (the game has them). */
        static const char *const hidden[] = {"\n  \"return\": ", "\n  \"static\": "};
        for (int i = 0; i < 2; i++) {
            char *k = strstr(text, hidden[i]);
            if (k) k[4] = '_';                               /* "return" -> "_eturn" */
        }
    }
    snprintf(cache[n_cache].name, sizeof cache[n_cache].name, "%s", file);
    cache[n_cache].text = text;
    cache[n_cache].len = n;
    n_cache++;
    *len = n;
    return text;
}


static FILE *fp;static char line[1024];static int lineno;
static void bad(void){fprintf(stderr,"malformed native form trace line%d: %s",lineno,line);exit(2);}
static void read_line(void)
{
 if(!fgets(line,sizeof line,fp)||!strchr(line,'\n'))bad();lineno++;
 if(!strncmp(line,"T ",2)||!strncmp(line,"P ",2))return;
 char *p=strchr(line,' '),*end;if(!p)bad();
 while(*p){
  while(*p==' '||*p=='\t'||*p=='\n')p++;
  if(!*p)break;
  char *digits=p;if(*digits=='-')digits++;
  if(*digits<'0'||*digits>'9')bad();
  errno=0;long long value=strtoll(p,&end,10);
  if(errno||end==p||value<INT_MIN||value>INT_MAX||(*end&&*end!=' '&&*end!='\t'&&*end!='\n'))bad();
  p=end;
 }
}
static void state(int *v){char extra;if(sscanf(line+2,"%d %d %d %d %d %d %d %d %d %d %d %d %d %c",v,v+1,v+2,v+3,v+4,v+5,v+6,v+7,v+8,v+9,v+10,v+11,v+12,&extra)!=13)bad();if(v[0]<0||v[0]>255||v[1]<0||v[1]>2||v[2]<0||v[2]>=PR_ABILITY_COUNT||v[3]<0||v[3]>=PR_ABILITY_COUNT||v[4]<0||v[4]>v[7]||v[5]<0||v[5]>1||v[6]<0||v[6]>2)bad();for(int j=7;j<13;j++)if(v[j]<1||v[j]>10000)bad();}
int main(int argc,char **argv){
 bool phase=true,run_phase=argc==3&&!strcmp(argv[2],"--phase");
 if((argc!=2&&!run_phase)||(fp=fopen(argv[1],"r"))==NULL)return 2;
 int n;char extra;read_line();if(sscanf(line,run_phase?"NATIVE_FORM_PHASE 2 %d %c":"NATIVE_FORM 2 %d %c",&n,&extra)!=1||n<512||n>100000)bad();
 int failures=0,unsupported=0;unsigned coverage=0;
 for(int i=0;i<n;i++){
  int id,sp,to,level,nature,custom,temp,tera,simulated,on_gain,weather=0,last_ball=-1,foe_ab=0,foe_passive=0,foe_hp=1,foe_onfield=1,foe_suppressed=0,iv[6],before[13],want[13];read_line();
  if(sscanf(line,"C %d %d %d %d %d %d %d %d %d %d %c",&id,&sp,&to,&level,&nature,&custom,&temp,&tera,&simulated,&on_gain,&extra)!=10||id!=i||sp<1||sp>65535||to<0||to>3||simulated<0||simulated>1||on_gain<0||on_gain>1||level<1||level>255||nature<0||nature>24||custom<-1||custom>=PR_ABILITY_COUNT||temp<-1||temp>=PR_ABILITY_COUNT||tera<0||tera>19)bad();
  const int species_matrix[]={SPECIES_ID_DARMANITAN,SPECIES_ID_GALAR_DARMANITAN,SPECIES_ID_MINIOR,SPECIES_ID_WISHIWASHI,SPECIES_ID_MORPEKO,SPECIES_ID_EISCUE,SPECIES_ID_MIMIKYU,SPECIES_ID_ZYGARDE};
  if(sp!=species_matrix[(i/64)%8]||to!=(i/2)%4)bad();
  int fsp,ff,fidx,fcustom;read_line();if(sscanf(line,"X %d %d %d %d %c",&fsp,&ff,&fidx,&fcustom,&extra)!=4||fsp<1||fsp>65535||ff<0||ff>255||fidx<0||fidx>2||fcustom<-1||fcustom>=PR_ABILITY_COUNT)bad();
  read_line();if(sscanf(line,"IV %d %d %d %d %d %d %c",iv,iv+1,iv+2,iv+3,iv+4,iv+5,&extra)!=6)bad();for(int j=0;j<6;j++)if(iv[j]<0||iv[j]>31)bad();
  read_line();if(!strncmp(line,"W ",2)){int fields=sscanf(line,"W %d %d %d %d %d %d %d %c",&weather,&last_ball,&foe_ab,&foe_passive,&foe_hp,&foe_onfield,&foe_suppressed,&extra);if((fields!=2&&fields!=7)||weather<0||weather>WEATHER_TYPE_FOG||last_ball<-1||last_ball>POKEBALL_TYPE_MASTER_BALL||foe_ab<0||foe_ab>=PR_ABILITY_COUNT||foe_passive<0||foe_passive>=PR_ABILITY_COUNT||foe_hp<0||foe_onfield<0||foe_onfield>1||foe_suppressed<0||foe_suppressed>1)bad();read_line();}if(strncmp(line,"I ",2))bad();state(before);
  read_line();int nq;char qname[32][64],qtext[32][1024];int qform[32],qpassive[32];if(sscanf(line,"Q %d %c",&nq,&extra)!=1||nq<0||nq>32)bad();
  for(int j=0;j<nq;j++){read_line();strcpy(qtext[j],"-");int fields=sscanf(line,"P %63s %d %d %1023s %c",qname[j],qform+j,qpassive+j,qtext[j],&extra);if((fields!=3&&fields!=4)||qform[j]<-1||qform[j]>255||qpassive[j]<-1||qpassive[j]>1)bad();
   bool quiet=!strcmp(qname[j],"QuietFormChangePhase"),show=!strcmp(qname[j],"ShowAbilityPhase"),message=!strcmp(qname[j],"MessagePhase"),hide=!strcmp(qname[j],"HideAbilityPhase");
   if((!quiet&&!show&&!message&&!hide)||(quiet?(qform[j]<0):(qform[j]!=-1))||(show?(qpassive[j]<0):(qpassive[j]!=-1)))bad();
   if(strcmp(qtext[j],"-")&&(strlen(qtext[j])%2||strspn(qtext[j],"0123456789abcdef")!=strlen(qtext[j])))bad();
  }
  read_line();if(strncmp(line,"O ",2))bad();state(want);
  int form_applied[17]={0};char expected_text[1024]="-";
  if(phase){
   read_line();if(strncmp(line,"F ",2))bad();char *v=line+2,*end;
   for(int j=0;j<17;j++){long x=strtol(v,&end,10);if(end==v||x<0||x>(j?PR_ABILITY_COUNT-1:16))bad();form_applied[j]=(int)x;v=end;}
   if(strcmp(v,"\n"))bad();
   for(int j=1;j<=16;j++){if(j>form_applied[0]&&form_applied[j])bad();for(int k=1;k<j&&j<=form_applied[0];k++)if(form_applied[j]==form_applied[k])bad();}
   read_line();if(strncmp(line,"T ",2))bad();size_t len=strlen(line+2);if(!len||line[2+len-1]!='\n')bad();line[2+len-1]=0;
   if(strcmp(line+2,"-")){if((len-1)%2)bad();for(size_t j=0;j<len-1;j++)if(!strchr("0123456789abcdef",line[2+j]))bad();}
   strcpy(expected_text,line+2);
  }
  pr_battle bt={0};pr_pokemon p={0};p.species=sp;const pr_species *species=pr_pokemon_species(&p);if(!species||before[0]>=species->nforms||want[0]>=species->nforms)bad();
  bt.game_mode=GAME_MODES_CLASSIC;bt.last_used_pokeball=last_ball;bt.weather=weather;bt.ignoring_effect_source=-1;p.is_player=i%2==0;p.on_field=true;p.form=before[0];p.ability_index=before[1];p.ability_lookup_known=true;p.custom_ability=custom;p.fusion_custom_ability=p.lookup_ability_override=p.daily_boss_ability=-1;p.ability_override=temp;p.ability=before[2];p.base_ability=before[3];p.hp=before[4];p.terastallized=before[5];p.stellar_types_boosted=before[6]?6:0;p.tera_type=tera;p.nature=nature;p.level=level;for(int j=0;j<6;j++){p.ivs[j]=iv[j];p.stats[j]=before[7+j];}
  p.fusion_species=fsp;p.fusion_form=ff;p.fusion_ability_index=fidx;p.fusion_custom_ability=fcustom;const pr_species *fusion=pr_pokemon_fusion_species(&p);if(!fusion||ff>=fusion->nforms)bad();
  if(pr_get_ability(&p,false)!=p.ability||pr_get_ability(&p,true)!=p.base_ability)bad();
  pr_pokemon foe={0};foe.species=SPECIES_ID_RATTATA;foe.hp=foe_hp;foe.on_field=foe_onfield;foe.ability=foe.base_ability=foe_ab;foe.ability_override=-1;foe.passive=foe_passive;foe.passive_flag=foe_passive!=0;foe.ability_suppressed=foe_suppressed;foe.is_player=!p.is_player;
  bt.field[p.is_player?2:0]=&foe;
  static pr_engine e;
  if(phase){
   memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;bt.engine=&e;int side=p.is_player?0:1,battler=side*2;
   bt.field[battler]=&p;bt.party[side][0]=&p;bt.n_party[side]=1;
   
   const uint16_t classes[]={PR_AC_PostSummonAbAttr,PR_AC_PostTurnAbAttr,PR_AC_PostBattleInitAbAttr,PR_AC_PostFaintAbAttr};
   pr_ab_params pp={.pokemon=&p,.move=-1,.simulated=simulated,.on_gain=on_gain,.only=on_gain?1:0};pr_apply_ab_attrs(&bt,classes[to],&pp);
  }else pr_change_form_state(&bt,&p,to);
  int got[13]={p.form,p.ability_index,p.ability,p.base_ability,p.hp,p.terastallized,p.stellar_types_boosted?2:0};for(int j=0;j<6;j++)got[7+j]=p.stats[j];
  bool failed=false;int qseen=0;
  for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){
   pr_phase *ph=&e.pool[e.levels[l][k]];int form=ph->type==PH_QUIET_FORM_CHANGE?ph->move_id:-1,passive=ph->type==PH_SHOW_ABILITY?ph->anim_result:-1;
   char text[1024]="-";
   if(ph->type==PH_MESSAGE){pr_text_source src={locale_get,NULL,"en"};char rendered[512];size_t len=pr_msg_format(&ph->msg,&src,rendered,sizeof rendered);if(len>=sizeof rendered)bad();for(size_t z=0;z<len;z++)snprintf(text+2*z,3,"%02x",(unsigned char)rendered[z]);}
   if(qseen<nq&&strcmp(text,qtext[qseen])){if(failures<20)fprintf(stderr,"case%d queued text mismatch\n",i);failed=true;}
   if(qseen>=nq||strcmp(pr_phase_name[ph->type],qname[qseen])||form!=qform[qseen]||passive!=qpassive[qseen]){if(failures<20)fprintf(stderr,"case%d queue%d C %s/%d/%d source %s/%d/%d\n",i,qseen,pr_phase_name[ph->type],form,passive,qseen<nq?qname[qseen]:"missing",qseen<nq?qform[qseen]:-1,qseen<nq?qpassive[qseen]:-1);failed=true;}qseen++;
  }
  if(qseen!=nq)failed=true;
  if(run_phase){
   int quiet[32],nquiet=0;
   for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){int idx=e.levels[l][k];if(e.pool[idx].type==PH_QUIET_FORM_CHANGE){if(nquiet==32)bad();quiet[nquiet++]=idx;}}
   for(int k=0;k<nquiet;k++){e.current=quiet[k];pr_phase_run(&bt,&e,quiet[k]);e.current=-1;}
   got[0]=p.form;got[1]=p.ability_index;got[2]=p.ability;got[3]=p.base_ability;got[4]=p.hp;got[5]=p.terastallized;got[6]=p.stellar_types_boosted?2:0;for(int j=0;j<6;j++)got[7+j]=p.stats[j];
  }
  for(int j=0;j<13;j++)if(got[j]!=want[j]){if(failures<20)fprintf(stderr,"case%d species%d form%d field%d C%d source%d\n",i,sp,to,j,got[j],want[j]);failed=true;}
  if(phase){
   if(p.n_form_change_abilities!=form_applied[0]){if(failures<20)fprintf(stderr,"case%d form ability set count%d source%d\n",i,p.n_form_change_abilities,form_applied[0]);failed=true;}
   for(int j=0;j<form_applied[0];j++)if(j>=p.n_form_change_abilities||p.form_change_abilities[j]!=form_applied[j+1])failed=true;
   char actual_text[1024]="-";int texts=0;pr_text_source src={locale_get,NULL,"en"};
   for(int j=0;j<e.n_events;j++)if(e.events[j].type==PR_EV_TEXT){
    char rendered[512];size_t len=pr_msg_format(&e.events[j].msg,&src,rendered,sizeof rendered);if(len>=sizeof rendered||++texts>1)bad();
    if(!len)strcpy(actual_text,"-");else for(size_t k=0;k<len;k++)snprintf(actual_text+2*k,3,"%02x",(unsigned char)rendered[k]);
   }
   if(strcmp(actual_text,expected_text)){if(failures<20)fprintf(stderr,"case%d form text mismatch\n",i);failed=true;}
   if(e.overflow)failed=true;
  }
  if(failed)failures++;
  if(bt.n_unported){unsupported++;if(unsupported<=30){fprintf(stderr,"case%d unsupported",i);for(int j=0;j<bt.n_unported;j++)fprintf(stderr," %d",bt.unported[j]);fputc('\n',stderr);}}
  coverage|=1u<<((i/2)%32);
 }
 read_line();int footer;if(sscanf(line,run_phase?"NATIVE_FORM_PHASE_COMPLETE %d %c":"NATIVE_FORM_COMPLETE %d %c",&footer,&extra)!=1||footer!=n||fgetc(fp)!=EOF||coverage!=0xffffffffu)bad();
 printf("Native form callback %s cases%d mismatches%d unsupported%d all32states\n",run_phase?"queued quiet-phase state/set/text":"API queue/state",n,failures,unsupported);return failures||unsupported?1:0;
}
