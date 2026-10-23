#include "football_render.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pr_data.h"

static float bounded(float value, float low, float high)
{
    return value<low ? low : value>high ? high : value;
}
static void label(FootballRenderer *r, const char *value, float x, float y, float scale)
{
    C2D_Text text;
    C2D_TextParse(&text,r->text,value);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text,C2D_WithColor,x,y,.8f,scale,scale,C2D_Color32(255,255,255,255));
}
bool football_render_init(FootballRenderer *r)
{
    if (!r || r->text) return false;
    r->text=C2D_TextBufNew(512);
    return r->text!=NULL;
}
void football_render_free(FootballRenderer *r)
{
    if (!r) return;
    for (unsigned i=0;i<FB_RENDER_ACTOR_CAP;++i) menu_sprite_free(&r->sprites[i]);
    if (r->text) C2D_TextBufDelete(r->text);
    memset(r,0,sizeof *r);
}
bool football_render_load_front(FootballRenderer *r, unsigned slot, const menu_mon *mon)
{
    if (!r || !r->text || !mon || slot>=FB_RENDER_ACTOR_CAP) return false;
    /* Unload before loading to keep transition memory bounded. A failed load
     * leaves this slot empty; render a marker until a suitable asset is chosen. */
    menu_sprite_free(&r->sprites[slot]);
    r->total_sprite_bytes-=r->sprite_bytes[slot]; r->sprite_bytes[slot]=0;
    if (!menu_sprite_load(&r->sprites[slot],mon,false)) return false; /* FRONT only */
    size_t bytes=0;
    rr_atlas *atlas=r->sprites[slot].atlas;
    for (int p=0;p<rr_atlas_pages(atlas);++p) bytes+=rr_tex_bytes(rr_atlas_page(atlas,p));
    if (bytes>FB_RENDER_TEXTURE_BUDGET-r->total_sprite_bytes) {
        menu_sprite_free(&r->sprites[slot]); return false;
    }
    r->sprite_bytes[slot]=bytes; r->total_sprite_bytes+=bytes;
    return true;
}
const rr_frame *football_render_front_frame(const FootballRenderer *r,
                                            unsigned slot, uint32_t animation_ms)
{
    return r && slot<FB_RENDER_ACTOR_CAP ? menu_sprite_frame(&r->sprites[slot],animation_ms) : NULL;
}
bool football_render_draw_sprite(const rr_frame *front, float x, float y,
                                 float scale, bool faces_left)
{
    if (!front || !isfinite(x) || !isfinite(y) || !isfinite(scale) || scale<=0) return false;
    Tex3DS_SubTexture sub;
    C2D_Image image;
    if (!rr_frame_c2d_image(front,&image,&sub)) return false;
    float scale_x=faces_left ? -scale : scale;
    C2D_DrawParams params={
        {x+(front->off_x-front->src_w*.5f)*scale_x,
         y+(front->off_y-front->src_h*.5f)*scale,
         front->w*scale_x,front->h*scale}, {0,0}, .2f, 0
    };
    /* The PICA200 culls negative-width quads. Preserve the requested -1 X
     * transform in the wrapper, then submit equivalent UVs and positive width. */
    if (params.pos.w<0) {
        params.pos.x+=params.pos.w;
        params.pos.w=-params.pos.w;
        float left=sub.left; sub.left=sub.right; sub.right=left;
    }
    return C2D_DrawImage(image,&params,NULL);
}
void football_render_top(FootballRenderer *r, const FootballRenderScene *scene)
{
    if (!r || !r->text || !scene) return;
    C2D_TextBufClear(r->text);
    const float x=20,y=48,w=360,h=176;
    const u32 white=C2D_Color32(235,245,235,255);
    C2D_DrawRectSolid(x,y,0,w,h,C2D_Color32(24,112,52,255));
    C2D_DrawRectSolid(x,y,.01f,30,h,C2D_Color32(25,65,135,255));
    C2D_DrawRectSolid(x+w-30,y,.01f,30,h,C2D_Color32(155,40,45,255));
    for (unsigned yard=10;yard<=110;yard+=10)
        C2D_DrawRectSolid(x+yard*3,y,.02f,1,h,white);
    C2D_DrawRectSolid(x,y,.02f,w,1,white);
    C2D_DrawRectSolid(x,y+h-1,.02f,w,1,white);
    /* Short hash marks keep the field legible on the 400x240 screen. */
    for (unsigned yard=15;yard<110;yard+=5) {
        C2D_DrawRectSolid(x+yard*3,y+h*.33f,.02f,1,4,white);
        C2D_DrawRectSolid(x+yard*3,y+h*.67f,.02f,1,4,white);
    }
    if (isfinite(scene->ball_x_yards))
        C2D_DrawRectSolid(x+bounded(scene->ball_x_yards,10,110)*3,y,.03f,1,h,
                         C2D_Color32(255,210,30,255));
    unsigned count=scene->actor_count<FB_RENDER_ACTOR_CAP ? scene->actor_count : FB_RENDER_ACTOR_CAP;
    for (unsigned i=0;i<count;++i) {
        const FootballRenderActor *a=&scene->actors[i];
        if (!isfinite(a->x_yards) || !isfinite(a->y_yards)) continue;
        float px=x+bounded(a->x_yards,0,120)*3;
        float py=y+bounded(a->y_yards,0,53.333333f)*(h/53.333333f);
        bool left=a->movement_x<0 || (a->movement_x==0 && a->away);
        const rr_frame *f=a->front_frame;
        float scale=f && f->src_w>0 && f->src_h>0 ?
            fminf(24.0f/f->src_w,24.0f/f->src_h) : 1;
        if (!football_render_draw_sprite(f,px,py,scale,left))
            C2D_DrawRectSolid(px-3,py-3,.2f,6,6,a->away ?
                             C2D_Color32(230,75,75,255) : C2D_Color32(75,145,255,255));
    }
    char score[64],situation[64];
    unsigned seconds=scene->seconds_left>3599 ? 3599 : scene->seconds_left;
    if (scene->quarter>4)
        snprintf(score,sizeof score,"HOME %u   AWAY %u   OT%u",(unsigned)scene->home_score,
                 (unsigned)scene->away_score,(unsigned)(scene->quarter-4));
    else
        snprintf(score,sizeof score,"HOME %u   AWAY %u   Q%u",(unsigned)scene->home_score,
                 (unsigned)scene->away_score,(unsigned)scene->quarter);
    snprintf(situation,sizeof situation,"Down %u  To go %u   %u:%02u",(unsigned)scene->down,
             (unsigned)scene->distance,seconds/60,seconds%60);
    label(r,score,12,4,.5f); label(r,situation,12,25,.45f);
}
void football_render_bottom(FootballRenderer *r, const FootballPlaycall *ui)
{
    if (!r || !r->text || !ui) return;
    /* Text from top draws is already submitted; buffer can now be reused. */
    C2D_TextBufClear(r->text);
    label(r,football_playcall_title(ui),20,16,.6f);
    unsigned count=football_playcall_option_count(ui);
    for (unsigned i=0;i<count;++i) {
        FootballTouchRect rect=football_playcall_rect(i);
        u32 color=i==ui->highlighted ? C2D_Color32(40,100,170,255) : C2D_Color32(35,48,65,255);
        C2D_DrawRectSolid(rect.x,rect.y,0,rect.w,rect.h,color);
        label(r,football_playcall_label(ui,i),rect.x+12,rect.y+14,.6f);
    }
    label(r,"Touch or D-pad + A     B: Back",20,208,.45f);
}
static const char *species_name(uint16_t id)
{
    int low=0,high=PR_SPECIES_COUNT-1;
    while (low<=high) {
        int mid=(low+high)/2;
        if (pr_species_table[mid].id==id) return pr_species_table[mid].name;
        if (pr_species_table[mid].id<id) low=mid+1; else high=mid-1;
    }
    return "Unknown";
}
void football_render_evolution_bottom(FootballRenderer *r, const FootballEvolutionMenu *menu)
{
    if (!r || !r->text || !menu || !menu->active) return;
    C2D_TextBufClear(r->text);
    label(r,"Choose evolution",20,16,.6f);
    unsigned page=football_evolution_page_start(menu);
    for (unsigned row=0;row<FB_EVOLUTION_VISIBLE && page+row<=menu->count;++row) {
        unsigned option=page+row;
        FootballTouchRect rect=football_evolution_rect(row);
        C2D_DrawRectSolid(rect.x,rect.y,0,rect.w,rect.h,option==menu->selected ?
            C2D_Color32(40,100,170,255) : C2D_Color32(35,48,65,255));
        const char *name=option==menu->count ? "Cancel evolution" : species_name(menu->species[option]);
        label(r,name,rect.x+12,rect.y+8,.55f);
    }
    label(r,"Touch or D-pad + A     B: Cancel",20,216,.4f);
}
void football_render_frame(FootballRenderer *r, const FootballRenderScene *scene,
                            const FootballPlaycall *ui)
{
    if (!r || !r->text || !scene || !ui) return;
    /* Reuse rr's existing citro2d targets and frame lifecycle. Do not nest this
     * wrapper inside the application's rr_frame_begin/end. */
    rr_frame_begin();
    rr_target(RR_TOP,RR_RGBA(12,22,30,255));
    football_render_top(r,scene);
    rr_target(RR_BOTTOM,RR_RGBA(12,22,30,255));
    football_render_bottom(r,ui);
    rr_frame_end();
}
