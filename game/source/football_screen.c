#include "football_screen.h"
#include "football_mode.h"
#include "football_saves.h"
#include "football_playbook.h"
#include "football_match_view.h"
#include "football_shop_render.h"
#include "starter_host.h"
#include "ui.h"
#include "ui_widgets.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    football_screen_args args;
    FootballMode mode;
    FootballRenderer renderer;
    FootballShopView shop_view;
    FootballRenderScene scene;
    gc_run *roster; /* Allocated once per entry; never on the main-thread stack. */
    bool leaving, save_failed, exit_requested, defer_save, save_blocked, defer_checkpoint;
    uint64_t saved_token, attempted_token;
    unsigned assets_round;
    FootballModePhase previous;
    uint32_t input_frame, animation_ms;
    float old_speed, fractional_ms;
} F;
static ui_screen screen;
static const char *const quit_options[]={"Keep playing","End playoff and return to title"};
static const char *const saved_quit_options[]={"Keep playing","Save and return to title","Delete football save and return"};
static const char *const delete_options[]={"Keep playoff","Delete football save"};
static const char *const unsaved_options[]={"Keep playing","Return without saving latest changes"};

static void dispose(void)
{
    C3D_FrameSync(); /* Do not free textures still sampled by the previous GPU frame. */
    football_render_free(&F.renderer);
    free(F.roster);F.roster=NULL;
    ui_set_game_speed(F.old_speed);
}
static void depart(void)
{
    F.leaving=true;
    if (F.args.on_exit) F.args.on_exit(F.args.arg);
}
static bool save_profile(void)
{
    if (!F.mode.profile_dirty) return true;
    if (F.args.save_profile && !F.args.save_profile(F.args.arg)) return false;
    F.mode.profile_dirty=false;return true;
}
static uint64_t checkpoint_token(void)
{
    const FootballSeason *s=&F.mode.season;
    uint64_t token=1+(unsigned)F.mode.phase+32*s->round+512*(unsigned)(s->mega_slot+1);
    token+=4096ull*F.mode.session.match.plays+4194304ull*s->wallet;
    for(unsigned i=0;i<s->shop_count && i<FB_SHOP_CAP;++i)if(s->shop[i].sold)token^=1ull<<(56+i);
    return token;
}
static bool save_all(void)
{
    if(!save_profile())return false;
    if(F.args.persistence && !F.save_blocked) {
        if(!football_save_ready(&F.mode) || !football_saves_write(&F.mode))return false;
        F.saved_token=checkpoint_token();F.defer_checkpoint=false;
    }
    return true;
}
static void delete_answer(int option,void *arg)
{
    (void)arg;
    if(option!=1)return;
    if(!football_saves_clear()) {
        static const char *const failed[]={"Football save could not be deleted. Try again."};
        ui_push_options(failed,1,true,NULL,NULL);return;
    }
    depart();
}
static void not_ready(void)
{
    static const char *const wait[]={"Finish this play or team progression before saving."};
    ui_push_options(wait,1,true,NULL,NULL);
}
static void attempt_exit(void)
{
    if(F.mode.phase==FB_MODE_ERROR){depart();return;}
    if(F.args.persistence && !F.save_blocked && !football_save_ready(&F.mode)) { not_ready();return; }
    if (!save_all()) { F.save_failed=true;F.exit_requested=true;return; }
    depart();
}
static void quit_answer(int option,void *arg)
{
    (void)arg;
    if (option==1) attempt_exit();
    else if(option==2 && F.args.persistence) {
        if(!F.save_blocked && !football_save_ready(&F.mode)){not_ready();return;}
        ui_push_options(delete_options,2,true,delete_answer,NULL);
    }
}
static void unsaved_answer(int option,void *arg)
{
    (void)arg;
    F.save_failed=false;F.exit_requested=false;F.defer_save=true;F.defer_checkpoint=true;
    if (option==1) depart();
}
static bool validate_team(const gc_starter *team,int count,void *arg,const char **why)
{
    (void)arg;
    return gc_football_validate_draft(F.args.profile,team,count,why);
}
static bool starter_save(void *arg)
{
    (void)arg;
    return !F.args.save_profile || F.args.save_profile(F.args.arg);
}
static void drafted(const gc_starter *team,int count,void *arg)
{
    (void)arg;
    const char *why=NULL;
    if (!football_mode_draft(&F.mode,team,count,&why)) {
        F.mode.phase=FB_MODE_ERROR;
        snprintf(F.mode.message,sizeof F.mode.message,"%s",why ? why : "The draft could not start.");
    }
    ui_set(&screen);
}
static void draft_exit(void *arg)
{
    (void)arg;
    dispose();depart();
}
static void entered(ui_screen *s)
{
    (void)s;
    if(F.mode.phase==FB_MODE_SHOP)football_shop_prepare(&F.shop_view,&F.mode.season);
    if (!football_render_init(&F.renderer)) {
        F.mode.phase=FB_MODE_ERROR;
        snprintf(F.mode.message,sizeof F.mode.message,"Not enough graphics memory to start football.");
    }
}
static void left(ui_screen *s)
{
    (void)s;dispose();
}
static void load_round(void)
{
    C3D_FrameSync();
    football_render_clear_sprites(&F.renderer);
    for (unsigned side=0;side<2;++side) {
        const FootballTeam *team=&F.mode.session.match.teams[side];
        for (unsigned i=0;i<team->count;++i) {
            menu_mon mon={0};
            mon.species=team->players[i].species_id;mon.form=team->players[i].form_index;mon.gender=-1;
            if (side==FB_HOME) {
                const gc_mon *r=&F.roster->party[i];
                mon.gender=r->b.gender;mon.shiny=r->b.shiny;mon.variant=r->variant;
            }
            /* Missing/over-budget art uses the renderer's existing team markers. */
            football_render_load_front(&F.renderer,side*FB_MATCH_TEAM_CAP+i,&mon);
        }
    }
    F.assets_round=F.mode.season.round;
}
static void update(ui_screen *s,float dt)
{
    (void)s;
    if (ui_top()!=&screen || ui_busy() || F.leaving || F.save_failed) return;
    if (!isfinite(dt) || dt<0) dt=0;
    F.fractional_ms+=dt>250 ? 250 : dt;
    uint32_t ms=(uint32_t)F.fractional_ms;F.fractional_ms-=ms;F.animation_ms+=ms;
    if (F.mode.phase==FB_MODE_PLAY && F.assets_round!=F.mode.season.round) load_round();
    football_mode_tick(&F.mode,ms);
    if (F.mode.phase!=F.previous) {
        if (F.mode.phase==FB_MODE_SHOP) football_shop_prepare(&F.shop_view,&F.mode.season);
        if (F.mode.phase!=FB_MODE_SHOP) F.defer_save=false;
        F.previous=F.mode.phase;
    }
    if(F.args.persistence && !F.save_blocked && football_save_ready(&F.mode)) {
        uint64_t token=checkpoint_token();
        if(token!=F.saved_token && (!F.defer_checkpoint || token!=F.attempted_token)) {
            F.attempted_token=token;if(!save_all())F.save_failed=true;
        }
    } else if (!F.args.persistence && F.mode.phase==FB_MODE_SHOP && !F.defer_save && !save_profile()) F.save_failed=true;
    if (F.mode.session.match.teams[0].count)
        football_match_scene(&F.scene,&F.mode.session.match,&F.renderer,F.animation_ms);
}
static bool dispatch(FootballCallInput input,int target_step,bool menu)
{
    uint32_t frame=ui_frame();
    if (F.input_frame==frame || F.leaving) return true;
    F.input_frame=frame;
    if (F.save_failed) {
        if (input.pressed&FB_INPUT_ACCEPT) {
            if (save_all()) {
                F.save_failed=false;
                if (F.exit_requested) depart();
            }
        } else if (input.pressed&FB_INPUT_BACK) {
            if (F.exit_requested) ui_push_options(unsaved_options,2,true,unsaved_answer,NULL);
            else { F.save_failed=false;F.defer_save=true;F.defer_checkpoint=true; }
        }
        return true;
    }
    if (menu || ((input.pressed&FB_INPUT_BACK) && F.mode.phase==FB_MODE_ROUND)) {
        ui_push_options(F.args.persistence ? saved_quit_options : quit_options,F.args.persistence ? 3 : 2,true,quit_answer,NULL);return true;
    }
    football_mode_input(&F.mode,input,target_step);
    if (F.mode.done) attempt_exit();
    return true;
}
static bool input(ui_screen *s,ui_button button)
{
    (void)s;
    if (ui_input_repeating() && button!=BTN_UP && button!=BTN_DOWN && button!=BTN_LEFT && button!=BTN_RIGHT) return true;
    FootballCallInput in={0};int target=0;
    if (button==BTN_ACTION || button==BTN_SUBMIT) in.pressed=FB_INPUT_ACCEPT;
    else if (button==BTN_CANCEL) in.pressed=FB_INPUT_BACK;
    else if (button==BTN_UP) in.pressed=FB_INPUT_UP;
    else if (button==BTN_DOWN) in.pressed=FB_INPUT_DOWN;
    else if (button==BTN_LEFT) target=-1;
    else if (button==BTN_RIGHT) target=1;
    else if (button!=BTN_MENU) return false;
    return dispatch(in,target,button==BTN_MENU);
}
static bool touch(ui_screen *s,float x,float y)
{
    (void)s;
    if (!isfinite(x) || !isfinite(y) || x<0 || x>=320 || y<0 || y>=240) return false;
    return dispatch((FootballCallInput){.touch_down=true,.touch_x=(uint16_t)x,.touch_y=(uint16_t)y},0,false);
}
static void text(float x,float y,const char *value,float scale)
{
    rr_text_sys(x,y,scale,value,RR_RGBA(255,255,255,255));
}
static void paragraph(const char *value,float y)
{
    for (unsigned line=0;*value && line<6;++line) {
        char row[40];size_t n=strlen(value);if(n>36)n=36;
        if (value[n]) { size_t split=n;while(split>0 && value[split]!=' ')--split;if(split)n=split; }
        memcpy(row,value,n);row[n]=0;text(12,y+line*20,row,.45f);
        value+=n;while(*value==' ')++value;
    }
}
static void button(void)
{
    rr_rect(12,164,296,52,RR_RGBA(40,100,170,255));text(24,180,"A / Tap: Continue",.5f);
}
static void draw(ui_screen *s,rr_screen which)
{
    (void)s;
    if (!ui_space(which==RR_TOP ? UI_PHYS_TOP : UI_PHYS_BOTTOM)) return;
    if (which==RR_TOP) {
        football_render_top(&F.renderer,&F.scene);
        if (F.mode.phase==FB_MODE_ROUND) {
            rr_rect(12,60,376,158,RR_RGBA(12,22,30,240));
            char line[96];snprintf(line,sizeof line,"Playoff round %u of %u",F.mode.season.round,FB_PLAYOFF_ROUNDS);
            text(20,64,line,.65f);
            static const char *const positions[]={"QB","WR","RB","OL/DL","LB","TE/S"};
            for (int i=0;i<F.roster->n_party;++i) {
                const FootballPlayer *p=&F.mode.season.players[i];const char *name="Pokemon";
                for(unsigned k=0;k<PR_SPECIES_COUNT;++k)if(pr_species_table[k].id==p->species_id){name=pr_species_table[k].name;break;}
                snprintf(line,sizeof line,"%s  %s  SPD %u STR %u",name,(unsigned)p->position<FB_POSITION_COUNT ? positions[p->position] : "",p->attributes.spd,p->attributes.str);
                text(20,94+18*i,line,.4f);
            }
        }
        return;
    }
    rr_rect(0,0,320,240,RR_RGBA(12,22,30,255));
    if (F.save_failed) {
        text(12,20,"Save failed",.6f);
        paragraph(F.exit_requested ? "A: retry saving. B: choose whether to return without saving latest changes." :
            "A: retry saving. B: keep playing; the next checkpoint will retry.",64);
        return;
    }
    if (F.mode.phase==FB_MODE_PLAY && football_match_waiting_for_home(&F.mode.session.match))
        football_render_bottom(&F.renderer,&F.mode.playcall);
    else if (F.mode.phase==FB_MODE_EVOLVE) football_render_evolution_bottom(&F.renderer,&F.mode.evolution);
    else if (F.mode.phase==FB_MODE_SHOP) football_shop_draw(&F.renderer,&F.shop_view,&F.mode.season,&F.mode.shop);
    else if (F.mode.phase==FB_MODE_LEARN) {
        char title[96];snprintf(title,sizeof title,"Learn %s?",football_mode_new_move_name(&F.mode));paragraph(title,10);
        unsigned count=football_mode_move_count(&F.mode);
        for(unsigned i=0;i<count;++i){
            FootballTouchRect r=football_mode_move_rect(i);
            rr_rect(r.x,r.y,r.w,r.h,i==F.mode.move_choice ? RR_RGBA(40,100,170,255) : RR_RGBA(35,48,65,255));
            text(r.x+8,r.y+4,football_mode_move_name(&F.mode,i),.4f);
        }
        text(12,216,"A: replace move   B: do not learn",.35f);
    } else if (F.mode.phase==FB_MODE_PLAY) {
        text(12,20,F.mode.session.match.phase==FB_MATCH_CALL ? "Opponent playcall" : "Play in progress",.6f);
        if (F.mode.session.match.phase==FB_MATCH_ANIMATION) {
            const FootballPlayDefinition *play=football_playbook_get(F.mode.session.match.play.id);
            if (play) text(12,52,play->name,.5f);
            paragraph(football_match_result_name(F.mode.session.match.resolved.kind),82);
            char yards[40];snprintf(yards,sizeof yards,"%+d yards",F.mode.session.match.resolved.yards);
            text(12,118,yards,.6f);
            const FootballMatch *m=&F.mode.session.match;
            if (m->contact.fumble) text(12,148,"Contact: fumble lost",.42f);
            else if (m->contact.broken) text(12,148,"Broken tackle: extra yards",.42f);
            else if (m->contact.attempted) text(12,148,m->contact.big_hit ? "Big hit" : "Tackled",.42f);
            if (m->blocking.pancake) text(12,172,"Pancake block",.42f);
        }
    } else {
        const char *title=F.mode.phase==FB_MODE_ROUND ? "Ready for the next match" :
            F.mode.phase==FB_MODE_RESULT ? "Match result" : F.mode.phase==FB_MODE_END ?
            (F.mode.season.phase==FB_SEASON_CLEAR ? "Playoff champions!" : "Playoff run ended") :
            F.mode.phase==FB_MODE_ERROR ? "Unable to continue" : "Team progress";
        text(12,20,title,.55f);
        if(F.mode.phase==FB_MODE_ROUND) {
            paragraph("Choose plays to win seven matches. Select one owned mega stone holder, or none.",64);
            char mega[96];int slot=F.mode.season.mega_slot;
            const pr_form *form=slot<0 ? NULL : gc_football_owned_mega(F.roster,(unsigned)slot);
            const char *name="None";
            if (form) for (unsigned i=0;i<PR_SPECIES_COUNT;++i)
                if (pr_species_table[i].id==form->species) { name=pr_species_table[i].name;break; }
            snprintf(mega,sizeof mega,"Mega: %s%s%s",name,form ? " " : "",form ? form->form_name : "");
            rr_rect(12,132,296,26,RR_RGBA(35,48,65,255));text(20,136,mega,.42f);
            text(12,116,"Left/Right or tap to choose",.35f);
        }
        else if(F.mode.phase==FB_MODE_END){char note[128];snprintf(note,sizeof note,"Won %u of 7 matches. Continue to return to the title.",F.mode.season.wins);paragraph(note,64);}
        else paragraph(F.mode.message,64);
        if (F.mode.phase!=FB_MODE_EVENTS) button();
    }
    if (F.mode.phase!=FB_MODE_LEARN && F.mode.phase!=FB_MODE_SHOP && F.mode.phase!=FB_MODE_EVOLVE) text(12,226,F.args.persistence ? "START: save / return / delete playoff" : "START: leave playoff (not saved)",.32f);
}
static ui_screen screen={.name="football-playoff",.flags=UI_FADE,.enter=entered,.leave=left,
    .input=input,.update=update,.draw=draw,.touch=touch};
bool football_screen_open(const football_screen_args *args)
{
    if (!args || !args->on_exit || F.roster) return false;
    memset(&F,0,sizeof F);F.args=*args;F.old_speed=ui_game_speed();F.input_frame=UINT32_MAX;
    F.roster=calloc(1,sizeof *F.roster);
    if (!F.roster) return false;
    if (!football_mode_new(&F.mode,F.roster,args->profile,args->entropy)) { free(F.roster);F.roster=NULL;return false; }
    F.scene=(FootballRenderScene){.quarter=1,.down=1,.distance=10,.seconds_left=FB_QUARTER_SECONDS,.ball_x_yards=35};
    F.previous=FB_MODE_DRAFT;ui_set_game_speed(1);
    if(args->persistence) {
        bool backup=false;
        FootballFileStatus status=football_saves_load(&F.mode,F.roster,args->profile,&backup);
        if(status==FB_FILE_OK) {
            F.saved_token=checkpoint_token();F.previous=FB_MODE_DRAFT;
            ui_set(&screen);
            static const char *const resume[]={"Resume saved football playoff"};
            static const char *const recovered[]={"Resume football playoff recovered from backup"};
            ui_push_options(backup ? recovered : resume,1,true,NULL,NULL);return true;
        }
        if(status==FB_FILE_BAD) {
            football_mode_new(&F.mode,F.roster,args->profile,args->entropy);
            F.save_blocked=true;F.mode.phase=FB_MODE_ERROR;
            snprintf(F.mode.message,sizeof F.mode.message,"Football save cannot be read. It has been preserved. START offers explicit deletion.");
            ui_set(&screen);return true;
        }
    }
    const sh_events events={.on_start=drafted,.on_exit=draft_exit,.save_profile=starter_save,.validate_team=validate_team};
    char notice[400];
    snprintf(notice,sizeof notice,"Football Playoff: draft base-form Pokemon. %s%s%s",
        args->persistence ? "Checkpoints save before snaps, between rounds and in the shop." : "This playoff cannot be saved or resumed.",
        args->profile_notice ? " " : "",args->profile_notice ? args->profile_notice : "");
    if (!sh_open(args->profile,notice,&events)) {
        dispose();return false;
    }
    return true;
}

#ifdef FOOTBALL_SCREEN_TEST
const FootballMode *football_screen_test_mode(void) { return F.roster ? &F.mode : NULL; }
#endif
