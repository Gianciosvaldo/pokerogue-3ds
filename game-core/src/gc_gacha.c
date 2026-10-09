/* Pinned Egg constructor + purchase/pity slice, with supplied offline entropy/time. */
#include "internal.h"
#include "gc/gacha.h"
static const char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static bool rng_ok(const pr_rng *r){return isfinite(r->c)&&r->c>=0&&r->c<=2091639&&r->c==floor(r->c)&&isfinite(r->s0)&&r->s0>=0&&r->s0<1&&isfinite(r->s1)&&r->s1>=0&&r->s1<1&&isfinite(r->s2)&&r->s2>=0&&r->s2<1;}
static int entropy_int(gc_gacha_ctx *c,int range){if(range<=1)return 0;c->entropy_draws++;return (int)floor(pr_rng_frac(&c->entropy)*range);}
uint16_t gc_gacha_featured_species(uint64_t timestamp){
 if(timestamp>UINT64_C(8640000000000000))return 0;
 uint16_t ids[1024];int n=0;for(int i=0;i<gc_egg_pool_count;i++)if(gc_egg_pool[i].tier==3&&gc_egg_pool[i].species!=SPECIES_ID_ETERNATUS){if(n>=1024)return 0;ids[n++]=gc_egg_pool[i].species;}
 if(!n)return 0;
 uint64_t day=timestamp/86400000;pr_seedstr base,shift;pr_seed_from_ascii(&base,"1073741824");pr_seed_shift(&shift,&base,(int)(day/n));pr_rng r;pr_rng_sow(&r,&shift);
 for(int i=n-1;i>0;i--){int j=(int)floor(pr_rng_frac(&r)*(i+1));uint16_t t=ids[i];ids[i]=ids[j];ids[j]=t;}return ids[day%n];
}
static bool has_species(const gc_system *s,int id){if(s->dex[id].caught_attr)return true;for(int i=0;i<s->n_eggs;i++)if(s->eggs[i].species==id)return true;return false;}
static bool has_variants(int id){for(int i=0;i<gc_egg_pool_count;i++)if(gc_egg_pool[i].species==id)return gc_egg_pool[i].variants;return false;}
static bool egg_new(gc_system *s,gc_gacha_ctx *ctx,gc_egg *e,int forced){
 char seed[25];for(int i=0;i<24;i++)seed[i]=chars[entropy_int(ctx,62)];seed[24]=0;pr_seedstr key;pr_seed_from_ascii(&key,seed);pr_rng r;pr_rng_sow(&r,&key);
 int offset=ctx->machine==1,tier=forced;
 if(tier<0){int value=entropy_int(ctx,256);tier=value>=52+offset?0:value>=8+offset?1:value>=1+offset?2:3;}
 s->egg_pity[1]++;s->egg_pity[2]++;s->egg_pity[3]+=1+offset;
 if(tier==0){if(s->egg_pity[3]>=412)tier=3;else if(s->egg_pity[2]>=59)tier=2;else if(s->egg_pity[1]>=9)tier=1;}s->egg_pity[tier]=0;
 memset(e,0,sizeof *e);e->tier=tier;e->id=(uint32_t)entropy_int(ctx,1073741824)+(uint32_t)tier*1073741824u;e->source_type=ctx->machine;e->timestamp_ms=ctx->timestamp_ms;
 static const int waves[]={10,25,50,100};e->hatch_waves=waves[tier];e->is_shiny=!pr_rng_seed_int(&r,ctx->machine==2?64:128,0);
 if(e->is_shiny){int v=pr_rng_seed_int(&r,10,0);e->variant_tier=v>=4?0:v>=1?1:2;}
 if(tier==0&&e->id%204==0)e->species=pr_rng_seed_int(&r,8,0)!=1?SPECIES_ID_PHIONE:SPECIES_ID_MANAPHY;
 else if(tier==3&&ctx->machine==1&&!pr_rng_seed_int(&r,2,0))e->species=gc_gacha_featured_species(ctx->timestamp_ms);
 else{
  int pool[1024],n=0,locked=0;bool force=s->unlock_pity[tier]>=9;
  for(int i=0;i<gc_egg_pool_count;i++){const gc_egg_pool_entry *p=&gc_egg_pool[i];if(p->tier!=tier||p->species==SPECIES_ID_PHIONE||p->species==SPECIES_ID_MANAPHY||p->species==SPECIES_ID_ETERNATUS)continue;if(n>=1024)return false;pool[n++]=i;if(!has_species(s,p->species))locked++;}
  int weights[1024],count=0,total=0;static const int mincost[]={1,4,6,8},maxcost[]={3,5,7,9};
  for(int i=0;i<n;i++){const gc_egg_pool_entry *p=&gc_egg_pool[pool[i]];if(force&&locked&&has_species(s,p->species))continue;if(e->variant_tier&&!p->variants)continue;int cost=p->cost;if(cost<mincost[tier])cost=mincost[tier];if(cost>maxcost[tier])cost=maxcost[tier];int weight=(int)floor(((double)(maxcost[tier]-cost)/(maxcost[tier]-mincost[tier]+1)*1.5+1)*100);total+=weight;weights[count]=total;pool[count++]=pool[i];}
  if(!count||!total)return false;
  int roll=pr_rng_seed_int(&r,total,0);for(int i=0;i<count;i++)if(roll<weights[i]){e->species=gc_egg_pool[pool[i]].species;break;}
  if(has_species(s,e->species)){if(s->unlock_pity[tier]<10)s->unlock_pity[tier]++;}else s->unlock_pity[tier]=0;
 }
 if(!e->species)return false;
 if(!has_variants(e->species))e->variant_tier=0;
 bool manaphy=e->species==SPECIES_ID_MANAPHY||e->species==SPECIES_ID_PHIONE;int mt=manaphy?2:tier;static const int normal[]={48,24,12,6},boosted[]={16,12,6,3};e->egg_move_index=pr_rng_seed_int(&r,ctx->machine==0?boosted[mt]:normal[mt],0)?pr_rng_seed_int(&r,3,0):3;
 s->stats.eggs_pulled++;if(manaphy){s->stats.manaphy_eggs_pulled++;e->hatch_waves=50;}else if(tier==1)s->stats.rare_eggs_pulled++;else if(tier==2)s->stats.epic_eggs_pulled++;else if(tier==3)s->stats.legendary_eggs_pulled++;
 return true;
}
bool gc_system_gacha_pull(gc_system *sys,gc_gacha_ctx *ctx,gc_gacha_result *out,const char **why){
 if(why)*why="unsupported offline gacha context";
 if(!sys||!ctx||!out||ctx->machine<0||ctx->machine>2||ctx->cursor<0||ctx->cursor>4||ctx->timestamp_ms>UINT64_C(8640000000000000)||ctx->entropy_draws>UINT32_MAX-650||!rng_ok(&ctx->entropy)||!rng_ok(&ctx->scene_rng)||sys->n_eggs<0||sys->n_eggs>GC_MAX_EGGS)return false;
 static const int types[]={0,0,1,2,3},costs[]={1,10,1,1,1},pulls[]={1,10,5,10,25};int n=pulls[ctx->cursor],type=types[ctx->cursor];
 if(sys->n_eggs+n>99){if(why)*why="egg:tooManyEggs";return false;}if(sys->voucher_counts[type]<(unsigned)costs[ctx->cursor]){if(why)*why="egg:notEnoughVouchers";return false;}
 uint32_t counts[]={sys->stats.eggs_pulled,sys->stats.rare_eggs_pulled,sys->stats.epic_eggs_pulled,sys->stats.legendary_eggs_pulled,sys->stats.manaphy_eggs_pulled};for(int i=0;i<5;i++)if(counts[i]>UINT32_MAX-25)return false;for(int i=0;i<4;i++)if(sys->egg_pity[i]>UINT32_MAX-50||sys->unlock_pity[i]>10)return false;
 for(int i=0;i<sys->n_eggs;i++)if(sys->eggs[i].species>GC_MAX_SPECIES_ID)return false;
 static gc_system next;next=*sys;gc_gacha_ctx c=*ctx;gc_gacha_result result={.n=n};next.voucher_counts[type]-=costs[ctx->cursor];int begin=next.n_eggs;bool guaranteed=false;
 for(int i=0;i<n;i++){int minimum=n==25?2:n==10?1:0,forced=i==n-1&&!guaranteed&&minimum?minimum:-1;gc_egg egg;if(!egg_new(&next,&c,&egg,forced)){if(why)*why="empty pinned egg pool";return false;}guaranteed|=egg.tier>=minimum;next.eggs[next.n_eggs++]=egg;result.order[i]=i;}
 for(int i=n-1;i>0;i--){int j=pr_rng_seed_int(&c.scene_rng,i+1,0);uint8_t t=result.order[i];result.order[i]=result.order[j];result.order[j]=t;}
 if(next.n_eggs!=begin+n)return false;
 *sys=next;*ctx=c;*out=result;if(why)*why=NULL;return true;
}
