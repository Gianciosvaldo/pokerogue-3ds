/* Numerical and bookkeeping results produced by the real upstream dispatcher. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int types(const char *text,int8_t *out)
{
    if(!strcmp(text,"-"))return 0;
    int n=0;const char *p=text;
    while(*p){
        char *end;long value=strtol(p,&end,10);
        if(end==p||value<0||value>18||n>=PR_MAX_SUMMON_TYPES)return -1;
        out[n++]=(int8_t)value;
        if(!*end)break;
        if(*end!=','||!end[1])return -1;
        p=end+1;
    }
    return n;
}

int main(int argc,char **argv)
{
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"r");if(!f)return 2;
    char line[512],extra;int wanted,version;
    if(!fgets(line,sizeof line,f)||sscanf(line,"PROTEAN_HELPERS %d %d %c",&version,&wanted,&extra)!=2||version!=1||wanted!=80){fclose(f);return 2;}
    int count=0,footer=0,fail=0;
    while(fgets(line,sizeof line,f)){
        if(!strchr(line,'\n')||footer){fclose(f);return 2;}
        if(!strncmp(line,"PROTEAN_HELPERS_COMPLETE ",25)){
            int n;if(sscanf(line,"PROTEAN_HELPERS_COMPLETE %d %c",&n,&extra)!=1||n!=wanted||count!=wanted){fclose(f);return 2;}
            footer=1;continue;
        }
        int id,side,ability,move,sim,mode,wave,summon;char input[64],expected[64];int8_t after[PR_MAX_SUMMON_TYPES];
        if(sscanf(line,"H %d %d %d %d %d %d %63s %63s %d %d %c",&id,&side,&ability,&move,&sim,&mode,input,expected,&wave,&summon,&extra)!=10
            ||id!=count||side<0||side>1||(ability!=ABILITY_ID_PROTEAN&&ability!=ABILITY_ID_LIBERO)||move<=0||move>=PR_MOVE_COUNT||sim<0||sim>1||mode<0||mode>9||wave<0||wave>1||summon<0||summon>1){fclose(f);return 2;}
        pr_battle bt={0};pr_pokemon p={0},other={0};static pr_engine engine;memset(&engine,0,sizeof engine);engine.n_levels=1;engine.current=-1;bt.engine=&engine;
        p.species=SPECIES_ID_RATTATA;p.ability=ability;p.on_field=true;p.is_player=!side;p.hp=p.stats[STAT_HP]=100;
        int before_n=types(input,p.summon_types),after_n=types(expected,after);
        if(before_n<0||after_n<0){fclose(f);return 2;}
        p.n_summon_types=(uint8_t)before_n;
        if(mode==5)p.terastallized=true,p.tera_type=POKEMON_TYPE_GRASS;
        if(mode==8)p.added_type=POKEMON_TYPE_GRASS;
        if(mode==9){p.ab_applied_wave[0]=p.ab_applied_summon[0]=(uint16_t)ability;p.n_ab_applied_wave=p.n_ab_applied_summon=1;}
        other.ability=ABILITY_ID_BALL_FETCH;other.on_field=true;other.is_player=!!side;other.hp=100;
        bt.field[side*2]=&p;bt.field[(1-side)*2]=&other;
        pr_battle rng_before=bt;
        pr_ab_params params={.pokemon=&p,.opponent=&other,.move=move,.simulated=!!sim};
        pr_apply_ab_attrs(&bt,PR_AC_PokemonTypeChangeAbAttr,&params);
        if(p.n_summon_types!=after_n||memcmp(p.summon_types,after,(size_t)after_n)||p.added_type!=(mode==8?POKEMON_TYPE_GRASS:0)
            ||p.n_ab_applied_wave!=wave||p.n_ab_applied_summon!=summon||bt.n_unported||bt.n_deferred
            ||memcmp(&bt.rng,&rng_before.rng,sizeof bt.rng)||memcmp(&bt.battle_rng,&rng_before.battle_rng,sizeof bt.battle_rng)){
            fprintf(stderr,"FAIL helper%d: types%d/%d wave%d/%d summon%d/%d unported%d\n",id,p.n_summon_types,after_n,p.n_ab_applied_wave,wave,p.n_ab_applied_summon,summon,bt.n_unported);fail++;
        }
        count++;
    }
    fclose(f);if(!footer||count!=wanted)return 2;
    printf("Protean / Libero source dispatch helpers %d: %d failures\n",count,fail);return fail?1:0;
}
