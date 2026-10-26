#ifndef MENU_FAKES_H
#define MENU_FAKES_H
/* Stand-ins for the classic UI's drawing calls (ui_window, rr_rect, text_draw ...) that record what a screen puts on the page, so a host test
 * can check the layout without pixels: every window, fill and text stays on the screen, and every text lies wholly inside a window, shrunk
 * to fit and clear of the others. A text is as wide as its characters at a deliberately generous 8 logical px each at 16 logical px (the
 * emerald font is narrower), and as high as its size. Include this once per test program. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_widgets.h"
#include "field_border.h"
typedef struct { float x,y,w,h; } FakeRect;
typedef struct { FakeRect box;char text[128];int px;float width; } FakeText;
#define FAKE_CAP 512
static struct {
    rr_screen drawing;float screen_w,screen_h,origin_x,origin_y;
    FakeRect windows[FAKE_CAP];unsigned window_count;
    FakeText texts[FAKE_CAP];unsigned text_count;
    unsigned rect_count,cursor_count,border_count,image_count;float cursor_x,cursor_y;
    bool has_images;char border_key[96];
    FakeRect rects[FAKE_CAP];
} fake;
/* Every distinct text checked so far, so a test can say which menus it has seen. */
static char fake_seen[1024][96];static unsigned fake_seen_count;
__attribute__((unused)) static bool fake_saw(const char *prefix){for(unsigned i=0;i<fake_seen_count;++i)if(!strncmp(fake_seen[i],prefix,strlen(prefix)))return true;return false;}
static void fake_begin(rr_screen screen)
{
    bool images=fake.has_images;memset(&fake,0,sizeof fake);fake.has_images=images;fake.drawing=screen;
    fake.screen_w=screen==RR_TOP?400:320;fake.screen_h=240;
}
bool ui_space(ui_space_id id)
{
    bool top=id==UI_FIELD || id==UI_FIELD_UI || id==UI_TOP_CANVAS || id==UI_TOP_CANVAS_UI || id==UI_PHYS_TOP;
    if(top!=(fake.drawing==RR_TOP))return false;
    fake.origin_x=id==UI_FIELD?40:0;fake.origin_y=id==UI_FIELD?30:0;return true;
}
static bool fake_inside(const FakeRect *r)
{
    float x=r->x+fake.origin_x,y=r->y+fake.origin_y;
    return x>=-.01f && y>=-.01f && x+r->w<=fake.screen_w+.01f && y+r->h<=fake.screen_h+.01f;
}
void ui_window(float x,float y,float w,float h,win_variant v)
{
    (void)v;assert(w>0 && h>0 && fake.window_count<FAKE_CAP);
    FakeRect r={x,y,w,h};assert(fake_inside(&r));fake.windows[fake.window_count++]=r;
}
void ui_cursor(float x,float y){++fake.cursor_count;fake.cursor_x=x;fake.cursor_y=y;}
void rr_rect(float x,float y,float w,float h,rr_color c)
{
    (void)c;assert(w>=0 && h>=0 && fake.rect_count<FAKE_CAP);
    FakeRect r={x,y,w,h};assert(fake_inside(&r));fake.rects[fake.rect_count++]=r;
}
bool img_has(const char *key){(void)key;return fake.has_images;}
void img_draw(const char *key,const char *frame,float x,float y,float ox,float oy){(void)key;(void)frame;(void)x;(void)y;(void)ox;(void)oy;++fake.image_count;}
void field_border_draw(const char *key){++fake.border_count;snprintf(fake.border_key,sizeof fake.border_key,"%s",key);}
text_opts text_defaults(void){text_opts o;memset(&o,0,sizeof o);o.line_spacing=-1;o.chars=TEXT_ALL;return o;}
float text_line_width(text_style st,int px,const char *s,int nbytes)
{
    (void)st;assert(px>=36 && px<=96 && s);return (float)(nbytes<0?(int)strlen(s):nbytes)*px/12.0f;
}
float text_draw(text_style st,float x,float y,const char *s,const text_opts *o)
{
    assert(s && s[0] && fake.text_count<FAKE_CAP);
    int px=o && o->font_px?o->font_px:(st==TS_PARTY?48:96);
    float width=text_line_width(st,px,s,-1),height=px/6.0f,origin=o?o->origin_x:0;
    if(o && o->wrap_w>0){ /* wrapped: as wide as the column, as high as its lines */
        int lines=1;float line=0;for(const char *p=s;*p;++p){line+=px/12.0f;if(line>o->wrap_w){++lines;line=px/12.0f;}}
        if(o->max_lines && lines>o->max_lines)lines=o->max_lines;
        width=o->wrap_w;height=lines*(px/6.0f+3);origin=0;
    }
    FakeText t={.box={x-width*origin,y,width,height},.px=px,.width=width};snprintf(t.text,sizeof t.text,"%s",s);
    if(!fake_inside(&t.box))fprintf(stderr,"text off the screen: \"%s\" at %.0f,%.0f %.0fx%.0f px %d\n",s,t.box.x,t.box.y,t.box.w,t.box.h,px);
    assert(fake_inside(&t.box));fake.texts[fake.text_count++]=t;return width;
}
/* The page-wide checks: each text wholly inside a window, and no two texts overlapping. */
static void fake_check(void)
{
    for(unsigned i=0;i<fake.text_count;++i){bool known=false;for(unsigned j=0;j<fake_seen_count && !known;++j)known=!strcmp(fake_seen[j],fake.texts[i].text);
        if(!known && fake_seen_count<1024)snprintf(fake_seen[fake_seen_count++],sizeof fake_seen[0],"%s",fake.texts[i].text);}
    for(unsigned i=0;i<fake.text_count;++i){
        const FakeRect *t=&fake.texts[i].box;bool housed=false;
        for(unsigned j=0;j<fake.window_count && !housed;++j){const FakeRect *w=&fake.windows[j];
            housed=t->x>=w->x-.01f && t->y>=w->y-.01f && t->x+t->w<=w->x+w->w+.01f && t->y+t->h<=w->y+w->h+.01f;}
        if(!housed)fprintf(stderr,"text outside every window: \"%s\" at %.0f,%.0f %.0fx%.0f\n",fake.texts[i].text,t->x,t->y,t->w,t->h);
        assert(housed);
        for(unsigned j=i+1;j<fake.text_count;++j){const FakeRect *u=&fake.texts[j].box;
            bool apart=t->x+t->w<=u->x+.01f || u->x+u->w<=t->x+.01f || t->y+t->h<=u->y+.01f || u->y+u->h<=t->y+.01f;
            if(!apart)fprintf(stderr,"texts overlap: \"%s\" and \"%s\"\n",fake.texts[i].text,fake.texts[j].text);
            assert(apart);}
    }
}
#endif
