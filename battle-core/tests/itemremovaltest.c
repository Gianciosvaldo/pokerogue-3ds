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
static int value(void) {int v;if(fscanf(in,"%d",&v)!=1)exit(2);return v;}
static void state(pr_rng *r) {
 char s[256];if(fscanf(in," %255s",s)!=1 || sscanf(s,"!rnd,%lf,%lf,%lf,%lf",&r->c,&r->s0,&r->s1,&r->s2)!=4)exit(2);
}
int main(int argc,char **argv) {
 if(argc<2||argc>3||(argc==3&&strcmp(argv[2],"--mutate-stack"))||!(in=fopen(argv[1],"r")))return 2;
 char tag;if(fscanf(in," %c",&tag)!=1||tag!='V'||value()!=1)return 2;
 int n=value(),bad=0,passed[2]={0},rejected[2]={0};if(n<1||n>100000)return 2;
 static const int cls[]={PR_MOD_TURN_HEAL,PR_MOD_HIT_HEAL,PR_MOD_SURVIVE_DAMAGE,PR_MOD_BYPASS_SPEED_CHANCE,PR_MOD_FLINCH_CHANCE,PR_MOD_RESET_NEGATIVE_STAT_STAGE};
 for(int i=0;i<n;i++) {
  pr_battle bt={0};pr_engine e={0};pr_pokemon p={0},enemy={0};
  int player=value(),berries=value(),ability=value(),supp=value(),passive=value(),count=value();
  if(player<0||player>1||berries<0||berries>1||count<0||count>6)return 2;
  p.species=enemy.species=SPECIES_ID_MAGIKARP;p.id=10;enemy.id=20;p.is_player=true;enemy.has_trainer=true;
  p.hp=enemy.hp=100;p.on_field=enemy.on_field=true;
  p.ability=enemy.ability=ABILITY_ID_BALL_FETCH;p.ability_override=enemy.ability_override=-1;
  p.fusion_ability_index=enemy.fusion_ability_index=-1;
  pr_pokemon *target=player?&p:&enemy,*user=player?&enemy:&p;
  target->ability=ability;target->ability_suppressed=supp;target->passive=passive;target->passive_flag=passive!=0;target->custom_passive=passive;
  pr_seed_from_ascii(&bt.wave_seed,"removal-wave");p.stats[STAT_SPD]=20;enemy.stats[STAT_SPD]=10;
  bt.field[0]=&p;bt.field[2]=&enemy;bt.engine=&e;e.current=-1;e.n_levels=1;
  int side=player?0:1;bt.n_mods[side]=count;
  for(int k=0;k<count;k++) {
   int kind=value(),stack=value(),transfer=value(),owner=value();if(kind<0||kind>16||stack<1||stack>3)return 2;
   pr_modifier *m=&bt.mods[side][k];m->cls=kind<6?cls[kind]:PR_MOD_BERRY;m->berry_type=kind-6;
   m->stack_count=stack;m->is_transferable=transfer;m->pokemon_id=owner?target->id:user->id;m->num=k;
  }
  bt.n_mods[1-side]=1;bt.mods[1-side][0]=(pr_modifier){.cls=PR_MOD_TURN_HEAL,.stack_count=2,.pokemon_id=user->id};
  pr_modifier original[6];memcpy(original,bt.mods[side],sizeof original);
  state(&bt.battle_rng);bt.battle_rng_live=true;pr_rng expected_rng={0};state(&expected_rng);
  pr_modifier removed={0};int ok=pr_remove_move_held_item(&bt,&e,user,target,berries,&removed);
  if(argc==3&&ok&&bt.n_mods[side])bt.mods[side][0].stack_count++;
  int expected=value(),unburden=value(),remaining=value();if(remaining<0||remaining>6)return 2;
  int fail=ok!=expected||bt.n_mods[side]!=remaining||!!pr_get_battler_tag(target,BATTLER_TAG_TYPE_UNBURDEN)!=unburden;
  for(int k=0;k<remaining;k++) {
   int idx=value(),stack=value();if(idx<0||idx>=count)return 2;
   if(k>=bt.n_mods[side]||bt.mods[side][k].num!=idx||bt.mods[side][k].stack_count!=stack
      ||bt.mods[side][k].pokemon_id!=original[idx].pokemon_id||bt.mods[side][k].cls!=original[idx].cls
      ||bt.mods[side][k].berry_type!=original[idx].berry_type||bt.mods[side][k].is_transferable!=original[idx].is_transferable)fail=1;
  }
  if(ok && (removed.num<0||removed.num>=count||memcmp(&removed,&original[removed.num],sizeof removed)))fail=1;
  int messages=value();if(messages<0||messages>8)return 2;
  const pr_msg *actual[8];int nm=0;
  for(int k=0;k<PR_MAX_PHASES;k++)if(e.used[k]&&e.pool[k].type==PH_MESSAGE){if(nm>=8)return 2;actual[nm++]=&e.pool[k].msg;}
  fail|=nm!=messages;
  for(int j=0;j<messages;j++) {
   int len=value();if(len<0||len>1000)return 2;char text[1001],rendered[1001]={0};
   for(int k=0;k<len;k++) text[k]=(char)value();
   text[len]=0;
   static const pr_text_source src={locale_get,NULL,"en"};
   if(j<nm)pr_msg_format(actual[j],&src,rendered,sizeof rendered);
   if(strcmp(text,rendered)){fail=1;if(bad<8)fprintf(stderr,"message [%s] / [%s]\n",text,rendered);}
  }
  fail|=bt.battle_rng.c!=expected_rng.c||bt.battle_rng.s0!=expected_rng.s0||bt.battle_rng.s1!=expected_rng.s1||bt.battle_rng.s2!=expected_rng.s2;
  fail|=bt.n_mods[1-side]!=1||bt.mods[1-side][0].stack_count!=2||bt.n_unported!=0;
  if(fail){if(bt.n_unported&&bad<8)fprintf(stderr,"unported %s\n",pr_unported_name(bt.unported[0]));if(bad<8)fprintf(stderr,"FAIL %d player%d berry%d ability%d supp%d passive%d ok%d/%d stacks%d/%d unburden%d/%d msgs%d/%d unported%d\n",i,player,berries,ability,supp,passive,ok,expected,bt.n_mods[side],remaining,!!pr_get_battler_tag(target,BATTLER_TAG_TYPE_UNBURDEN),unburden,nm,messages,bt.n_unported);bad++;}
  (ok?passed:rejected)[berries]++;
 }
 if(fscanf(in," %c",&tag)!=1||tag!='Z'||value()!=n||fscanf(in," %c",&tag)==1)return 2;
 printf("Removal: %d cases, %d identical, %d mismatches; success %d/%d rejected %d/%d\n",n,n-bad,bad,passed[0],passed[1],rejected[0],rejected[1]);
 return bad?1:0;
}
