#include "football_screen.h"
#include "football_career.h"
#include "football_live_art.h"
#include "football_shop_render.h"
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
    gc_run *scratch;
    uint16_t catalog[PR_SPECIES_COUNT];unsigned catalog_count,cursor,member,play_page,preview,ms;
    int asset_offense;bool editing,save_bad,save_failed,leaving,ignore_keys;
    float old_speed,remainder;
    unsigned pending;int receiver;
    uint64_t checkpoint;
    FootballCareerPhase old_phase;
} L;
static unsigned char checkpoint_bytes[FB_CAREER_SAVE_MAX];
static ui_screen live_screen;
bool football_legacy_screen_open(const football_screen_args *args);
static const char *name(unsigned species)
{
    for(unsigned i=0;i<PR_SPECIES_COUNT;++i)if(pr_species_table[i].id==species)return pr_species_table[i].name;
    return "Pokemon";
}
const char *football_live_art_species(unsigned species){return name(species);}
static void text(const char *s,float x,float y,float size,u32 color)
{
    C2D_Text t;C2D_TextParse(&t,L.renderer.text,s);C2D_TextOptimize(&t);C2D_DrawText(&t,C2D_WithColor,x,y,.9f,size,size,color);
}
static u32 ink(void){return C2D_Color32(246,246,242,255);}
static u32 gold(void){return C2D_Color32(255,210,63,255);}
static void panel(float x,float y,float w,float h,bool selected)
{
    C2D_DrawRectSolid(x,y,0,w,h,selected?gold():C2D_Color32(74,80,104,255));
    C2D_DrawRectSolid(x+2,y+2,.01f,w-4,h-4,C2D_Color32(27,37,71,255));
}
static uint64_t token(void)
{
    const FootballCareer *c=&L.career;uint64_t hash=1469598103934665603ull;
    hash=(hash^c->phase)*1099511628211ull;hash=(hash^c->round)*1099511628211ull;hash=(hash^c->wallet)*1099511628211ull;hash=(hash^c->live.snaps)*1099511628211ull;hash=(hash^(unsigned)(c->mega+1))*1099511628211ull;
    for(unsigned i=0;i<22;++i)hash=(hash^c->roster.lineup[i])*1099511628211ull;
    for(unsigned i=0;i<3;++i)hash=(hash^c->sold[i])*1099511628211ull;
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
static void preview(void)
{
    unsigned id=0;const FootballRoster *r=&L.career.roster;
    if(L.career.phase==FB_CAREER_DRAFT && L.catalog_count)id=L.catalog[L.cursor];
    else if(L.career.phase==FB_CAREER_EXP || L.career.phase==FB_CAREER_EVOLVE)id=L.scratch->party[0].b.species;
    else if(r->count)id=r->mon[r->lineup[L.cursor%r->count]].b.species;
    if(!id || id==L.preview)return;
    C3D_FrameSync();football_render_clear_sprites(&L.renderer);menu_mon mon={.species=(uint16_t)id,.gender=-1};football_render_load_front(&L.renderer,0,&mon);L.preview=id;L.asset_offense=-1;
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
    L.asset_offense=m->snap_offense;L.preview=0;
}
static void shop_assets(void)
{
    FootballSeason season={.phase=FB_SEASON_SHOP,.shop_count=3};
    season.shop[0]=(FootballShopOffer){.item=FB_ITEM_CARBOS,.classic_arg=3};
    season.shop[1]=(FootballShopOffer){.item=FB_ITEM_PROTEIN,.classic_arg=1};
    season.shop[2]=(FootballShopOffer){.item=FB_ITEM_MEGA_STONE,.classic_arg=(int16_t)L.career.stone_item};
    football_shop_prepare(&L.shop,&season);
}
/* The playcall has plays, more plays and special teams; the two-point try has no kicks to offer. */
static unsigned pages(void){return L.career.live.conversion?2:3;}
static unsigned option_count(void)
{
    switch(L.career.phase){case FB_CAREER_DRAFT:return L.catalog_count;case FB_CAREER_LINEUP:return 22;case FB_CAREER_EVOLVE:return L.career.evolution_count+1;case FB_CAREER_SHOP:return 4;case FB_CAREER_MATCH:{const FootballLive *m=&L.career.live;return m->rules.offense!=FB_HOME?3:m->conversion==1 || L.play_page==2?2:4;}default:return 1;}
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
        if(!football_roster_add(&c->roster,L.scratch,&s,L.args.profile,&why))snprintf(c->message,sizeof c->message,"%s",why?why:"Cannot draft this Pokemon.");
        else {c->message[0]=0;if(c->roster.count==22){football_roster_autofill(&c->roster);c->phase=FB_CAREER_LINEUP;L.cursor=0;}}break;}
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
    case FB_CAREER_SHOP:if(L.cursor==3){football_career_continue(c);L.cursor=0;}else football_career_buy(c,L.cursor,L.member);break;
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
    if(button==BTN_UP)L.cursor=(L.cursor+count-1)%count;
    else if(button==BTN_DOWN)L.cursor=(L.cursor+1)%count;
    else if(button==BTN_LEFT || button==BTN_RIGHT){int delta=button==BTN_RIGHT?1:-1;
        if(c->phase==FB_CAREER_LINEUP && L.editing){unsigned member=c->roster.lineup[L.cursor];football_roster_assign(&c->roster,L.cursor,(member+22+delta)%22);}
        else if(c->phase==FB_CAREER_DRAFT)L.cursor=(L.cursor+count+(delta>0?6:count-6%count))%count;
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
        if(c->live.rules.offense==FB_AWAY && y>=70 && y<180){
            unsigned nearest=0;float best=100000;
            for(unsigned i=0;i<11;++i){float dx=x-(10+c->live.actors[i].x*2.5f),dy=y-(70+c->live.actors[i].y*2);float d=dx*dx+dy*dy;if(d<best){best=d;nearest=i;}}
            c->live.controlled=(uint8_t)nearest;return true;
        }
        if(c->live.special && c->live.stage!=FB_STAGE_RETURN && c->live.kick_side==FB_HOME){L.pending|=FB_LIVE_ACTION;}
        else if(y>=78 && y<176 && x>=10 && x<310){unsigned column=(unsigned)((x-10)/100),row=(unsigned)((y-78)/49);unsigned target=row*3+column;if(target<5)L.receiver=(int)target;else L.pending|=FB_LIVE_ACTION;}
        else if(y>=186 && y<224){if(x<160)L.pending|=FB_LIVE_JUKE|FB_LIVE_SWITCH;else L.pending|=FB_LIVE_ACTION;}
        return true;
    }
    if(c->phase==FB_CAREER_MATCH){if(y>=40 && y<214){L.cursor=(x>=160?1:0)+(y>=127?2:0);if(L.cursor<option_count())choose_current();}else if(y>=216 && c->live.conversion!=1){L.play_page=(L.play_page+1)%pages();L.cursor=0;}return true;}
    if(c->phase==FB_CAREER_READY){if(y>140 && y<180)cycle_mega(1);else if(y>=184){if(x<160){c->phase=FB_CAREER_LINEUP;L.cursor=0;}else choose_current();}return true;}
    if(c->phase==FB_CAREER_LINEUP && y>=214){if(L.editing){unsigned member=c->roster.lineup[L.cursor];football_roster_assign(&c->roster,L.cursor,(member+(x<160?21:1))%22);return true;}if(x<160)football_roster_autofill(&c->roster);else {c->phase=FB_CAREER_READY;L.editing=false;L.cursor=0;}return true;}
    if(c->phase==FB_CAREER_SHOP && y>=30 && y<65){L.member=(L.member+1)%22;return true;}
    if(y>=42 && y<210){unsigned row=(unsigned)((y-42)/28),page=L.cursor/6;unsigned option=page*6+row;if(option<option_count()){L.cursor=option;choose_current();}}
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
    if(c->phase==FB_CAREER_MATCH || c->phase==FB_CAREER_RESULT){
        if(c->live.phase!=FB_LIVE_DEAD && c->live.phase!=FB_LIVE_END)assets();
        float tx,ty,tvx;football_live_focus(&c->live,&tx,&ty,&tvx);
        football_camera_follow(&L.camera,tx,ty,tvx,(float)ms);
    }else preview();
    if(L.args.persistence && !L.save_bad && football_career_save_ready(c) && token()!=L.checkpoint){
        if(!save_checkpoint()){L.save_failed=true;L.checkpoint=token();} /* retry explicitly from START; avoid frame write loops */
    }
}
static void rows(unsigned count)
{
    FootballCareer *c=&L.career;unsigned first=L.cursor/6*6;
    for(unsigned row=0;row<6 && first+row<count;++row){unsigned i=first+row;char line[100];line[0]=0;
        switch(c->phase){
        case FB_CAREER_DRAFT:{unsigned id=L.catalog[i];bool drafted=false;for(unsigned j=0;j<c->roster.count;++j)if(c->roster.mon[j].b.species==id)drafted=true;
            snprintf(line,sizeof line,"%s%.17s   %.2f PT",drafted?"+ ":"",name(id),gc_starter_value(L.args.profile,id));break;}
        case FB_CAREER_LINEUP:snprintf(line,sizeof line,"%-4s  %.18s",football_slot_names[i],name(c->roster.mon[c->roster.lineup[i]].b.species));break;
        case FB_CAREER_EVOLVE:snprintf(line,sizeof line,"%s",i<c->evolution_count?name(c->evolutions[i]):"Keep current Pokemon");break;
        case FB_CAREER_SHOP:if(i==3)snprintf(line,sizeof line,"Continue to next round");else snprintf(line,sizeof line,"%s  %u  %s",i==0?"CARBOS":i==1?"PROTEIN":"MEGA STONE",c->prices[i],c->sold[i]?"SOLD":i==2 && c->stone_target<0?"NO STOCK":"");break;
        default:break;
        }
        panel(10,42+row*28,300,26,i==L.cursor);text(line,18,46+row*28,.42f,i==L.cursor?gold():ink());
        if(c->phase==FB_CAREER_SHOP && i<3 && L.shop.icons[i])football_render_draw_sprite(L.shop.icons[i],295,55+row*28,.5f,false);
    }
}
static void draw(ui_screen *s,rr_screen which)
{
    (void)s;FootballCareer *c=&L.career;if(!L.renderer.text)return;
    if(!ui_space(which==RR_TOP?UI_PHYS_TOP:UI_PHYS_BOTTOM))return;
    if(which==RR_TOP && (c->phase==FB_CAREER_MATCH || c->phase==FB_CAREER_RESULT)){football_live_art_top(&L.renderer,c,&L.camera,L.ms);return;}
    C2D_TextBufClear(L.renderer.text);float width=which==RR_TOP?400:320;
    C2D_DrawRectSolid(0,0,0,width,240,C2D_Color32(11,13,20,255));
    if(which==RR_TOP){
        if(c->phase==FB_CAREER_DRAFT && !L.catalog_count){text("NO UNLOCKED BASE-FORM STARTERS",18,35,.52f,gold());text("Unlock Pokemon in Classic / Endless first.",18,90,.43f,ink());return;}
        text("POKEMON PLAYOFF",22,14,.8f,gold());text("22 PLAYERS  /  55 POINT DRAFT",24,47,.46f,ink());
        const rr_frame *f=football_render_front_frame(&L.renderer,0,L.ms);if(f && f->src_w && f->src_h)football_render_draw_sprite(f,85,139,fminf(90.0f/f->src_w,90.0f/f->src_h),false);
        unsigned i=c->roster.count?c->roster.lineup[L.cursor%c->roster.count]:0;unsigned id=c->phase==FB_CAREER_DRAFT?L.catalog[L.cursor]:c->roster.mon[i].b.species;
        text(name(id),154,85,.65f,ink());char info[96];
        if(c->phase==FB_CAREER_DRAFT)snprintf(info,sizeof info,"COST %.2f / 55",gc_starter_value(L.args.profile,id));
        else {const FootballAttributes *a=&c->roster.player[i].attributes;snprintf(info,sizeof info,"SPD %u  AGI %u  STR %u",a->spd,a->agi,a->str);}
        text(info,154,120,.43f,gold());
        if(c->roster.count){const FootballAttributes *a=&c->roster.player[i].attributes;snprintf(info,sizeof info,"TAK %u  CTH %u  THA %u",a->tak,a->cth,a->tha);text(info,154,147,.43f,ink());}
        text("ASSIGN YOUR LINEUP. CONTROL EVERY SNAP.",18,210,.43f,ink());return;
    }
    char title[100];
    switch(c->phase){
    case FB_CAREER_DRAFT:snprintf(title,sizeof title,"DRAFT %u/22   %.2f/55 PT",c->roster.count,c->roster.cost);text(title,12,10,.55f,gold());rows(L.catalog_count);text(L.catalog_count<22?"Unlock 22 base forms in Classic / Endless":c->message[0]?c->message:"A: draft  B: undo  Left/Right: page",12,214,.32f,ink());break;
    case FB_CAREER_LINEUP:text(L.editing?"ASSIGN: LEFT/RIGHT SWAPS MEMBER":"LINEUP: A TO EDIT SLOT",12,10,.49f,gold());rows(22);panel(10,214,145,24,false);panel(165,214,145,24,true);text(L.editing?"PREV MEMBER":"AUTO-ASSIGN",18,218,.36f,ink());text(L.editing?"NEXT MEMBER":"B / TAP: READY",173,218,.36f,gold());break;
    case FB_CAREER_READY:
        snprintf(title,sizeof title,"PLAYOFF ROUND %u / 7",c->round);text(title,16,15,.65f,gold());text("11 OFFENSE + 11 DEFENSE",16,55,.48f,ink());
        text("Circle Pad: move    A: snap / tackle",16,87,.40f,ink());text("X/Y/L/R/SELECT: receivers 1-5",16,111,.40f,ink());
        snprintf(title,sizeof title,"MEGA: %s",c->mega<0?"NONE":name(c->roster.mon[c->mega].b.species));panel(10,142,300,36,false);text(title,18,151,.43f,gold());
        panel(10,190,145,40,false);panel(165,190,145,40,true);text("B: LINEUP",20,201,.45f,ink());text("A: KICK OFF",174,201,.45f,gold());break;
    case FB_CAREER_MATCH:
        if(c->live.phase==FB_LIVE_CALL){
            bool home=c->live.rules.offense==FB_HOME,attempt=home && c->live.conversion==1;
            const char *heading=!home?(c->live.conversion?"SET THE DEFENSE FOR THE TRY":"CHOOSE DEFENSIVE SCHEME"):attempt?"TOUCHDOWN!  THE TRY":c->live.conversion?"GO FOR TWO: CHOOSE A PLAY":L.play_page==2?"SPECIAL TEAMS":"CHOOSE YOUR PLAY";
            text(heading,12,8,.51f,gold());
            if(attempt){
                char detail[48];
                snprintf(detail,sizeof detail,"+1   %u%% TO MAKE IT",football_live_extra_point_chance(&c->live));
                football_live_art_special_card(&L.renderer,FB_SPECIAL_EXTRA_POINT,detail,10,40,145,80,L.cursor==0,true);
                football_live_art_special_card(&L.renderer,FB_SPECIAL_NONE,"+2   PLAY IT FROM THE 4",165,40,145,80,L.cursor==1,true);
                snprintf(detail,sizeof detail,"%u - %u after the touchdown",c->live.rules.score[0],c->live.rules.score[1]);
                text(detail,12,134,.4f,gold());
            }else if(home && L.play_page==2){
                char detail[48];
                snprintf(detail,sizeof detail,"Kick it away, then cover the return");
                football_live_art_special_card(&L.renderer,FB_SPECIAL_PUNT,detail,10,40,145,80,L.cursor==0,true);
                bool range=football_live_special_allowed(&c->live,FB_SPECIAL_FIELD_GOAL);
                if(range)snprintf(detail,sizeof detail,"%u YD   %u%% TO MAKE IT",football_live_fg_distance(&c->live),football_live_fg_chance(&c->live));
                else snprintf(detail,sizeof detail,"%u YD: OUT OF RANGE",football_live_fg_distance(&c->live));
                football_live_art_special_card(&L.renderer,FB_SPECIAL_FIELD_GOAL,detail,165,40,145,80,L.cursor==1,range);
                snprintf(detail,sizeof detail,"%s DOWN, %u YD TO GO",c->live.rules.down==1?"1ST":c->live.rules.down==2?"2ND":c->live.rules.down==3?"3RD":"4TH",football_rules_distance(&c->live.rules));
                text(detail,12,134,.4f,gold());
            }else for(unsigned i=0;i<(home?4u:3u);++i){
                if(home)football_live_art_card(&L.renderer,(FootballPlayId)(L.play_page*4+i),10+(i%2)*155,40+(i/2)*87,145,80,i==L.cursor);
                else football_live_art_defense_card(&L.renderer,(FootballDefenseId)i,10+(i%2)*155,40+(i/2)*87,145,80,i==L.cursor);
            }
            text(attempt?"A or tap: choose the try":home?(c->live.conversion?"Left/Right or tap: more plays":"Left/Right or tap: plays / special teams"):"A: set defense, then control your defender",12,219,.34f,ink());
        }else{
            const FootballLive *m=&c->live;
            /* Which controls the human has right now: the kicker, the return team, or the ordinary offense/defense. */
            bool kicking=m->special && m->stage!=FB_STAGE_RETURN && m->kick_side==FB_HOME;
            bool covering=m->special && ((m->stage==FB_STAGE_RETURN && m->rules.offense==FB_AWAY));
            bool receiving=m->special && ((m->stage!=FB_STAGE_RETURN && m->kick_side==FB_AWAY) || (m->stage==FB_STAGE_RETURN && m->rules.offense==FB_HOME));
            text(m->phase==FB_LIVE_SET?"A: SNAP / TOUCH SNAP":"LIVE PLAY",12,10,.6f,gold());
            bool free_kick=m->special==FB_SPECIAL_PUNT || m->special==FB_SPECIAL_KICKOFF;
            if(kicking)text(m->special==FB_SPECIAL_PUNT?"Circle Pad up/down aims the punt":m->special==FB_SPECIAL_KICKOFF?"Circle Pad up/down aims the kick":"The kicker takes the snap and kicks",12,44,.4f,ink());
            else if(receiving && free_kick)text(m->stage==FB_STAGE_RETURN?"Circle Pad: run   B: juke":m->special==FB_SPECIAL_KICKOFF?"Circle Pad: get under it, then run it back":"Circle Pad: get under it   A: fair catch",12,44,.4f,ink());
            else if(receiving)text("Circle Pad: rush the kicker   A: dive   B: switch",12,44,.4f,ink());
            else if(covering || m->special)text("Circle Pad: chase the returner   B: switch   A: dive",12,44,.4f,ink());
            else text(m->rules.offense==FB_HOME?"Circle Pad: run   B: juke":"Circle Pad: move   B: switch defender",12,44,.4f,ink());
            if(kicking){
                panel(10,78,300,98,false);text(football_live_art_special_name((FootballSpecial)m->special),20,84,.55f,gold());
                char info[48];if(m->special==FB_SPECIAL_FIELD_GOAL)snprintf(info,sizeof info,"%u YD   %u%% TO MAKE IT",football_live_fg_distance(m),football_live_fg_chance(m));
                else if(m->special==FB_SPECIAL_EXTRA_POINT)snprintf(info,sizeof info,"%u%% TO MAKE IT",football_live_extra_point_chance(m));
                else snprintf(info,sizeof info,m->special==FB_SPECIAL_KICKOFF?"Tap or press A to kick off":"Tap or press A to snap");
                text(info,20,118,.42f,ink());text(m->special==FB_SPECIAL_KICKOFF?"A  KICK":"A  SNAP",20,148,.45f,gold());
            }else if(m->rules.offense==FB_HOME && !m->special){
                const char *const targets[]={"1  X","2  Y","3  L","4  R","5 SELECT","A  SNAP"};
                static const unsigned slots[5]={2,3,4,5,1};
                for(unsigned i=0;i<6;++i){float x=10+(i%3)*100,y=78+(i/3)*49;panel(x,y,94,43,false);text(targets[i],x+7,y+5,.42f,ink());
                    if(i<5){unsigned member=c->live.actors[slots[i]].roster;char mon[15];snprintf(mon,sizeof mon,"%.12s",name(c->roster.mon[member].b.species));text(mon,x+5,y+25,.29f,gold());}
                }
            }else{
                C2D_DrawRectSolid(10,70,0,300,108,C2D_Color32(42,128,52,255));
                for(unsigned i=0;i<22;++i){const FootballLiveActor *a=&c->live.actors[i];float x=10+a->x*2.5f,y=70+a->y*2;
                    C2D_DrawRectSolid(x-2,y-2,.1f,5,5,i==c->live.controlled?gold():a->home?C2D_Color32(76,201,240,255):C2D_Color32(232,53,74,255));}
                text("Tap map to select any player",16,164,.30f,ink());
            }
            bool offense=kicking || (!m->special && m->rules.offense==FB_HOME);
            bool fair=receiving && m->special==FB_SPECIAL_PUNT && m->stage!=FB_STAGE_RETURN;
            const char *kick_label=m->special==FB_SPECIAL_KICKOFF?"A: KICK":"A: SNAP";
            panel(10,186,145,38,false);panel(165,186,145,38,true);text(offense?"B: JUKE":"B: SWITCH",20,195,.45f,ink());text(offense?kick_label:fair?"A: FAIR CATCH":receiving && free_kick && m->stage!=FB_STAGE_RETURN?"":"A: DIVE",174,195,.45f,gold());
        }break;
    case FB_CAREER_RESULT:{
        bool won=c->live.rules.winner==FB_HOME;char gain[96];
        text(won?"MATCH WON":"PLAYOFF ELIMINATION",14,30,.65f,gold());text("A / Tap: team progression",14,90,.47f,ink());
        snprintf(gain,sizeof gain,"SPECIES CANDY: +%u FROM PLAYS",c->candy_plays);text(gain,14,132,.42f,gold());
        snprintf(gain,sizeof gain,"+%u MORE FOR THE TEAM",22*(FB_CANDY_PLAYED+(won?FB_CANDY_WIN:0)));text(gain,14,156,.42f,ink());
        break;}
    case FB_CAREER_EXP:snprintf(title,sizeof title,"EXP: PLAYER %u / 22",c->exp_member+1);text(title,14,30,.6f,gold());text("Applying existing level-up and evolution logic",14,90,.35f,ink());break;
    case FB_CAREER_EVOLVE:text("CHOOSE EVOLUTION",12,10,.56f,gold());rows(c->evolution_count+1);break;
    case FB_CAREER_SHOP:snprintf(title,sizeof title,"SHOP   CASH %u",c->wallet);text(title,12,8,.55f,gold());rows(4);snprintf(title,sizeof title,"TARGET %u: %.16s",L.member+1,name(c->roster.mon[L.member].b.species));text(title,12,185,.40f,gold());text("Left/Right: target    A: buy / continue",12,214,.34f,ink());break;
    case FB_CAREER_END:snprintf(title,sizeof title,"PLAYOFF COMPLETE: %u / 7 WINS",c->wins);text(title,12,35,.54f,gold());text("A / Tap: return to title",12,95,.45f,ink());
        snprintf(title,sizeof title,"SPECIES CANDY THIS MATCH: +%u",c->candy_plays);text(title,12,132,.42f,gold());break;
    case FB_CAREER_LEARN:break; /* level-up moves are learned without a prompt */
    case FB_CAREER_ERROR:text("CHECKPOINT PRESERVED",12,30,.54f,gold());text(c->message,12,80,.33f,ink());break;
    }
    if(L.save_failed)text("SAVE FAILED: START TO RETRY OR RETURN",12,230,.32f,C2D_Color32(255,110,95,255));
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
