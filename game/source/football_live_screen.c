#include "football_screen.h"
#include "football_career.h"
#include "football_live_art.h"
#include "football_menu.h"
#include "football_shop_render.h"
#include "gm_item_ui.h"
#include "mui.h"
#include "gc/data.h"
#include "save.h"
#include "ui.h"
#include "ui_widgets.h"
#include <3ds.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
/* The classic party and its queues remain six-wide. One scratch run bridges
 * one football member at a time to existing EXP/evolution/item logic. */
static struct {
    FootballCareer career;
    football_screen_args args;
    FootballRenderer renderer;
    FootballCamera camera;
    FootballShopView shop;
    const rr_frame *evo_icons[FB_SHOP_EVO_MAX]; /* the item icon of each evolution offer, NULL when the pack lacks it */
    FootballPlayer evo_player;unsigned evo_key;bool evo_after; /* the evolved player the highlighted evolution offer would make */
    gc_run *scratch;
    uint16_t catalog[PR_SPECIES_COUNT];unsigned catalog_count,cursor,member,play_page,preview,preview_after,ms;
    int asset_offense;bool editing,save_bad,save_failed,leaving,ignore_keys;
    float old_speed,remainder;
    unsigned pending;int receiver;
    uint64_t checkpoint;
    FootballCareerPhase old_phase;
} L;
static unsigned char checkpoint_bytes[FB_CAREER_SAVE_MAX];
static ui_screen live_screen;
/* Where the field map and the receiver buttons sit on the bottom screen during a live play (the touch handler reads the same numbers). */
enum { MAP_X=10,MAP_Y=76,TARGET_X=4,TARGET_Y=68,TARGET_W=100,TARGET_H=62,TARGET_PITCH_X=106,TARGET_PITCH_Y=68 };
bool football_legacy_screen_open(const football_screen_args *args);
static const char *name(unsigned species)
{
    for(unsigned i=0;i<PR_SPECIES_COUNT;++i)if(pr_species_table[i].id==species)return pr_species_table[i].name;
    return "Pokemon";
}
const char *football_live_art_species(unsigned species){return name(species);}
unsigned football_live_art_candy_milli(const FootballCareer *career,unsigned member,unsigned points){return football_candy_milli(career,member,points);}
/* The classic game's evolution items, common and rare (ids above 50), as the reward screens name and draw them. */
static const gm_item_ui *evo_item(unsigned item){return gm_item_ui_find(item>50?"RARE_EVOLUTION_ITEM":"EVOLUTION_ITEM",(int)item);}
static const char *evo_item_name(unsigned item){const gm_item_ui *u=evo_item(item);return u && u->name?u->name:"Evolution item";}
/* The evolution offer under the cursor in the shop, or -1. */
static int evo_cursor(void)
{
    const FootballCareer *c=&L.career;
    return c->phase==FB_CAREER_SHOP && L.cursor>=FB_SHOP_FIXED && L.cursor<FB_SHOP_FIXED+c->evo_count?(int)(L.cursor-FB_SHOP_FIXED):-1;
}
/* What the highlighted evolution offer would make of its member: worked out when the cursor or the offer changes, because it runs the evolution. */
static void evo_refresh(void)
{
    FootballCareer *c=&L.career;int index=evo_cursor();unsigned key=0;
    if(index>=0)key=1u+(unsigned)index+16u*c->evo[index].state+256u*(unsigned)c->roster.mon[c->evo[index].member].b.species;
    if(key==L.evo_key)return;
    L.evo_key=key;L.evo_after=index>=0 && c->evo[index].state==FB_EVO_OFFER && football_career_evo_preview(c,(unsigned)index,&L.evo_player);
}
static uint64_t token(void)
{
    const FootballCareer *c=&L.career;uint64_t hash=1469598103934665603ull;
    hash=(hash^c->phase)*1099511628211ull;hash=(hash^c->round)*1099511628211ull;hash=(hash^c->wallet)*1099511628211ull;hash=(hash^c->live.snaps)*1099511628211ull;hash=(hash^(unsigned)(c->mega+1))*1099511628211ull;
    for(unsigned i=0;i<22;++i)hash=(hash^c->roster.lineup[i])*1099511628211ull;
    for(unsigned i=0;i<3;++i)hash=(hash^c->sold[i])*1099511628211ull;
    for(unsigned i=0;i<c->evo_count;++i)hash=(hash^c->evo[i].state)*1099511628211ull;
    return hash;
}
static bool save_checkpoint(void)
{
    if(!L.args.persistence)return true;
    if(L.save_bad || !football_career_save_ready(&L.career))return false;
    size_t n=football_career_save(&L.career,checkpoint_bytes,sizeof checkpoint_bytes);
    if(!n || save_write("football_live",checkpoint_bytes,n)!=SAVE_OK)return false;
    football_career_flush_candy(&L.career); /* the candy earned up to this checkpoint goes to the profile with it, never before */
    if(L.args.save_profile && !L.args.save_profile(L.args.arg))return false;
    L.checkpoint=token();L.save_failed=false;return true;
}
static int load_checkpoint(void)
{
    const char *paths[]={SAVE_DIR "/football_live.sav",SAVE_DIR "/football_live.sav.bak"};
    for(unsigned i=0;i<2;++i){struct stat st;if(!stat(paths[i],&st)){if(st.st_size<0 || (uint64_t)st.st_size>FB_CAREER_SAVE_MAX+12u)return -1;}else if(errno!=ENOENT)return -1;}
    char *bytes=NULL;size_t size=0;bool backup=false;save_result status=save_read("football_live",&bytes,&size,&backup);
    if(status==SAVE_NOT_FOUND)return 0;
    if(status!=SAVE_OK)return -1;
    bool ok=football_career_load(&L.career,L.scratch,L.args.profile,bytes,size);free(bytes);
    return ok?1:-1;
}
static void leave(ui_screen *s)
{
    (void)s;C3D_FrameSync();football_render_free(&L.renderer);free(L.scratch);L.scratch=NULL;ui_set_game_speed(L.old_speed);
}
static void exit_game(void){L.leaving=true;if(L.args.on_exit)L.args.on_exit(L.args.arg);}
static void delete_done(int selected,void *arg)
{
    (void)arg;if(selected!=1)return;
    const char *paths[]={SAVE_DIR "/football_live.sav",SAVE_DIR "/football_live.sav.bak",SAVE_DIR "/football_live.sav.tmp"};
    for(unsigned i=0;i<3;++i)if(remove(paths[i]) && errno!=ENOENT){L.save_failed=true;return;}
    exit_game();
}
static void pause_done(int selected,void *arg)
{
    (void)arg;L.ignore_keys=true;
    if(selected==1){if(football_career_save_ready(&L.career)){if(!save_checkpoint()){L.save_failed=true;return;}}exit_game();}
    if(selected==2){football_screen_args args=L.args;/* Old six-player saves are preserved and remain explicitly accessible. */
        ui_set_game_speed(L.old_speed);
        if(!football_legacy_screen_open(&args))L.save_failed=true;
        ui_set_game_speed(1);
    }
    if(selected==3){static const char *const options[]={"Keep live playoff","Delete live playoff checkpoint"};ui_push_options(options,2,true,delete_done,NULL);}
    if(selected==4)exit_game();
}
static void pause_menu(void)
{
    L.pending=0;L.receiver=-1;
    static const char *const options[]={"Resume live playoff","Return (save at safe checkpoint)","Open legacy six-player playoff","Delete live playoff save","Return without saving latest changes"};
    ui_push_options(options,5,true,pause_done,NULL);
}
static void catalog(void)
{
    for(int i=0;i<gc_starter_cost_count && L.catalog_count<PR_SPECIES_COUNT;++i){
        unsigned id=gc_starter_costs[i].id;const pr_species *sp=NULL;for(unsigned j=0;j<PR_SPECIES_COUNT;++j)if(pr_species_table[j].id==id)sp=&pr_species_table[j];
        gc_starter starter;
        if(!sp || sp->prevolution>0 || !football_roster_starter(L.args.profile,id,&starter))continue;
        unsigned n=L.catalog_count++;L.catalog[n]=(uint16_t)id;
        while(n>0 && gc_starter_value(L.args.profile,L.catalog[n])<gc_starter_value(L.args.profile,L.catalog[n-1])){uint16_t t=L.catalog[n];L.catalog[n]=L.catalog[n-1];L.catalog[n-1]=t;--n;}
    }
}
/* The player a catalog species would make, from its base form: the stats and the position it is best at, shown while it is picked. */
static bool draft_player(unsigned species,FootballPlayer *out)
{
    for(unsigned j=0;j<PR_FORM_COUNT;++j)if(pr_forms[j].species==species && pr_forms[j].form_index==0)return football_stats_generate(out,0,&pr_forms[j],&L.career.roster.maxima);
    return false;
}
/* The slot the next pick fills. */
static unsigned draft_slot(void){return L.career.roster.count<22?L.career.roster.count:21;}
static unsigned top_species(void);
static unsigned top_after(void);
static void preview(void)
{
    unsigned id=top_species(),after=top_after();
    if(!id || (id==L.preview && after==L.preview_after))return;
    C3D_FrameSync();football_render_clear_sprites(&L.renderer);menu_mon mon={.species=(uint16_t)id,.gender=-1};football_render_load_front(&L.renderer,0,&mon);
    if(after){menu_mon next={.species=(uint16_t)after,.form=L.evo_player.form_index,.gender=-1};football_render_load_front(&L.renderer,1,&next);}
    L.preview=id;L.preview_after=after;L.asset_offense=-1;
}
static void assets(void)
{
    FootballLive *m=&L.career.live;if(L.asset_offense==m->snap_offense)return;
    C3D_FrameSync();football_render_clear_sprites(&L.renderer);
    for(unsigned i=0;i<22;++i){const FootballLiveActor *a=&m->actors[i];unsigned member=a->roster;
        /* Opponent uses existing front sprites too, with a distinct 22-member pool. */
        const gc_mon *source=&L.career.roster.mon[member];menu_mon mon={.species=source->b.species,.form=source->b.form,.gender=source->b.gender,.shiny=source->b.shiny,.variant=source->variant};
        if(!a->home){mon.species=m->away_species[member];mon.form=m->away_form[member];mon.gender=-1;mon.shiny=false;mon.variant=0;}
        if(a->home && (int)member==L.career.mega && L.career.roster.mega_item[member]){
            gc_run *scratch=L.scratch;gc_football_new(scratch,NULL,"mega-art",L.career.seed);scratch->party[0]=*source;scratch->n_party=1;
            const pr_form *f=gc_football_mega_form(scratch,0,L.career.roster.mega_item[member]);if(f)mon.form=f->form_index;
        }
        football_render_load_compact(&L.renderer,i,&mon);
    }
    L.asset_offense=m->snap_offense;L.preview=L.preview_after=0;
}
static void shop_assets(void)
{
    FootballSeason season={.phase=FB_SEASON_SHOP,.shop_count=3};
    season.shop[0]=(FootballShopOffer){.item=FB_ITEM_CARBOS,.classic_arg=3};
    season.shop[1]=(FootballShopOffer){.item=FB_ITEM_PROTEIN,.classic_arg=1};
    season.shop[2]=(FootballShopOffer){.item=FB_ITEM_MEGA_STONE,.classic_arg=(int16_t)L.career.stone_item};
    football_shop_prepare(&L.shop,&season);
    for(unsigned i=0;i<FB_SHOP_EVO_MAX;++i){
        const gm_item_ui *u=i<L.career.evo_count?evo_item(L.career.evo[i].item):NULL;
        L.evo_icons[i]=u && u->icon?mui_frame("items",u->icon):NULL;
    }
}
/* The playcall has plays, more plays and special teams; the two-point try has no kicks to offer. */
static unsigned pages(void){return L.career.live.conversion?2:3;}
static unsigned option_count(void)
{
    switch(L.career.phase){case FB_CAREER_DRAFT:return L.catalog_count;case FB_CAREER_LINEUP:return 22;case FB_CAREER_EVOLVE:return L.career.evolution_count+1;case FB_CAREER_SHOP:return FB_SHOP_FIXED+L.career.evo_count+1;case FB_CAREER_MATCH:{const FootballLive *m=&L.career.live;return m->rules.offense!=FB_HOME?3:m->conversion==1 || L.play_page==2?2:4;}default:return 1;}
}
static void cycle_mega(int direction)
{
    for(unsigned n=0;n<23;++n){L.career.mega+=direction;if(L.career.mega>21)L.career.mega=-1;if(L.career.mega< -1)L.career.mega=21;
        if(L.career.mega==-1 || L.career.roster.mega_item[L.career.mega])break;}
}
static void choose_current(void)
{
    FootballCareer *c=&L.career;
    switch(c->phase){
    case FB_CAREER_DRAFT:{gc_starter s;const char *why=NULL;
        if(!L.catalog_count || !football_roster_starter(L.args.profile,L.catalog[L.cursor],&s))return;
        /* Picks fill the slots in order (QB, RB, WR1 ...) and lineup[] starts as the identity, so each pick is already in its position. */
        if(!football_roster_add(&c->roster,L.scratch,&s,L.args.profile,&why))snprintf(c->message,sizeof c->message,"%s",why?why:"Cannot draft this Pokemon.");
        else {c->message[0]=0;if(c->roster.count==22){c->phase=FB_CAREER_READY;L.cursor=0;}}break;}
    case FB_CAREER_LINEUP:L.editing=!L.editing;break;
    case FB_CAREER_READY:if(football_career_start(c)){L.cursor=0;L.asset_offense=-1;L.camera.ready=false;}break;
    case FB_CAREER_MATCH:
        if(c->live.phase==FB_LIVE_CALL){FootballPlayId id=(FootballPlayId)(L.play_page*4+L.cursor);if(c->live.rules.offense==FB_AWAY)football_live_defend(&c->live,(FootballDefenseId)L.cursor);
            else if(c->live.conversion==1){
                /* The try after a touchdown: kick it, or play it from the 4 with the ordinary playcall. */
                if(L.cursor==0){if(!football_live_special(&c->live,FB_SPECIAL_EXTRA_POINT))return;}
                else{if(football_live_go_for_two(&c->live)){L.cursor=0;L.play_page=0;}return;}
            }
            else if(L.play_page==2){if(!football_live_special(&c->live,L.cursor?FB_SPECIAL_FIELD_GOAL:FB_SPECIAL_PUNT))return;}
            else football_live_call(&c->live,id);
            L.ignore_keys=true;
        }break;
    case FB_CAREER_RESULT:football_career_continue(c);L.cursor=0;break;
    case FB_CAREER_EVOLVE:football_career_evolve(c,L.cursor<c->evolution_count?(int)L.cursor:-1);L.cursor=0;break;
    case FB_CAREER_SHOP:if(L.cursor==FB_SHOP_FIXED+c->evo_count){football_career_continue(c);L.cursor=0;}else football_career_buy(c,L.cursor,L.member);break;
    case FB_CAREER_END:exit_game();break;
    default:break;
    }
}
static bool input(ui_screen *s,ui_button button)
{
    (void)s;if(ui_input_repeating() && (button==BTN_ACTION || button==BTN_SUBMIT))return true;
    if(button==BTN_MENU){pause_menu();return true;}
    FootballCareer *c=&L.career;
    if(c->phase==FB_CAREER_MATCH && c->live.phase!=FB_LIVE_CALL)return true; /* hardware held/edge state owns live controls */
    unsigned count=option_count();if(!count)return true;
    if(button==BTN_UP || button==BTN_DOWN)c->message[0]=0; /* the shop's note on the last purchase goes when the cursor moves */
    if(button==BTN_UP)L.cursor=(L.cursor+count-1)%count;
    else if(button==BTN_DOWN)L.cursor=(L.cursor+1)%count;
    else if(button==BTN_LEFT || button==BTN_RIGHT){int delta=button==BTN_RIGHT?1:-1;
        if(c->phase==FB_CAREER_LINEUP && L.editing){unsigned member=c->roster.lineup[L.cursor];football_roster_assign(&c->roster,L.cursor,(member+22+delta)%22);}
        else if(c->phase==FB_CAREER_DRAFT)L.cursor=(L.cursor+count+(delta>0?FM_ROWS:count-FM_ROWS%count))%count;
        else if(c->phase==FB_CAREER_READY)cycle_mega(delta);
        else if(c->phase==FB_CAREER_SHOP)L.member=(L.member+22+delta)%22;
        else if(c->phase==FB_CAREER_MATCH && c->live.conversion!=1){L.play_page=(L.play_page+pages()+(unsigned)delta)%pages();L.cursor=0;}
    }else if(button==BTN_ACTION || button==BTN_SUBMIT)choose_current();
    else if(button==BTN_CANCEL){
        if(c->phase==FB_CAREER_DRAFT && c->roster.count){unsigned i=--c->roster.count;c->roster.cost-=gc_starter_value(L.args.profile,c->roster.mon[i].b.species);}
        else if(c->phase==FB_CAREER_LINEUP){if(L.editing)L.editing=false;else {c->phase=FB_CAREER_READY;L.cursor=0;}}
        else if(c->phase==FB_CAREER_READY){c->phase=FB_CAREER_LINEUP;L.cursor=0;}
        else if(c->phase==FB_CAREER_EVOLVE){football_career_evolve(c,-1);L.cursor=0;}
    }
    return true;
}
static bool touch(ui_screen *s,float x,float y)
{
    (void)s;FootballCareer *c=&L.career;
    if(c->phase==FB_CAREER_RESULT || c->phase==FB_CAREER_END){choose_current();return true;}
    if(c->phase==FB_CAREER_MATCH && c->live.phase!=FB_LIVE_CALL){
        if(c->live.rules.offense==FB_AWAY && y>=MAP_Y && y<MAP_Y+108){
            unsigned nearest=0;float best=100000;
            for(unsigned i=0;i<11;++i){float dx=x-(MAP_X+c->live.actors[i].x*2.5f),dy=y-(MAP_Y+c->live.actors[i].y*2);float d=dx*dx+dy*dy;if(d<best){best=d;nearest=i;}}
            c->live.controlled=(uint8_t)nearest;return true;
        }
        if(c->live.special && c->live.stage!=FB_STAGE_RETURN && c->live.kick_side==FB_HOME){L.pending|=FB_LIVE_ACTION;}
        else if(y>=TARGET_Y && y<TARGET_Y+2*TARGET_PITCH_Y-6 && x>=TARGET_X && x<TARGET_X+3*TARGET_PITCH_X){
            unsigned column=(unsigned)((x-TARGET_X)/TARGET_PITCH_X),row=(unsigned)((y-TARGET_Y)/TARGET_PITCH_Y);unsigned target=row*3+column;
            if(target<5)L.receiver=(int)target;else L.pending|=FB_LIVE_ACTION;
        }
        else if(y>=FM_FOOT_Y-4){if(x<160)L.pending|=FB_LIVE_JUKE|FB_LIVE_SWITCH;else L.pending|=FB_LIVE_ACTION;}
        return true;
    }
    if(c->phase==FB_CAREER_MATCH){if(y>=38 && y<FM_FOOT_Y-2){L.cursor=(x>=160?1:0)+(y>=127?2:0);if(L.cursor<option_count())choose_current();}else if(y>=FM_FOOT_Y-2 && c->live.conversion!=1){L.play_page=(L.play_page+1)%pages();L.cursor=0;}return true;}
    if(c->phase==FB_CAREER_READY){if(y>=124 && y<FM_FOOT_Y-4)cycle_mega(1);else if(y>=FM_FOOT_Y-4){if(x<160){c->phase=FB_CAREER_LINEUP;L.cursor=0;}else choose_current();}return true;}
    if(c->phase==FB_CAREER_LINEUP && y>=FM_FOOT_Y-4){if(L.editing){unsigned member=c->roster.lineup[L.cursor];football_roster_assign(&c->roster,L.cursor,(member+(x<160?21:1))%22);return true;}if(x<160)football_roster_autofill(&c->roster);else {c->phase=FB_CAREER_READY;L.editing=false;L.cursor=0;}return true;}
    if(c->phase==FB_CAREER_SHOP && y>=FM_FOOT_Y-4){L.member=(L.member+1)%22;return true;}
    int row=football_menu_row_at(x,y);
    if(row>=0){unsigned option=L.cursor/FM_ROWS*FM_ROWS+(unsigned)row;if(option<option_count()){L.cursor=option;if(c->phase==FB_CAREER_SHOP)c->message[0]=0;choose_current();}}
    return true;
}
static void update(ui_screen *s,float dt)
{
    (void)s;if(ui_top()!=&live_screen || ui_busy() || L.leaving)return;
    if(!isfinite(dt) || dt<0)dt=0;
    if(dt>100)dt=100;
    L.remainder+=dt;unsigned ms=(unsigned)L.remainder;L.remainder-=ms;L.ms+=ms;
    FootballCareer *c=&L.career;
    FootballLiveInput controls={.receiver=L.receiver,.pressed=L.pending};L.receiver=-1;L.pending=0;
    if(c->phase==FB_CAREER_MATCH && (c->live.phase==FB_LIVE_PLAY || c->live.phase==FB_LIVE_SET)){
        circlePosition stick;hidCircleRead(&stick);u32 held=hidKeysHeld(),down=L.ignore_keys?0:hidKeysDown();L.ignore_keys=false;
        controls.x=abs(stick.dx)>18?stick.dx/156.0f:0;controls.y=abs(stick.dy)>18?-stick.dy/156.0f:0;
        if(held&KEY_DLEFT)controls.x=-1;
        if(held&KEY_DRIGHT)controls.x=1;
        if(held&KEY_DUP)controls.y=-1;
        if(held&KEY_DDOWN)controls.y=1;
        if(down&KEY_A)controls.pressed|=FB_LIVE_ACTION;
        if(down&KEY_B)controls.pressed|=FB_LIVE_JUKE|FB_LIVE_SWITCH;
        if(down&KEY_X)controls.receiver=0;
        if(down&KEY_Y)controls.receiver=1;
        if(down&KEY_L)controls.receiver=2;
        if(down&KEY_R)controls.receiver=3;
        if(down&KEY_SELECT)controls.receiver=4;
    }
    /* Keep the set formation in view before play can start after a long drive
     * transition. The camera continues easing; neither game clock nor AI advances. */
    float focus_x=0,focus_y=0,focus_vx=0;if(c->phase==FB_CAREER_MATCH)football_live_focus(&c->live,&focus_x,&focus_y,&focus_vx);
    bool framing=c->phase==FB_CAREER_MATCH && c->live.phase==FB_LIVE_SET && L.camera.ready && fabsf(L.camera.x-focus_x)>14;
    football_career_tick(c,framing?0:ms,controls);
    if(c->phase!=L.old_phase){L.cursor=0;L.editing=false;if(c->phase==FB_CAREER_SHOP)shop_assets();L.old_phase=c->phase;}
    evo_refresh();
    if(c->phase==FB_CAREER_MATCH || c->phase==FB_CAREER_RESULT){
        if(c->live.phase!=FB_LIVE_DEAD && c->live.phase!=FB_LIVE_END)assets();
        float tx,ty,tvx;football_live_focus(&c->live,&tx,&ty,&tvx);
        football_camera_follow(&L.camera,tx,ty,tvx,(float)ms);
    }else preview();
    if(L.args.persistence && !L.save_bad && football_career_save_ready(c) && token()!=L.checkpoint){
        if(!save_checkpoint()){L.save_failed=true;L.checkpoint=token();} /* retry explicitly from START; avoid frame write loops */
    }
}
static char row_label[FM_ROWS][48],row_value[FM_ROWS][24];
static FootballMenuRow page_rows[FM_ROWS];
static const char *const shop_names[3]={"Carbos","Protein","Mega stone"};
/* The page of the list the cursor is on, in the classic option list's look; the rows are built from the career as the old menus built them. */
static void list_menu(unsigned count)
{
    FootballCareer *c=&L.career;unsigned first=L.cursor/FM_ROWS*FM_ROWS,n=0;
    for(;n<FM_ROWS && first+n<count;++n){
        unsigned i=first+n;FootballMenuRow *r=&page_rows[n];*r=(FootballMenuRow){.label=row_label[n]};row_label[n][0]=row_value[n][0]=0;
        switch(c->phase){
        case FB_CAREER_DRAFT:{unsigned id=L.catalog[i];bool drafted=false;
            for(unsigned j=0;j<c->roster.count;++j)if(c->roster.mon[j].b.species==id)drafted=true;
            snprintf(row_label[n],sizeof row_label[n],"%s",name(id));
            {FootballPlayer p;if(draft_player(id,&p))r->tag=football_position_label(p.position);} /* what it is best at */
            if(drafted){r->value="Picked";r->value_style=TS_SUMMARY_GREEN;}
            else{snprintf(row_value[n],sizeof row_value[n],"%.2f",gc_starter_value(L.args.profile,id));r->value=row_value[n];r->value_style=TS_MONEY;}
            break;}
        case FB_CAREER_LINEUP:
            snprintf(row_label[n],sizeof row_label[n],"%s",name(c->roster.mon[c->roster.lineup[i]].b.species));r->tag=football_slot_names[i];
            if(L.editing && i==L.cursor){r->value="< >";r->value_style=TS_SUMMARY_GOLD;}
            break;
        case FB_CAREER_EVOLVE:snprintf(row_label[n],sizeof row_label[n],"%s",i<c->evolution_count?name(c->evolutions[i]):"Keep current Pokemon");break;
        case FB_CAREER_SHOP:
            if(i==FB_SHOP_FIXED+c->evo_count){snprintf(row_label[n],sizeof row_label[n],"Continue to next round");break;}
            r->indent=24;
            if(i>=FB_SHOP_FIXED){ /* an evolution item, for the member it fits: "Growlithe: Fire Stone" */
                const FootballEvoOffer *o=&c->evo[i-FB_SHOP_FIXED];
                snprintf(row_label[n],sizeof row_label[n],"%s: %s",name(c->roster.mon[o->member].b.species),evo_item_name(o->item));
                if(o->state==FB_EVO_SOLD){r->value="Sold";r->dim=true;}
                else if(o->state==FB_EVO_VOID){r->value="Evolved";r->dim=true;}
                else{snprintf(row_value[n],sizeof row_value[n],"%u",o->price);r->value=row_value[n];r->value_style=TS_MONEY;}
                break;
            }
            snprintf(row_label[n],sizeof row_label[n],"%s",shop_names[i]);
            if(c->sold[i]){r->value="Sold";r->dim=true;}
            else if(i==2 && c->stone_target<0){r->value="No stock";r->dim=true;}
            else{snprintf(row_value[n],sizeof row_value[n],"%u",c->prices[i]);r->value=row_value[n];r->value_style=TS_MONEY;}
            break;
        default:break;
        }
    }
    football_menu_list(page_rows,n,(int)(L.cursor-first),first,count);
    if(c->phase==FB_CAREER_SHOP)for(unsigned k=0;k<n;++k){
        unsigned i=first+k;const rr_frame *icon=i<FB_SHOP_FIXED?L.shop.icons[i]:i<FB_SHOP_FIXED+c->evo_count?L.evo_icons[i-FB_SHOP_FIXED]:NULL;
        if(icon)football_render_draw_sprite(icon,FM_LIST_X+FM_LABEL_X+12,FM_LIST_Y+FM_LIST_PAD+8+k*FM_ROW_PITCH,.45f,false);
    }
}
/* Two buttons along the bottom, the right one the main action. */
static void buttons(const char *left,const char *right,bool right_enabled)
{
    football_menu_button(FM_FOOT_X,FM_FOOT_Y,152,FM_FOOT_H,left,false,true);
    football_menu_button(FM_FOOT_X+160,FM_FOOT_Y,152,FM_FOOT_H,right,right_enabled,right_enabled);
}
static void save_failed_notice(void)
{
    football_menu_panel(FM_HEAD_X,FM_HEAD_Y,FM_HEAD_W,FM_HEAD_H);
    football_menu_text(TS_SETTINGS_SELECTED,FM_PX_SMALL,FM_HEAD_X+12,FM_HEAD_Y+FM_TEXT_PAD+1,FM_HEAD_W-24,"Save failed: press START to retry or return");
}
static const char *const down_names[4]={"1st","2nd","3rd","4th"};
static void match_call(void)
{
    FootballCareer *c=&L.career;bool home=c->live.rules.offense==FB_HOME,attempt=home && c->live.conversion==1;
    const char *heading=!home?(c->live.conversion?"Set the defense for the try":"Choose a defensive scheme"):attempt?"Touchdown!  The try":c->live.conversion?"Go for two: choose a play":L.play_page==2?"Special teams":"Choose your play";
    char right[24]="";
    if(home && !c->live.conversion && c->live.rules.down>=1 && c->live.rules.down<=4)snprintf(right,sizeof right,"%s & %u",down_names[c->live.rules.down-1],football_rules_distance(&c->live.rules));
    football_menu_head(heading,right);
    if(attempt){
        char detail[48];
        snprintf(detail,sizeof detail,"+1   %u%% to make it",football_live_extra_point_chance(&c->live));
        football_live_art_special_card(&L.renderer,FB_SPECIAL_EXTRA_POINT,detail,10,40,145,80,L.cursor==0,true);
        football_live_art_special_card(&L.renderer,FB_SPECIAL_NONE,"+2   play it from the 4",165,40,145,80,L.cursor==1,true);
        snprintf(detail,sizeof detail,"%u - %u after the touchdown",c->live.rules.score[0],c->live.rules.score[1]);
        football_menu_panel(FM_HEAD_X,126,FM_HEAD_W,FM_HEAD_H);football_menu_text(TS_MONEY,FM_PX_BODY,FM_HEAD_X+12,126+FM_TEXT_PAD,FM_HEAD_W-24,detail);
    }else if(home && L.play_page==2){
        char detail[48];
        snprintf(detail,sizeof detail,"Kick it away, then cover the return");
        football_live_art_special_card(&L.renderer,FB_SPECIAL_PUNT,detail,10,40,145,80,L.cursor==0,true);
        bool range=football_live_special_allowed(&c->live,FB_SPECIAL_FIELD_GOAL);
        if(range)snprintf(detail,sizeof detail,"%u yd   %u%% to make it",football_live_fg_distance(&c->live),football_live_fg_chance(&c->live));
        else snprintf(detail,sizeof detail,"%u yd: out of range",football_live_fg_distance(&c->live));
        football_live_art_special_card(&L.renderer,FB_SPECIAL_FIELD_GOAL,detail,165,40,145,80,L.cursor==1,range);
        snprintf(detail,sizeof detail,"%s down, %u yd to go",down_names[c->live.rules.down>=1 && c->live.rules.down<=4?c->live.rules.down-1:3],football_rules_distance(&c->live.rules));
        football_menu_panel(FM_HEAD_X,126,FM_HEAD_W,FM_HEAD_H);football_menu_text(TS_MONEY,FM_PX_BODY,FM_HEAD_X+12,126+FM_TEXT_PAD,FM_HEAD_W-24,detail);
    }else for(unsigned i=0;i<(home?4u:3u);++i){
        if(home)football_live_art_card(&L.renderer,(FootballPlayId)(L.play_page*4+i),10+(i%2)*155,40+(i/2)*87,145,80,i==L.cursor);
        else football_live_art_defense_card(&L.renderer,(FootballDefenseId)i,10+(i%2)*155,40+(i/2)*87,145,80,i==L.cursor);
    }
    football_menu_foot(attempt?"A or tap: choose the try":home?(c->live.conversion?"Left/Right or tap: more plays":"Left/Right or tap: plays / special teams"):"A: set the defense, then control a defender");
}
static void match_live(void)
{
    FootballCareer *c=&L.career;const FootballLive *m=&c->live;
    /* Which controls the human has right now: the kicker, the return team, or the ordinary offense/defense. */
    bool kicking=m->special && m->stage!=FB_STAGE_RETURN && m->kick_side==FB_HOME;
    bool covering=m->special && ((m->stage==FB_STAGE_RETURN && m->rules.offense==FB_AWAY));
    bool receiving=m->special && ((m->stage!=FB_STAGE_RETURN && m->kick_side==FB_AWAY) || (m->stage==FB_STAGE_RETURN && m->rules.offense==FB_HOME));
    bool free_kick=m->special==FB_SPECIAL_PUNT || m->special==FB_SPECIAL_KICKOFF;
    football_menu_head(m->phase==FB_LIVE_SET?"A or tap: snap the ball":"Live play",NULL);
    const char *help;
    if(kicking)help=m->special==FB_SPECIAL_PUNT?"Circle Pad up/down aims the punt":m->special==FB_SPECIAL_KICKOFF?"Circle Pad up/down aims the kick":"The kicker takes the snap and kicks";
    else if(receiving && free_kick)help=m->stage==FB_STAGE_RETURN?"Circle Pad: run     B: juke":m->special==FB_SPECIAL_KICKOFF?"Circle Pad: get under it, then run it back":"Circle Pad: get under it     A: fair catch";
    else if(receiving)help="Circle Pad: rush the kicker     A: dive     B: switch";
    else if(covering || m->special)help="Circle Pad: chase the returner     B: switch     A: dive";
    else help=m->rules.offense==FB_HOME?"Circle Pad: run     B: juke":"Circle Pad: move     B: switch defender";
    football_menu_panel(FM_HEAD_X,32,FM_HEAD_W,30);football_menu_text(TS_WINDOW,FM_PX_SMALL,FM_HEAD_X+12,32+FM_TEXT_PAD+1,FM_HEAD_W-24,help);
    if(kicking){
        football_menu_panel(FM_HEAD_X,68,FM_HEAD_W,100);
        football_menu_text(TS_SUMMARY_GOLD,FM_PX_BODY,16,78,288,football_live_art_special_name((FootballSpecial)m->special));
        char info[48];if(m->special==FB_SPECIAL_FIELD_GOAL)snprintf(info,sizeof info,"%u yd   %u%% to make it",football_live_fg_distance(m),football_live_fg_chance(m));
        else if(m->special==FB_SPECIAL_EXTRA_POINT)snprintf(info,sizeof info,"%u%% to make it",football_live_extra_point_chance(m));
        else snprintf(info,sizeof info,m->special==FB_SPECIAL_KICKOFF?"Tap or press A to kick off":"Tap or press A to snap");
        football_menu_text(TS_WINDOW,FM_PX_BODY,16,104,288,info);
    }else if(m->rules.offense==FB_HOME && !m->special){
        const char *const targets[]={"1  X","2  Y","3  L","4  R","5  Select","A  Snap"};
        static const unsigned slots[5]={2,3,4,5,1};
        for(unsigned i=0;i<6;++i){float x=TARGET_X+(i%3)*TARGET_PITCH_X,y=TARGET_Y+(i/3)*TARGET_PITCH_Y;
            football_menu_panel(x,y,TARGET_W,TARGET_H);football_menu_text(i<5?TS_STATS_LABEL:TS_SUMMARY_GOLD,FM_PX_BODY,x+10,y+9,TARGET_W-20,targets[i]);
            if(i<5)football_menu_text(TS_WINDOW,FM_PX_TINY,x+10,y+34,TARGET_W-18,name(c->roster.mon[m->actors[slots[i]].roster].b.species));
        }
    }else{
        football_menu_panel(FM_HEAD_X,66,FM_HEAD_W,130);
        rr_rect(MAP_X,MAP_Y,300,108,RR_RGBA(42,128,52,255));
        for(unsigned i=0;i<22;++i){const FootballLiveActor *a=&m->actors[i];float x=MAP_X+a->x*2.5f,y=MAP_Y+a->y*2;
            C2D_DrawRectSolid(x-2,y-2,.1f,5,5,i==m->controlled?C2D_Color32(255,210,63,255):a->home?C2D_Color32(76,201,240,255):C2D_Color32(232,53,74,255));}
        football_menu_text(TS_WINDOW,FM_PX_TINY,MAP_X+2,MAP_Y+110,296,"Tap the map to select any player");
    }
    bool offense=kicking || (!m->special && m->rules.offense==FB_HOME);
    bool fair=receiving && m->special==FB_SPECIAL_PUNT && m->stage!=FB_STAGE_RETURN;
    const char *kick_label=m->special==FB_SPECIAL_KICKOFF?"A: Kick":"A: Snap";
    const char *action=offense?kick_label:fair?"A: Fair catch":receiving && free_kick && m->stage!=FB_STAGE_RETURN?"":"A: Dive";
    buttons(offense?"B: Juke":"B: Switch",action,action[0]!=0);
}
static void bottom_menu(void)
{
    FootballCareer *c=&L.career;char left[96],right[64];
    switch(c->phase){
    case FB_CAREER_DRAFT:
        snprintf(left,sizeof left,"Pick your %s",football_slot_titles[draft_slot()]);snprintf(right,sizeof right,"%.2f / 55 pt",c->roster.cost);
        football_menu_head(left,right);list_menu(L.catalog_count);
        snprintf(left,sizeof left,"A: pick for %s    B: undo    Left/Right: page",football_slot_names[draft_slot()]);
        football_menu_foot(L.catalog_count<22?"Unlock 22 base forms in Classic / Endless":c->message[0]?c->message:left);break;
    case FB_CAREER_LINEUP:
        football_menu_head(L.editing?"Pick a member for this slot":"Lineup",L.editing?"Left/Right: swap":"A: edit    B: ready");list_menu(22);
        buttons(L.editing?"Previous":"Auto-assign",L.editing?"Next":"Ready",true);break;
    case FB_CAREER_READY:
        snprintf(right,sizeof right,"Round %u / 7",c->round);football_menu_head("Playoffs",right);
        football_menu_panel(4,32,312,92);
        football_menu_text(TS_STATS_LABEL,FM_PX_BODY,16,41,288,"11 offense + 11 defense");
        football_menu_text(TS_WINDOW,FM_PX_SMALL,16,63,288,"Circle Pad: move     A: snap / tackle");
        football_menu_text(TS_WINDOW,FM_PX_SMALL,16,81,288,"X / Y / L / R / Select: receivers 1-5");
        football_menu_text(TS_WINDOW,FM_PX_SMALL,16,99,288,"B: juke / switch defender");
        football_menu_panel(4,128,312,60);
        football_menu_text(TS_STATS_LABEL,FM_PX_BODY,16,137,130,"Mega evolution");
        football_menu_text_right(TS_MONEY,FM_PX_BODY,304,137,160,c->mega<0?"None":name(c->roster.mon[c->mega].b.species));
        football_menu_text(TS_WINDOW,FM_PX_SMALL,16,163,288,"Left/Right or tap: choose who mega evolves");
        buttons("B: Lineup","A: Kick off",true);break;
    case FB_CAREER_MATCH:if(c->live.phase==FB_LIVE_CALL)match_call();else match_live();break;
    case FB_CAREER_RESULT:{
        bool won=c->live.rules.winner==FB_HOME;
        football_menu_head(won?"Match won":"Playoff elimination",NULL);
        football_menu_panel(4,32,312,112);
        snprintf(left,sizeof left,"Final   Home %u - %u Away",c->live.rules.score[0],c->live.rules.score[1]);
        football_menu_text(TS_WINDOW,FM_PX_BODY,16,42,288,left);
        football_menu_text(TS_STATS_LABEL,FM_PX_BODY,16,72,288,"Species candy");
        /* Candy points fill each species' candy bar, so a common Pokemon earns whole candies and a legendary a sliver: the totals are in hundredths. */
        snprintf(left,sizeof left,"+%u.%02u from plays",c->candy_plays_milli/1000,c->candy_plays_milli%1000/10);football_menu_text(TS_MONEY,FM_PX_BODY,16,92,288,left);
        { const unsigned team=football_career_team_candy_milli(c,won);snprintf(left,sizeof left,"+%u.%02u more for the team",team/1000,team%1000/10);football_menu_text(TS_WINDOW,FM_PX_BODY,16,110,288,left); }
        football_menu_text(TS_WINDOW,FM_PX_SMALL,16,129,288,"Each candy bar is longer for rarer Pokemon.");
        football_menu_foot("A or tap: team progression");break;}
    case FB_CAREER_EXP:
        snprintf(right,sizeof right,"Player %u / 22",c->exp_member+1);football_menu_head("Experience",right);
        football_menu_panel(4,32,312,60);football_menu_text(TS_WINDOW,FM_PX_BODY,16,48,288,"Applying level-ups and evolutions...");break;
    case FB_CAREER_EVOLVE:
        football_menu_head("Choose an evolution",name(L.scratch->party[0].b.species));list_menu(c->evolution_count+1);football_menu_foot("A: choose    B: keep the current Pokemon");break;
    case FB_CAREER_SHOP:
        snprintf(right,sizeof right,"Cash %u",c->wallet);football_menu_head("Shop",right);list_menu(option_count());
        if(c->message[0])football_menu_foot(c->message); /* the last purchase */
        else if(evo_cursor()>=0){
            const FootballEvoOffer *o=&c->evo[evo_cursor()];
            if(o->state==FB_EVO_OFFER)snprintf(left,sizeof left,"A: buy for %u    Up/Down: choose%s",o->price,c->wallet<o->price?"    Not enough cash":"");
            else snprintf(left,sizeof left,"%s",o->state==FB_EVO_SOLD?"Bought: this Pokemon has evolved.":"This Pokemon evolved another way.");
            football_menu_foot(left);
        }
        else {snprintf(left,sizeof left,"Target %u: %.16s    Left/Right: change    A: buy",L.member+1,name(c->roster.mon[L.member].b.species));football_menu_foot(left);}
        break;
    case FB_CAREER_END:
        snprintf(left,sizeof left,"Playoff complete: %u / 7 wins",c->wins);football_menu_head(left,NULL);
        football_menu_panel(4,32,312,76);football_menu_text(TS_STATS_LABEL,FM_PX_BODY,16,42,288,"Species candy this match");
        { const unsigned total=c->candy_plays_milli+c->candy_team_milli;snprintf(left,sizeof left,"+%u.%02u",total/1000,total%1000/10);football_menu_text(TS_MONEY,FM_PX_BODY,16,66,288,left); }
        football_menu_foot("A or tap: return to title");break;
    case FB_CAREER_LEARN:break; /* level-up moves are learned without a prompt */
    case FB_CAREER_ERROR:
        football_menu_head("Checkpoint preserved",NULL);football_menu_panel(4,32,312,120);football_menu_wrap(TS_WINDOW,FM_PX_SMALL,16,44,288,6,c->message);break;
    }
    if(L.save_failed)save_failed_notice();
}
/* The member and species the top screen's info panel shows. */
static unsigned top_member(void)
{
    const FootballCareer *c=&L.career;
    if(c->phase==FB_CAREER_SHOP)return evo_cursor()>=0?c->evo[evo_cursor()].member%22u:L.member%22;
    if(c->phase==FB_CAREER_EXP || c->phase==FB_CAREER_EVOLVE)return c->exp_member%22;
    return c->roster.count?c->roster.lineup[L.cursor%c->roster.count]%22:0;
}
static unsigned top_species(void)
{
    const FootballCareer *c=&L.career;
    if(c->phase==FB_CAREER_DRAFT)return L.catalog_count?L.catalog[L.cursor%L.catalog_count]:0;
    if(c->phase==FB_CAREER_EXP || c->phase==FB_CAREER_EVOLVE)return L.scratch->party[0].b.species;
    return c->roster.count?c->roster.mon[top_member()].b.species:0;
}
/* The species the highlighted evolution offer would make, 0 when the cursor is not on one that can still be bought. */
static unsigned top_after(void){return evo_cursor()>=0 && L.evo_after?L.career.evo[evo_cursor()].species:0;}
static void top_menu(void)
{
    FootballCareer *c=&L.career;char right[48],line[64];
    football_menu_top_bg(c->round);
    football_menu_panel(10,6,380,28);football_menu_text(TS_WINDOW,FM_PX_BODY,24,13,170,"Pokemon Playoff");
    if(c->phase==FB_CAREER_DRAFT)snprintf(right,sizeof right,"%s  %u/22   %.2f / 55 pt",football_slot_names[draft_slot()],c->roster.count,c->roster.cost);
    else snprintf(right,sizeof right,"Round %u / 7",c->round);
    football_menu_text_right(TS_MONEY,FM_PX_BODY,376,13,190,right);
    if(c->phase==FB_CAREER_DRAFT && !L.catalog_count){
        football_menu_panel(10,40,380,150);
        football_menu_text(TS_SUMMARY_GOLD,FM_PX_BODY,26,56,348,"No unlocked base-form starters");
        football_menu_wrap(TS_WINDOW,FM_PX_BODY,26,86,348,4,"Unlock Pokemon in Classic / Endless first.");
        return;
    }
    if(c->phase==FB_CAREER_ERROR){
        football_menu_panel(10,40,380,150);
        football_menu_text(TS_SUMMARY_GOLD,FM_PX_BODY,26,56,348,"Checkpoint preserved");
        football_menu_wrap(TS_WINDOW,FM_PX_BODY,26,86,348,4,c->message);
        return;
    }
    unsigned id=top_species(),member=top_member(),after=top_after();
    football_menu_panel(10,40,150,150);
    const rr_frame *f=football_render_front_frame(&L.renderer,0,L.ms);
    if(after){ /* an evolution offer: the Pokemon as it is, and as it would become */
        const rr_frame *g=football_render_front_frame(&L.renderer,1,L.ms);
        if(f && f->src_w && f->src_h)football_render_draw_sprite(f,46,118,fminf(62.0f/f->src_w,62.0f/f->src_h),false);
        if(g && g->src_w && g->src_h)football_render_draw_sprite(g,124,118,fminf(62.0f/g->src_w,62.0f/g->src_h),false);
        football_menu_text_center(TS_WINDOW,FM_PX_BODY,85,106,18,">");
    }
    else if(f && f->src_w && f->src_h)football_render_draw_sprite(f,85,115,fminf(108.0f/f->src_w,108.0f/f->src_h),false);
    football_menu_panel(168,40,222,150);
    if(after){snprintf(line,sizeof line,"%.15s > %.15s",name(id),name(after));football_menu_text(TS_WINDOW,FM_PX_BODY,184,49,190,line);}
    else football_menu_text(TS_WINDOW,FM_PX_BODY,184,49,c->phase==FB_CAREER_DRAFT?130:190,id?name(id):"");
    {
        /* The stats of whoever is highlighted: a member of the roster, or while drafting the species as it would be picked. */
        FootballPlayer drafting={0};
        if(c->phase==FB_CAREER_DRAFT){
            if(!draft_player(id,&drafting))drafting=(FootballPlayer){0};
            snprintf(line,sizeof line,"%.2f",gc_starter_value(L.args.profile,id));football_menu_text_right(TS_MONEY,FM_PX_BODY,374,49,60,line);
            snprintf(line,sizeof line,"Best at %s",football_position_label(drafting.position));football_menu_text(TS_STATS_LABEL,FM_PX_SMALL,184,66,190,line);
        }
        const FootballAttributes *a=c->phase==FB_CAREER_DRAFT?&drafting.attributes:&c->roster.player[member].attributes,*b=&L.evo_player.attributes;
        static const char *const labels[8]={"SPD","AGI","STR","TAK","STA","THP","THA","CTH"};
        const unsigned values[8]={a->spd,a->agi,a->str,a->tak,a->sta,a->thp,a->tha,a->cth};
        const unsigned later[8]={b->spd,b->agi,b->str,b->tak,b->sta,b->thp,b->tha,b->cth};
        if(c->phase==FB_CAREER_LINEUP)football_menu_text_right(TS_STATS_LABEL,FM_PX_BODY,374,49,60,football_slot_names[L.cursor%22]);
        for(unsigned i=0;i<8;++i){
            float x=184+(i/4)*100,y=84+(i%4)*24;
            football_menu_text(TS_STATS_LABEL,FM_PX_SMALL,x,y,after?28:34,labels[i]);
            if(after){ /* today's value, then the evolved one: "52>67", the bar showing the evolved one */
                football_menu_bar(x+30,y+4,32,8,later[i],100);
                if(later[i]!=values[i])snprintf(line,sizeof line,"%u>%u",values[i],later[i]);else snprintf(line,sizeof line,"%u",values[i]);
                football_menu_text_right(later[i]>values[i]?TS_SUMMARY_GREEN:TS_WINDOW,FM_PX_SMALL,x+96,y,32,line);
            }else{
                football_menu_bar(x+34,y+4,40,8,values[i],100);
                snprintf(line,sizeof line,"%u",values[i]);football_menu_text_right(TS_WINDOW,FM_PX_SMALL,x+92,y,22,line);
            }
        }
    }
    football_menu_panel(10,196,380,32);
    char shop_tip[96];
    if(c->phase==FB_CAREER_SHOP){
        shop_tip[0]=0;
        if(c->message[0])snprintf(shop_tip,sizeof shop_tip,"%.90s",c->message);
        else if(evo_cursor()>=0){const FootballEvoOffer *o=&c->evo[evo_cursor()];
            snprintf(shop_tip,sizeof shop_tip,o->state==FB_EVO_OFFER?"%.20s evolves into %.20s.%s":"%.20s: %.20s.%s",name(id),o->state==FB_EVO_OFFER?name(o->species):evo_item_name(o->item),o->item>50 && o->state==FB_EVO_OFFER?" A rare item.":"");}
    }
    if(c->phase==FB_CAREER_DRAFT)snprintf(shop_tip,sizeof shop_tip,"Choose who plays %s. Its stats are on the right.",football_slot_titles[draft_slot()]);
    const char *tip=(c->phase==FB_CAREER_SHOP || c->phase==FB_CAREER_DRAFT) && shop_tip[0]?shop_tip:c->phase==FB_CAREER_ERROR?"Your checkpoint is kept exactly as it was.":c->phase==FB_CAREER_LINEUP?"Assign your lineup. Control every snap.":c->phase==FB_CAREER_SHOP?"Spend your winnings before the next round.":c->phase==FB_CAREER_EXP || c->phase==FB_CAREER_EVOLVE?"Level-ups and evolutions from the match.":"Seven rounds stand between you and the title.";
    football_menu_text_center(TS_WINDOW,FM_PX_SMALL,200,196+FM_TEXT_PAD+1,356,tip);
}
static void draw(ui_screen *s,rr_screen which)
{
    (void)s;FootballCareer *c=&L.career;if(!L.renderer.text)return;
    if(!ui_space(which==RR_TOP?UI_PHYS_TOP:UI_PHYS_BOTTOM))return;
    if(which==RR_TOP){
        if(c->phase==FB_CAREER_MATCH || c->phase==FB_CAREER_RESULT)football_live_art_top(&L.renderer,c,&L.camera,L.ms);
        else top_menu();
        return;
    }
    C2D_TextBufClear(L.renderer.text);football_menu_bottom();bottom_menu();
}
static ui_screen live_screen={.name="football-live",.flags=UI_FADE,.leave=leave,.input=input,.touch=touch,.update=update,.draw=draw};
bool football_screen_open(const football_screen_args *args)
{
    if(!args || !args->profile || !args->on_exit || L.scratch)return false;
    memset(&L,0,sizeof L);L.args=*args;L.old_speed=ui_game_speed();L.asset_offense=-1;L.receiver=-1;L.old_phase=FB_CAREER_ERROR;
    L.scratch=calloc(1,sizeof *L.scratch);if(!L.scratch)return false;
    if(!football_render_init(&L.renderer) || !football_career_new(&L.career,L.scratch,args->profile,args->entropy)){football_render_free(&L.renderer);free(L.scratch);L.scratch=NULL;return false;}
    catalog();
    if(L.catalog_count>=22){double cheapest=0;for(unsigned i=0;i<22;++i)cheapest+=gc_starter_value(args->profile,L.catalog[i]);
        if(cheapest>FB_ROSTER_BUDGET)snprintf(L.career.message,sizeof L.career.message,"Unlock cheaper starters or reduce Classic costs to fit 55 points.");
    }
    if(args->persistence){int loaded=load_checkpoint();if(loaded<0){L.save_bad=true;L.career.phase=FB_CAREER_ERROR;snprintf(L.career.message,sizeof L.career.message,"Live save unavailable for this profile or unreadable. START offers deletion.");}else if(loaded>0)L.checkpoint=token();}
    ui_set_game_speed(1);ui_set(&live_screen);return true;
}

#ifdef FOOTBALL_LIVE_SCREEN_TEST
const FootballCareer *football_live_screen_test_career(void){return L.scratch?&L.career:NULL;}
#endif
