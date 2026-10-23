/* Actual scene transfer -> game inventory -> save, compared with native classes. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
#include <ctype.h>
#include <errno.h>

static FILE *capture;
static char row[512];
static unsigned char altered[GC_SAVE_MAX];
static uint32_t word32(const unsigned char *p){uint32_t v;memcpy(&v,p,4);return v;}
static uint16_t word16(const unsigned char *p){uint16_t v;memcpy(&v,p,2);return v;}
static uint32_t field_hash(const char *s){uint32_t h=2166136261u;while(*s){h^=(unsigned char)*s++;h*=16777619u;}return h;}
static void checksum(size_t length){
    uint32_t crc=~0u;for(size_t i=20;i<length;i++){crc^=altered[i];for(int bit=0;bit<8;bit++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);}crc=~crc;memcpy(altered+16,&crc,4);
}
static void marker_schema(size_t length,bool legacy){
    memcpy(altered,save,length);size_t section=20;
    while(section+8<=length&&word32(altered+section)!=field_hash("items"))section+=8+word32(altered+section+4);
    CHECK(section+10<=length);size_t block=section+8;int nf=word16(altered+block);
    CHECK(nf>0&&nf<=128&&block+2+6*nf+2<=length);
    size_t offset=0,descriptor=0;
    for(int k=0;k<nf;k++){size_t d=block+2+6*k;if(word32(altered+d)==field_hash("eviolite_base")){descriptor=d;break;}offset+=word16(altered+d+4);}
    CHECK(descriptor&&word16(altered+descriptor+4)==1&&word16(altered+block+2+6*nf)==1);
    const char *why=NULL;
    if(legacy){
        uint32_t unused=field_hash("old_unknown_item_field");memcpy(altered+descriptor,&unused,4);checksum(length);
        CHECK(gc_run_load(&back,altered,length,&why)&&!back.items[0].eviolite_base&&!back.load_dropped);
        gc_items_to_battle(&back);CHECK(back.bt.mods[0][0].cls==PR_MOD_EVOLUTION_STAT_BOOSTER);
    }else{
        size_t value=block+2+6*nf+2+offset;CHECK(value<length);
        altered[value]=2;checksum(length);CHECK(!gc_run_load(&back,altered,length,&why));
        altered[value]=1;checksum(length);CHECK(!gc_run_load(&back,altered,length,&why));
        altered[value]=0;uint16_t wrong_size=2;memcpy(altered+descriptor+4,&wrong_size,2);checksum(length);
        CHECK(!gc_run_load(&back,altered,length,&why));
    }
}
static void invalid(void){fprintf(stderr,"invalid stat-loot save capture\n");exit(2);}
static char *read_row(const char *prefix){
    if(!fgets(row,sizeof row,capture)||strncmp(row,prefix,strlen(prefix)))invalid();
    return row+strlen(prefix);
}
static void stock(const char *prefix,const pr_modifier *m){
    char cls[80];int mask,ns,n=0;double mult;
    char *p=read_row(prefix);
    if(sscanf(p,"%79s%d%lf%d%n",cls,&mask,&mult,&ns,&n)!=4||mask<0||mask>127||!isfinite(mult)||ns<0||ns>4)invalid();
    CHECK(!strcmp(pr_mod_class_name[m->cls],cls)&&m->stats_mask==mask&&m->mult==mult&&m->n_species==ns);
    CHECK(m->stack_count==1&&!m->virtual_stack_count&&!m->unknown_subclass);
    p+=n;
    for(int k=0;k<ns;k++){
        char *end;errno=0;long id=strtol(p,&end,10);
        if(errno||p==end||id<1||id>65535)invalid();p=end;CHECK(m->species[k]==id);
    }
    while(isspace((unsigned char)*p))p++;if(*p)invalid();
}
int main(int argc,char **argv){
    if(argc!=2||(capture=fopen(argv[1],"r"))==NULL)return 2;
    if(strcmp(read_row("STATLOOT_SAVE "),"1 16\n"))invalid();
    const char *keys[]={"EVIOLITE","LIGHT_BALL","THICK_CLUB","METAL_POWDER","QUICK_POWDER","DEEP_SEA_SCALE","DEEP_SEA_TOOTH","GRIP_CLAW"};
    for(int i=0;i<16;i++){
        int id,clone;char key[80],extra;
        if(sscanf(read_row("C "),"%d%79s%d %c",&id,key,&clone,&extra)!=3||id!=i||clone!=i%2||strcmp(key,keys[i/2]))invalid();
        setup(1);r.n_items=r.n_enemy_items=0;
        for(int k=0;k<2;k++){r.enemy[k].b.custom_ability=ABILITY_ID_NONE;pr_refresh_ability(&r.enemy[k].b);}
        gc_item item={.key=i/2==0?"EVIOLITE":i/2==7?"GRIP_CLAW":i/2<5?"RARE_SPECIES_STAT_BOOSTER":"SPECIES_STAT_BOOSTER",.arg=i/2==0||i/2==7?-1:i/2-1,.stack=1,.pokemon_id=clone?r.enemy[0].b.id:r.party[0].b.id};
        if(clone){r.enemy_items[0]=item;r.n_enemy_items=1;}else{r.items[0]=item;r.n_items=1;}
        gc_items_to_battle(&r);
        if(clone){CHECK(pr_try_transfer_held_item(&r.bt,&r.eng,1,0,&r.party[0].b,1,false,false));gc_items_from_battle(&r);}
        CHECK(r.bt.n_mods[0]==1&&r.n_items==1);
        stock("I ",&r.bt.mods[0][0]);
        CHECK(r.items[0].eviolite_base==(clone&&i/2==0));
        const char *why=NULL;size_t length=gc_run_save(&r,save,sizeof save);
        CHECK(length&&gc_run_load(&back,save,length,&why));gc_items_to_battle(&back);
        CHECK(back.bt.n_mods[0]==1&&back.n_items==1);stock("O ",&back.bt.mods[0][0]);
        CHECK(back.items[0].eviolite_base==r.items[0].eviolite_base);
        if(i==0)marker_schema(length,true);
        if(i==2)marker_schema(length,false);
        if(strcmp(read_row("E"),"\n"))invalid();
    }
    if(strcmp(read_row("STATLOOT_SAVE_COMPLETE "),"16\n")||fgetc(capture)!=EOF)invalid();
    printf("Stat-loot native save: 16 factory/clone cases, %d checks passed\n",checks);return 0;
}
