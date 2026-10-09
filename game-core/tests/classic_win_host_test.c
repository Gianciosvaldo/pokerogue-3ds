/* Additive atomic admission, terminal transport and exactly-once guards. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static gc_system sys,before;static unsigned char saved[GC_SAVE_MAX],mutated[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Classic host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(void){gc_system_init_defaults(&sys);gc_run_new(&run,GAME_MODES_CLASSIC,"classic-terminal",777);gc_starter st={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};memset(st.ivs,15,6);const char *why;CHECK(gc_run_set_starters(&run,&st,1,&why));run.sys=&sys;run.wave=200;}
static void finish(bool victory){gc_rphase phase=gc_rp(RP_GAME_OVER,victory,0,0);CHECK(!gc_phase_start(&run,&phase));}
static uint32_t hashname(const char *s){uint32_t v=2166136261u;for(;*s;s++)v=(v^(unsigned char)*s)*16777619u;return v;}
static uint32_t u32(const unsigned char *p){uint32_t n;memcpy(&n,p,4);return n;}
static void p32(unsigned char *p,uint32_t n){memcpy(p,&n,4);}
static uint32_t crc(const unsigned char *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int b=0;b<8;b++)c=c>>1^((c&1)?0xedb88320u:0);}return ~c;}
static size_t section_at(unsigned char *buf,size_t n,const char *name){for(size_t at=20;at+8<=n;){uint32_t len=u32(buf+at+4);CHECK(len<=n-at-8);if(u32(buf+at)==hashname(name))return at;at+=8+len;}CHECK(false);return 0;}
static void damaged(size_t n){p32(mutated+12,n-20);p32(mutated+16,crc(mutated+20,n-20));const char *why;CHECK(!gc_run_load(&loaded,mutated,n,&why));}
static void terminal(void){setup();run.party[1]=run.party[0];run.party[1].b.id++;run.n_party=2;run.party_order[1]=1;run.bt.party[0][1]=&run.party[1].b;run.bt.n_party[0]=2;finish(true);CHECK(sys.stats.classic_wins==1&&sys.starters[1].classic_win_count==2&&sys.stats.ribbons_owned==1);CHECK(sys.voucher_counts[3]==1&&sys.voucher_counts[1]==1&&sys.voucher_counts[2]==0&&sys.classic_voucher_unlocked);CHECK(sys.dex[1].ribbon_flags&GC_RIBBON_CLASSIC);before=sys;finish(true);CHECK(!memcmp(&sys,&before,sizeof sys));finish(false);CHECK(!memcmp(&sys,&before,sizeof sys)&&run.screen.kind==GC_SCREEN_CLEAR);CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,saved,sizeof saved);const char *why;CHECK(n&&gc_run_load(&loaded,saved,n,&why));CHECK(loaded.finished&&!loaded.in_battle&&loaded.terminal_result==2&&loaded.screen.kind==GC_SCREEN_CLEAR&&gc_run_can_save(&loaded));loaded.sys=&sys;gc_rphase p=gc_rp(RP_GAME_OVER,1,0,0);CHECK(!gc_phase_start(&loaded,&p)&&!memcmp(&sys,&before,sizeof sys));CHECK(gc_run_save(&loaded,mutated,sizeof mutated)>0);
 gc_run valid=run;for(int g=0;g<7;g++){run=valid;switch(g){case 0:run.wave=199;break;case 1:run.run.has_challenges=true;break;case 2:run.finished=false;break;case 3:run.in_battle=true;break;case 4:run.screen.kind=GC_SCREEN_GAME_OVER;break;case 5:run.terminal_result=3;break;case 6:run.terminal_result=1;run.wave=201;run.screen.kind=GC_SCREEN_GAME_OVER;break;}CHECK(!gc_run_can_save(&run)&&gc_run_save(&run,mutated,sizeof mutated)==0);}run=valid;
 size_t at=section_at(saved,n,"terminal");CHECK(u32(saved+at+4)==11);
 for(int guard=0;guard<8;guard++){memcpy(mutated,saved,n);size_t count=n;switch(guard){case 0:mutated[at+18]=0;break;case 1:mutated[at+18]=3;break;case 2:memcpy(mutated+n,saved+at,19);count+=19;break;case 3:mutated[n]=0;count++;p32(mutated+at+4,12);break;case 4:mutated[at+16]=2;break;case 5:mutated[at+10]^=1;break;case 6:mutated[count++]=0;break;case 7:mutated[at+8]=2;break;}damaged(count);}
 before=sys;setup();sys=before;finish(true);CHECK(sys.stats.classic_wins==2&&sys.starters[1].classic_win_count==3&&sys.voucher_counts[3]==1&&sys.voucher_counts[1]==1&&sys.voucher_counts[2]==1);before=sys;setup();sys=before;run.wave=20;finish(false);CHECK(!memcmp(&sys,&before,sizeof sys)&&run.terminal_result==1&&run.screen.kind==GC_SCREEN_GAME_OVER);CHECK(gc_run_can_save(&run));n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why)&&loaded.terminal_result==1&&loaded.screen.kind==GC_SCREEN_GAME_OVER);
 setup();run.wave=201;finish(false);CHECK(run.terminal_result==2&&sys.stats.classic_wins==1);
}
static void guards(void){for(int guard=0;guard<18;guard++){setup();switch(guard){case 0:run.run.mode=GAME_MODES_ENDLESS;break;case 1:run.run.has_challenges=true;break;case 2:run.wave=199;break;case 3:run.n_party=0;break;case 4:run.n_party=7;break;case 5:run.party_order[0]=1;break;case 6:run.party[0].b.id=0;break;case 7:run.party[0].b.species=9999;break;case 8:run.party[0].b.form=255;break;case 9:run.party[0].b.fusion_species=9999;break;case 10:sys.stats.classic_wins=UINT32_MAX;break;case 11:sys.starters[1].classic_win_count=UINT16_MAX;break;case 12:sys.stats.ribbons_owned=UINT32_MAX;break;case 13:sys.voucher_counts[1]=UINT32_MAX;break;case 14:sys.voucher_counts[3]=UINT32_MAX;break;case 15:sys.stats.classic_wins=1;sys.voucher_counts[2]=UINT32_MAX;break;case 16:run.terminal_result=3;break;case 17:run.n_party=2;run.party[1]=run.party[0];run.party_order[1]=1;break;}
 before=sys;gc_run snapshot=run;const char *why;CHECK(!gc_classic_finish(&run,true,&why));CHECK(!memcmp(&sys,&before,sizeof sys)&&!memcmp(&run,&snapshot,sizeof run));}
 setup();before=sys;run.sys=NULL;CHECK(!gc_classic_finish(&run,true,NULL)&&!memcmp(&sys,&before,sizeof sys)&&run.terminal_result==0);CHECK(!gc_classic_finish(NULL,true,NULL));
 setup();run.run.mode=GAME_MODES_ENDLESS;before=sys;finish(true);CHECK(!memcmp(&sys,&before,sizeof sys)&&run.terminal_result==0);
 setup();run.run.has_challenges=true;before=sys;finish(true);CHECK(!memcmp(&sys,&before,sizeof sys)&&run.terminal_result==0&&run.stubs[GC_STUB_REWARD_INPUT]==1&&!gc_run_can_save(&run));
}
static uint16_t u16(const unsigned char *p){uint16_t n;memcpy(&n,p,2);return n;}
/* Delete one named field from every record: the pre-addition wire contract. */
static size_t omit_field(size_t n,const char *section,const char *field){
 size_t at=section_at(saved,n,section),b=at+8;unsigned nf=u16(saved+b),idx=nf;size_t stride=0,off=0,sz=0;
 for(unsigned i=0;i<nf;i++){size_t len=u16(saved+b+2+6*i+4);if(u32(saved+b+2+6*i)==hashname(field)){idx=i;off=stride;sz=len;}stride+=len;}
 CHECK(idx<nf);unsigned count=u16(saved+b+2+6*nf);size_t start=b+4+6*nf,dest=b;
 uint16_t newnf=nf-1;memcpy(mutated,saved,n);memcpy(mutated+dest,&newnf,2);dest+=2;
 for(unsigned i=0;i<nf;i++)if(i!=idx){memcpy(mutated+dest,saved+b+2+6*i,6);dest+=6;}
 memcpy(mutated+dest,&count,2);dest+=2;
 for(unsigned i=0;i<count;i++){const unsigned char *src=saved+start+i*stride;memcpy(mutated+dest,src,off);dest+=off;memcpy(mutated+dest,src+off+sz,stride-off-sz);dest+=stride-off-sz;}
 size_t end=at+8+u32(saved+at+4),removed=6+count*sz;CHECK(dest==end-removed);memcpy(mutated+dest,saved+end,n-end);n-=removed;p32(mutated+at+4,u32(saved+at+4)-removed);p32(mutated+12,n-20);p32(mutated+16,crc(mutated+20,n-20));memcpy(saved,mutated,n);return n;
}
static void dense_profile(void){
 static gc_system roundtrip;gc_system_init_defaults(&sys);
 for(int i=1;i<=400;i++){sys.dex[i].seen_attr=1;sys.dex[i].caught_attr=1;sys.dex[i].ribbon_flags=GC_RIBBON_CLASSIC;sys.starters[i].classic_win_count=i;}
 sys.stats.ribbons_owned=400;sys.classic_voucher_unlocked=true;before=sys;
 size_t n=gc_system_save(&sys,saved,sizeof saved);const char *why;CHECK(n&&gc_system_load(&roundtrip,saved,n,&why));CHECK(!memcmp(&sys,&roundtrip,sizeof sys));
 n=omit_field(n,"dex","entry.ribbon_flags");n=omit_field(n,"stats","ribbons_owned");n=omit_field(n,"achievements","classic_voucher_unlocked");
 CHECK(gc_system_load(&roundtrip,saved,n,&why));for(int i=1;i<=400;i++)before.dex[i].ribbon_flags=0;before.stats.ribbons_owned=0;before.classic_voucher_unlocked=false;CHECK(!memcmp(&roundtrip,&before,sizeof before));
}
int main(int argc,char **argv){terminal();guards();dense_profile();for(int i=1;i<argc;i++){FILE *f=fopen(argv[i],"rb");CHECK(f!=NULL);size_t n=fread(saved,1,sizeof saved,f);CHECK(!ferror(f)&&feof(f));fclose(f);const char *why;CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(loaded.terminal_result==0&&!loaded.finished&&loaded.in_battle);}printf("Classic terminal host: %d checks passed\n",checks);return 0;}
