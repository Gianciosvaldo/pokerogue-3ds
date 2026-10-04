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


static FILE *in;
static int read_int(void) { int v; if(fscanf(in,"%d",&v)!=1) {fprintf(stderr,"malformed Illusion trace\n");exit(2);}return v; }
static void record(char want) { char c; if(fscanf(in," %c",&c)!=1||c!=want) {fprintf(stderr,"expected record %c\n",want);exit(2);} }
static pr_pokemon identity(void) {
 pr_pokemon p={0};p.species=read_int();p.form=read_int();p.fusion_species=read_int();p.fusion_form=read_int();p.gender=read_int();return p;
}
static int verify(const pr_pokemon *p) {
 int bad=0;record('A');int active=read_int(),sp=read_int(),form=read_int(),fs=read_int(),ff=read_int(),gender=read_int();
 const pr_form *a=pr_apparent_form(p,true),*f=pr_apparent_fusion_form(p,true);
 int identity_bad=p->illusion.active!=active||a->species!=sp||a->form_index!=form||(f?f->species:0)!=fs||(f?f->form_index:0)!=ff||pr_apparent_gender(p,true)!=gender;
 bad|=identity_bad;
 if(identity_bad)fprintf(stderr,"identity expected %d/%d fusion %d/%d gender %d active%d actual %d/%d fusion%d/%d gender%d active%d\n",sp,form,fs,ff,gender,active,a->species,a->form_index,f?f->species:0,f?f->form_index:0,pr_apparent_gender(p,true),p->illusion.active);
 for(int flags=0;flags<32;flags++) {record('T');if(read_int()!=flags)exit(2);int expected=read_int();if(expected<1||expected>PR_MAX_TYPES)exit(2);
 int8_t types[PR_MAX_TYPES];int n=pr_get_types(p,types,flags);bad|=n!=expected;for(int k=0;k<expected;k++) {int t=read_int();if(k>=n||types[k]!=t) {bad=1;fprintf(stderr,"type flags%d at%d expected%d actual%d realsp%d fusion%d tera%d\n",flags,k,t,k<n?types[k]:-1,p->species,p->fusion_species,p->tera_type);}} }
 record('M');int named=read_int();
 if(named) {
  static const pr_text_source text={locale_get,NULL,"en"};pr_battle bt={0};bt.game_mode=0;
  const char *keys[]={"battle:singleWildAppeared","battle:playerComeBack"};
  for(int j=0;j<2;j++) {
   record('S');int len=read_int();if(len<0||len>=512)exit(2);char expected[512];for(int k=0;k<len;k++) {int b=read_int();if(b<0||b>255)exit(2);expected[k]=(char)b;}expected[len]=0;
   pr_msg_arg arg=j?pr_arg_pokemon(&bt,"pokemonName",p):pr_arg_pokemon_name(&bt,"pokemonName",p);
   pr_msg msg=pr_msg1(keys[j],arg);char actual[512];size_t got=pr_msg_format(&msg,&text,actual,sizeof actual);
   if(got!=(size_t)len||strcmp(actual,expected)) {bad=1;fprintf(stderr,"name: expected [%s], actual [%s]\n",expected,actual);}
  }
 }
 return bad;
}
int main(int argc,char **argv) {
 if ((argc != 2 && argc != 3) || (argc == 3 && strcmp(argv[2], "--mutate-identity")) || (in = fopen(argv[1], "r")) == NULL) return 2;
 record('V');if(read_int()!=1)return 2;int n=read_int(),failed=0;if(n<=0)return 2;
 for(int i=0;i<n;i++) {record('C');pr_pokemon p=identity();p.id=(uint32_t)read_int();p.terastallized=read_int();p.tera_type=read_int();p.added_type=read_int();p.n_summon_types=read_int();p.summon_types[0]=read_int();p.summon_types[1]=read_int();
 if(p.n_summon_types>2)return 2;
 pr_pokemon source=identity();int mode=read_int();p.is_player=read_int();p.has_trainer=read_int();pr_pokemon original=p;
 if(mode)pr_set_illusion_species(&p,source.species,0);else pr_set_illusion(&p,&source);
 if(argc==3) p.illusion.species=p.species;
 int bad=verify(&p);source.species=SPECIES_ID_MAGIKARP;source.form=0;source.fusion_species=0;source.fusion_form=0;source.gender=0;bad|=verify(&p);
 bad|=!pr_break_illusion(&p)||pr_break_illusion(&p);bad|=verify(&p);
 pr_set_illusion(&p,&source);pr_reset_summon_data(&p);bad|=verify(&p);
 bad|=p.species!=original.species||p.form!=original.form||p.fusion_species!=original.fusion_species||p.fusion_form!=original.fusion_form||p.gender!=original.gender||p.id!=original.id;
 if(bad){if(failed<10)printf("FAIL CASE %d\n",i);failed++;}
 }
 record('Z');if(read_int()!=n)return 2;char extra;if(fscanf(in," %c",&extra)==1)return 2;fclose(in);
 printf("Illusion helpers: %d cases, %d identical, %d mismatches, %d type reads; thrown/skipped 0\n",n,n-failed,failed,n*4*32);
 return failed?1:0;
}
