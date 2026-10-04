/* Strict additive replay of actual pinned trigger/callback queue selection. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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


static FILE *in;
static int value(void) { int v;if(fscanf(in,"%d",&v)!=1){fprintf(stderr,"malformed weather form trace\n");exit(2);}return v; }
int main(int argc,char **argv) {
 if((argc!=2&&argc!=3)||(argc==3&&strcmp(argv[2],"--mutate-form"))||!(in=fopen(argv[1],"r")))return 2;
 char tag;if(fscanf(in," %c",&tag)!=1||tag!='V'||value()!=1)return 2;
 int n=value(),bad=0,queued=0;if(n<=0)return 2;
 for(int i=0;i<n;i++) {
  pr_battle bt={0};pr_engine e={0};pr_pokemon p={0},enemy={0};
  p.species=value();p.form=value();bt.weather=value();p.ability_suppressed=value();p.passive=value();
  int override=value(),cloud=value(),mode=value(),old_weather=value();
  p.passive_flag=value();p.custom_passive=value();
  p.level=value();p.nature=value();for(int k=0;k<6;k++)p.ivs[k]=value();
  for(int k=0;k<6;k++)p.stats[k]=value();
  p.hp=value();
  int expected=value(),original_form=p.form;
  if(expected<0||expected>4||mode<0||mode>8)return 2;
  int targets[4];for(int k=0;k<expected;k++)targets[k]=value();
  p.base_ability=pr_pokemon_form(&p)->ability1;p.ability=override?override:p.base_ability;
  p.ability_override=override?override:-1;p.on_field=p.is_player=true;p.switch_out_status=mode==7;p.fusion_ability_index=-1;
  pr_update_passive(&bt,&p);
  enemy.species=SPECIES_ID_MAGIKARP;enemy.ability=cloud?ABILITY_ID_CLOUD_NINE:ABILITY_ID_BALL_FETCH;
  enemy.hp=mode==8?0:100;enemy.on_field=true;enemy.stats[STAT_SPD]=20;enemy.ability_override=-1;enemy.fusion_ability_index=-1;
  bt.field[0]=&p;bt.field[2]=&enemy;bt.engine=&e;e.n_levels=1;e.current=-1;
  if(mode<2)pr_form_change(&bt,&p,mode?PR_FCT_SpeciesFormChangeRevertWeatherFormTrigger:PR_FCT_SpeciesFormChangeWeatherTrigger);
  else if(mode<4) {pr_ab_params pp={.pokemon=&p,.move=-1,.field_type=bt.weather};
   pr_apply_ab_attrs(&bt,mode==2?PR_AC_PostWeatherChangeAbAttr:PR_AC_PostSummonFormChangeByWeatherAbAttr,&pp);}
  else if(mode==4){int weather=bt.weather;bt.weather=old_weather;pr_try_set_weather(&bt,&e,weather,NULL);}
  else if(mode==5){pr_ab_params pp={.pokemon=&enemy,.move=-1};pr_apply_ab_attrs(&bt,PR_AC_PostSummonWeatherSuppressedFormChangeAbAttr,&pp);}
  else if(mode==6)pr_weather_form_changes(&bt,false,&enemy);
  else if(mode==7)pr_form_change(&bt,&p,PR_FCT_SpeciesFormChangeActiveTrigger);
  else {pr_ab_params pp={.pokemon=&enemy,.move=-1};pr_apply_ab_attrs(&bt,PR_AC_PostFaintAbAttr,&pp);}
  int actual[4],phases[4],count=0;
  /* Same insertion order as the producer's unshift capture, reversed allocation order. */
  for(int k=PR_MAX_PHASES-1;k>=0;k--)if(e.pool[k].type==PH_QUIET_FORM_CHANGE){if(count>=4)return 2;phases[count]=k;actual[count++]=e.pool[k].move_id;}
  int fail=count!=expected;
  for(int k=0;k<count&&k<expected;k++)fail|=actual[k]!=targets[k];
  fail|=p.form!=original_form;
  for(int k=0;k<expected;k++) {
   int form=value(),nt=value();if(nt<1||nt>PR_MAX_TYPES)return 2;
   int types[PR_MAX_TYPES];for(int j=0;j<nt;j++)types[j]=value();
   int stats[6];for(int j=0;j<6;j++)stats[j]=value();int hp=value(),ability=value(),weather=value(),passive=value(),len=value();
   if(len<0||len>=512)return 2;
   char text[512];for(int j=0;j<len;j++)text[j]=(char)value();text[len]=0;
   if(k>=count){fail=1;continue;}
   int before_events=e.n_events;
   if(argc==3)e.pool[phases[k]].move_id=(uint16_t)original_form;
   e.current=phases[k];pr_phase_run(&bt,&e,phases[k]);e.current=-1;
   int8_t got_types[PR_MAX_TYPES];int got_nt=pr_get_types(&p,got_types,0);
   fail|=p.form!=form||got_nt!=nt||p.hp!=hp||p.ability!=ability||bt.weather!=weather||p.passive!=passive;
   for(int j=0;j<nt;j++)fail|=j>=got_nt||got_types[j]!=types[j];
   for(int j=0;j<6;j++)fail|=p.stats[j]!=stats[j];
   char rendered[512]={0};static const pr_text_source src={locale_get,NULL,"en"};
   for(int j=before_events;j<e.n_events;j++)if(e.events[j].msg.key)
       pr_msg_format(&e.events[j].msg,&src,rendered,sizeof rendered);
   if(strcmp(text,rendered)){fail=1;if(bad<12)fprintf(stderr,"message expected [%s] got [%s]\n",text,rendered);}
   if(fail&&bad<12)fprintf(stderr,"phase form %d/%d hp%d/%d ability%d/%d speed%d/%d weather%d/%d passive%d/%d\n",p.form,form,p.hp,hp,p.ability,ability,p.stats[STAT_SPD],stats[STAT_SPD],bt.weather,weather,p.passive,passive);
  }
  if(fail){if(bad<12)fprintf(stderr,"FAIL %d species%d form%d weather%d suppressed%d override%d cloud%d mode%d queues%d/%d target%d/%d\n",i,p.species,p.form,bt.weather,p.ability_suppressed,override,cloud,mode,count,expected,count?actual[0]:-1,expected?targets[0]:-1);bad++;}
  queued+=count;
 }
 if(fscanf(in," %c",&tag)!=1||tag!='Z'||value()!=n||fscanf(in," %c",&tag)==1)return 2;
 printf("Weather form dispatch: %d cases, %d identical, %d mismatches, %d queued forms\n",n,n-bad,bad,queued);
 return bad?1:0;
}
