/* Actual EncounterPhase pair ordering and constructor fields. Replay native moveset
 * results/RNG at the existing bounded moveset adapter, whose wider pools remain unported. */
#include "../src/internal.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
static pr_rng move_before[2],move_after[2];static int move_ids[2][4],move_count;
static gc_run r;static jmp_buf stop;static pr_rng before;static int wave,level,checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"double encounter case wave%d line%d: %s\n",wave,__LINE__,#x);exit(1);}}while(0)
int __wrap_pr_run_new_battle(pr_run *run,int w,const pr_new_battle_ctx *ctx,pr_new_battle *out){
 CHECK(w==wave);memset(out,0,sizeof *out);out->battle_type=BATTLE_TYPE_WILD;out->trainer_type=out->variant=out->template_index=-1;
 out->double_battle=true;out->n_levels=2;out->levels[0]=level;out->levels[1]=level+1;strcpy(out->battle_seed,"controlled");run->rng=before;return 0;
}
int __wrap_pr_run_generate_enemy_items(pr_run *run,const pr_enemy_items_ctx *ctx,pr_enemy_items *out){CHECK(ctx->double_battle&&ctx->n_party==2);longjmp(stop,1);return 0;}
static bool same(pr_rng a,pr_rng b){return a.c==b.c&&a.s0==b.s0&&a.s1==b.s1&&a.s2==b.s2;}
bool __wrap_gc_enemy_moveset(gc_run *run,gc_mon *m,const pr_enemy_gen *g){
 if(move_count<2&&!same(run->run.rng,move_before[move_count]))fprintf(stderr,"species%d id%u moveset%d initial RNG C%.17g %.17g %.17g %.17g native%.17g %.17g %.17g %.17g\n",m->b.species,m->b.id,move_count,run->run.rng.c,run->run.rng.s0,run->run.rng.s1,run->run.rng.s2,move_before[move_count].c,move_before[move_count].s0,move_before[move_count].s1,move_before[move_count].s2);
 CHECK(move_count<2&&same(run->run.rng,move_before[move_count]));
 for(int k=0;k<4;k++)if(move_ids[move_count][k])gc_mon_set_move(m,k,move_ids[move_count][k]);
 run->run.rng=move_after[move_count++];return true;
}
static void mon(FILE *f,char tag,int index){char got;CHECK(fscanf(f," %c",&got)==1&&got==tag);double v[32];for(int k=0;k<32;k++)CHECK(fscanf(f,"%lf",&v[k])==1);gc_mon *m=&r.enemy[index];pr_pokemon *p=&m->b;double out[32];int k=0;
#define W(x) out[k++]=(x)
 W(p->id);W(p->species);W(p->form);W(p->level);W(p->gender);W(p->nature);W(m->ability_index);W(p->shiny);W(m->variant);W(p->friendship);W(p->tera_type);W(p->boss_segments);W(p->fusion_species);W(p->fusion_form);W(p->n_moves);
 for(int j=0;j<4;j++){W(p->moves[j]);}for(int j=0;j<6;j++){W(p->ivs[j]);}for(int j=0;j<6;j++){W(p->stats[j]);}W(p->hp);
#undef W
 CHECK(k==32);for(int j=0;j<32;j++){if(v[j]!=out[j])fprintf(stderr,"enemy%d field%d native%.17g C%.17g\n",index,j,v[j],out[j]);CHECK(v[j]==out[j]);}}
int main(int argc,char **argv){CHECK(argc==2);FILE *f=fopen(argv[1],"r");CHECK(f);int version,n;CHECK(fscanf(f,"DOUBLE_ENCOUNTER %d %d",&version,&n)==2&&version==1&&n==120);
 for(int ci=0;ci<n;ci++){int i;char seed[64],wseed[64];CHECK(fscanf(f," C %d %d %d %63s %63s",&i,&wave,&level,seed,wseed)==5&&i==ci);CHECK(fscanf(f," R %lf %lf %lf %lf",&before.c,&before.s0,&before.s1,&before.s2)==4);
 gc_run_new(&r,GAME_MODES_ENDLESS,seed,0);pr_seed_from_ascii(&r.run.wave_seed,wseed);r.wave=wave-1;r.biome=0;r.wave_cycle_offset=0;r.run.offset_gym=false;r.trainer_id=123;r.secret_id=456;
 r.n_party=1;r.party_order[0]=0;r.party[0].b=(pr_pokemon){.id=1,.species=1,.level=5,.hp=20,.is_player=true,.ability_override=-1,.custom_passive=-1};r.party[0].b.stats[0]=20;
 for(int j=0;j<2;j++){int idx;CHECK(fscanf(f," M %d %lf %lf %lf %lf %lf %lf %lf %lf %d %d %d %d",&idx,&move_before[j].c,&move_before[j].s0,&move_before[j].s1,&move_before[j].s2,&move_after[j].c,&move_after[j].s0,&move_after[j].s1,&move_after[j].s2,&move_ids[j][0],&move_ids[j][1],&move_ids[j][2],&move_ids[j][3])==13&&idx==j);}
 move_count=0;
 if(!setjmp(stop)){gc_new_battle(&r);}CHECK(move_count==2);CHECK(r.n_enemy==2);mon(f,'A',0);mon(f,'B',1);pr_rng expected;CHECK(fscanf(f," S %lf %lf %lf %lf",&expected.c,&expected.s0,&expected.s1,&expected.s2)==4);CHECK(same(r.run.rng,expected));
 }
 int cases,mons;CHECK(fscanf(f," COMPLETE %d %d",&cases,&mons)==2&&cases==n&&mons==n*2);char trailing;CHECK(fscanf(f," %c",&trailing)==EOF);fclose(f);printf("Actual native double EncounterPhase: %d cases, %d checks passed\n",n,checks);return 0;}
