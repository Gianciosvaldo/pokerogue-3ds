/* Replay of the actual pinned names of nicknamed Pokemon: getNameToRender (pr_arg_pokemon_name), getPokemonNameWithAffix (pr_arg_pokemon) and
 * pokemon.name (pr_arg_pokemon_plain_name), which a nickname must not change. Incomplete or malformed captures are rejected (exit 2). */
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
/* The same message with the name given as text: what the source built with the string it got from getNameToRender and friends */
static size_t render(const pr_msg_arg *a,const pr_text_source *src,char *out,size_t cap) {
 pr_msg m=pr_msg1("battle:singleWildAppeared",*a);
 return pr_msg_format(&m,src,out,cap);
}
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 FILE *f=fopen(argv[1],"r");if(!f)return 2;
 char line[16384],extra;unsigned version,n,seed;
 if(!fgets(line,sizeof line,f)||sscanf(line,"NICKNAME %u %u %u %c",&version,&n,&seed,&extra)!=3||version!=1||n<1024||n>1000000)return 2;
 const pr_text_source src={locale_get,NULL,"en"};unsigned failed=0,with_nick=0,plain_checked=0;
 for(unsigned i=0;i<n;i++) {
  unsigned idx;int v[13];char kh[2048],nh[2048],ah[2048],ph[2048],nick[PR_NICKNAME_BYTES+8],name[1024],affix[1024],plain[1024];
  if(!fgets(line,sizeof line,f)||sscanf(line,"C %u %d %d %d %d %d %d %d %d %d %d %d %d %d %2047s %2047s %2047s %2047s %c",&idx,&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7],&v[8],&v[9],&v[10],&v[11],&v[12],kh,nh,ah,ph,&extra)!=18||idx!=i)return 2;
  if(!valid_identity(v)||!decode(nh,name,sizeof name)||!decode(ah,affix,sizeof affix)||!decode(ph,plain,sizeof plain))return 2;
  nick[0]=0;
  if(strcmp(kh,"-")){size_t k=strlen(kh);if(k%2||k/2>=PR_NICKNAME_BYTES||!decode(kh,nick,sizeof nick))return 2;}
  pr_pokemon p={0};p.species=v[0];p.form=v[1];p.fusion_species=v[2];p.fusion_form=v[3];p.is_player=v[4];p.has_trainer=v[5];p.illusion.active=v[7];p.illusion.species=v[8];p.illusion.form=v[9];p.illusion.fusion_species=v[10];p.illusion.fusion_form=v[11];p.transformed=v[12];p.transform_species=SPECIES_ID_MAGIKARP;
  memcpy(p.nickname,nick,strlen(nick)+1);if(nick[0])with_nick++;
  pr_battle bt={0};bt.wave_index=v[6];bt.game_mode=GAME_MODES_CLASSIC;
  pr_msg_arg args[3]={pr_arg_pokemon_name(&bt,"pokemonName",&p),pr_arg_pokemon(&bt,"pokemonName",&p),pr_arg_pokemon_plain_name(&bt,"pokemonName",&p)};
  const char *expects[3]={name,affix,plain};
  for(int j=0;j<3;j++){
   if(j==2&&v[7])continue;   /* pokemon.name under a disguise: the existing arguments name the disguise (unchanged), the source names the real one */
   if(j==2)plain_checked++;
   pr_msg_arg want_arg={"pokemonName",PR_ARG_STR,0,expects[j],0};
   char actual[2048],want[2048];
   size_t a=render(&args[j],&src,actual,sizeof actual),b=render(&want_arg,&src,want,sizeof want);
   if(a!=b||strcmp(actual,want)){if(failed<12)fprintf(stderr,"case %u kind%d nick[%s] expected [%s] got [%s]\n",i,j,nick,want,actual);failed++;}
   /* the argument is a snapshot: the Pokemon changing or going away afterwards does not change the message */
   pr_pokemon saved=p;memset(&p,0,sizeof p);strcpy(p.nickname,"changed afterwards");
   char again[2048];render(&args[j],&src,again,sizeof again);if(strcmp(again,actual)){fprintf(stderr,"case %u kind%d: the message changed with its Pokemon\n",i,j);return 1;}
   p=saved;
  }
 }
 unsigned done;if(!fgets(line,sizeof line,f)||sscanf(line,"NICKNAME_COMPLETE %u %c",&done,&extra)!=1||done!=n||fgetc(f)!=EOF)return 2;fclose(f);
 if(!with_nick||!plain_checked)return 2;
 printf("nickname messages: %u cases (%u with a nickname, %u species-name checks), %u failures (seed %u)\n",n,with_nick,plain_checked,failed,seed);return failed?1:0;
}
