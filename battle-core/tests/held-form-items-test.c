/* Pinned source inputs and exact native predicate/selection/queue observations. */
#include "pr/engine.h"
#include "pr_item_forms.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pr_battle bt;
static pr_engine engine;
static pr_pokemon actors[4];
static uint32_t ids[4];
static FILE *input;
static unsigned checks, line;
static char text[2048], *cursor;
typedef struct {
    int species, source_index, from, to, item, active, quiet, default_form, ability, caught;
} native_rule;
static native_rule rules[416];
static bool enum_seen[200], selected_seen[416], eligible_seen[416], shadowed[416];
static bool seen0[416][32][4], seen1[6][32][8], seen2[4][32][4][25], seen3[416][8], seen4[416][32][4], seen5[2][32][4];
static bool routes_seen[416][2];
static int route_count;
static int dex_slot[416], default_slot[416], totals[6];
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"held form line%u C%d: %s\n",line,__LINE__,#x);exit(1); } } while(0)
static void record(char tag) { CHECK(fgets(text,sizeof text,input));line++;CHECK(text[0]==tag&&text[1]==' '&&strchr(text,'\n'));cursor=text+2; }
static int64_t number(void) { char *end;errno=0;long long n=strtoll(cursor,&end,10);CHECK(!errno&&end!=cursor);cursor=end;return n; }
static int bounded(int low,int high) { int64_t n=number();CHECK(n>=low&&n<=high);return (int)n; }
static void finish(void) { CHECK(!strcmp(cursor,"\n")); }
static void setup(void)
{
    memset(&bt,0,sizeof bt);memset(&engine,0,sizeof engine);memset(actors,0,sizeof actors);
    bt.engine=&engine;engine.current=-1;engine.n_levels=1;bt.double_battle=true;
    bt.wave_index=1;bt.turn=1;bt.game_mode=GAME_MODES_CLASSIC;bt.max_exp_level=10;
    bt.last_used_pokeball=-1;bt.ignoring_effect_source=-1;bt.rng_live=bt.battle_rng_live=true;
    pr_seedstr seed;pr_seed_from_ascii(&seed,"form-global");pr_rng_sow(&bt.rng,&seed);
    pr_seed_from_ascii(&seed,"form-battle");pr_rng_sow(&bt.battle_rng,&seed);
    for(int i=0;i<4;i++){
        pr_pokemon *p=&actors[i];p->id=ids[i];p->species=SPECIES_ID_MEW;p->hp=1;p->level=5;
        p->on_field=true;p->is_player=i<2;p->base_ability=ABILITY_ID_SYNCHRONIZE;p->ability=ABILITY_ID_LEVITATE;
        bt.field[i]=p;bt.party[i/2][i%2]=p;
    }
    bt.n_party[0]=bt.n_party[1]=2;bt.form_item_dex_known=true;
}
int main(int argc,char **argv)
{
    CHECK(argc==2);input=fopen(argv[1],"r");CHECK(input);
    CHECK(fgets(text,sizeof text,input)&&!strcmp(text,"FORMITEMS 1 159 416 124416\n"));line++;
    CHECK(pr_mod_class_by_name("PokemonFormChangeItemModifier")==PR_MOD_FORM_CHANGE_ITEM);
    CHECK(pr_mod_is(&(pr_modifier){.cls=PR_MOD_FORM_CHANGE_ITEM},PR_MOD_HELD_ITEM));
    record('U');for(int i=0;i<4;i++){int64_t id=number();CHECK(id>0&&id<=UINT32_MAX);ids[i]=id;for(int j=0;j<i;j++)CHECK(ids[j]!=ids[i]);}finish();
    record('D');CHECK(bounded(6,6)==6);for(int i=0;i<6;i++)CHECK(bounded(1,UINT16_MAX)==pr_form_item_caught_species[i]);finish();
    for(int i=0;i<159;i++){record('E');int item=bounded(0,199);finish();CHECK(!enum_seen[item]);enum_seen[item]=true;CHECK(pr_form_item_known(item));}
    for(int i=-1;i<=200;i++)CHECK(pr_form_item_known(i)==(i>=0&&i<200&&enum_seen[i]));
    int ndex=0,ndefault=0;
    for(int i=0;i<416;i++){
        record('R');CHECK(bounded(0,415)==i);native_rule *r=&rules[i];
        r->species=bounded(1,UINT16_MAX);r->source_index=bounded(0,255);r->from=bounded(0,255);r->to=bounded(0,255);
        r->item=bounded(0,199);r->active=bounded(0,1);r->quiet=bounded(0,1);r->default_form=bounded(-1,254);
        r->ability=bounded(0,PR_ABILITY_COUNT-1);r->caught=bounded(0,UINT16_MAX);finish();CHECK(enum_seen[r->item]&&r->from!=r->to);
        const pr_item_form_rule *c=&pr_item_form_rules[i];
        CHECK(c->species==r->species&&c->from==r->from&&c->to==r->to&&c->item==r->item&&c->flags==(r->active|(r->quiet<<1))
              &&c->default_form==(r->default_form<0?255:r->default_form)&&c->ability==r->ability&&c->caught_species==r->caught);
        dex_slot[i]=r->caught?ndex++:-1;default_slot[i]=r->default_form>=0?ndefault++:-1;
    }
    CHECK(ndex==6&&ndefault==4);
    for(int item=0;item<200;item++)if(enum_seen[item])for(int active=0;active<2;active++){
        pr_modifier canonical={.cls=PR_MOD_FORM_CHANGE_ITEM,.pokemon_id=1,.num=item,.stack_count=1,.form_item_active=active};
        CHECK(pr_mod_is_ported_item(&canonical));
        for(int invalid=0;invalid<7;invalid++){
            pr_modifier m=canonical;
            if(invalid==0)m.stack_count=0;
            if(invalid==1)m.stack_count=2;
            if(invalid==2)m.virtual_stack_count=1;
            if(invalid==3)m.is_transferable=true;
            if(invalid==4)m.unknown_subclass=true;
            if(invalid==5)m.pokemon_id=-1;
            if(invalid==6)m.num=200;
            CHECK(!pr_mod_is_ported_item(&m));
        }
    }
    int caught_rule=-1;for(int i=0;i<416;i++)if(rules[i].caught){caught_rule=i;break;}CHECK(caught_rule>=0);
    setup();pr_pokemon *guard=&actors[0];native_rule *gr=&rules[caught_rule];guard->species=gr->species;guard->form=gr->from;
    bt.n_mods[0]=1;bt.mods[0][0]=(pr_modifier){.cls=PR_MOD_FORM_CHANGE_ITEM,.pokemon_id=guard->id,.num=gr->item,.stack_count=1,.form_item_active=gr->active};
    bt.form_item_dex_known=false;CHECK(!pr_item_form_rule_eligible(&bt,guard,caught_rule)&&bt.n_unported);
    bt.n_unported=0;bt.form_item_dex_known=true;bt.form_item_dex_mask=63;
    CHECK(pr_item_form_rule_eligible(&bt,guard,caught_rule));bt.engine=NULL;int missing=-2;
    CHECK(!pr_trigger_item_form_change(&bt,guard,false,&missing)&&missing>=0&&bt.n_unported);
    setup();bt.n_mods[0]=PR_MAX_MODIFIERS+1;CHECK(!pr_item_form_rule_eligible(&bt,&actors[0],0)&&bt.n_unported);
    CHECK(!pr_apply_form_change_item(NULL,NULL,NULL,false,NULL));
    record('S');CHECK(bounded(11,11)==11);
    for(int i=0;i<11;i++){int rule=bounded(0,415);CHECK(!shadowed[rule]);shadowed[rule]=true;}finish();
    for(int i=0;i<124416;i++){
        record('C');CHECK(bounded(0,124415)==i);int rule=bounded(0,415),group=bounded(0,5),shape=bounded(0,31),mask=bounded(0,63),over=bounded(0,1),gender=bounded(-1,2),nature=bounded(0,24);
        int base=bounded(0,PR_ABILITY_COUNT-1),ability=bounded(0,PR_ABILITY_COUNT-1),on_field=bounded(0,1),should=bounded(0,1),eligible=bounded(0,1),during=bounded(0,1),result=bounded(0,1),selected=bounded(-1,415),kind=bounded(0,2),after=bounded(0,1);finish();
        native_rule *r=&rules[rule];totals[group]++;
        CHECK(on_field==(group!=4));
        if(group==0||group==4){CHECK(mask==0||mask==63);int gate=over+(mask==63?2:0);bool (*seen)[32][4]=group==0?seen0:seen4;CHECK(!seen[rule][shape][gate]);seen[rule][shape][gate]=true;}
        if(group==1){CHECK(dex_slot[rule]>=0&&!over);int slot=-1;for(int j=0;j<8;j++)if(mask==(int[]){0,1,2,4,8,16,32,63}[j])slot=j;CHECK(slot>=0&&!seen1[dex_slot[rule]][shape][slot]);seen1[dex_slot[rule]][shape][slot]=true;}
        if(group==2){CHECK(default_slot[rule]>=0&&mask==63&&!over);CHECK(!seen2[default_slot[rule]][shape][gender+1][nature]);seen2[default_slot[rule]][shape][gender+1][nature]=true;}
        if(group==3){CHECK((shape&5)==1&&mask==63&&!over);int slot=((shape>>1)&1)|(((shape>>3)&1)<<1)|(((shape>>4)&1)<<2);CHECK(!seen3[rule][slot]);seen3[rule][slot]=true;}
        if(group==5){CHECK(rule>=316&&rule<=317&&mask<4&&!over);CHECK(!seen5[rule-316][shape][mask]);seen5[rule-316][shape][mask]=true;}
        setup();bt.form_item_dex_mask=mask;int actor=(shape&8)?2:0;pr_pokemon *p=&actors[actor];
        p->on_field=on_field;p->species=r->species;p->form=r->from;p->nature=nature;p->gender=gender;p->base_ability=base;p->ability=ability;
        record('B');int counts[2]={bounded(0,3),bounded(0,3)};finish();
        for(int side=0;side<2;side++){
            bt.n_mods[side]=counts[side];
            for(int j=0;j<counts[side];j++){
                record('M');int64_t owner=number();CHECK(owner>=0&&owner<=UINT32_MAX);int item=bounded(0,199),active=bounded(0,1);finish();CHECK(enum_seen[item]);
                bt.mods[side][j]=(pr_modifier){.cls=PR_MOD_FORM_CHANGE_ITEM,.pokemon_id=owner,.num=item,.form_item_active=active,.stack_count=1};
            }
        }
        int side=(shape&16)?1:0;CHECK(counts[side]>0);pr_modifier *m=&bt.mods[side][0];
        CHECK(m->num==r->item&&m->form_item_active==!!(shape&1));CHECK((m->pokemon_id==p->id)==should);
        CHECK(pr_mod_is_ported_item(m));pr_rng global=bt.rng,battle=bt.battle_rng;bool original=m->form_item_active;
        m->form_item_active=during;bool actual_eligible=pr_item_form_rule_eligible(&bt,p,rule);m->form_item_active=original;
        CHECK(actual_eligible==eligible);if(actual_eligible)eligible_seen[rule]=true;
        int chosen=-2;bool actual=pr_apply_form_change_item(&bt,p,m,!!(shape&2),&chosen);
        if(actual!=result||chosen!=selected)fprintf(stderr,"case%d rule%d shape%d mask%d ability%d eligible%d result%d/%d selected%d/%d\n",i,rule,shape,mask,ability,actual_eligible,actual,result,chosen,selected);
        CHECK(actual==result&&chosen==selected&&m->form_item_active==after);if(chosen>=0)selected_seen[chosen]=true;
        int n=0;
        for(int level=0;level<engine.n_levels;level++)for(int k=0;k<engine.level_len[level];k++){
            pr_phase *phase=&engine.pool[engine.levels[level][k]];n++;
            CHECK(selected>=0&&phase->type==(kind==1?PH_FORM_CHANGE:PH_QUIET_FORM_CHANGE)&&phase->pokemon==p&&phase->move_id==rules[selected].to);
        }
        CHECK(n==(kind?1:0)&&!engine.overflow&&!bt.n_unported&&!bt.n_deferred);
        CHECK(!memcmp(&global,&bt.rng,sizeof global)&&!memcmp(&battle,&bt.battle_rng,sizeof battle));
        record('N');int routes=bounded(0,2);finish();CHECK(routes==0||routes==2);
        if(routes){CHECK(!routes_seen[rule][result]);routes_seen[rule][result]=true;route_count++;}
        for(int j=0;j<routes;j++){
            record('Q');int delayed=bounded(0,1),want=bounded(0,1),levels=bounded(1,2);finish();CHECK(delayed==j&&want==result);
            memset(&engine,0,sizeof engine);engine.current=-1;engine.n_levels=1;
            int sentinel=pr_phase_new(&engine,PH_MESSAGE,-1);CHECK(sentinel>=0);pr_phase_push(&bt,&engine,sentinel);
            m->form_item_active=during;
            if(delayed)pr_form_change_delayed(&bt,p,PR_FCT_SpeciesFormChangeItemTrigger);
            else pr_form_change(&bt,p,PR_FCT_SpeciesFormChangeItemTrigger);
            CHECK(engine.n_levels==levels);
            for(int l=0;l<levels;l++){
                record('L');int len=bounded(0,2);CHECK(engine.level_len[l]==len);
                for(int k=0;k<len;k++){
                    int expected=bounded(-2,415);pr_phase *phase=&engine.pool[engine.levels[l][k]];
                    if(expected==-2)CHECK(engine.levels[l][k]==sentinel);
                    else {CHECK(expected>=0);CHECK(expected==selected&&phase->pokemon==p&&phase->move_id==rules[expected].to&&phase->type==(kind==1?PH_FORM_CHANGE:PH_QUIET_FORM_CHANGE));}
                }finish();
            }
            CHECK(!engine.overflow&&!bt.n_unported&&!bt.n_deferred);
            CHECK(!memcmp(&global,&bt.rng,sizeof global)&&!memcmp(&battle,&bt.battle_rng,sizeof battle));
            m->form_item_active=after;
        }
    }
    CHECK(totals[0]==53248&&totals[1]==1536&&totals[2]==12800&&totals[3]==3328&&totals[4]==53248&&totals[5]==256);
    int selected_count=0;
    for(int i=0;i<416;i++){CHECK(eligible_seen[i]);CHECK(selected_seen[i]!=shadowed[i]);selected_count+=selected_seen[i];}
    CHECK(selected_count==405&&route_count==832);
    for(int i=0;i<416;i++)CHECK(routes_seen[i][0]&&routes_seen[i][1]);
    CHECK(fgets(text,sizeof text,input)&&!strcmp(text,"END 159 416 124416\n")&&fgetc(input)==EOF);fclose(input);
    printf("Held forms: 416 rule predicates, 159 item identities, 124416 native callback/queue snapshots, all 11 shadowed rules retained; %u checks\n",checks);
    return 0;
}
