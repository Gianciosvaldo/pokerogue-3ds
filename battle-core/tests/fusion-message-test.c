/* Strict additive replay of actual dormant upstream helpers, not natural ability dispatch. */
#include "pr/battle.h"
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


static int decode(const char *s,char *out,size_t cap) {
 size_t n=strlen(s);if(n%2||n/2>=cap)return 0;
 for(size_t i=0;i<n;i+=2){unsigned v=0;for(int j=0;j<2;j++){unsigned c=(unsigned char)s[i+j];if(c>='0'&&c<='9')c-='0';else if(c>='a'&&c<='f')c=c-'a'+10;else return 0;v=v*16+c;}if(!v)return 0;out[i/2]=(char)v;}out[n/2]=0;return 1;
}
static int valid_form(int id,int form,int optional) {
 if(!id)return optional&&form>=0&&form<=255;
 for(size_t i=0;i<PR_SPECIES_COUNT;i++)if(pr_species_table[i].id==id)return form>=0&&form<pr_species_table[i].nforms;
 return 0;
}
static int valid_identity(const int *v) {
 return valid_form(v[0],v[1],0)&&valid_form(v[2],v[3],1)
  &&v[4]>=0&&v[4]<=1&&v[5]>=0&&v[5]<=1&&(v[6]==1||v[6]==200)
  &&v[7]>=0&&v[7]<=1&&valid_form(v[8],v[9],!v[7])&&valid_form(v[10],v[11],1)&&v[12]>=0&&v[12]<=1;
}
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 FILE *f=fopen(argv[1],"r");if(!f)return 2;
 char line[8192],extra;unsigned version,n,seed;
 if(!fgets(line,sizeof line,f)||sscanf(line,"FUSIONMSG %u %u %u %c",&version,&n,&seed,&extra)!=3||version!=1||n<1024||n>1000000)return 2;
 const pr_text_source src={locale_get,NULL,"en"};unsigned failed=0;
 for(unsigned i=0;i<n;i++) {
  unsigned idx;int v[13];char nh[2048],ah[2048],name[1024],affix[1024],actual[2048];
  if(!fgets(line,sizeof line,f)||sscanf(line,"C %u %d %d %d %d %d %d %d %d %d %d %d %d %d %2047s %2047s %c",&idx,&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7],&v[8],&v[9],&v[10],&v[11],&v[12],nh,ah,&extra)!=16||idx!=i||!decode(nh,name,sizeof name)||!decode(ah,affix,sizeof affix))return 2;
  if(!valid_identity(v))return 2;
  pr_pokemon p={0};p.species=v[0];p.form=v[1];p.fusion_species=v[2];p.fusion_form=v[3];p.is_player=v[4];p.has_trainer=v[5];p.illusion.active=v[7];p.illusion.species=v[8];p.illusion.form=v[9];p.illusion.fusion_species=v[10];p.illusion.fusion_form=v[11];p.transformed=v[12];p.transform_species=SPECIES_ID_MAGIKARP;
  pr_battle bt={0};bt.wave_index=v[6];bt.game_mode=GAME_MODES_CLASSIC;
  pr_msg plain=pr_msg1("battle:singleWildAppeared",pr_arg_pokemon_name(&bt,"pokemonName",&p));
  pr_msg with=pr_msg1("battle:singleWildAppeared",pr_arg_pokemon(&bt,"pokemonName",&p));
  const char *expects[]={name,affix};pr_msg msgs[]={plain,with};
  for(int j=0;j<2;j++){
   msgs[j].key="battle:singleWildAppeared";
   pr_msg expected=pr_msg1(msgs[j].key,(pr_msg_arg){"pokemonName",PR_ARG_STR,0,expects[j],0});char want[2048];
   size_t a=pr_msg_format(&msgs[j],&src,actual,sizeof actual),b=pr_msg_format(&expected,&src,want,sizeof want);
   if(a!=b||strcmp(actual,want)){if(failed<12)fprintf(stderr,"case %u kind%d expected [%s] got [%s]\n",i,j,want,actual);failed++;}
   /* Arguments are immutable snapshots of both halves and Illusion. */
   memset(&p,0,sizeof p);char again[2048];pr_msg_format(&msgs[j],&src,again,sizeof again);if(strcmp(again,actual))return 1;
  }
 }
 unsigned done;if(!fgets(line,sizeof line,f)||sscanf(line,"FUSIONMSG_COMPLETE %u %c",&done,&extra)!=1||done!=n||fgetc(f)!=EOF)return 2;fclose(f);
 printf("fusion messages: %u cases, %u failures (seed %u)\n",n,failed,seed);return failed?1:0;
}
