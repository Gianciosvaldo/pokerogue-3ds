/* Actual source changeForm API state. Trigger eligibility and phase queues are separate. */
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


static FILE *fp;static char line[1024];
static void bad(void){fputs("malformed form lifecycle trace\n",stderr);exit(2);}
static void read_line(void)
{
 if(!fgets(line,sizeof line,fp)||!strchr(line,'\n'))bad();
 if(!strncmp(line,"T ",2))return;
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
 bool phase=argc==3&&!strcmp(argv[2],"--phase");
 if((argc!=2&&!phase)||(fp=fopen(argv[1],"r"))==NULL)return 2;
 int n;char extra;read_line();if(sscanf(line,phase?"QUIET_FORM 1 %d %c":"FORM_LIFECYCLE 1 %d %c",&n,&extra)!=1||n<64||n>100000)bad();
 int failures=0,unsupported=0;unsigned coverage=0;
 for(int i=0;i<n;i++){
  int id,sp,to,level,nature,custom,temp,tera,iv[6],before[13],want[13];read_line();
  if(sscanf(line,"C %d %d %d %d %d %d %d %d %c",&id,&sp,&to,&level,&nature,&custom,&temp,&tera,&extra)!=8||id!=i||sp<1||sp>65535||to<0||to>255||level<1||level>255||nature<0||nature>24||custom<-1||custom>=PR_ABILITY_COUNT||temp<-1||temp>=PR_ABILITY_COUNT||tera<0||tera>19)bad();
  read_line();if(sscanf(line,"IV %d %d %d %d %d %d %c",iv,iv+1,iv+2,iv+3,iv+4,iv+5,&extra)!=6)bad();for(int j=0;j<6;j++)if(iv[j]<0||iv[j]>31)bad();
  read_line();if(strncmp(line,"I ",2))bad();state(before);read_line();if(strncmp(line,"O ",2))bad();state(want);
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
  pr_battle bt={0};pr_pokemon p={0};p.species=sp;const pr_species *species=pr_pokemon_species(&p);if(!species||to>=species->nforms||before[0]>=species->nforms||want[0]>=species->nforms)bad();
  bt.game_mode=GAME_MODES_CLASSIC;bt.ignoring_effect_source=-1;p.is_player=i%2==0;p.on_field=true;p.form=before[0];p.ability_index=before[1];p.ability_lookup_known=true;p.custom_ability=custom;p.fusion_custom_ability=p.lookup_ability_override=p.daily_boss_ability=-1;p.ability_override=temp;p.ability=before[2];p.base_ability=before[3];p.hp=before[4];p.terastallized=before[5];p.stellar_types_boosted=before[6]?6:0;p.tera_type=tera;p.nature=nature;p.level=level;for(int j=0;j<6;j++){p.ivs[j]=iv[j];p.stats[j]=before[7+j];}
  if(pr_get_ability(&p,false)!=p.ability||pr_get_ability(&p,true)!=p.base_ability)bad();
  static pr_engine e;
  if(phase){
   memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;bt.engine=&e;int side=p.is_player?0:1,battler=side*2;
   bt.field[battler]=&p;bt.party[side][0]=&p;bt.n_party[side]=1;
   int ph=pr_phase_new(&e,PH_QUIET_FORM_CHANGE,battler);e.pool[ph].pokemon=&p;e.pool[ph].move_id=to;e.current=ph;pr_phase_run(&bt,&e,ph);e.current=-1;
  }else pr_change_form_state(&bt,&p,to);
  int got[13]={p.form,p.ability_index,p.ability,p.base_ability,p.hp,p.terastallized,p.stellar_types_boosted?2:0};for(int j=0;j<6;j++)got[7+j]=p.stats[j];
  bool failed=false;for(int j=0;j<13;j++)if(got[j]!=want[j]){if(failures<20)fprintf(stderr,"case%d species%d form%d field%d C%d source%d\n",i,sp,to,j,got[j],want[j]);failed=true;}
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
  if(bt.n_unported)unsupported++;
  coverage|=1u<<((i/2)%32);
 }
 read_line();int footer;if(sscanf(line,phase?"QUIET_FORM_COMPLETE %d %c":"FORM_LIFECYCLE_COMPLETE %d %c",&footer,&extra)!=1||footer!=n||fgetc(fp)!=EOF||coverage!=0xffffffffu)bad();
 printf("Form lifecycle %s cases%d mismatches%d unsupported%d all32states\n",phase?"phase state/set/text":"API",n,failures,unsupported);return failures||unsupported?1:0;
}
