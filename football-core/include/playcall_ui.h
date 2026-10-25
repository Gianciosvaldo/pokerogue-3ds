#ifndef PLAYCALL_UI_H
#define PLAYCALL_UI_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { FB_FORM_SHOTGUN, FB_FORM_UNDER_CENTER } FootballFormation;
typedef enum { FB_PLAY_RUN, FB_PLAY_PASS } FootballPlayType;
typedef enum { FB_CALL_MAIN, FB_CALL_FORMATION, FB_CALL_TYPE, FB_CALL_PLAY, FB_CALL_READY } FootballCallState;
typedef enum { FB_RUN_INSIDE_ZONE, FB_PASS_SLANTS, FB_RUN_DIVE, FB_PASS_PLAY_ACTION,
    FB_RUN_DRAW, FB_PASS_VERTICALS, FB_RUN_TOSS, FB_PASS_POST, FB_PLAY_COUNT } FootballPlayId;
typedef struct PlayInfo {
    FootballFormation formation;
    FootballPlayType type;
    FootballPlayId id;
    uint8_t target_slot; /* Engine can resolve the eligible receiver/runner. */
} PlayInfo;
typedef struct { int x,y,w,h; } FootballTouchRect;
enum { FB_INPUT_ACCEPT=1, FB_INPUT_BACK=2, FB_INPUT_UP=4, FB_INPUT_DOWN=8 };
typedef struct {
    unsigned pressed;
    bool touch_down;
    uint16_t touch_x, touch_y;
} FootballCallInput;
typedef struct {
    FootballCallState state;
    unsigned highlighted;
    FootballFormation formation;
    FootballPlayType type;
    PlayInfo pending;
} FootballPlaycall;
void football_playcall_reset(FootballPlaycall *ui);
void football_playcall_update(FootballPlaycall *ui, FootballCallInput input);
bool football_playcall_take(FootballPlaycall *ui, PlayInfo *out);
unsigned football_playcall_option_count(const FootballPlaycall *ui);
const char *football_playcall_title(const FootballPlaycall *ui);
const char *football_playcall_label(const FootballPlaycall *ui, unsigned option);
FootballTouchRect football_playcall_rect(unsigned option);
#ifdef __3DS__
/* Call after the application's single hidScanInput per frame. */
FootballCallInput football_playcall_read_input(void);
void football_playcall_poll(FootballPlaycall *ui);
#endif
#endif
