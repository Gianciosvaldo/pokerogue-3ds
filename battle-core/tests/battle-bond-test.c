/* Strict transport/replay for the actual pinned Transform/Imposter boundary. */
#include "pr/engine.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

enum { WIDTH = 125 };
static FILE *input;
static int lineno;
static char line[8192];
static void bad(void) { fprintf(stderr, "invalid Battle Bond fixture at line %d\n", lineno); exit(2); }
static void read_vector(const char *key, double *v, int n)
{
    if (!fgets(line, sizeof line, input)) bad();
    lineno++;
    if (strncmp(line, key, strlen(key))) bad();
    char *p = line + strlen(key), *end;
    for (int i = 0; i < n; i++) {
        v[i] = strtod(p, &end);
        if (end == p || !isfinite(v[i])) bad();
        p = end;
    }
    while (isspace((unsigned char)*p)) p++;
    if (*p) bad();
}
static int integer(double v, double low, double high)
{
    if (v < low || v > high || v != floor(v)) bad();
    return (int)v;
}
static void load(pr_pokemon *p, const double *v, int b)
{
    memset(p, 0, sizeof *p);
    p->species = integer(v[0], 1, 65535); p->form = integer(v[1], 0, 255); p->gender = integer(v[2], -1, 1);
    pr_pokemon id = {0}; id.species = p->species; const pr_species *sp = pr_pokemon_species(&id);
    if (!sp || p->form >= sp->nforms || v[3] < 0 || v[3] > UINT32_MAX || v[3] != floor(v[3])) bad();
    p->id = (uint32_t)v[3]; p->level = integer(v[4], 1, 255); p->nature = integer(v[5], 0, 24);
    p->hp = integer(v[6], 0, INT32_MAX); p->status = integer(v[7], 0, STATUS_EFFECT_FAINT);
    p->shiny = integer(v[8], 0, 1); p->base_ability = integer(v[9], 0, PR_ABILITY_COUNT - 1);
    p->ability = p->ability_override = integer(v[10], 0, PR_ABILITY_COUNT - 1);
    p->passive = integer(v[11], 0, PR_ABILITY_COUNT - 1); p->passive_flag = integer(v[12], 0, 1);
    p->custom_passive = integer(v[13], -1, PR_ABILITY_COUNT - 1); p->fusion_species = integer(v[14], 0, 65535);
    p->fusion_ability_index = -1; p->illusion.species = integer(v[15], 0, 65535); p->illusion.active = v[15] != 0;
    for(int j=0;j<6;j++)p->ivs[j]=integer(v[104+j],0,31);
    p->n_ab_applied_summon=integer(v[112],0,4);for(int i=0;i<4;i++)p->ab_applied_summon[i]=integer(v[113+i],0,PR_ABILITY_COUNT-1);
    p->n_ab_applied_wave=integer(v[117],0,4);for(int i=0;i<4;i++)p->ab_applied_wave[i]=integer(v[118+i],0,PR_ABILITY_COUNT-1);
    p->temp_turn_count=integer(v[122],0,32767);p->temp_wave_turn_count=integer(v[123],0,32767);p->ability_override=integer(v[124],-1,PR_ABILITY_COUNT-1);
    p->friendship=integer(v[110],0,255);p->ability_suppressed=integer(v[111],0,1);
    p->on_field = integer(v[16], 0, 1); p->switch_out_status = integer(v[17], 0, 1); p->is_player = b < 2;
    for (int i = 0; i < 6; i++) { p->stats[i] = integer(v[18+i], 1, INT32_MAX); p->summon_stats[i] = v[24+i]; }
    for (int i = 0; i < 7; i++) p->stat_stages[i] = integer(v[30+i], -6, 6);
    p->n_moves = integer(v[37], 0, 4);
    for (int i = 0; i < 4; i++) {
        p->moves[i] = integer(v[38+i], 0, PR_MOVE_COUNT-1); p->pp_used[i] = integer(v[42+i], 0, 255);
        p->pp_up[i] = integer(v[46+i], 0, 3); p->max_pp_override[i] = integer(v[50+i], -1, 32767);
    }
    p->n_summon_types = integer(v[54], 0, PR_MAX_SUMMON_TYPES);
    for (int i = 0; i < 4; i++) p->summon_types[i] = integer(v[55+i], POKEMON_TYPE_UNKNOWN, POKEMON_TYPE_STELLAR);
    p->added_type = integer(v[59], 0, 19); p->terastallized = integer(v[60], 0, 1); p->tera_type = integer(v[61], -1, 19);
    p->transformed = integer(v[62], 0, 1); p->transform_species = integer(v[63], 0, 65535);
    p->transform_form = integer(v[64], 0, 255); p->transform_gender = integer(v[65], -1, 1);
    if (p->transformed) {
        pr_pokemon identity={0};identity.species=p->transform_species;
        const pr_species *species=pr_pokemon_species(&identity);
        if(!species||p->transform_form>=species->nforms)bad();
        p->transform_n_moves = p->n_moves;
        memcpy(p->transform_moves, p->moves, sizeof p->moves);
        memcpy(p->transform_pp_used, p->pp_used, sizeof p->pp_used);
        memcpy(p->transform_pp_up, p->pp_up, sizeof p->pp_up);
        memcpy(p->transform_max_pp_override, p->max_pp_override, sizeof p->max_pp_override);
        p->n_moves = integer(v[66], 0, 4);
        for (int i = 0; i < 4; i++) {
            p->moves[i] = integer(v[67+i], 0, PR_MOVE_COUNT-1); p->pp_used[i] = integer(v[71+i], 0, 255);
            p->pp_up[i] = integer(v[75+i], 0, 3); p->max_pp_override[i] = integer(v[79+i], -1, 32767);
        }
    }
    for (int i = 0; i < 2; i++) if (integer(v[83+i], 0, 1)) {
        p->tags[p->n_tags++].type = i ? BATTLER_TAG_TYPE_SUBSTITUTE : BATTLER_TAG_TYPE_POWER_TRICK;
    }
    p->variant = integer(v[100], 0, 2);
    integer(v[85],1,65535);integer(v[86],0,255);integer(v[87],-1,1);
    int n_types=integer(v[94],1,PR_MAX_TYPES);
    for(int i=0;i<n_types;i++)integer(v[95+i],POKEMON_TYPE_UNKNOWN,POKEMON_TYPE_STELLAR);
    integer(v[102],0,1);integer(v[103],0,1);
}
static void capture(const pr_pokemon *p, double *v)
{
    memset(v, 0, WIDTH * sizeof *v);
    v[0]=p->species; v[1]=p->form; v[2]=p->gender; v[3]=p->id; v[4]=p->level; v[5]=p->nature;
    v[6]=p->hp; v[7]=p->status; v[8]=p->shiny; v[9]=p->base_ability; v[10]=p->ability;
    v[11]=p->passive; v[12]=p->passive_flag; v[13]=p->custom_passive; v[14]=p->fusion_species;
    v[15]=p->illusion.active ? p->illusion.species : 0; v[16]=p->on_field && !p->switch_out_status; v[17]=p->switch_out_status;
    for (int i=0;i<6;i++){v[18+i]=p->stats[i];v[24+i]=p->summon_stats[i];}
    for (int i=0;i<7;i++)v[30+i]=p->stat_stages[i];
    v[37]=PR_N_MOVES(p);
    for(int i=0;i<4;i++){v[38+i]=i<PR_N_MOVES(p)?PR_MOVES(p)[i]:0;v[42+i]=i<PR_N_MOVES(p)?PR_PP_USED(p)[i]:0;v[46+i]=i<PR_N_MOVES(p)?PR_PP_UP(p)[i]:0;v[50+i]=i<PR_N_MOVES(p)?PR_MAX_PP(p)[i]:-1;}
    v[54]=p->n_summon_types;for(int i=0;i<4;i++)v[55+i]=i<p->n_summon_types?p->summon_types[i]:0;
    v[59]=p->added_type;v[60]=p->terastallized;v[61]=p->tera_type;v[62]=p->transformed;
    if(p->transformed){v[63]=p->transform_species;v[64]=p->transform_form;v[65]=p->transform_gender;v[66]=p->n_moves;
        for(int i=0;i<4;i++){v[67+i]=i<p->n_moves?p->moves[i]:0;v[71+i]=i<p->n_moves?p->pp_used[i]:0;v[75+i]=i<p->n_moves?p->pp_up[i]:0;v[79+i]=i<p->n_moves?p->max_pp_override[i]:-1;}}
    v[83]=!!pr_get_battler_tag(p,BATTLER_TAG_TYPE_POWER_TRICK);v[84]=!!pr_get_battler_tag(p,BATTLER_TAG_TYPE_SUBSTITUTE);
    const pr_form *form=pr_pokemon_form(p);v[85]=form->species;v[86]=form->form_index;v[87]=pr_get_gender(p);
    for(int i=0;i<6;i++)v[88+i]=pr_get_stat(p,i,false);
    int8_t types[PR_MAX_TYPES];int n=pr_get_types(p,types,0);v[94]=n;for(int i=0;i<n;i++)v[95+i]=types[i];
    v[112]=p->n_ab_applied_summon;for(int i=0;i<4;i++)v[113+i]=i<p->n_ab_applied_summon?p->ab_applied_summon[i]:0;
    v[117]=p->n_ab_applied_wave;for(int i=0;i<4;i++)v[118+i]=i<p->n_ab_applied_wave?p->ab_applied_wave[i]:0;
    v[122]=p->temp_turn_count;v[123]=p->temp_wave_turn_count;v[124]=p->ability_override;
    for(int j=0;j<6;j++)v[104+j]=p->ivs[j];v[110]=p->friendship;v[111]=p->ability_suppressed;
    v[100]=p->variant;v[101]=pr_get_sprite_scale(p);v[102]=pr_is_mega(p);v[103]=pr_is_max(p);
}
static bool compare(const pr_pokemon *p, const double *want, int c, int b, char stage)
{
    double got[WIDTH];capture(p,got);bool ok=true;
    for(int i=0;i<WIDTH;i++)if(got[i]!=want[i]){if(c<100)fprintf(stderr,"case%d %c%d field%d C:%.17g JS:%.17g\n",c,stage,b,i,got[i],want[i]);ok=false;}
    return ok;
}
static void rng_read(const char *key, pr_rng *r)
{
    double v[4];read_vector(key,v,4);r->c=integer(v[0],0,INT32_MAX);
    if(v[1]<0||v[1]>=1||v[2]<0||v[2]>=1||v[3]<0||v[3]>=1)bad();
    r->s0=v[1];r->s1=v[2];r->s2=v[3];
}
static bool rng_equal(pr_rng a, pr_rng b){return a.c==b.c&&a.s0==b.s0&&a.s1==b.s1&&a.s2==b.s2;}

static char *line_read(const char *prefix){if(!fgets(line,sizeof line,input))bad();lineno++;if(strncmp(line,prefix,strlen(prefix)))bad();return line+strlen(prefix);}
static const char *locale_get(void *ctx,const char *file,int fallback,size_t *len){
 (void)ctx;if(fallback)return NULL;static struct{char name[64];char *text;size_t len;}cache[16];static int count;
 for(int i=0;i<count;i++)if(!strcmp(file,cache[i].name)){*len=cache[i].len;return cache[i].text;}
 if(count==16)bad();const char *dir=getenv("TURNTEST_LOCALES");if(!dir)bad();char path[4096];snprintf(path,sizeof path,"%s/%s.json",dir,file);FILE *f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END))bad();long n=ftell(f);if(n<1||n>1048576)bad();rewind(f);char *s=malloc(n+1);if(!s||fread(s,1,n,f)!=(size_t)n)bad();s[n]=0;fclose(f);snprintf(cache[count].name,64,"%s",file);cache[count].text=s;cache[count++].len=(size_t)n;*len=(size_t)n;return s;
}
static void queue_numbers(void)
{
 const char *p=line;
 for(int column=0;column<9;column++){
  while(*p==' '||*p=='\t')p++;
  const char *start=p;while(*p&&*p!=' '&&*p!='\t'&&*p!='\n')p++;
  if(start==p)bad();
  if(column==2||column==3||column==5||column==6){
   char *end;errno=0;long long v=strtoll(start,&end,10);
   if(errno||end!=p||v<INT_MIN||v>INT_MAX)bad();
  }
 }
}
static int nq;static char queue[128][4096];
static void emit(const pr_phase *p,char scope,int level,int j){
 char text[2048]="-",stages[128]="-",name[96];
 if(p->type==PH_MESSAGE){char rendered[1024];pr_text_source src={locale_get,NULL,NULL};size_t n=pr_msg_format(&p->msg,&src,rendered,sizeof rendered);if(n>=sizeof rendered)bad();for(size_t i=0;i<n;i++)snprintf(text+2*i,3,"%02x",(unsigned char)rendered[i]);if(!n)strcpy(text,"-");}
 if(p->type==PH_STAT_STAGE_CHANGE){stages[0]=0;size_t n=0;for(int i=0;i<p->n_ssc;i++)n+=(size_t)snprintf(stages+n,sizeof stages-n,"%s%d:%d",i?",":"",p->ssc_stat[i],p->ssc_stages[i]);if(!n)strcpy(stages,"-");}
 if(p->type==PH_QUIET_FORM_CHANGE)snprintf(stages,sizeof stages,"F:%u",p->move_id);
 if(p->type==PH_MARKER)snprintf(name,sizeof name,"Marker:%s",pr_phase_name[p->marker_of]);else snprintf(name,sizeof name,"%s",pr_phase_name[p->type]);
 if(nq>=128)bad();snprintf(queue[nq++],sizeof queue[0],"Q %c %d %d %s %d %d %s %s\n",scope,level,j,name,p->type==PH_MARKER?-1:p->battler,p->type==PH_SHOW_ABILITY?p->anim_result:-1,text,stages);
}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 input=fopen(argv[1],"r");if(!input)return 2;
 pr_closures_init();
 if(!getenv("TURNTEST_LOCALES"))bad();
 double h[3];read_vector("BATTLEBOND ",h,3);if(h[0]!=1)bad();int n=integer(h[1],192,100000),seed=integer(h[2],0,INT32_MAX),failures=0;
 for(int c=0;c<n;c++){
  static pr_battle bt;static pr_engine e;static pr_pokemon mons[4];memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);
  bt.engine=&e;e.n_levels=1;e.current=-1;bt.double_battle=true;bt.game_mode=GAME_MODES_CLASSIC;bt.ignoring_effect_source=-1;bt.wave_index=3;bt.turn=1;
  char wave[64],battle[64];snprintf(wave,sizeof wave,"battlebond-wave-%d",seed);snprintf(battle,sizeof battle,"battlebond-%d-%d",seed,c);pr_seed_from_ascii(&bt.wave_seed,wave);pr_seed_from_ascii(&bt.battle_seed,battle);
  double row[5];read_vector("C ",row,5);if(row[0]!=c)bad();int kind=integer(row[1],0,5),holder=integer(row[2],0,3),victim=integer(row[3],0,3);bt.max_exp_level=integer(row[4],1,255);
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;bool ok=true;
  for(int b=0;b<4;b++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=b)bad();load(&mons[b],v+1,b);bt.field[b]=&mons[b];bt.party[b<2?0:1][b%2]=&mons[b];bt.n_party[b<2?0:1]++;ok&=compare(&mons[b],v+1,c,b,'I');}
  if(kind!=(c/32)%6||holder!=(c%2?2:0)||victim!=(holder<2?2:0))bad();
  bt.participants[0]=mons[0].id;bt.participants[1]=mons[1].id;bt.n_participants=2;
  if(kind==2){mons[victim].n_attacks_received=1;mons[victim].attacks_received[0].source_id=mons[holder].id;mons[victim].attacks_received[0].source_battler_index=holder;mons[victim].attacks_received[0].move=MOVE_ID_TACKLE;mons[victim].attacks_received[0].damage=1;int ph=pr_phase_new(&e,PH_FAINT,victim);if(ph<0)bad();e.pool[ph].prevent_instant_revive=true;e.pool[ph].source=holder;pr_phase_run(&bt,&e,ph);e.used[ph]=false;}
  else if(kind==5){int ph=pr_phase_new(&e,PH_FAINT,holder);if(ph<0)bad();e.pool[ph].prevent_instant_revive=true;pr_phase_run(&bt,&e,ph);e.used[ph]=false;}
  else{pr_ab_params pp={.pokemon=&mons[holder],.victim=&mons[victim],.move=-1,.simulated=kind==1};pr_apply_ab_attrs(&bt,kind==3?PR_AC_PostBattleInitAbAttr:kind==4?PR_AC_PostFaintAbAttr:PR_AC_PostVictoryAbAttr,&pp);}
  double world[14];read_vector("WORLD ",world,14);uint32_t ids[2]={0,0};for(int k=0;k<bt.n_participants&&k<2;k++)ids[k]=bt.participants[k];if(bt.n_participants==2&&ids[0]>ids[1]){uint32_t id=ids[0];ids[0]=ids[1];ids[1]=id;}
  double actual_world[]={bt.weather,bt.weather_turns_left,bt.weather_max_duration,bt.terrain,bt.terrain_turns_left,bt.terrain_max_duration,bt.player_faints,bt.enemy_faints,bt.battle_score,bt.post_battle_loot,bt.ability_bar_visible,bt.n_participants,ids[0],ids[1]};for(int k=0;k<14;k++)if(actual_world[k]!=world[k]){if(c<100)fprintf(stderr,"case%d world%d C:%.17g JS:%.17g\n",c,k,actual_world[k],world[k]);ok=false;}
  for(int b=0;b<4;b++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=b)bad();ok&=compare(&mons[b],v+1,c,b,'O');}
  pr_rng want; rng_read("RO ",&want);if(!rng_equal(want,bt.rng))ok=false;rng_read("BO ",&want);if(!rng_equal(want,bt.battle_rng))ok=false;
  double qr[3];read_vector("QCOUNT ",qr,3);int count=integer(qr[0],0,128);if(qr[1]!=e.n_levels||qr[2]!=e.deferred_active)ok=false;nq=0;
  for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++)emit(&e.pool[e.levels[l][j]],'L',l,j);
  int types[PH_COUNT_],nt=0;for(int t=0;t<PH_COUNT_;t++)if(e.n_dyn[t])types[nt++]=t;for(int j=1;j<nt;j++)for(int k=j;k>0&&strcmp(pr_phase_name[types[k]],pr_phase_name[types[k-1]])<0;k--){int t=types[k];types[k]=types[k-1];types[k-1]=t;}for(int k=0;k<nt;k++)for(int j=0;j<e.n_dyn[types[k]];j++)emit(&e.pool[e.dyn[types[k]][j]],'D',k,j);
  if(nq!=count)ok=false;
  for(int j=0;j<count;j++){
   line_read("Q ");queue_numbers();char scope,name[96],hex[2048],stages[128],extra;int group,order,battler,passive;
   if(sscanf(line,"Q %c %d %d %95s %d %d %2047s %127s %c",&scope,&group,&order,name,&battler,&passive,hex,stages,&extra)!=8 || (scope!='L'&&scope!='D') || group<0 || group>=PR_MAX_PHASES || order<0 || order>=PR_MAX_PHASES || battler< -1 || battler>3)bad();
   const char *phase=!strncmp(name,"Marker:",7)?name+7:name;bool known=false;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(phase,pr_phase_name[t]))known=true;if(!known)bad();
   if(!strcmp(name,"ShowAbilityPhase")){if(passive<0||passive>1)bad();}else if(passive!=-1)bad();
   if(strcmp(hex,"-")){size_t n=strlen(hex);if(n%2)bad();for(size_t k=0;k<n;k++)if(!isxdigit((unsigned char)hex[k]))bad();}
   if(!strcmp(name,"QuietFormChangePhase")){if(strncmp(stages,"F:",2))bad();char *end;errno=0;long target=strtol(stages+2,&end,10);if(errno||end==stages+2||*end||target<0||target>255)bad();}
   else if(strcmp(stages,"-")){const char *at=stages;while(*at){char *end;long stat=strtol(at,&end,10);if(end==at||*end!=':'||stat<STAT_ATK||stat>STAT_EVA)bad();at=end+1;long amount=strtol(at,&end,10);if(end==at||amount< -128||amount>127||(*end&&*end!=','))bad();at=*end?end+1:end;if(*end==','&&!*at)bad();}}
   if(j>=nq||strcmp(line,queue[j])){if(c<20)fprintf(stderr,"case%d queue%d C:%sJS:%s",c,j,j<nq?queue[j]:"missing\n",line);ok=false;}}
  for(int b=0;b<4;b++){pr_reset_summon_data(&mons[b]);double v[WIDTH+1];read_vector("Z ",v,WIDTH+1);if(v[0]!=b)bad();ok&=compare(&mons[b],v+1,c,b,'Z');}
  if(strcmp(line_read("E"),"\n"))bad();
  if(bt.n_unported||bt.n_deferred||e.overflow){if(c<20)fprintf(stderr,"case%d unsupported=%d deferred=%d overflow=%d\n",c,bt.n_unported,bt.n_deferred,e.overflow);ok=false;}
  if(!ok)failures++;
 }
 double end[1];read_vector("BATTLEBOND_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();fclose(input);
 printf("Battle Bond boundary: %d complete cases, %d exact, %d failures\n",n,n-failures,failures);return failures?1:0;
}
