/* Bounded pinned GameOverPhase bookkeeping. Legacy per-species storage helper
 * keeps its established contract; this caller increments session totals once. */
#include "internal.h"
static bool evolves(int species) {
 for(int i=0;i<gc_species_evo_count;i++)if(gc_species_evos[i].id==species)return gc_species_evos[i].n!=0;
 return false;
}
static bool count_root(gc_system *s,int root) {
 if(root<1||root>GC_MAX_SPECIES_ID||s->starters[root].classic_win_count==UINT16_MAX)return false;
 if(!s->starters[root].classic_win_count){
  if(s->stats.ribbons_owned==UINT32_MAX||s->voucher_counts[GC_VOUCHER_PLUS]==UINT32_MAX)return false;
  s->stats.ribbons_owned++;s->voucher_counts[GC_VOUCHER_PLUS]++;
 }
 s->starters[root].classic_win_count++;
 static const int thresholds[]={10,25,50,75,100};
 for(int i=0;i<5;i++)if(s->stats.ribbons_owned>=(unsigned)thresholds[i])s->achievement_flags[GC_ACHV_10_RIBBONS+i]=1;
 return true;
}
bool gc_classic_finish(gc_run *run,bool victory,const char **why) {
 if(why)*why="unsupported Classic terminal context";
 if(!run||run->run.mode!=GAME_MODES_CLASSIC||run->wave<1||run->terminal_result>2||run->n_party<1||run->n_party>6)return false;
 if(run->terminal_result)return true; /* local exactly-once safety, not source phase idempotence */
 if(!victory){run->terminal_result=1;return true;}
 if(run->wave<200||run->run.has_challenges||!run->sys)return false;
 bool used[6]={0},unevolved=false,fused=false;
 for(int k=0;k<run->n_party;k++){
  int j=run->party_order[k];if(j<0||j>=run->n_party||used[j])return false;used[j]=true;
  const pr_pokemon *p=&run->party[j].b;
  const pr_species *species=gc_species(p->species);if(!species||p->form>=species->nforms||!p->id)return false;
  for(int q=0;q<k;q++)if(run->party[run->party_order[q]].b.id==p->id)return false;
  if(p->fusion_species){const pr_species *fusion=gc_species(p->fusion_species);if(!fusion||p->fusion_form>=fusion->nforms)return false;}
  unevolved|=evolves(p->species);fused|=p->fusion_species!=0;
 }
 static gc_system next;next=*run->sys;
 if(next.stats.classic_wins==UINT32_MAX)return false;
 bool first=!next.achievement_flags[GC_ACHV_CLASSIC_VICTORY]&&!next.stats.classic_wins;
 if(first){
  next.achievement_flags[GC_ACHV_CLASSIC_VICTORY]=1;
  if(!next.classic_voucher_unlocked){if(next.voucher_counts[GC_VOUCHER_GOLDEN]==UINT32_MAX)return false;next.classic_voucher_unlocked=true;next.voucher_counts[GC_VOUCHER_GOLDEN]++;}
 }
 if(unevolved)next.achievement_flags[GC_ACHV_UNEVOLVED_CLASSIC]=1;
 next.stats.classic_wins++;
 for(int k=0;k<run->n_party;k++){
  int species=run->party[run->party_order[k]].b.species;
  int root=gc_species_root_id(species),starter=gc_species_root_for_starter(species);
  if(!count_root(&next,root)||(root!=starter&&!count_root(&next,starter)))return false;
  for(int depth=0;species>0&&depth<10;depth++){next.dex[species].ribbon_flags|=GC_RIBBON_CLASSIC;species=gc_species_prevolution(species);}
 }
 next.unlocks.endless_mode=next.unlocks.mini_black_hole=true;
 next.unlocks.spliced_endless_mode|=fused;next.unlocks.eviolite|=unevolved;
 if(!first){if(next.voucher_counts[GC_VOUCHER_PREMIUM]==UINT32_MAX)return false;next.voucher_counts[GC_VOUCHER_PREMIUM]++;}
 *run->sys=next;run->terminal_result=2;if(why)*why=NULL;return true;
}
