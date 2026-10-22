#include "football_shop_render.h"
#include "gm_item_ui.h"
#include "mui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
bool football_shop_prepare(FootballShopView *view, const FootballSeason *s)
{
    if (!view || !s || s->phase!=FB_SEASON_SHOP || s->shop_count>FB_SHOP_CAP) return false;
    memset(view,0,sizeof *view);
    for (unsigned i=0;i<s->shop_count;++i) {
        const FootballShopOffer *offer=&s->shop[i];
        const char *key=football_shop_asset_key(offer);
        const gm_item_ui *item=gm_item_ui_find(key,offer->classic_arg);
        if (!item) item=gm_item_ui_find(key,-1);
        if (!item) continue;
        view->names[i]=item->name;
        if (item->icon) view->icons[i]=mui_frame("items",item->icon);
    }
    return true; /* Missing artwork falls back to text. */
}
static void text(FootballRenderer *r, const char *value, float x, float y, float scale)
{
    C2D_Text t;
    C2D_TextParse(&t,r->text,value); C2D_TextOptimize(&t);
    C2D_DrawText(&t,C2D_WithColor,x,y,.8f,scale,scale,C2D_Color32(255,255,255,255));
}
void football_shop_draw(FootballRenderer *r, const FootballShopView *v,
                        const FootballSeason *s, const FootballShopMenu *m)
{
    if (!r || !r->text || !v || !s || !m || !s->roster || s->phase!=FB_SEASON_SHOP ||
        s->shop_count>FB_SHOP_CAP || m->party>=(unsigned)s->roster->n_party) return;
    C2D_TextBufClear(r->text);
    char line[96];
    snprintf(line,sizeof line,"SHOP  Round %u   Cash %lu",s->round,(unsigned long)s->wallet);
    text(r,line,12,6,.5f);
    FootballTouchRect target=football_shop_target_rect();
    C2D_DrawRectSolid(target.x,target.y,0,target.w,target.h,C2D_Color32(40,65,90,255));
    const FootballPlayer *player=&s->players[m->party];
    const char *name="Pokemon";
    for (unsigned i=0;i<PR_SPECIES_COUNT;++i)
        if (pr_species_table[i].id==player->species_id) { name=pr_species_table[i].name; break; }
    snprintf(line,sizeof line,"%u: %.16s  SPD %u  STR %u",m->party+1,name,
             (unsigned)player->attributes.spd,(unsigned)player->attributes.str);
    text(r,line,20,38,.35f);
    for (unsigned i=0;i<s->shop_count;++i) {
        const FootballShopOffer *offer=&s->shop[i];
        FootballTouchRect rect=football_shop_offer_rect(i);
        C2D_DrawRectSolid(rect.x,rect.y,0,rect.w,rect.h,m->selected==i ?
            C2D_Color32(40,100,170,255) : C2D_Color32(35,48,65,255));
        const rr_frame *icon=v->icons[i];
        if (icon && icon->src_w>0 && icon->src_h>0)
            football_render_draw_sprite(icon,30,rect.y+18,
                fminf(24.0f/icon->src_w,24.0f/icon->src_h),false);
        const char *name=v->names[i];
        if (!name) name=offer->item==FB_ITEM_CARBOS ? "Carbos" :
                       offer->item==FB_ITEM_PROTEIN ? "Protein" : "Mega stone";
        if (offer->sold) snprintf(line,sizeof line,"%.22s  SOLD",name);
        else if (offer->item==FB_ITEM_MEGA_STONE)
            snprintf(line,sizeof line,"%.16s  P%u  %lu",name,(unsigned)(offer->target+1),(unsigned long)offer->price);
        else snprintf(line,sizeof line,"%.22s  %lu",name,(unsigned long)offer->price);
        text(r,line,50,rect.y+8,.42f);
    }
    FootballTouchRect leave=football_shop_leave_rect();
    C2D_DrawRectSolid(leave.x,leave.y,0,leave.w,leave.h,m->selected==s->shop_count ?
        C2D_Color32(40,100,170,255) : C2D_Color32(35,48,65,255));
    text(r,s->round==FB_PLAYOFF_ROUNDS ? "Finish playoff run" : "Next match",20,192,.45f);
    const char *status=m->last_action==FB_SHOP_BOUGHT ? "Purchased" :
        m->last_action==FB_SHOP_REFUSED ? "Cannot buy: check cash, target or stat cap" :
        "Left/Right: player   A: buy   B: next";
    text(r,status,12,220,.32f);
}
