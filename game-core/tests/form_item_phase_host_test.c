/* Real party toggle -> shared form owner, against native state/profile/text. */
#include "../src/internal.h"
#include "pr_item_forms.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
static gc_run r;
static gc_system profile,expected_profile;
static gc_mon base;
static gc_rqueue kept_queue;
static FILE *input;
static char text[8192],*cursor;
static unsigned checks,line;
static int cid=-1;
static uint32_t ids[3];
static bool profile_seen[2][GC_MAX_SPECIES_ID+1];
typedef struct {int species,from,to,item,active,companion;} rule;
static rule rules[416];
static bool seen0[416][2],seen1[416][2][3][25],seen2[416][2][2][3][2];
static int groups[4];
static bool seen3[416][2][2];
typedef struct {int species,form,level,nature,gender,index,base_ability,ability,hp,stats[6],ivs[6],tera,tera_type,auto_tag,on_field,fusion_species,fusion_form,fusion_index;} state;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"form phase case%d line%u C%d: %s\n",cid,line,__LINE__,#x);exit(1);}}while(0)
static void record(char tag){CHECK(fgets(text,sizeof text,input));line++;CHECK(text[0]==tag&&text[1]==' '&&strchr(text,'\n'));cursor=text+2;}
static int64_t number(void){char *end;errno=0;long long n=strtoll(cursor,&end,10);CHECK(!errno&&end!=cursor);cursor=end;return n;}
static uint64_t wide(void){char *end;while(*cursor==' ')cursor++;CHECK(*cursor!='-');errno=0;unsigned long long n=strtoull(cursor,&end,10);CHECK(!errno&&end!=cursor);cursor=end;return n;}
static uint32_t word(void){uint64_t n=wide();CHECK(n<=UINT32_MAX);return n;}
static int bounded(int lo,int hi){int64_t n=number();CHECK(n>=lo&&n<=hi);return n;}
static void finish(void){CHECK(!strcmp(cursor,"\n"));}
static state read_state(char tag){
    record(tag);state s={0};s.species=bounded(1,GC_MAX_SPECIES_ID);s.form=bounded(0,255);s.level=bounded(1,65536);s.nature=bounded(0,24);s.gender=bounded(-1,1);
    s.index=bounded(0,2);s.base_ability=bounded(0,PR_ABILITY_COUNT-1);s.ability=bounded(0,PR_ABILITY_COUNT-1);s.hp=bounded(0,INT32_MAX);
    for(int k=0;k<6;k++)s.stats[k]=bounded(1,INT32_MAX);
    for(int k=0;k<6;k++)s.ivs[k]=bounded(0,31);
    s.tera=bounded(0,1);s.tera_type=bounded(-1,19);s.auto_tag=bounded(0,1);s.on_field=bounded(0,1);s.fusion_species=bounded(0,GC_MAX_SPECIES_ID);s.fusion_form=bounded(0,255);s.fusion_index=bounded(-1,2);finish();return s;
}
static void apply_state(gc_mon *m,const state *s){
    *m=base;pr_pokemon *p=&m->b;p->fusion_species=s->fusion_species;p->fusion_form=s->fusion_form;p->fusion_ability_index=s->fusion_index;p->species=s->species;p->form=s->form;p->level=s->level;p->nature=s->nature;p->gender=s->gender;
    m->base_nature=s->nature;m->has_custom_nature=false;m->ability_index=s->index;p->ability_index=s->index;p->base_ability=s->base_ability;p->ability=s->ability;p->hp=s->hp;
    p->tera_type=s->tera_type;p->terastallized=s->tera;p->on_field=s->on_field;p->is_player=true;p->passive_flag=false;p->passive=0;
    for(int k=0;k<6;k++){p->stats[k]=s->stats[k];p->ivs[k]=s->ivs[k];}
}
static void compare_state(const gc_mon *m,const state *s){
    const pr_pokemon *p=&m->b;
    CHECK(p->fusion_species==s->fusion_species&&p->fusion_form==s->fusion_form&&p->fusion_ability_index==s->fusion_index);
    if(p->form!=s->form||p->ability_index!=s->index||p->base_ability!=s->base_ability||p->ability!=s->ability)
        fprintf(stderr,"expected form/index/base/ability %d/%d/%d/%d got %d/%d/%d/%d\n",s->form,s->index,s->base_ability,s->ability,p->form,p->ability_index,p->base_ability,p->ability);
    CHECK(p->species==s->species&&p->form==s->form&&p->level==s->level&&p->nature==s->nature&&p->gender==s->gender);
    CHECK(m->ability_index==s->index&&p->ability_index==s->index&&p->base_ability==s->base_ability&&p->ability==s->ability&&p->hp==s->hp);
    for(int k=0;k<6;k++)CHECK(p->stats[k]==s->stats[k]&&p->ivs[k]==s->ivs[k]);
    CHECK(p->terastallized==s->tera&&p->tera_type==s->tera_type&&p->on_field==s->on_field);
    CHECK(!!pr_get_battler_tag(p,BATTLER_TAG_TYPE_AUTOTOMIZED)==s->auto_tag);
}
static int read_dex(char tag,gc_system *sys){
    record(tag);int id=bounded(1,GC_MAX_SPECIES_ID);gc_dex_entry *d=&sys->dex[id];d->seen_attr=wide();d->caught_attr=wide();
    d->nature_attr=word();d->seen_count=word();d->caught_count=word();d->hatched_count=word();
    for(int k=0;k<6;k++)d->ivs[k]=bounded(0,31);
    d->ribbon_flags=wide();finish();return id;
}
static int read_starter(char tag,gc_system *sys){
    record(tag);int id=bounded(1,GC_MAX_SPECIES_ID);gc_starter_entry *s=&sys->starters[id];s->ability_attr=word();s->passive_attr=word();s->value_reduction=word();s->candy_count=bounded(0,INT32_MAX);
    s->friendship=bounded(0,UINT16_MAX);s->egg_moves=bounded(0,255);s->classic_win_count=bounded(0,UINT16_MAX);
    for(int k=0;k<4;k++)s->moveset[k]=bounded(0,PR_MOVE_COUNT-1);
    finish();return id;
}
static const char *locale(void *unused,const char *space,int lang,size_t *length){
    static char data[16][400000],names[16][96];static size_t sizes[16];static int count;(void)unused;(void)lang;
    for(int k=0;k<count;k++)if(!strcmp(names[k],space)){*length=sizes[k];return data[k];}
    CHECK(count<16&&strlen(space)<sizeof names[0]);char path[1024];snprintf(path,sizeof path,"%s/%s.json",getenv("TURNTEST_LOCALES"),space);
    fprintf(stderr,"native locale: %s\n",space);FILE *f=fopen(path,"rb");CHECK(f);sizes[count]=fread(data[count],1,sizeof data[0]-1,f);CHECK(fgetc(f)==EOF);fclose(f);strcpy(names[count],space);*length=sizes[count];return data[count++];
}
static void setup(void){
    gc_run_new(&r,GAME_MODES_CLASSIC,"form-native",777);r.sys=&profile;r.n_party=3;r.n_enemy=2;r.double_battle=r.bt.double_battle=true;r.bt.challenge_flags_known=true;
    r.wave=r.bt.wave_index=1;r.bt.turn=1;r.bt.max_exp_level=10;
    pr_seedstr seed;pr_seed_from_ascii(&seed,"native-form-wave");r.bt.wave_seed=seed;
    pr_seed_from_ascii(&seed,"native-form-global");pr_rng_sow(&r.bt.rng,&seed);
    pr_seed_from_ascii(&seed,"native-form-battle");pr_rng_sow(&r.bt.battle_rng,&seed);r.bt.rng_live=r.bt.battle_rng_live=true;
    gc_starter starter={.species=SPECIES_ID_MEW,.tera_type=-1};memset(starter.ivs,31,6);gc_mon_new_player(&r,&base,&starter,5);
    for(int k=0;k<3;k++){r.party[k]=base;r.party[k].b.id=ids[k];r.party_order[k]=k;r.bt.party[0][k]=&r.party[k].b;r.party[k].b.on_field=k<2;}
    uint32_t enemy_id=1;
    for(int k=0;k<2;k++){bool collision;do{collision=false;for(int j=0;j<3;j++)if(ids[j]==enemy_id){enemy_id++;collision=true;break;}}while(collision);
        r.enemy[k]=base;r.enemy[k].b.id=enemy_id++;r.enemy[k].b.is_player=false;r.enemy[k].b.ability=ABILITY_ID_LEVITATE;r.enemy[k].b.on_field=true;r.enemy[k].b.hp=1;r.bt.party[1][k]=&r.enemy[k].b;}
    r.bt.n_party[0]=3;r.bt.n_party[1]=2;r.bt.field[0]=&r.party[0].b;r.bt.field[1]=&r.party[1].b;r.bt.field[2]=&r.enemy[0].b;r.bt.field[3]=&r.enemy[1].b;
    r.screen.kind=GC_SCREEN_REWARD;gc_rq_clear(&r);gc_int(&r)->rq.current=gc_rp(RP_SELECT_MODIFIER,0,0,0);gc_int(&r)->rq.waiting=true;kept_queue=gc_int(&r)->rq;
}
int main(int argc,char **argv){
    CHECK(argc==2&&getenv("TURNTEST_LOCALES"));input=fopen(argv[1],"r");CHECK(input);CHECK(fgets(text,sizeof text,input)&&!strcmp(text,"FORMPHASE 1 416 74880\n"));line++;
    for(int i=0;i<416;i++){
        record('R');CHECK(bounded(0,415)==i);rule *r=&rules[i];r->species=bounded(1,GC_MAX_SPECIES_ID);r->from=bounded(0,255);r->to=bounded(0,255);r->item=bounded(0,199);r->active=bounded(0,1);r->companion=bounded(-1,199);finish();
        const pr_item_form_rule *p=&pr_item_form_rules[i];CHECK(p->species==r->species&&p->from==r->from&&p->to==r->to&&p->item==r->item&&(p->flags&1)==r->active&&pr_form_item_known(r->item));
    }
    for(int dataset=0;dataset<3;dataset++){
        record('G');int n=bounded(1,74880);CHECK(n==(dataset==2?1664:dataset?9984:63232));
        for(int k=0;k<3;k++){uint64_t id=wide();CHECK(id&&id<=UINT32_MAX);ids[k]=id;for(int j=0;j<k;j++)CHECK(ids[k]!=ids[j]);}finish();setup();
        for(int i=0;i<n;i++){
            record('C');int expected_id=bounded(0,74879);CHECK(expected_id==++cid);int group=bounded(0,3),which=bounded(0,415),owner=bounded(0,2),selected=bounded(-1,415),active=bounded(0,1);finish();CHECK(owner!=1&&group==(dataset==2?3:dataset?2:(i<832?0:1)));groups[group]++;
            state before=read_state('B'),after=read_state('A');rule *fact=&rules[which];CHECK(before.species==fact->species&&before.form==fact->from&&active==fact->active&&before.on_field==(owner==0));
            int pos=owner/2,lev=before.level==5?0:before.level==1000?1:before.level==65536?2:-1;CHECK(lev>=0);
            if(group==0){CHECK(!seen0[which][pos]&&before.level==5&&before.index==0&&!before.tera&&selected>=0);seen0[which][pos]=true;}
            if(group==1){CHECK(!seen1[which][pos][lev][before.nature]&&before.index==0&&!before.tera);seen1[which][pos][lev][before.nature]=true;}
            if(group==2){CHECK(lev!=1);int l=lev==2;CHECK(!seen2[which][pos][l][before.index][before.tera]);seen2[which][pos][l][before.index][before.tera]=true;}
            if(group==3){CHECK(before.level==5&&(before.fusion_species==151||before.fusion_species==479));int donor=before.fusion_species==479;CHECK(!seen3[which][pos][donor]);seen3[which][pos][donor]=true;}
            memset(&profile,0,sizeof profile);memset(&expected_profile,0,sizeof expected_profile);memset(profile_seen,0,sizeof profile_seen);
            for(int k=0;k<6;k++)profile.dex[pr_form_item_caught_species[k]].caught_attr=expected_profile.dex[pr_form_item_caught_species[k]].caught_attr=1;
            record('D');int nd=bounded(0,32);finish();for(int k=0;k<nd;k++){int id=read_dex('L',&profile);CHECK(!profile_seen[0][id]);profile_seen[0][id]=true;CHECK(read_dex('V',&expected_profile)==id);}
            record('S');int ns=bounded(0,32);finish();for(int k=0;k<ns;k++){int id=read_starter('L',&profile);CHECK(!profile_seen[1][id]);profile_seen[1][id]=true;CHECK(read_starter('V',&expected_profile)==id);}
            record('T');char expected_text[4096];size_t len=strcspn(cursor,"\n");CHECK(cursor[len]=='\n'&&len<sizeof expected_text);memcpy(expected_text,cursor,len);expected_text[len]=0;CHECK((selected<0)==!strcmp(expected_text,"-"));
            if(selected>=0){CHECK(!(len%2));for(size_t k=0;k<len;k++)CHECK((expected_text[k]>='0'&&expected_text[k]<='9')||(expected_text[k]>='a'&&expected_text[k]<='f'));}
            for(int k=0;k<3;k++){r.party[k]=base;r.party[k].b.id=ids[k];r.party[k].b.ability=ABILITY_ID_LEVITATE;r.party[k].b.hp=1;r.party[k].b.on_field=k<2;}
            apply_state(&r.party[owner],&before);pr_pokemon *pokemon=&r.party[owner].b;pokemon->id=ids[owner];r.n_items=1;r.n_enemy_items=0;
            r.items[0]=(gc_item){.key="FORM_CHANGE_ITEM",.arg=fact->item,.stack=1,.pokemon_id=ids[owner],.form_inactive=!!fact->active,.non_transferable=true};
            if(fact->companion>=0)r.items[r.n_items++]=(gc_item){.key="FORM_CHANGE_ITEM",.arg=fact->companion,.stack=1,.pokemon_id=ids[owner],.non_transferable=true};
            memset(&r.eng,0,sizeof r.eng);r.eng.current=-1;r.eng.n_levels=1;r.bt.engine=&r.eng;r.bt.n_unported=r.bt.n_deferred=0;
            if(before.auto_tag)CHECK(pr_add_tag(&r.bt,&r.eng,pokemon,BATTLER_TAG_TYPE_AUTOTOMIZED,1,MOVE_ID_AUTOTOMIZE,-1));
            memset(&r.eng,0,sizeof r.eng);r.eng.current=-1;r.eng.n_levels=1;gc_items_to_battle(&r);r.ev_count=0;r.ev_head=0;r.ev_overflow=false;
            if(r.bt.n_unported)fprintf(stderr,"unsupported name %s\n",pr_unported_name(r.bt.unported[0]));
            if(r.bt.n_unported||r.bt.n_deferred)fprintf(stderr,"prep unsupported count%d first%d deferred%d first%d\n",r.bt.n_unported,r.bt.n_unported?r.bt.unported[0]:-1,r.bt.n_deferred,r.bt.n_deferred?r.bt.deferred[0]:-1);
            CHECK(!r.bt.n_unported&&!r.bt.n_deferred);compare_state(&r.party[owner],&before);
            pr_rng global=r.bt.rng,battle=r.bt.battle_rng,run_rng=r.run.rng;
            const char *why=NULL;CHECK(gc_form_item_toggle(&r,owner,0,&why)&&!why);compare_state(&r.party[owner],&after);
            CHECK(r.items[0].form_inactive==!active&&!memcmp(&kept_queue,&gc_int(&r)->rq,sizeof kept_queue)&&r.screen.kind==GC_SCREEN_REWARD);
            if(memcmp(&profile,&expected_profile,sizeof profile)){for(int k=1;k<=GC_MAX_SPECIES_ID;k++)if(memcmp(&profile.dex[k],&expected_profile.dex[k],sizeof profile.dex[k])||memcmp(&profile.starters[k],&expected_profile.starters[k],sizeof profile.starters[k])){fprintf(stderr,"profile mismatch species%d\n",k);break;}CHECK(false);}
            int messages=0,events=0;gc_event ev;while(gc_run_next_event(&r,&ev)){
                if(ev.kind==GC_EV_MESSAGE){messages++;char message[2048],encoded[4096];pr_text_source src={locale,NULL,"en"};size_t length=pr_msg_format(&ev.msg,&src,message,sizeof message);CHECK(length<sizeof message);for(size_t k=0;k<length;k++)sprintf(encoded+2*k,"%02x",(unsigned char)message[k]);if(!length)strcpy(encoded,"-");CHECK(!strcmp(encoded,expected_text));}
                else {CHECK(ev.kind==GC_EV_EVOLVED&&ev.a==owner&&ev.b==before.species&&ev.c==before.species);events++;}
            }
            CHECK(messages==(selected>=0)&&events==(selected>=0)&&!r.eng.overflow&&!r.ev_overflow&&!r.bt.n_unported&&!r.bt.n_deferred);
            CHECK(!memcmp(&global,&r.bt.rng,sizeof global)&&!memcmp(&battle,&r.bt.battle_rng,sizeof battle)&&!memcmp(&run_rng,&r.run.rng,sizeof run_rng));
        }
    }
    CHECK(groups[0]==832&&groups[1]==62400&&groups[2]==9984&&groups[3]==1664);
    CHECK(fgets(text,sizeof text,input)&&!strcmp(text,"END 416 74880\n")&&fgetc(input)==EOF);fclose(input);
    printf("Form party action: 74880 native state/profile/message/modal-return snapshots; %u checks\n",checks);return 0;
}
