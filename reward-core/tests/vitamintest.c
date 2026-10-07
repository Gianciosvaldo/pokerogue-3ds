/* Additive bounded vitamin application replay; compares every produced field.
 * Strict record order: every produced record and field is consumed and compared. */
#include "pr/reward.h"
#include "pr/battle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define MON_FIELDS 65
static FILE *input;
static char line[4096];
static int line_no;
static void die(const char *s) { fprintf(stderr, "invalid trace line %d: %s\n", line_no, s); exit(2); }
static char *record(char kind)
{
    if (!fgets(line, sizeof line, input)) die("unexpected EOF");
    line_no++;
    if (!strchr(line, '\n') || line[0] != kind || (line[1] != ' ' && line[1] != '\n')) die("record order/length");
    return line + 1;
}
static double number(char **s)
{
    char *end; double v = strtod(*s, &end);
    if (end == *s || !isfinite(v)) die("number");
    *s = end; return v;
}
static void end(char *s) { while (isspace((unsigned char)*s)) s++; if (*s) die("extra fields"); }
static void vector(char *s, double *v, int n) { for (int i = 0; i < n; i++) v[i] = number(&s); end(s); }
static void read_mon(pr_reward_mon *m, const double *v)
{
    for (int i = 0; i < MON_FIELDS; i++) {
        double max = i == 0 ? UINT32_MAX : INT32_MAX;
        if (v[i] != floor(v[i]) || v[i] < -1 || v[i] > max) die("mon integer range");
    }
    int k = 0;
#define R(f) m->f = v[k++]
    R(id); R(species); R(form); R(fusion_species); R(fusion_form); R(gender); R(fusion_gender);
    R(level); R(exp); R(hp); R(max_hp); R(status); R(nature); R(tera_type); R(friendship);
    R(ability); R(cur_ability); R(passive); R(pause_evolutions); R(allowed_in_challenge);
    R(ability_index); R(fusion_ability_index); R(shiny); R(fusion_shiny); R(variant); R(fusion_variant);
    R(luck); R(fusion_luck); R(passive_flag); R(n_moves);
    for (int j = 0; j < 4; j++) { R(moves[j].move); R(moves[j].pp_used); R(moves[j].pp_up); R(moves[j].max_pp); }
    R(n_base_moves); for (int j = 0; j < 4; j++) { R(base_moves[j]); }
    for (int j = 0; j < 6; j++) { R(ivs[j]); }
    for (int j = 0; j < 6; j++) { R(stats[j]); }
    R(base_nature); R(custom_nature); m->has_explicit_nature = true;
#undef R
    if (k != MON_FIELDS || m->n_moves > 4 || m->n_base_moves > 4) die("mon shape");
}
static void mon_vector(const pr_reward_mon *m, double *v)
{
    int k = 0;
#define W(f) v[k++] = m->f
    W(id); W(species); W(form); W(fusion_species); W(fusion_form); W(gender); W(fusion_gender);
    W(level); W(exp); W(hp); W(max_hp); W(status); W(nature); W(tera_type); W(friendship);
    W(ability); W(cur_ability); W(passive); W(pause_evolutions); W(allowed_in_challenge);
    W(ability_index); W(fusion_ability_index); W(shiny); W(fusion_shiny); W(variant); W(fusion_variant);
    W(luck); W(fusion_luck); W(passive_flag); W(n_moves);
    for (int j = 0; j < 4; j++) { W(moves[j].move); W(moves[j].pp_used); W(moves[j].pp_up); W(moves[j].max_pp); }
    W(n_base_moves); for (int j = 0; j < 4; j++) { W(base_moves[j]); }
    for (int j = 0; j < 6; j++) { W(ivs[j]); }
    for (int j = 0; j < 6; j++) { W(stats[j]); }
    W(base_nature); W(custom_nature);
#undef W
    if (k != MON_FIELDS) die("mon fields");
}
static uint16_t item_id(const char *s)
{
    if (!strcmp(s, "LEFTOVERS")) return PR_ITEM_LEFTOVERS;
    if (!strcmp(s, "BASE_STAT_BOOSTER")) return PR_ITEM_BASE_STAT_BOOSTER;
    if (!strcmp(s, "HEALING_CHARM")) return PR_ITEM_HEALING_CHARM;
    die("item class"); return 0;
}
static pr_owned_item read_item(char kind)
{
    char name[64]; int n = 0; char *s = record(kind);
    if (sscanf(s, " %63s%n", name, &n) != 1) die("item");
    s += n; pr_owned_item item = {0}; item.item = item_id(name);
    double v[5]; for (int j = 0; j < 5; j++) v[j] = number(&s); end(s);
    if (v[0] < 1 || v[0] > 31 || v[0] != floor(v[0]) || v[1] < -1 || v[1] > 5 || v[1] != floor(v[1])
        || v[2] < 0 || v[2] > 5 || v[2] != floor(v[2]) || v[3] != 0 || v[4] != 1
        || (item.item != PR_ITEM_BASE_STAT_BOOSTER && (v[0] != 1 || v[2] != 0))) die("inventory shape");
    item.stack = v[0]; item.owner = v[1]; item.arg = v[2];
    item.battle_count = v[3]; item.active = v[4]; return item;
}
static bool same_item(const pr_owned_item *a, const pr_owned_item *b)
{
    return a->item == b->item && a->arg == b->arg && a->stack == b->stack && a->owner == b->owner
        && a->battle_count == b->battle_count && a->active == b->active;
}
static pr_reward_ctx fixture(void)
{
    pr_reward_ctx c = {0}; c.n_party = 1;
    pr_reward_mon *m = &c.party[0];
    m->id = 42; m->species = SPECIES_ID_BULBASAUR; m->fusion_species = -1;
    m->level = 20; m->nature = 0;
    for (int j = 0; j < 6; j++) m->ivs[j] = 10;
    pr_battle bt = {0}; pr_pokemon p = {0}; p.species = m->species; p.level = m->level;
    memcpy(p.ivs, m->ivs, sizeof p.ivs); pr_calculate_stats(&bt, &p);
    memcpy(m->stats, p.stats, sizeof m->stats); m->max_hp = p.stats[STAT_HP]; m->hp = m->max_hp / 2;
    return c;
}
static int guards;
static void refused(pr_reward_ctx *c, int stat, int target, double cost, enum pr_reward_vitamin_status expected)
{
    pr_reward_ctx before = *c;
    if (pr_reward_apply_vitamin(c, stat, target, cost, NULL) != expected || memcmp(&before, c, sizeof *c)) die("refusal mutated context/status");
    guards++;
}
static void selftest(void)
{
    guards = 0; pr_reward_ctx c = fixture();
    if (pr_reward_apply_vitamin(NULL, 0, 0, -1, NULL) != PR_REWARD_VITAMIN_UNSUPPORTED) die("null");
    refused(&c, -1, 0, -1, 0); refused(&c, 6, 0, -1, 0); refused(&c, 0, -1, -1, 0); refused(&c, 0, 1, -1, 0);
    refused(&c, 0, 0, -2, 0); refused(&c, 0, 0, NAN, 0);
    c.party[0].ivs[0] = 0; refused(&c, 0, 0, -1, PR_REWARD_VITAMIN_INOPERABLE);
    c = fixture(); c.n_items = 1; c.items[0] = (pr_owned_item){.item=PR_ITEM_BASE_STAT_BOOSTER,.arg=0,.owner=0,.stack=10};
    refused(&c, 0, 0, -1, PR_REWARD_VITAMIN_MAX_STACK);
    c.items[0].stack = 11; refused(&c, 0, 0, -1, 0);
    c.items[0].stack = 1; c.items[0].owner = -1; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.party[0].level = 256; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.party[0].level = 65535; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.party[0].nature = 25; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.party[0].has_explicit_nature = true; c.party[0].base_nature = 25; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.party[0].has_explicit_nature = true; c.party[0].custom_nature = 25; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.party[0].has_explicit_nature = true; c.party[0].custom_nature = 1; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.party[0].species = -1; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.party[0].form = -1; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.party[0].fusion_species = -2; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.challenges = 1u << CHALLENGES_FLIP_STAT; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.n_items = 1; c.items[0] = (pr_owned_item){.item=PR_ITEM_MINT,.owner=0,.stack=1}; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.n_items = 1; c.items[0] = (pr_owned_item){.item=PR_ITEM_HEALING_CHARM,.owner=0,.stack=1}; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.n_items = 1; c.items[0] = (pr_owned_item){.item=PR_ITEM_FORM_CHANGE_ITEM,.owner=-1,.stack=1}; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.n_items = 1; c.items[0] = (pr_owned_item){.item=PR_ITEM_EVOLUTION_TRACKER_GIMMIGHOUL,.owner=0,.stack=1}; refused(&c, 0, 0, -1, 0);
    c = fixture(); c.n_party = 2; c.party[1] = c.party[0]; refused(&c, 0, 0, -1, 0);
    c = fixture(); pr_run run = {0}; run.mode = GAME_MODES_SPLICED_ENDLESS; c.run = &run; refused(&c, 0, 0, -1, 0);
    const int excluded[] = {PR_ITEM_SOUL_DEW,PR_ITEM_MYSTERY_ENCOUNTER_SHUCKLE_JUICE,PR_ITEM_MYSTERY_ENCOUNTER_OLD_GATEAU,PR_ITEM_MYSTERY_ENCOUNTER_MACHO_BRACE};
    for (unsigned j = 0; j < sizeof excluded / sizeof *excluded; j++) {
        c = fixture(); c.n_items = 1; c.items[0] = (pr_owned_item){.item=excluded[j],.owner=0,.stack=1};
        refused(&c, 0, 0, -1, 0);
    }
    c = fixture(); c.n_items = PR_REWARD_MAX_OWNED;
    for (int j = 0; j < c.n_items; j++) c.items[j]=(pr_owned_item){.item=PR_ITEM_HEALING_CHARM,.owner=-1,.stack=1};
    refused(&c, 0, 0, -1, 0);
    c = fixture(); int oldhp=c.party[0].hp, oldmax=c.party[0].max_hp; pr_reward_vitamin_result result;
    if (pr_reward_apply_vitamin(&c, 0, 0, 500, &result) != PR_REWARD_VITAMIN_APPLIED
        || !result.added || result.add_modifier_result || !result.purchase_error || result.reward_finished
        || c.party[0].hp - oldhp != c.party[0].max_hp - oldmax) die("purchased vitamin/HP increment");
    c = fixture(); c.party[0].hp=0;
    if (!pr_reward_apply(&c, PR_ITEM_BASE_STAT_BOOSTER, 0, 0, -1, NULL) || c.party[0].hp) die("generic/fainted vitamin");
    c = fixture(); c.party[0].hp = c.party[0].max_hp; oldmax = c.party[0].max_hp;
    if (pr_reward_apply_vitamin(&c, 0, 0, -1, &result) != PR_REWARD_VITAMIN_APPLIED
        || c.party[0].hp != c.party[0].max_hp || c.party[0].max_hp <= oldmax
        || result.purchase_error || !result.reward_finished) die("full HP/free vitamin");
    printf("VITAMIN host guards: %d whole-context refusals passed; purchase metadata/HP/fainted application pass\n", guards);
}
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--selftest")) { selftest(); return 0; }
    bool mutate_cap=argc==3&&!strcmp(argv[2],"--mutate-cap"), mutate_money=argc==3&&!strcmp(argv[2],"--mutate-money");
    bool mutate_owner=argc==3&&!strcmp(argv[2],"--mutate-owner"), mutate_stats=argc==3&&!strcmp(argv[2],"--mutate-stats");
    if (argc<2||argc>3||(argc==3&&!mutate_cap&&!mutate_money&&!mutate_owner&&!mutate_stats)) return 2;
    input=fopen(argv[1],"r"); if (!input) {perror(argv[1]);return 2;}
    double header[2]; vector(record('V'),header,2);
    if(header[0]!=1||header[1]<100||header[1]>1000000||header[1]!=floor(header[1]))die("version/count");
    int cases=0, failures=0, accepted[6]={0}, inoperable[6]={0}, full[6]={0}, paid_stat[6]={0}, ended_stat[6]={0}, fused=0,fainted=0,custom=0,forms=0,wonder=0,purchased=0,ended=0;
    while(cases<header[1]) {
        double c[6]; vector(record('C'),c,6);
        if(c[0]!=cases||c[1]<1||c[1]>6||c[1]!=floor(c[1])||c[2]<0||c[2]>=c[1]||c[2]!=floor(c[2])
            ||c[3]<0||c[3]>=6||c[3]!=floor(c[3])||(c[4]!=-1&&c[4]<0)||c[5]<0)die("case header");
        int target=c[2],stat=c[3]; double money=c[5]; pr_run run={0}; pr_reward_ctx ctx={0};ctx.run=&run;ctx.n_party=c[1];
        double r[4];vector(record('R'),r,4);run.rng.c=r[0];run.rng.s0=r[1];run.rng.s1=r[2];run.rng.s2=r[3];
        for(int j=0;j<ctx.n_party;j++) {
            double v[MON_FIELDS],got[MON_FIELDS];vector(record('P'),v,MON_FIELDS);read_mon(&ctx.party[j],v);mon_vector(&ctx.party[j],got);
            for(int k=0;k<MON_FIELDS;k++)if(v[k]!=got[k])die("mon narrowing");
        }
        double count;vector(record('I'),&count,1);if(count<0||count>PR_REWARD_MAX_OWNED||count!=floor(count))die("inventory count");ctx.n_items=count;
        for(int j=0;j<ctx.n_items;j++)ctx.items[j]=read_item('H');
        const pr_reward_mon *m=&ctx.party[target];fused+=m->fusion_species>=0;fainted+=m->hp==0;custom+=m->custom_nature>=0;
        forms+=m->form>0||m->fusion_form>0;wonder+=m->ability==ABILITY_ID_WONDER_GUARD||m->passive==ABILITY_ID_WONDER_GUARD;
        if(mutate_cap)ctx.party[target].ivs[stat]=31;
        pr_reward_vitamin_result out;
        enum pr_reward_vitamin_status status=pr_reward_apply_vitamin(&ctx,stat,target,c[4],&out);
        /* Caller projection of SelectModifierPhase: debit only on the raw scene return. */
        if(c[4]>=0&&out.add_modifier_result)money-=c[4];
        if(mutate_money&&out.added&&c[4]>=0){money-=c[4];out.add_modifier_result=true;out.purchase_error=false;}
        if(mutate_owner&&out.added)for(int j=0;j<ctx.n_items;j++)if(ctx.items[j].item==PR_ITEM_BASE_STAT_BOOSTER&&ctx.items[j].arg==stat&&ctx.items[j].owner==target){ctx.items[j].owner=-1;break;}
        if(mutate_stats)ctx.party[target].stats[STAT_ATK]++;
        double a[6];vector(record('A'),a,6);bool ok=status==a[0]&&out.added==a[1]&&out.add_modifier_result==a[2]&&out.purchase_error==a[3]&&out.reward_finished==a[4]&&money==a[5];
        for(int j=1;j<5;j++)if(a[j]!=0&&a[j]!=1)die("result flags");
        if(a[0]==3){accepted[stat]++;purchased+=c[4]>=0;ended+=a[4];paid_stat[stat]+=c[4]>=0;ended_stat[stat]+=a[4];}else if(a[0]==1)inoperable[stat]++;else if(a[0]==2)full[stat]++;else die("result status");
        for(int j=0;j<ctx.n_party;j++){
            double v[MON_FIELDS],got[MON_FIELDS];vector(record('T'),v,MON_FIELDS);mon_vector(&ctx.party[j],got);
            for(int k=0;k<MON_FIELDS;k++)if(v[k]!=got[k]){if(failures<6)fprintf(stderr,"case%d mon%d field%d got%.17g expected%.17g\n",cases,j,k,got[k],v[k]);ok=false;}
        }
        vector(record('J'),&count,1);if(count<0||count>PR_REWARD_MAX_OWNED||count!=floor(count))die("after inventory count");
        if(ctx.n_items!=(int)count||ctx.n_party!=(int)c[1])ok=false;
        for(int j=0;j<(int)count;j++){pr_owned_item expected=read_item('K');if(!same_item(&ctx.items[j],&expected))ok=false;}
        vector(record('S'),r,4);if(run.rng.c!=r[0]||run.rng.s0!=r[1]||run.rng.s1!=r[2]||run.rng.s2!=r[3])ok=false;
        end(record('E'));if(!ok&&failures++<6)fprintf(stderr,"FAIL VITAMIN case%d status%d/%.0f\n",cases,status,a[0]);cases++;
    }
    double footer;vector(record('Z'),&footer,1);if(footer!=header[1]||fgetc(input)!=EOF||ferror(input))die("completion/extra records");fclose(input);
    printf("VITAMIN cases=%d identical=%d mismatches=%d applied=",cases,cases-failures,failures);
    for(int j=0;j<6;j++)printf("%s%d",j?"/":"",accepted[j]);
    printf(" inoperable=");
    for(int j=0;j<6;j++)printf("%s%d",j?"/":"",inoperable[j]);
    printf(" full=");
    for(int j=0;j<6;j++)printf("%s%d",j?"/":"",full[j]);
    printf(" fusion=%d fainted=%d custom=%d forms=%d wonder=%d purchasedWithError=%d rewardEnded=%d skipped=0\n",fused,fainted,custom,forms,wonder,purchased,ended);
    if(failures)return 1;
    for(int j=0;j<6;j++)if(!accepted[j]||!inoperable[j]||!full[j]||!paid_stat[j]||!ended_stat[j])return 3;
    if(!fused||!fainted||!custom||!forms||!wonder||!purchased||!ended)return 3;
    selftest();return 0;
}
