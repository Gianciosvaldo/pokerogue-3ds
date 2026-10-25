/* Actual native modifier DTO observations -> game inventory and save bridge. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
#include "pr_item_forms.h"
#include "gc/football.h"
#include <errno.h>
static FILE *input;
static char row[512], *cursor;
static unsigned char altered[GC_SAVE_MAX], scratch[GC_SAVE_MAX];
static uint32_t ids[4];
static gc_system profile;
static bool seen[200][2][4][2];
static void record(char tag){CHECK(fgets(row,sizeof row,input));CHECK(row[0]==tag&&row[1]==' '&&strchr(row,'\n'));cursor=row+2;}
static int64_t number(void){char *end;errno=0;long long n=strtoll(cursor,&end,10);CHECK(!errno&&end!=cursor);cursor=end;return n;}
static int bounded(int low,int high){int64_t n=number();CHECK(n>=low&&n<=high);return n;}
static void finish(void){CHECK(!strcmp(cursor,"\n"));}
static uint32_t word32(const unsigned char *p){uint32_t n;memcpy(&n,p,4);return n;}
static uint16_t word16(const unsigned char *p){uint16_t n;memcpy(&n,p,2);return n;}
static uint32_t name_hash(const char *s){uint32_t h=2166136261u;while(*s){h^=(unsigned char)*s++;h*=16777619u;}return h;}
static void checksum(size_t n){uint32_t len=n-20,crc=~0u;for(size_t i=20;i<n;i++){crc^=altered[i];for(int j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);}crc=~crc;memcpy(altered+12,&len,4);memcpy(altered+16,&crc,4);}
static size_t section(size_t n,const char *key){size_t p=20;while(p+8<=n){uint32_t len=word32(altered+p+4);CHECK(len<=n-p-8);if(word32(altered+p)==name_hash(key))return p;p+=8+len;}CHECK(false);return 0;}
static size_t remove_field(size_t n,const char *key,const char *field){
    size_t s=section(n,key),b=s+8;int nf=word16(altered+b),index=-1;size_t offset=0,stride=0;
    CHECK(nf>0&&nf<=128);
    for(int k=0;k<nf;k++){size_t d=b+2+6*k;int width=word16(altered+d+4);if(word32(altered+d)==name_hash(field)){index=k;offset=stride;CHECK(width==1);}stride+=width;}
    CHECK(index>=0);size_t data=b+2+6*nf+2;int count=word16(altered+data-2);size_t end=s+8+word32(altered+s+4);
    CHECK(data+count*stride==end&&end<=n);
    memcpy(scratch,altered,b);uint16_t reduced=nf-1;memcpy(scratch+b,&reduced,2);size_t w=b+2;
    for(int k=0;k<nf;k++)if(k!=index){memcpy(scratch+w,altered+b+2+6*k,6);w+=6;}
    memcpy(scratch+w,&count,2);w+=2;
    for(int k=0;k<count;k++){const unsigned char *src=altered+data+k*stride;memcpy(scratch+w,src,offset);w+=offset;memcpy(scratch+w,src+offset+1,stride-offset-1);w+=stride-offset-1;}
    uint32_t len=w-b;memcpy(scratch+s+4,&len,4);memcpy(scratch+w,altered+end,n-end);w+=n-end;
    CHECK(w==n-6-count);memcpy(altered,scratch,w);checksum(w);return w;
}
static void native_stock(char tag,const pr_modifier *m){
    record(tag);int item=bounded(0,199);int64_t owner=number();int active=bounded(0,1),stack=bounded(1,1),virt=bounded(0,0),locked=bounded(1,1);finish();
    CHECK(owner>0&&owner<=UINT32_MAX);CHECK(m->cls==PR_MOD_FORM_CHANGE_ITEM&&!m->unknown_subclass&&pr_mod_is_ported_item(m));
    CHECK(m->num==item&&m->pokemon_id==owner&&m->form_item_active==active&&m->stack_count==stack&&m->virtual_stack_count==virt&&!m->is_transferable==locked);
}
static void malformed(size_t n){
    const char *why=NULL;memcpy(altered,save,n);size_t b=section(n,"items")+8;int nf=word16(altered+b);size_t offset=0,d=0,other=0;
    for(int k=0;k<nf;k++){size_t p=b+2+6*k;if(word32(altered+p)==name_hash("form_inactive")){d=p;break;}offset+=word16(altered+p+4);}
    CHECK(d);for(int k=0;k<nf;k++){size_t p=b+2+6*k;if(word32(altered+p)==name_hash("eviolite_base"))other=p;}CHECK(other);
    size_t v=b+2+6*nf+2+offset;CHECK(word16(altered+b+2+6*nf)==1&&v<n);
    altered[v]=2;checksum(n);CHECK(!gc_run_load(&back,altered,n,&why));
    memcpy(altered,save,n);uint16_t width=2;memcpy(altered+d+4,&width,2);checksum(n);CHECK(!gc_run_load(&back,altered,n,&why));
    memcpy(altered,save,n);uint32_t duplicate=name_hash("form_inactive");memcpy(altered+other,&duplicate,4);checksum(n);CHECK(!gc_run_load(&back,altered,n,&why));
    r.items[0].key="LEFTOVERS";r.items[0].form_inactive=true;CHECK(!gc_run_save(&r,altered,sizeof altered));
    r.items[0].form_inactive=false;n=gc_run_save(&r,save,sizeof save);CHECK(n);memcpy(altered,save,n);b=section(n,"items")+8;nf=word16(altered+b);offset=0;d=0;
    for(int k=0;k<nf;k++){size_t p=b+2+6*k;if(word32(altered+p)==name_hash("form_inactive")){d=p;break;}offset+=word16(altered+p+4);}CHECK(d);
    altered[b+2+6*nf+2+offset]=1;checksum(n);CHECK(!gc_run_load(&back,altered,n,&why));
}
int main(int argc,char **argv){
    CHECK(argc==3);input=fopen(argv[1],"r");CHECK(input);CHECK(fgets(row,sizeof row,input)&&!strcmp(row,"FORMCTOR 1 2544\n"));
    record('U');for(int k=0;k<4;k++){int64_t id=number();CHECK(id>0&&id<=UINT32_MAX);ids[k]=id;for(int j=0;j<k;j++)CHECK(ids[k]!=ids[j]);}finish();
    setup(1);for(int k=0;k<4;k++){CHECK(r.bt.field[k]);r.bt.field[k]->id=ids[k];}uint32_t bench=1;bool collision;
    do{collision=false;for(int k=0;k<4;k++)if(ids[k]==bench){collision=true;bench++;break;}}while(collision);
    r.party[2].b.id=bench;
    for(int i=0;i<2544;i++){
        record('C');CHECK(bounded(0,2543)==i);int rare=bounded(0,1),item=bounded(0,199),owner=bounded(0,3),active=bounded(0,1);CHECK(bounded(1,1)==1&&bounded(1,1)==1);int64_t arg_owner=number();CHECK(arg_owner==ids[owner]&&bounded(0,199)==item&&bounded(0,1)==active);finish();
        CHECK(pr_form_item_known(item)&&!seen[item][rare][owner][active]);seen[item][rare][owner][active]=true;
        const char *key=rare?"RARE_FORM_CHANGE_ITEM":"FORM_CHANGE_ITEM";int side=owner/2;
        r.n_items=r.n_enemy_items=0;gc_item stock={.key=key,.arg=item,.stack=1,.pokemon_id=ids[owner],.form_inactive=!active};
        if(side){r.enemy_items[0]=stock;r.n_enemy_items=1;}else{r.items[0]=stock;r.n_items=1;}
        pr_rng global=r.bt.rng,battle=r.bt.battle_rng,run_rng=r.run.rng;gc_items_to_battle(&r);native_stock('L',&r.bt.mods[side][0]);
        if(!side){pr_reward_ctx *ctx=gc_reward_ctx(&r);CHECK(ctx->n_items==1&&ctx->items[0].active==active);}
        gc_items_from_battle(&r);CHECK((side?r.enemy_items:r.items)[0].form_inactive==!active);
        const char *why=NULL;size_t n=gc_run_save(&r,save,sizeof save);CHECK(n&&gc_run_load(&back,save,n,&why)&&!back.load_dropped);
        gc_item *loaded=side?back.enemy_items:back.items;CHECK(loaded[0].form_inactive==!active&&loaded[0].non_transferable&&!strcmp(loaded[0].key,key));
        gc_items_to_battle(&back);native_stock('V',&back.bt.mods[side][0]);
        CHECK(rngsame(global,r.bt.rng)&&rngsame(battle,r.bt.battle_rng)&&rngsame(run_rng,r.run.rng));
        CHECK(rngsame(global,back.bt.rng)&&rngsame(battle,back.bt.battle_rng)&&rngsame(run_rng,back.run.rng));
        for(int flags=1;flags<=3;flags++){
            memcpy(altered,save,n);size_t length=n;
            for(int s=0;s<2;s++){const char *section_key=s?"enemy_items":"items";if(flags&1)length=remove_field(length,section_key,"form_inactive");if(flags&2)length=remove_field(length,section_key,"eviolite_base");}
            CHECK(gc_run_load(&back,altered,length,&why)&&!back.load_dropped);loaded=side?back.enemy_items:back.items;
            CHECK(loaded[0].form_inactive==((flags&1)?false:!active));gc_items_to_battle(&back);CHECK(back.bt.mods[side][0].form_item_active==((flags&1)?true:active));
        }
    }
    CHECK(fgets(row,sizeof row,input)&&!strcmp(row,"END 2544\n")&&fgetc(input)==EOF);fclose(input);
    for(int item=0;item<200;item++)for(int rare=0;rare<2;rare++)for(int owner=0;owner<4;owner++)for(int active=0;active<2;active++)CHECK(seen[item][rare][owner][active]==pr_form_item_known(item));
    r.sys=&profile;for(int mask=0;mask<64;mask++){for(int k=0;k<6;k++)profile.dex[pr_form_item_caught_species[k]].caught_attr=!!(mask&(1<<k));gc_items_to_battle(&r);CHECK(r.bt.form_item_dex_known&&r.bt.form_item_dex_mask==mask);}
    r.sys=NULL;gc_items_to_battle(&r);CHECK(!r.bt.form_item_dex_known&&!r.bt.form_item_dex_mask);
    r.n_items=1;r.n_enemy_items=0;
    gc_item canonical={.key="FORM_CHANGE_ITEM",.arg=FORM_CHANGE_ITEM_GRISEOUS_CORE,.stack=1,.pokemon_id=r.party[0].b.id};
    for(int invalid=0;invalid<10;invalid++){
        r.items[0]=canonical;
        if(invalid==0)r.items[0].stack=0;
        if(invalid==1)r.items[0].stack=2;
        if(invalid==2)r.items[0].virtual_stack=1;
        if(invalid==3)r.items[0].virtual_stack=-1;
        if(invalid==4)r.items[0].detached=true;
        if(invalid==5)r.items[0].pokemon_id=0;
        if(invalid==6)r.items[0].pokemon_id=0xffffffffu;
        if(invalid==7)r.items[0].arg=200;
        if(invalid==8)r.items[0].eviolite_base=true;
        if(invalid==9){r.items[0].key="LEFTOVERS";r.items[0].form_inactive=true;}
        gc_items_to_battle(&r);CHECK(r.bt.mods[0][0].unknown_subclass&&!pr_mod_is_ported_item(&r.bt.mods[0][0]));
    }
    for(int normal=0;normal<2;normal++){
        r.n_items=0;r.items[0]=canonical;r.items[0].form_inactive=true;
        gc_item_add(&r,normal?"LEFTOVERS":"FORM_CHANGE_ITEM",normal?-1:canonical.arg,canonical.pokemon_id,1);
        CHECK(r.n_items==1&&!r.items[0].form_inactive);gc_items_to_battle(&r);
        CHECK(!r.bt.mods[0][0].unknown_subclass&&(normal||r.bt.mods[0][0].form_item_active));
    }
    r.n_items=1;
    r.items[0]=canonical;int party_count=r.n_party,enemy_count=r.n_enemy;
    for(int invalid=0;invalid<4;invalid++){
        r.n_party=invalid<2?(invalid?7:-1):party_count;r.n_enemy=invalid>=2?(invalid==3?7:-1):enemy_count;
        r.bt.n_unported=0;gc_items_to_battle(&r);CHECK(!r.bt.n_mods[0]&&!r.bt.n_mods[1]&&r.bt.n_unported);
    }
    r.n_party=party_count;r.n_enemy=enemy_count;r.bt.n_unported=0;
    r.items[0]=canonical;r.items[0].form_inactive=true;
    const char *why=NULL;size_t n=gc_run_save(&r,save,sizeof save);CHECK(n);malformed(n);
    FILE *legacy=fopen(argv[2],"rb");CHECK(legacy);n=fread(save,1,sizeof save,legacy);CHECK(n>20&&feof(legacy));fclose(legacy);
    CHECK(gc_run_load(&back,save,n,&why)&&!back.load_dropped&&back.n_items==3&&back.n_enemy_items==1);
    CHECK(gc_item_key_form_change(back.items[0].key)&&gc_item_key_form_change(back.items[1].key)&&back.items[2].eviolite_base);
    CHECK(!back.items[0].form_inactive&&!back.items[1].form_inactive&&!back.enemy_items[0].form_inactive);
    gc_items_to_battle(&back);CHECK(back.bt.mods[0][0].form_item_active&&back.bt.mods[0][1].form_item_active&&back.bt.mods[1][0].form_item_active);
    gc_football_new(&r,NULL,"form-roster",1);gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};
    CHECK(gc_football_draft(&r,&starter,1,&why));r.party[0].b.species=SPECIES_ID_VENUSAUR;
    int mega=gc_football_mega_item(&r,0);CHECK(mega>=0&&gc_football_give_mega(&r,0,mega));
    r.items[0].form_inactive=true;n=gc_football_roster_save(&r,save,sizeof save);CHECK(n);
    CHECK(gc_football_roster_load(&back,NULL,save,n)&&back.items[0].form_inactive&&!back.bt.mods[0][0].form_item_active);
    memcpy(altered,save,n);n=remove_field(n,"items","form_inactive");
    CHECK(gc_football_roster_load(&back,NULL,altered,n)&&!back.items[0].form_inactive&&back.bt.mods[0][0].form_item_active);
    printf("Held form inventory: 2544 native constructors/DTOs, legacy schemas and actual prior-writer save; %d checks\n",checks);return 0;
}
