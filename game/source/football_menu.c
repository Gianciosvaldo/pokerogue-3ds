#include "football_menu.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "field_border.h"
#include "ui.h"
#include "ui_widgets.h"
#define TEAL RR_RGBA(0x00,0x68,0x60,255)
#define YELLOW RR_RGBA(0xf8,0xe8,0x58,255)
#define MIN_PX 36
/* The step down in size: from px toward 6 logical px, the first that fits. */
static int fit_px(text_style style,int px,float max_w,const char *s)
{
    if(max_w<=0)return MIN_PX;
    for(;px>MIN_PX;px-=6)if(text_line_width(style,px,s,-1)<=max_w)return px;
    return MIN_PX;
}
static float place(text_style style,int px,float x,float y,float max_w,const char *s,float origin_x)
{
    if(!s || !s[0])return 0;
    text_opts o=text_defaults();o.font_px=fit_px(style,px,max_w,s);o.origin_x=origin_x;
    return text_draw(style,x,y,s,&o);
}
float football_menu_text(text_style style,int px,float x,float y,float max_w,const char *s){return place(style,px,x,y,max_w,s,0);}
float football_menu_text_right(text_style style,int px,float right,float y,float max_w,const char *s){return place(style,px,right,y,max_w,s,1);}
float football_menu_text_center(text_style style,int px,float center,float y,float max_w,const char *s){return place(style,px,center,y,max_w,s,.5f);}
void football_menu_wrap(text_style style,int px,float x,float y,float w,int max_lines,const char *s)
{
    if(!s || !s[0])return;
    text_opts o=text_defaults();o.font_px=px;o.wrap_w=w;o.max_lines=max_lines;
    text_draw(style,x,y,s,&o);
}
void football_menu_panel(float x,float y,float w,float h){ui_window(x,y,w,h,WIN_NORMAL);}
void football_menu_select(float x,float y,float w,float h)
{
    rr_rect(x-2,y-2,w+4,2,YELLOW);rr_rect(x-2,y+h,w+4,2,YELLOW);rr_rect(x-2,y,2,h,YELLOW);rr_rect(x+w,y,2,h,YELLOW);
}
void football_menu_bottom(void){rr_rect(0,0,320,240,TEAL);}
void football_menu_head(const char *left,const char *right)
{
    ui_window(FM_HEAD_X,FM_HEAD_Y,FM_HEAD_W,FM_HEAD_H,WIN_NORMAL);
    float used=0;
    if(right && right[0])used=football_menu_text_right(TS_MONEY,FM_PX_BODY,FM_HEAD_X+FM_HEAD_W-12,FM_HEAD_Y+FM_TEXT_PAD,150,right);
    football_menu_text(TS_WINDOW,FM_PX_BODY,FM_HEAD_X+12,FM_HEAD_Y+FM_TEXT_PAD,FM_HEAD_W-36-used,left);
}
void football_menu_list(const FootballMenuRow *rows,unsigned count,int selected,unsigned first,unsigned total)
{
    ui_window(FM_LIST_X,FM_LIST_Y,FM_LIST_W,FM_LIST_H,WIN_NORMAL);
    if(!rows)count=0;
    if(count>FM_ROWS)count=FM_ROWS;
    const float right=FM_LIST_X+FM_LIST_W-(total>count?20:14),left=FM_LIST_X+FM_LABEL_X;
    for(unsigned i=0;i<count;++i){
        const FootballMenuRow *r=&rows[i];float y=FM_LIST_Y+FM_LIST_PAD+i*FM_ROW_PITCH;
        float used=0,start=left+r->indent;
        if(r->value && r->value[0])used=football_menu_text_right(r->value_style?r->value_style:TS_WINDOW,FM_PX_BODY,right,y,100,r->value)+10;
        if(r->tag && r->tag[0]){football_menu_text(TS_STATS_LABEL,FM_PX_BODY,start,y,FM_TAG_W-4,r->tag);start+=FM_TAG_W;}
        football_menu_text(r->dim?TS_SHADOW_TEXT:TS_WINDOW,FM_PX_BODY,start,y,right-start-used,r->label);
    }
    if(selected>=0 && (unsigned)selected<count)ui_cursor(FM_LIST_X+12,FM_LIST_Y+FM_LIST_PAD+8+selected*FM_ROW_PITCH);
    if(total>count && count){
        /* The option list's scroll bar: a 3 px trough inside the right edge, and a thumb for the rows in view. */
        const float bx=FM_LIST_X+FM_LIST_W-12,by=FM_LIST_Y+5,bh=FM_LIST_H-10;
        float th=bh*count/total;if(th<4)th=4;
        float span=(float)(total-count),at=first>total-count?span:(float)first;
        rr_rect(bx,by,3,bh,RR_RGBA(0x30,0x30,0x30,160));
        rr_rect(bx,by+(bh-th)*(span>0?at/span:0),3,th,RR_RGBA(0xf8,0xf8,0xf8,255));
    }
}
void football_menu_foot(const char *text)
{
    ui_window(FM_FOOT_X,FM_FOOT_Y,FM_FOOT_W,FM_FOOT_H,WIN_NORMAL);
    football_menu_text(TS_WINDOW,FM_PX_SMALL,FM_FOOT_X+12,FM_FOOT_Y+FM_TEXT_PAD+1,FM_FOOT_W-24,text);
}
void football_menu_button(float x,float y,float w,float h,const char *label,bool selected,bool enabled)
{
    ui_window(x,y,w,h,WIN_NORMAL);
    if(selected)football_menu_select(x,y,w,h);
    football_menu_text_center(enabled?TS_WINDOW:TS_SHADOW_TEXT,FM_PX_BODY,x+w*.5f,y+(h-14)*.5f,w-16,label);
}
void football_menu_bar(float x,float y,float w,float h,unsigned value,unsigned max)
{
    rr_rect(x,y,w,h,RR_RGBA(0x30,0x30,0x30,255));
    if(!max)return;
    if(value>max)value=max;
    float fill=(w-2)*value/max;
    rr_color c=value*3>=max*2?RR_RGBA(0x78,0xc8,0x50,255):value*3>=max?RR_RGBA(0xe8,0xe8,0xa8,255):RR_RGBA(0xf8,0x58,0x58,255);
    rr_rect(x+1,y+1,fill,h-2,c);
}
int football_menu_row_at(float x,float y)
{
    if(!isfinite(x) || !isfinite(y) || x<FM_LIST_X || x>=FM_LIST_X+FM_LIST_W)return -1;
    float top=FM_LIST_Y+FM_LIST_PAD-2;
    if(y<top)return -1;
    int row=(int)((y-top)/FM_ROW_PITCH);
    return row<FM_ROWS?row:-1;
}
void football_menu_top_bg(unsigned round)
{
    /* One arena for each round of the playoffs, the last the loftiest; any that the pack lacks falls back to the next, then to plain teal. */
    static const char *const arenas[]={"plains","grass","forest","metropolis","mountain","power_plant","space"};
    const unsigned n=sizeof arenas/sizeof arenas[0],base=round?round-1:0;
    char key[64];const char *found=NULL;
    for(unsigned i=0;i<n && !found;++i){
        snprintf(key,sizeof key,"images/arenas/%s_bg",arenas[(base+i)%n]);
        if(img_has(key))found=arenas[(base+i)%n];
    }
    if(ui_space(UI_PHYS_TOP))rr_rect(0,0,400,240,found?RR_RGBA(0,0,0,255):TEAL);
    if(!found)return;
    snprintf(key,sizeof key,"images/arenas/%s_bg",found);
    if(ui_space(UI_FIELD))img_draw(key,NULL,0,0,0,0);
    if(ui_space(UI_PHYS_TOP))field_border_draw(key);
}
