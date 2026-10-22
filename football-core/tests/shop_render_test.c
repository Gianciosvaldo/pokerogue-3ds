#include "football_shop_render.h"
#include "mui.h"
#include <3ds.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned icon_loads,icon_draws,rectangles;
static bool saw_carbos,saw_protein,saw_sold;
static FootballRenderer renderer;
static gc_run roster;
static const rr_frame icon={.src_w=16,.src_h=16};
u32 hidKeysDown(void) { return 0; }
void hidTouchRead(touchPosition *p) { p->px=0; p->py=0; }
const rr_frame *mui_frame(const char *key,const char *frame)
{
    assert(!strcmp(key,"items"));
    assert(!strcmp(frame,"carbos") || !strcmp(frame,"protein"));
    ++icon_loads; return &icon;
}
bool football_render_draw_sprite(const rr_frame *f,float x,float y,float scale,bool left)
{
    (void)x;(void)y;assert(f==&icon && scale>0 && !left); ++icon_draws;return true;
}
void C2D_TextBufClear(C2D_TextBuf b) { assert(b); }
void C2D_TextParse(C2D_Text *t,C2D_TextBuf b,const char *s)
{
    (void)t;assert(b && s);
    saw_carbos|=strstr(s,"Carbos")!=NULL;
    saw_protein|=strstr(s,"Protein")!=NULL;
    saw_sold|=strstr(s,"SOLD")!=NULL;
}
void C2D_TextOptimize(C2D_Text *t) { (void)t; }
void C2D_DrawText(const C2D_Text *t,unsigned f,float x,float y,float z,float sx,float sy,u32 c)
{ (void)t;(void)f;(void)x;(void)y;(void)z;(void)sx;(void)sy;(void)c; }
bool C2D_DrawRectSolid(float x,float y,float z,float w,float h,u32 c)
{ (void)x;(void)y;(void)z;(void)c;assert(w>0 && h>0);++rectangles;return true; }
int main(void)
{
    FootballSeason season={0};
    season.roster=&roster;season.phase=FB_SEASON_SHOP;season.round=1;season.wallet=500;
    season.shop_count=2;roster.n_party=1;
    season.shop[0]=(FootballShopOffer){FB_ITEM_CARBOS,125,STAT_SPD,-1,false};
    season.shop[1]=(FootballShopOffer){FB_ITEM_PROTEIN,125,STAT_ATK,-1,true};
    season.players[0].species_id=SPECIES_ID_BULBASAUR;
    FootballShopMenu menu={0};FootballShopView view;
    assert(football_shop_prepare(&view,&season));
    assert(icon_loads==2 && view.icons[0]==&icon && view.icons[1]==&icon);
    assert(!strcmp(view.names[0],"Carbos") && !strcmp(view.names[1],"Protein"));
    renderer.text=&renderer;
    football_shop_draw(&renderer,&view,&season,&menu);
    assert(icon_loads==2 && icon_draws==2 && rectangles==4);
    assert(saw_carbos && saw_protein && saw_sold);
    football_shop_draw(&renderer,&view,&season,&menu);
    assert(icon_loads==2); /* Draw never loads assets. */
    season.phase=FB_SEASON_READY;
    assert(!football_shop_prepare(&view,&season));
    unsigned draws=icon_draws;
    football_shop_draw(&renderer,&view,&season,&menu);
    assert(icon_draws==draws);
    puts("football shop render: classic Carbos/Protein atlas keys, sold state and no draw-time loading passed");
}
