/* Nicknames in messages that need no upstream: what a message holds, the Wild/Foe affix, a disguise, the species name kept for the messages that
 * use pokemon.name, and the pool a message's nickname lives in. The differential against upstream is nickname-message-test. */
#include "pr/battle.h"
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* A locale of the few strings these checks read, so the test runs without an upstream checkout (the differential does not: it names every species) */
static const char *locale_get(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    static const char battle[] = "{\"singleWildAppeared\":\"A wild {{pokemonName}} appeared!\",\"wildPokemonWithAffix\":\"Wild {{pokemonName}}\","
                                 "\"foePokemonWithAffix\":\"Foe {{pokemonName}}\"}";
    static const char pokemon[] = "{\"pikachu\":\"Pikachu\",\"magikarp\":\"Magikarp\"}";
    if (fallback) return NULL;
    if (!strcmp(file, "battle")) { *len = sizeof battle - 1; return battle; }
    if (!strcmp(file, "pokemon")) { *len = sizeof pokemon - 1; return pokemon; }
    return NULL;
}

static int fails;
#define CHECK(cond,...) do{if(!(cond)){fprintf(stderr,"FAIL %s:%d: ",__FILE__,__LINE__);fprintf(stderr,__VA_ARGS__);fputc('\n',stderr);fails++;}}while(0)
static const pr_text_source src={locale_get,NULL,"en"};
static const char *say(const pr_msg_arg *a) {
 static char out[8][512];static int k;char *o=out[k++%8];
 pr_msg m=pr_msg1("battle:singleWildAppeared",*a);   /* "A wild {{pokemonName}} appeared!" */
 char text[512];pr_msg_format(&m,&src,text,sizeof text);
 const char *pre="A wild ",*post=" appeared!";
 size_t n=strlen(text);
 if(strncmp(text,pre,strlen(pre))||n<strlen(pre)+strlen(post)||strcmp(text+n-strlen(post),post)){snprintf(o,512,"?%s",text);return o;}
 text[n-strlen(post)]=0;snprintf(o,512,"%s",text+strlen(pre));return o;
}
static pr_pokemon mon(int species,bool player,bool trainer,const char *nick) {
 pr_pokemon p={0};p.species=species;p.is_player=player;p.has_trainer=trainer;snprintf(p.nickname,sizeof p.nickname,"%s",nick?nick:"");return p;
}
int main(void) {
 pr_battle bt={0};bt.wave_index=5;bt.game_mode=GAME_MODES_CLASSIC;
 /* the species name stands alone, with the nickname it is replaced, and the species name stays for pokemon.name */
 pr_pokemon plain=mon(SPECIES_ID_PIKACHU,true,false,NULL),nick=mon(SPECIES_ID_PIKACHU,true,false,"Sparky");
 pr_msg_arg a=pr_arg_pokemon_name(&bt,"pokemonName",&plain);CHECK(!strcmp(say(&a),"Pikachu"),"no nickname: %s",say(&a));
 a=pr_arg_pokemon_name(&bt,"pokemonName",&nick);CHECK(!strcmp(say(&a),"Sparky"),"nickname alone: %s",say(&a));
 a=pr_arg_pokemon(&bt,"pokemonName",&nick);CHECK(!strcmp(say(&a),"Sparky"),"player with affix: %s",say(&a));
 a=pr_arg_pokemon_plain_name(&bt,"pokemonName",&nick);CHECK(!strcmp(say(&a),"Pikachu"),"pokemon.name keeps the species: %s",say(&a));
 /* an empty nickname is none; spaces are shown as typed (the rename form trims, getNameToRender does not) */
 pr_pokemon blank=mon(SPECIES_ID_PIKACHU,true,false,""),spaced=mon(SPECIES_ID_PIKACHU,true,false,"  Pika  ");
 a=pr_arg_pokemon(&bt,"pokemonName",&blank);CHECK(!strcmp(say(&a),"Pikachu"),"blank: %s",say(&a));
 a=pr_arg_pokemon(&bt,"pokemonName",&spaced);CHECK(!strcmp(say(&a),"  Pika  "),"spaces: %s",say(&a));
 /* the Wild / Foe affix wraps the nickname as it wraps a species name; the final boss uses Foe */
 pr_pokemon wild=mon(SPECIES_ID_PIKACHU,false,false,"Zappy"),foe=mon(SPECIES_ID_PIKACHU,false,true,"Zappy"),wild0=mon(SPECIES_ID_PIKACHU,false,false,NULL);
 a=pr_arg_pokemon(&bt,"pokemonName",&wild);CHECK(!strcmp(say(&a),"Wild Zappy"),"wild: %s",say(&a));
 a=pr_arg_pokemon(&bt,"pokemonName",&foe);CHECK(!strcmp(say(&a),"Foe Zappy"),"foe: %s",say(&a));
 a=pr_arg_pokemon(&bt,"pokemonName",&wild0);CHECK(!strcmp(say(&a),"Wild Pikachu"),"wild, none: %s",say(&a));
 a=pr_arg_pokemon_name(&bt,"pokemonName",&wild);CHECK(!strcmp(say(&a),"Zappy"),"name alone has no affix: %s",say(&a));
 pr_battle boss=bt;boss.wave_index=200;a=pr_arg_pokemon(&boss,"pokemonName",&wild);CHECK(!strcmp(say(&a),"Foe Zappy"),"final boss: %s",say(&a));
 /* a nickname replaces a fusion's blended name */
 pr_pokemon fused=mon(SPECIES_ID_PIKACHU,true,false,"Fuse");fused.fusion_species=SPECIES_ID_MAGIKARP;
 a=pr_arg_pokemon(&bt,"pokemonName",&fused);CHECK(!strcmp(say(&a),"Fuse"),"fusion with nickname: %s",say(&a));
 a=pr_arg_pokemon_plain_name(&bt,"pokemonName",&fused);CHECK(strcmp(say(&a),"Fuse")&&strcmp(say(&a),"Pikachu")&&say(&a)[0],"fusion plain: %s",say(&a));
 /* a disguise shows its own name, not the nickname behind it */
 pr_pokemon ill=mon(SPECIES_ID_PIKACHU,true,false,"Sparky");ill.illusion.active=true;ill.illusion.species=SPECIES_ID_MAGIKARP;
 a=pr_arg_pokemon(&bt,"pokemonName",&ill);CHECK(!strcmp(say(&a),"Magikarp"),"disguised: %s",say(&a));
 /* the longest nickname: 20 four-byte characters */
 char longest[PR_NICKNAME_BYTES];longest[0]=0;for(int i=0;i<PR_NICKNAME_MAX_CHARS;i++)strcat(longest,"\xf0\x9f\x94\xa5");
 pr_pokemon big=mon(SPECIES_ID_PIKACHU,true,false,longest);a=pr_arg_pokemon(&bt,"pokemonName",&big);CHECK(!strcmp(say(&a),longest),"longest: %s",say(&a));
 CHECK(strlen(longest)==PR_NICKNAME_BYTES-1,"buffer is exactly the longest nickname");
 /* no Pokemon, no battle: "MissingNo." as before */
 a=pr_arg_pokemon(NULL,"pokemonName",NULL);CHECK(!strcmp(say(&a),"MissingNo."),"no Pokemon: %s",say(&a));
 a=pr_arg_pokemon_name(NULL,"pokemonName",&nick);CHECK(!strcmp(say(&a),"Sparky"),"no battle: %s",say(&a));
 /* a message made now still reads the same after the Pokemon changed or the party moved (a rename, a release, a swap) */
 pr_pokemon t=mon(SPECIES_ID_PIKACHU,true,false,"Before");pr_msg_arg held=pr_arg_pokemon(&bt,"pokemonName",&t);
 snprintf(t.nickname,sizeof t.nickname,"After");CHECK(!strcmp(say(&held),"Before"),"renamed after: %s",say(&held));
 memset(&t,0,sizeof t);CHECK(!strcmp(say(&held),"Before"),"gone after: %s",say(&held));
 /* the same text shares one copy; another text has its own */
 pr_pokemon x1=mon(SPECIES_ID_PIKACHU,true,false,"Twin"),x2=mon(SPECIES_ID_MAGIKARP,true,false,"Twin"),x3=mon(SPECIES_ID_PIKACHU,true,false,"Other");
 pr_msg_arg p1=pr_arg_pokemon(&bt,"a",&x1),p2=pr_arg_pokemon(&bt,"a",&x2),p3=pr_arg_pokemon(&bt,"a",&x3);
 CHECK(p1.s==p2.s&&p1.s!=p3.s,"one copy per text");
 /* the pool is a ring of 64: a message holds its nickname while fewer than 64 other distinct nicknames are made after it, and every
  * newest message is always right */
 enum{K=300};static pr_msg_arg held_args[K];char nm[K][16];
 for(int i=0;i<K;i++){snprintf(nm[i],sizeof nm[i],"ring-%d",i);pr_pokemon q=mon(SPECIES_ID_PIKACHU,true,false,nm[i]);held_args[i]=pr_arg_pokemon(&bt,"pokemonName",&q);
  for(int j=(i>=63?i-63:0);j<=i;j++)CHECK(!strcmp(say(&held_args[j]),nm[j]),"ring: message %d after %d more nicknames: %s",j,i-j,say(&held_args[j]));}
 printf("nickname tests: %d failures\n",fails);return fails?1:0;
}
