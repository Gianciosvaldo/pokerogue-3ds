#include <math.h>
#include "football_menu.h"
#include "menu_fakes.h"
/* football_menu (game/source/football_menu.c) on the host, over the recording fakes of the classic UI's drawing calls (menu_fakes.h). */
static unsigned texts_with(const char *s){unsigned n=0;for(unsigned i=0;i<fake.text_count;++i)n+=!strcmp(fake.texts[i].text,s);return n;}
static const FakeText *text_of(const char *s){for(unsigned i=0;i<fake.text_count;++i)if(!strcmp(fake.texts[i].text,s))return &fake.texts[i];return NULL;}
static void bottom(void){fake_begin(RR_BOTTOM);assert(ui_space(UI_PHYS_BOTTOM));}
int main(void)
{
    /* The slots of the bottom screen are inside it and clear of each other: header, list, hint bar. */
    assert(FM_HEAD_X>=0 && FM_HEAD_X+FM_HEAD_W<=320 && FM_LIST_X+FM_LIST_W<=320 && FM_FOOT_X+FM_FOOT_W<=320);
    assert(FM_HEAD_Y+FM_HEAD_H<=FM_LIST_Y && FM_LIST_Y+FM_LIST_H<=FM_FOOT_Y && FM_FOOT_Y+FM_FOOT_H<=240);
    assert(FM_LIST_PAD+(FM_ROWS-1)*FM_ROW_PITCH+16<=FM_LIST_H); /* the last row's text fits the list window */

    /* Background and header. */
    bottom();football_menu_bottom();assert(fake.rect_count==1 && fake.rects[0].w==320 && fake.rects[0].h==240);
    bottom();football_menu_head("Draft  3/22","12.50 / 55 pt");
    assert(fake.window_count==1 && fake.text_count==2);fake_check();
    assert(text_of("12.50 / 55 pt")->box.x+text_of("12.50 / 55 pt")->width<=FM_HEAD_X+FM_HEAD_W); /* the right text ends inside the window */
    bottom();football_menu_head("Choose an evolution for this Pokemon now","a long right side too, which takes room");fake_check();
    for(unsigned i=0;i<fake.text_count;++i)assert(fake.texts[i].px<96 && fake.texts[i].px>=36); /* both shrank */
    bottom();football_menu_head("Only a left side",NULL);assert(fake.text_count==1);fake_check();
    bottom();football_menu_head("",NULL);assert(fake.text_count==0 && fake.window_count==1); /* nothing to say draws no text */

    /* The list: rows at the option list's pitch, the cursor at the selected one, a value pushed to the right. */
    FootballMenuRow rows[FM_ROWS];char label[FM_ROWS][32];
    for(unsigned i=0;i<FM_ROWS;++i){snprintf(label[i],sizeof label[i],"Pokemon number %u",i);rows[i]=(FootballMenuRow){.label=label[i],.value=i%2?"1.50":NULL,.value_style=TS_MONEY};}
    rows[2].tag="WR1";rows[3].dim=true;rows[4].indent=24;
    bottom();football_menu_list(rows,FM_ROWS,4,0,FM_ROWS);
    assert(fake.window_count==1 && fake.cursor_count==1 && fake.rect_count==0); /* nine rows fit: no scroll bar */
    assert(fake.cursor_y==FM_LIST_Y+FM_LIST_PAD+8+4*FM_ROW_PITCH);
    assert(texts_with("Pokemon number 0")==1 && texts_with("WR1")==1 && texts_with("1.50")==4);
    assert(text_of("Pokemon number 4")->box.x>text_of("Pokemon number 0")->box.x+20); /* indent */
    assert(text_of("Pokemon number 2")->box.x>text_of("Pokemon number 0")->box.x+FM_TAG_W-8); /* a tag takes its column */
    assert(text_of("Pokemon number 8")->box.y==FM_LIST_Y+FM_LIST_PAD+8*FM_ROW_PITCH);
    fake_check();
    /* A long list shows a scroll bar: a trough and a thumb whose place follows the first row. */
    float thumb_top[3];unsigned firsts[3]={0,40,91};
    for(unsigned k=0;k<3;++k){
        bottom();football_menu_list(rows,FM_ROWS,-1,firsts[k],100);
        assert(fake.rect_count==2 && fake.cursor_count==0 && fake.rects[1].h>=4 && fake.rects[1].h<fake.rects[0].h);
        assert(fake.rects[1].y>=fake.rects[0].y && fake.rects[1].y+fake.rects[1].h<=fake.rects[0].y+fake.rects[0].h+.01f);
        thumb_top[k]=fake.rects[1].y;fake_check();
    }
    assert(thumb_top[0]<thumb_top[1] && thumb_top[1]<thumb_top[2]);
    bottom();football_menu_list(rows,FM_ROWS,-1,0,100000);assert(fake.rects[1].h==4); /* a huge list keeps a thumb that can be seen */
    bottom();football_menu_list(rows,FM_ROWS,-1,200,100);assert(fake.rects[1].y+fake.rects[1].h<=fake.rects[0].y+fake.rects[0].h+.01f); /* a first past the end clamps */
    /* Long labels shrink to the room a value leaves them; hostile input draws nothing and does not fall over. */
    FootballMenuRow wide[1]={{.label="A name that is far too long to sit beside its value in one row of the list",.value="Picked"}};
    bottom();football_menu_list(wide,1,0,0,1);assert(fake.texts[1].px<96 || fake.texts[0].px<96);fake_check();
    bottom();football_menu_list(NULL,5,0,0,5);assert(fake.window_count==1 && fake.text_count==0 && fake.cursor_count==0);
    bottom();football_menu_list(rows,FM_ROWS+5,1,0,14);assert(fake.text_count<=FM_ROWS*2+FM_ROWS); /* never more than a page */

    /* The hint bar and the buttons. */
    bottom();football_menu_foot("A: draft    B: undo    Left/Right: page");assert(fake.window_count==1 && fake.text_count==1);fake_check();
    bottom();football_menu_foot("A hint longer than a hint should be, so it must shrink to stay in the bar");
    assert(fake.texts[0].px<72);fake_check();
    bottom();football_menu_button(4,210,152,28,"Auto-assign",false,true);assert(fake.rect_count==0);football_menu_button(164,210,152,28,"Ready",true,true);
    assert(fake.rect_count==4 && fake.window_count==2);fake_check(); /* the chosen button has a four-sided yellow frame */
    for(unsigned i=0;i<fake.rect_count;++i)assert(fake.rects[i].x>=162 && fake.rects[i].x+fake.rects[i].w<=318.01f);
    bottom();football_menu_button(4,210,152,28,"",false,false);assert(fake.text_count==0);

    /* Text that fits is left alone, text that does not shrinks and never goes below 6 logical px. */
    bottom();football_menu_panel(0,0,320,100);football_menu_text(TS_WINDOW,96,10,10,300,"Short");assert(fake.texts[0].px==96);
    football_menu_text(TS_WINDOW,96,10,30,100,"Fourteen chars");assert(fake.texts[1].px<96 && fake.texts[1].width<=100);
    football_menu_text(TS_WINDOW,96,10,50,20,"Hopelessly long for twenty pixels");assert(fake.texts[2].px==36);
    football_menu_text(TS_WINDOW,96,10,70,0,"Nothing fits");assert(fake.texts[3].px==36);
    float w=football_menu_text_right(TS_WINDOW,96,300,80,280,"Right");assert(fake.texts[4].box.x+fake.texts[4].width==300 && w==fake.texts[4].width);
    football_menu_text_center(TS_WINDOW,96,160,90,280,"Middle");assert(fabsf(fake.texts[5].box.x+fake.texts[5].width/2-160)<.01f);
    assert(football_menu_text(TS_WINDOW,96,0,0,10,NULL)==0 && football_menu_text(TS_WINDOW,96,0,0,10,"")==0);
    unsigned before=fake.text_count;football_menu_wrap(TS_WINDOW,72,10,10,200,3,"");football_menu_wrap(TS_WINDOW,72,10,10,200,3,NULL);assert(fake.text_count==before);
    football_menu_wrap(TS_WINDOW,72,10,10,200,3,"A paragraph that wraps over a few lines inside its column");assert(fake.text_count==before+1);

    /* Stat bars: a trough and a fill proportional to the value, clamped, and colour-free of bad input. */
    bottom();football_menu_bar(10,10,100,8,50,100);assert(fake.rect_count==2 && fabsf(fake.rects[1].w-49.0f)<.01f);
    bottom();football_menu_bar(10,10,100,8,500,100);assert(fabsf(fake.rects[1].w-98.0f)<.01f); /* clamped to full */
    bottom();football_menu_bar(10,10,100,8,0,100);assert(fake.rects[1].w==0);
    bottom();football_menu_bar(10,10,100,8,5,0);assert(fake.rect_count==1); /* no maximum: just the trough */

    /* A tap lands on the row it is over, and only inside the list. */
    for(int i=0;i<FM_ROWS;++i){
        float y=FM_LIST_Y+FM_LIST_PAD+i*FM_ROW_PITCH+8;
        assert(football_menu_row_at(FM_LIST_X+100,y)==i && football_menu_row_at(FM_LIST_X+2,y-7)==i && football_menu_row_at(FM_LIST_X+FM_LIST_W-1,y+7)==i);
    }
    assert(football_menu_row_at(FM_LIST_X+100,FM_HEAD_Y+10)==-1 && football_menu_row_at(FM_LIST_X+100,FM_FOOT_Y+5)==-1 && football_menu_row_at(FM_LIST_X+100,FM_LIST_Y+FM_LIST_H+1)==-1);
    assert(football_menu_row_at(-5,100)==-1 && football_menu_row_at(FM_LIST_X+FM_LIST_W,100)==-1 && football_menu_row_at(NAN,100)==-1 && football_menu_row_at(100,NAN)==-1);

    /* The top screen's background: the arena of the round when the pack has it, plain teal when not. */
    fake_begin(RR_TOP);fake.has_images=false;assert(ui_space(UI_PHYS_TOP));football_menu_top_bg(3);
    assert(fake.rect_count==1 && fake.rects[0].w==400 && fake.border_count==0 && fake.image_count==0);
    fake_begin(RR_TOP);fake.has_images=true;assert(ui_space(UI_PHYS_TOP));football_menu_top_bg(3);
    assert(fake.border_count==1 && fake.image_count==1 && !strcmp(fake.border_key,"images/arenas/forest_bg") && fake.origin_x==0 && fake.origin_y==0);
    fake_begin(RR_TOP);fake.has_images=true;assert(ui_space(UI_PHYS_TOP));football_menu_top_bg(0);assert(!strcmp(fake.border_key,"images/arenas/plains_bg"));
    fake_begin(RR_TOP);fake.has_images=true;assert(ui_space(UI_PHYS_TOP));football_menu_top_bg(7);assert(!strcmp(fake.border_key,"images/arenas/space_bg"));
    fake_begin(RR_BOTTOM);football_menu_top_bg(2);assert(fake.rect_count==0 && fake.image_count==0); /* not the screen being drawn: nothing */
    puts("football menu tests passed");
    return 0;
}
