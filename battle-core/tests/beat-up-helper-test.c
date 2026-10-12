/* Real source party-count and power results; no expected formula in this test. */
#include "pr/battle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(int argc,char **argv)
{
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"r");if(!f)return 2;
    char line[512],extra;int version,wanted,wanted_powers;
    if(!fgets(line,sizeof line,f)||sscanf(line,"BEATUP_HELPERS %d %d %d %c",&version,&wanted,&wanted_powers,&extra)!=3||version!=1||wanted!=72||wanted_powers<144){fclose(f);return 2;}
    int cases=0,powers=0,footer=0,failures=0;
    while(fgets(line,sizeof line,f)){
        if(footer||!strchr(line,'\n')){fclose(f);return 2;}
        if(!strncmp(line,"BEATUP_HELPERS_COMPLETE ",24)){
            int n,p;if(sscanf(line,"BEATUP_HELPERS_COMPLETE %d %d %c",&n,&p,&extra)!=2||n!=wanted||p!=wanted_powers||cases!=wanted||powers!=wanted_powers){fclose(f);return 2;}
            footer=1;continue;
        }
        int id,side,size,user_index,count,samples;
        if(sscanf(line,"B %d %d %d %d %d %d %c",&id,&side,&size,&user_index,&count,&samples,&extra)!=6||id!=cases||side<0||side>1||size<1||size>6||user_index<0||user_index>=size||count<1||count>size||samples!=count+2){fclose(f);return 2;}
        pr_battle bt={0};pr_pokemon mons[6]={{0}},target={0};bt.n_party[side]=(uint8_t)size;
        for(int k=0;k<size;k++){
            int index,sp,form,fsp,hp,status;unsigned long long pid;
            if(!fgets(line,sizeof line,f)||sscanf(line,"M %d %llu %d %d %d %d %d %c",&index,&pid,&sp,&form,&fsp,&hp,&status,&extra)!=7||index!=k||pid>UINT32_MAX||sp<=0||sp>=PR_SPECIES_COUNT||form<0||form>31||fsp<0||fsp>=PR_SPECIES_COUNT||hp<0||hp>100||status<0||status>7){fclose(f);return 2;}
            pr_pokemon *p=&mons[k];p->id=(uint32_t)pid;p->species=(uint16_t)sp;p->form=(uint8_t)form;p->fusion_species=(uint16_t)fsp;p->hp=hp;p->status=(uint8_t)status;p->is_player=!side;p->ability=ABILITY_ID_BALL_FETCH;p->stats[STAT_ATK]=123;
            bt.party[side][k]=p;
        }
        pr_pokemon *user=&mons[user_index];pr_battle before=bt;
        if(pr_beat_up_hit_count(&bt,user)!=count)failures++;
        user->hit_count=(int8_t)count;
        for(int h=0;h<samples;h++){
            int index;double expected;
            if(!fgets(line,sizeof line,f)||sscanf(line,"P %d %lf %c",&index,&expected,&extra)!=2||index!=h||!isfinite(expected)||expected<0||expected>100){fclose(f);return 2;}
            user->hits_left=(int8_t)(count-h);double value=-1;
            pr_apply_variable_power(&bt,user,&target,MOVE_ID_BEAT_UP,&value);
            if(value!=expected){fprintf(stderr,"FAIL helper%d power%d: %.17g/%.17g\n",id,h,value,expected);failures++;}
            powers++;
        }
        if(bt.n_unported||bt.n_deferred||memcmp(&bt.rng,&before.rng,sizeof bt.rng)||memcmp(&bt.battle_rng,&before.battle_rng,sizeof bt.battle_rng))failures++;
        if(!fgets(line,sizeof line,f)||strcmp(line,"E\n")){fclose(f);return 2;}
        cases++;
    }
    fclose(f);if(!footer||cases!=wanted||powers!=wanted_powers)return 2;
    printf("Beat Up source count helpers %d / power probes %d: %d failures\n",cases,powers,failures);return failures?1:0;
}
