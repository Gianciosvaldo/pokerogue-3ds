/* Strict native TM label corpus reader. Positive/negative and already-known states. */
#include "gc/run.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "pr/reward.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"TM corpus failure line %d, row %d: %s\n",__LINE__,row,#x); return 1; } } while(0)
int main(int argc, char **argv) {
    int row = -1, n, version; char key[64];
    CHECK(argc == 2);
    FILE *f = fopen(argv[1], "r"); CHECK(f);
    CHECK(fscanf(f,"%63s %d %d",key,&version,&n)==3);
    CHECK(!strcmp(key,"TM_COMPAT") && version==1 && n==4500);
    int able=0, unable=0, learned=0, fused=0, si=0, expected_form=0;
    for(row=0;row<n;row++) {
        int index, species, form, fusion, fusion_form, profile; char states[PR_MOVE_COUNT+2];
        CHECK(fscanf(f,"%63s %d %d %d %d %d %d %922s",key,&index,&species,&form,&fusion,&fusion_form,&profile,states)==8);
        CHECK(!strcmp(key,"C") && index==row && profile==row%3 && strlen(states)==PR_MOVE_COUNT);
        CHECK(si < pr_reward_species_count && species==pr_reward_species_data[si].id && form==expected_form);
        const pr_reward_species *donor=&pr_reward_species_data[(si+317)%pr_reward_species_count];
        const int nf=donor->n_forms ? donor->n_forms : 1;
        CHECK(fusion==(profile==2?donor->id:0) && fusion_form==(profile==2?(si+form)%nf:0));
        gc_mon m={0}; m.b.species=species; m.b.form=form; m.b.fusion_species=fusion; m.b.fusion_form=fusion_form;
        m.b.hp=profile==2?0:1; m.n_used_tms=1; m.used_tms[0]=MOVE_ID_FLAMETHROWER;
        if(profile) { m.b.n_moves=4; m.b.moves[0]=MOVE_ID_BIND; m.b.moves[1]=MOVE_ID_FLAMETHROWER; m.b.moves[2]=MOVE_ID_THUNDERBOLT; m.b.moves[3]=MOVE_ID_SPLASH; }
        fused += !!fusion;
        for(int move=0;move<PR_MOVE_COUNT;move++) {
            CHECK(states[move]>='0' && states[move]<='2');
            const int want=states[move]-'0', got=gc_mon_tm_state(&m,move);
            if(got!=want) fprintf(stderr,"species%d form%d fusion%d/%d move%d: got%d want%d\n",species,form,fusion,fusion_form,move,got,want);
            CHECK(got==want);
            able+=want==1; unable+=want==0; learned+=want==2;
        }
        if (profile==2) {
            const int forms=pr_reward_species_data[si].n_forms ? pr_reward_species_data[si].n_forms : 1;
            if (++expected_form==forms) { si++; expected_form=0; }
        }
    }
    CHECK(si==pr_reward_species_count && si==1084 && expected_form==0);
    int count;
    CHECK(fscanf(f,"%63s %d",key,&count)==2 && !strcmp(key,"TM_COMPAT_COMPLETE") && count==n);
    CHECK(fscanf(f,"%63s",key)==EOF); CHECK(!ferror(f)); fclose(f);
    CHECK(fused==n/3 && able>100000 && unable>100000 && learned==n/3*8);
    printf("TM compatibility: %d profiles, %d move states; able%d unable%d learned%d, all exact\n",n,n*PR_MOVE_COUNT,able,unable,learned);
    return 0;
}
