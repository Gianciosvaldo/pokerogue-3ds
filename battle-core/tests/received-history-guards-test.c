#include "pr/engine.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pr_battle bt;static pr_pokemon p,enemy;static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"received guard line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void flags(bool unknown){CHECK((bt.n_unported!=0)==unknown);for(int i=0;i<bt.n_unported;i++)CHECK(bt.unported[i]==PR_FEAT_MOVE_HISTORY);bt.n_unported=0;}
static void counter(int category,bool found,bool unknown,int move,int idx,double damage){pr_attack_record r={0};CHECK(pr_received_counter(&bt,&p,category,&r)==found);flags(unknown);if(found){CHECK(r.move==move);CHECK(r.source_battler_index==idx);CHECK(r.damage==damage);}}
static void any(bool positive,bool super,bool oldest,bool unknown){CHECK(pr_received_positive(&bt,&p)==positive);flags(unknown);CHECK(pr_received_super_effective(&bt,&p)==super);flags(unknown);CHECK(pr_received_oldest_positive(&bt,&p)==oldest);flags(unknown);}
static pr_attack_record hit(int move,double damage,int64_t id,int source,int result){return(pr_attack_record){.move=move,.damage=damage,.source_id=id,.source_battler_index=source,.result=result};}
int main(void){pr_closures_init();bt.field[0]=&p;bt.field[2]=&enemy;bt.double_battle=true;p.id=0;p.on_field=true;p.hp=100;enemy.hp=100;enemy.on_field=true;
 counter(-1,false,false,0,0,0);any(false,false,false,false);
 /* A complete short raw tail is authoritative even after cached observations. */
 pr_attack_record r=hit(MOVE_ID_PECK,7,UINT32_MAX,2,HIT_RESULT_SUPER_EFFECTIVE);pr_unshift_attack_received(&p,&r);any(true,true,true,false);
 p.attacks_received[0]=hit(MOVE_ID_GROWL,0,9,0,HIT_RESULT_EFFECTIVE);counter(-1,false,false,0,0,0);any(false,false,false,false);
 /* Long imported history supplies a tail, not evidence about its missing prefix. */
 memset(&p,0,sizeof p);p.n_attacks_received=33;for(int i=0;i<32;i++)p.attacks_received[i]=hit(MOVE_ID_GROWL,0,9,0,HIT_RESULT_EFFECTIVE);
 counter(-1,false,true,0,0,0);any(false,false,false,true);CHECK(!pr_received_damage_from(&bt,&p,77));flags(true);
 r=hit(MOVE_ID_PECK,7,77,2,HIT_RESULT_SUPER_EFFECTIVE);pr_unshift_attack_received(&p,&r);counter(MOVE_CATEGORY_PHYSICAL,true,false,MOVE_ID_PECK,2,7);counter(MOVE_CATEGORY_SPECIAL,false,true,0,0,0);
 CHECK(pr_received_positive(&bt,&p));flags(false);CHECK(pr_received_super_effective(&bt,&p));flags(false);CHECK(!pr_received_oldest_positive(&bt,&p));flags(true);CHECK(pr_received_damage_from(&bt,&p,77));flags(false);CHECK(!pr_received_damage_from(&bt,&p,78));flags(true);
 for(int i=0;i<33;i++){r=hit(MOVE_ID_GROWL,0,9,0,HIT_RESULT_EFFECTIVE);pr_unshift_attack_received(&p,&r);}counter(MOVE_CATEGORY_PHYSICAL,true,false,MOVE_ID_PECK,2,7);CHECK(pr_received_damage_from(&bt,&p,77));flags(false);
 bt.field[0]=NULL;counter(-1,true,false,MOVE_ID_PECK,2,7);bt.field[2]=&p;counter(-1,false,true,0,0,0);bt.field[0]=&p;bt.field[2]=&enemy;
 /* Twelve stable identities are the two six-member parties. Beyond that raw
  * budget, retained witnesses still answer positively and unknowns stay visible. */
 pr_reset_turn_data(&p);for(int i=0;i<12;i++){r=hit(MOVE_ID_TACKLE,1,i==0?UINT32_MAX:i,2,HIT_RESULT_EFFECTIVE);pr_unshift_attack_received(&p,&r);}
 r=hit(MOVE_ID_TACKLE,-1,99,2,HIT_RESULT_EFFECTIVE);pr_unshift_attack_received(&p,&r);CHECK(pr_received_damage_from(&bt,&p,99));flags(false);
 for(int i=0;i<33;i++){r=hit(MOVE_ID_GROWL,0,9,0,HIT_RESULT_EFFECTIVE);pr_unshift_attack_received(&p,&r);}for(int i=0;i<12;i++){CHECK(pr_received_damage_from(&bt,&p,i==0?UINT32_MAX:i));flags(false);}CHECK(!pr_received_damage_from(&bt,&p,99));flags(true);CHECK(!pr_received_damage_from(&bt,&p,100));flags(true);
 /* Received records use JS truthiness, not damage > 0, for Revenge. */
 pr_reset_turn_data(&p);r=hit(MOVE_ID_GROWL,-1,0,2,HIT_RESULT_EFFECTIVE);pr_unshift_attack_received(&p,&r);CHECK(pr_received_damage_from(&bt,&p,0));flags(false);any(false,false,false,false);r.damage=NAN;r.source_id=5;pr_unshift_attack_received(&p,&r);CHECK(!pr_received_damage_from(&bt,&p,5));flags(false);
 /* Saturating the old length field must not lose any query answer. */
 pr_reset_turn_data(&p);r=hit(MOVE_ID_PECK,7,UINT32_MAX,2,HIT_RESULT_SUPER_EFFECTIVE);pr_unshift_attack_received(&p,&r);r=hit(MOVE_ID_GROWL,0,9,0,HIT_RESULT_EFFECTIVE);for(int i=0;i<65540;i++)pr_unshift_attack_received(&p,&r);CHECK(p.n_attacks_received==UINT16_MAX);any(true,true,true,false);counter(MOVE_CATEGORY_PHYSICAL,true,false,MOVE_ID_PECK,2,7);CHECK(pr_received_damage_from(&bt,&p,UINT32_MAX));flags(false);
 pr_reset_summon_data(&p);CHECK(p.n_attacks_received==UINT16_MAX);any(true,true,true,false);
 p.n_attacks_received=40;for(int i=0;i<32;i++)p.attacks_received[i]=hit(MOVE_ID_GROWL,0,0,0,HIT_RESULT_EFFECTIVE);counter(-1,false,true,0,0,0);any(false,false,false,true);
 pr_reset_turn_data(&p);counter(-1,false,false,0,0,0);any(false,false,false,false);CHECK(!pr_received_damage_from(&bt,&p,UINT32_MAX));flags(false);
 /* An aliased record must be copied before shifting its storage. */
 r=hit(MOVE_ID_PECK,5,1,2,HIT_RESULT_EFFECTIVE);pr_unshift_attack_received(&p,&r);r=hit(MOVE_ID_EMBER,9,2,2,HIT_RESULT_SUPER_EFFECTIVE);pr_unshift_attack_received(&p,&r);pr_unshift_attack_received(&p,&p.attacks_received[1]);CHECK(p.attacks_received[0].move==MOVE_ID_PECK&&p.attacks_received[0].damage==5);counter(MOVE_CATEGORY_PHYSICAL,true,false,MOVE_ID_PECK,2,5);counter(MOVE_CATEGORY_SPECIAL,true,false,MOVE_ID_EMBER,2,9);
 /* Invalid cache metadata cannot make raw missing prefixes appear complete. */
 memset(&p,0,sizeof p);p.n_attacks_received=40;for(int i=0;i<32;i++)p.attacks_received[i]=hit(MOVE_ID_GROWL,0,0,0,HIT_RESULT_EFFECTIVE);p.received_history.valid=true;p.received_history.count=40;p.received_history.n_sources=255;p.received_history.counter_known=65535;p.received_history.any_known=255;counter(-1,false,true,0,0,0);any(false,false,false,true);
 printf("Received raw/legacy/identity/reset/saturation guards: %d checks passed\n",checks);return 0;}
