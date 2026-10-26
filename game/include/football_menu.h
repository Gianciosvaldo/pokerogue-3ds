#ifndef FOOTBALL_MENU_H
#define FOOTBALL_MENU_H
#include <stdbool.h>
#include "ui_text.h"
/* The football playoff's menus in the classic game's look: the grey bevelled windows, the emerald font, the option list's cursor arrow
 * and scroll bar, the save-slot list's yellow frame for a chosen button. Plain drawing with the widgets the rest of the game uses (the
 * screen decides what to show, football_live_screen.c); nothing here keeps state. Physical pixels, so the caller is in
 * ui_space(UI_PHYS_BOTTOM) or ui_space(UI_PHYS_TOP). */

/* The bottom screen (320 x 240): a header, a list of FM_ROWS rows, a hint bar. The same slots serve every menu, so a tap lands on the same
 * row wherever the menu is. */
enum {
    FM_HEAD_X=4,FM_HEAD_Y=2,FM_HEAD_W=312,FM_HEAD_H=28,
    FM_LIST_X=4,FM_LIST_Y=32,FM_LIST_W=312,FM_LIST_H=176,
    FM_FOOT_X=4,FM_FOOT_Y=210,FM_FOOT_W=312,FM_FOOT_H=28,
    FM_ROWS=9,FM_ROW_PITCH=18,FM_LIST_PAD=7, /* the first row's text sits this far under the window's top */
    FM_TEXT_PAD=7                             /* and the text of a one-line window this far under its top */
};
/* Canvas sizes of the two text sizes the menus use (ui_text.h: 96 = 16 logical px, 72 = 12). */
enum { FM_PX_BODY=96,FM_PX_SMALL=72,FM_PX_TINY=60 };

typedef struct {
    const char *label,*value; /* value is drawn at the right edge; NULL for none */
    const char *tag;          /* a short orange caption before the label (a lineup slot); NULL for none */
    text_style value_style;   /* TS_NONE = TS_WINDOW */
    bool dim;                 /* greyed out like a disabled option */
    float indent;             /* the label starts this much further right (room for an icon) */
} FootballMenuRow;
/* Where a row's text starts (after the cursor) and how wide a tag's column is. */
enum { FM_LABEL_X=22,FM_TAG_W=44 };

/* The bottom screen's background. */
void football_menu_bottom(void);
/* The header: left in white, right in gold. */
void football_menu_head(const char *left,const char *right);
/* The list window with up to FM_ROWS rows. selected is the row the cursor is on (-1 for none); first is the index of rows[0] in the whole list
 * of total, for the scroll bar. */
void football_menu_list(const FootballMenuRow *rows,unsigned count,int selected,unsigned first,unsigned total);
/* The hint bar along the bottom: text shrinks to fit. */
void football_menu_foot(const char *text);
/* A button window with its label centred; selected gets the yellow frame, !enabled greys the label. */
void football_menu_button(float x,float y,float w,float h,const char *label,bool selected,bool enabled);
/* A plain window. */
void football_menu_panel(float x,float y,float w,float h);
/* The yellow frame around a window (just outside its edge) that marks the one chosen. */
void football_menu_select(float x,float y,float w,float h);
/* Text at (x, y) that never runs past max_w: the size steps down (to 6 logical px) until it fits. Returns the width drawn. */
float football_menu_text(text_style style,int px,float x,float y,float max_w,const char *s);
float football_menu_text_right(text_style style,int px,float right,float y,float max_w,const char *s);
float football_menu_text_center(text_style style,int px,float center,float y,float max_w,const char *s);
/* Word-wrapped text in a w-wide column, at most max_lines lines. */
void football_menu_wrap(text_style style,int px,float x,float y,float w,int max_lines,const char *s);
/* A stat bar: a dark trough with a fill of value/max, green through gold to red toward empty. */
void football_menu_bar(float x,float y,float w,float h,unsigned value,unsigned max);
/* The list row (0..FM_ROWS-1) at a tap, or -1 when the tap is not on one. */
int football_menu_row_at(float x,float y);
/* The top screen (400 x 240): an arena background, picked by the round, behind the windows the caller draws. */
void football_menu_top_bg(unsigned round);
#endif
