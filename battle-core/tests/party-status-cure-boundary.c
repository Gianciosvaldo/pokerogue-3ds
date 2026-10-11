/* Explicit unsupported active-partner boundary: refuse before partial mutation. */
#include "pr/engine.h"
#include <stdio.h>
#include <string.h>
static pr_battle bt;static pr_engine e;
int main(void){int checked=0;for(int move=1;move<PR_MOVE_COUNT;move++){const pr_move *m=&pr_moves[move];for(int a=m->attr0;a<m->attr0+m->nattr;a++)if(pr_attrs[a].cls==PR_AC_PartyStatusCureAttr){for(int mode=0;mode<3;mode++){
memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);pr_pokemon p={0},q={0},t={0};p.id=1;p.hp=100;p.is_player=true;p.status=STATUS_EFFECT_POISON;q=p;q.id=2;q.on_field=true;t=p;t.id=3;bt.party[0][0]=&p;bt.party[0][1]=&q;bt.n_party[0]=mode==2?0:2;bt.double_battle=mode==0;bt.engine=&e;e.pool[0].move_id=move;e.used[0]=true;pr_pokemon before=p,other=q;pr_apply_move_effect(&bt,&e,0,a,&p,&t);if(memcmp(&p,&before,sizeof p)||memcmp(&q,&other,sizeof q)||bt.n_unported!=1)return 1;checked++;
}}}printf("Party cure active-partner/empty boundary: %d atomic refusals\n",checked);return checked==9?0:1;}
