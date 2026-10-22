#include "playcall_ui.h"
#include <string.h>
#ifdef __3DS__
#include <3ds.h>
#endif
void football_playcall_reset(FootballPlaycall *ui)
{
    if (ui) memset(ui,0,sizeof *ui);
}
unsigned football_playcall_option_count(const FootballPlaycall *ui)
{
    if (!ui) return 0;
    switch (ui->state) {
    case FB_CALL_MAIN: return 1;
    case FB_CALL_FORMATION: case FB_CALL_TYPE: return 2;
    default: return 0;
    }
}
const char *football_playcall_title(const FootballPlaycall *ui)
{
    if (!ui) return "";
    switch (ui->state) {
    case FB_CALL_MAIN: return "Playcalling";
    case FB_CALL_FORMATION: return "Choose formation";
    case FB_CALL_TYPE: return "Choose play type";
    case FB_CALL_READY: return "Play selected";
    }
    return "";
}
const char *football_playcall_label(const FootballPlaycall *ui, unsigned option)
{
    if (option>=football_playcall_option_count(ui)) return "";
    switch (ui->state) {
    case FB_CALL_MAIN: return "Offense";
    case FB_CALL_FORMATION: return option ? "Under Center" : "Shotgun";
    case FB_CALL_TYPE: return option ? "Pass" : "Run";
    default: return "";
    }
}
FootballTouchRect football_playcall_rect(unsigned option)
{
    if (option>1) return (FootballTouchRect){0,0,0,0};
    return (FootballTouchRect){20,60+(int)option*64,280,52};
}
static bool inside(FootballTouchRect r, unsigned x, unsigned y)
{
    return (int)x>=r.x && (int)y>=r.y && (int)x<r.x+r.w && (int)y<r.y+r.h;
}
void football_playcall_update(FootballPlaycall *ui, FootballCallInput input)
{
    if (!ui) return;
    if (input.pressed&FB_INPUT_BACK) {
        if (ui->state==FB_CALL_FORMATION) ui->state=FB_CALL_MAIN;
        else if (ui->state==FB_CALL_TYPE || ui->state==FB_CALL_READY) ui->state=FB_CALL_FORMATION;
        ui->highlighted=0;
        return;
    }
    unsigned count=football_playcall_option_count(ui);
    if (!count) return;
    if (ui->highlighted>=count) ui->highlighted=0;
    if (input.pressed&FB_INPUT_UP) ui->highlighted=(ui->highlighted+count-1)%count;
    else if (input.pressed&FB_INPUT_DOWN) ui->highlighted=(ui->highlighted+1)%count;
    bool activate=(input.pressed&FB_INPUT_ACCEPT)!=0;
    if (input.touch_down) {
        activate=false;
        if (input.touch_x<320 && input.touch_y<240)
            for (unsigned i=0;i<count;++i)
                if (inside(football_playcall_rect(i),input.touch_x,input.touch_y)) {
                    ui->highlighted=i; activate=true; break;
                }
    }
    if (!activate) return;
    switch (ui->state) {
    case FB_CALL_MAIN: ui->state=FB_CALL_FORMATION; break;
    case FB_CALL_FORMATION:
        ui->formation=(FootballFormation)ui->highlighted;
        ui->state=FB_CALL_TYPE;
        break;
    case FB_CALL_TYPE:
        ui->pending.formation=ui->formation;
        ui->pending.type=(FootballPlayType)ui->highlighted;
        ui->pending.id=(FootballPlayId)(ui->formation*2+ui->highlighted);
        ui->pending.target_slot=ui->pending.type==FB_PLAY_PASS ? 1 : 2;
        ui->state=FB_CALL_READY;
        break;
    default: break;
    }
    ui->highlighted=0;
}
bool football_playcall_take(FootballPlaycall *ui, PlayInfo *out)
{
    if (!ui || !out || ui->state!=FB_CALL_READY) return false;
    *out=ui->pending;
    football_playcall_reset(ui);
    return true;
}
#ifdef __3DS__
FootballCallInput football_playcall_read_input(void)
{
    u32 keys=hidKeysDown();
    FootballCallInput input={0};
    if (keys&KEY_A) input.pressed|=FB_INPUT_ACCEPT;
    if (keys&KEY_B) input.pressed|=FB_INPUT_BACK;
    if (keys&KEY_UP) input.pressed|=FB_INPUT_UP;
    if (keys&KEY_DOWN) input.pressed|=FB_INPUT_DOWN;
    if (keys&KEY_TOUCH) {
        touchPosition touch;
        hidTouchRead(&touch);
        input.touch_down=true; input.touch_x=touch.px; input.touch_y=touch.py;
    }
    return input;
}
void football_playcall_poll(FootballPlaycall *ui)
{
    football_playcall_update(ui,football_playcall_read_input());
}
#endif
