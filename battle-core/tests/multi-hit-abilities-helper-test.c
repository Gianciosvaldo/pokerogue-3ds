/* Actual-source dispatcher results, including numeric thresholds and simulation. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
int main(int argc,char **argv)
{
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"r");if(!f)return 2;
    char line[512],extra;int wanted,version;
    if(!fgets(line,sizeof line,f)||sscanf(line,"MULTIHITABIL_HELPERS %d %d %c",&version,&wanted,&extra)!=2||version!=1||wanted!=440){fclose(f);return 2;}
    int count=0,footer=0,fail=0;
    while(fgets(line,sizeof line,f)){
        if(!strchr(line,'\n')||footer){fclose(f);return 2;}
        if(!strncmp(line,"MULTIHITABIL_HELPERS_COMPLETE ",29)){
            int n;if(sscanf(line,"MULTIHITABIL_HELPERS_COMPLETE %d %c",&n,&extra)!=1||n!=wanted||count!=wanted){fclose(f);return 2;}footer=1;continue;
        }
        int id,side,ability,move,sim,wave,summon;double input,expected;
        if(sscanf(line,"H %d %d %d %d %d %lf %lf %d %d %c",&id,&side,&ability,&move,&sim,&input,&expected,&wave,&summon,&extra)!=9||id!=count||side<0||side>1
            ||(move<=0||move>=PR_MOVE_COUNT)||(ability!=ABILITY_ID_PARENTAL_BOND&&ability!=ABILITY_ID_SKILL_LINK)||sim<0||sim>1||wave<0||wave>1||summon<0||summon>1
            ||!isfinite(input)||!isfinite(expected)||input< -1||input>19||expected< -1||expected>20){fclose(f);return 2;}
        pr_battle bt={0};pr_pokemon p={0},other={0};static pr_engine engine;memset(&engine,0,sizeof engine);engine.n_levels=1;engine.current=-1;bt.engine=&engine;
        p.species=SPECIES_ID_RATTATA;p.ability=ability;p.on_field=true;p.is_player=!side;p.hp=100;p.stats[STAT_HP]=100;
        other.ability=ABILITY_ID_BALL_FETCH;other.on_field=true;other.is_player=!!side;other.hp=100;
        bt.field[side*2]=&p;bt.field[(1-side)*2]=&other;
        double value=input;pr_ab_params params={.pokemon=&p,.opponent=&other,.move=move,.simulated=!!sim,.value=&value};
        pr_battle before=bt;
        pr_apply_ab_attrs(&bt,ability==ABILITY_ID_PARENTAL_BOND?PR_AC_AddSecondStrikeAbAttr:PR_AC_MaxMultiHitAbAttr,&params);
        if(value!=expected||p.n_ab_applied_wave!=wave||p.n_ab_applied_summon!=summon||bt.n_unported||bt.n_deferred||memcmp(&bt.rng,&before.rng,sizeof bt.rng)||memcmp(&bt.battle_rng,&before.battle_rng,sizeof bt.battle_rng)){fprintf(stderr,"FAIL helper%d: value%.17g/%.17g wave%d/%d summon%d/%d unported%d\n",id,value,expected,p.n_ab_applied_wave,wave,p.n_ab_applied_summon,summon,bt.n_unported);fail++;}
        count++;
    }
    fclose(f);if(!footer||count!=wanted)return 2;
    printf("Multi-hit abilities source dispatch helpers %d: %d failures\n",count,fail);return fail?1:0;
}
