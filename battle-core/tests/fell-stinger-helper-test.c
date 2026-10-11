/* Hand-constructed history/field controls through actual C FaintPhase, not whole turns. */
#include "pr/engine.h"
#include <stdio.h>
#include <string.h>
static pr_battle bt;static pr_engine e;
int main(int argc,char **argv){if(argc!=2)return 2;FILE *f=fopen(argv[1],"r");char l[256];if(!f||!fgets(l,sizeof l,f)||strcmp(l,"FELL_STINGER_HELPERS 1 6\n"))return 2;int fails=0;
for(int i=0;i<6;i++){int mode,first,last,n,on,present,want;char tail;if(!fgets(l,sizeof l,f)||sscanf(l,"H %d %d %d %d %d %d %d %c",&mode,&first,&last,&n,&on,&present,&want,&tail)!=7||mode!=i||first<1||first>=PR_MOVE_COUNT||last<1||last>=PR_MOVE_COUNT||n<0||n>2||on<0||on>1||present<0||present>1||want<0||want>1)return 2;
memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);pr_pokemon a={0},d={0};a.id=1;a.species=SPECIES_ID_RATTATA;a.hp=50;a.stats[0]=50;a.is_player=true;a.on_field=on;a.ability_override=ABILITY_ID_BALL_FETCH;d=a;d.id=2;d.hp=0;d.is_player=false;d.on_field=true;d.n_attacks_received=n;d.attacks_received[0].move=first;d.attacks_received[1].move=last;bt.field[0]=&a;bt.field[2]=&d;bt.party[0][0]=&a;bt.party[1][0]=&d;bt.n_party[0]=bt.n_party[1]=1;bt.engine=&e;e.current=-1;int ph=pr_phase_new(&e,PH_FAINT,2);e.pool[ph].source=present?0:-1;e.pool[ph].prevent_instant_revive=true;pr_phase_run(&bt,&e,ph);int got=0;for(int k=0;k<PR_MAX_PHASES;k++)if(e.used[k]&&e.pool[k].type==PH_STAT_STAGE_CHANGE)got++;if(got!=want){printf("callback case%d got%d want%d\n",i,got,want);fails++;}
}if(!fgets(l,sizeof l,f)||strcmp(l,"END 6\n")||fgetc(f)!=EOF)return 2;printf("Fell callback probes6 identical%d failed%d\n",6-fails,fails);return fails?1:0;}
