/* API recording test; CI's devkitPro build validates the real GPU headers. */
#include "football_render.h"
#include <3ds.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static u32 keys_down;
static touchPosition touch;
u32 hidKeysDown(void) { return keys_down; }
void hidTouchRead(touchPosition *p) { *p=touch; }
static FootballRenderer renderer;
static C2D_DrawParams last;
static Tex3DS_SubTexture uv;
static unsigned images, rectangles, begins,ends,targets,loads;
static bool saw_ninjask,saw_shedinja,saw_cancel;
static size_t atlas_bytes=1024*1024;
static bool compact_mode;static unsigned freed_compact;
static rr_frame compact_source={.tex=(rr_tex*)&renderer,.w=64,.h=64,.src_w=64,.src_h=64};
C2D_TextBuf C2D_TextBufNew(size_t n) { assert(n==512); return &renderer; }
void C2D_TextBufDelete(C2D_TextBuf b) { (void)b; }
void C2D_TextBufClear(C2D_TextBuf b) { (void)b; }
void C2D_TextParse(C2D_Text *t,C2D_TextBuf b,const char *s) {
    (void)t;(void)b;assert(s);
    saw_ninjask|=strcmp(s,"Ninjask")==0;
    saw_shedinja|=strcmp(s,"Shedinja")==0;
    saw_cancel|=strcmp(s,"Cancel evolution")==0;
}
void C2D_TextOptimize(C2D_Text *t) { (void)t; }
void C2D_DrawText(const C2D_Text *t,unsigned f,float x,float y,float z,float sx,float sy,u32 c)
{ (void)t;(void)f;(void)x;(void)y;(void)z;(void)sx;(void)sy;(void)c; }
bool C2D_DrawImage(C2D_Image img,const C2D_DrawParams *p,const void *t)
{
    (void)t; assert(p->pos.w>0 && p->pos.h>0);
    last=*p; uv=*img.subtex; ++images; return true;
}
bool C2D_DrawRectSolid(float x,float y,float z,float w,float h,u32 c)
{ (void)x;(void)y;(void)z;(void)c; assert(w>0 && h>0); ++rectangles; return true; }
bool rr_frame_c2d_image(const rr_frame *f,C2D_Image *image,Tex3DS_SubTexture *sub)
{
    if (!f || !f->tex) return false;
    *sub=(Tex3DS_SubTexture){(uint16_t)f->w,(uint16_t)f->h,0,1,1,0};
    *image=(C2D_Image){f->tex,sub}; return true;
}
void rr_frame_begin(void) { ++begins; }
void rr_frame_end(void) { ++ends; }
void rr_target(rr_screen s,rr_color c) { (void)c; assert((unsigned)s==targets%2); ++targets; }
bool menu_sprite_load(menu_sprite *s,const menu_mon *mon,bool back)
{ assert(!back && mon); ++loads; s->atlas=(rr_atlas*)s; return true; }
void menu_sprite_free(menu_sprite *s) { memset(s,0,sizeof *s); }
const rr_frame *menu_sprite_frame(const menu_sprite *s,uint32_t ms)
{ (void)s;(void)ms;return compact_mode?&compact_source:NULL; }
int rr_atlas_pages(const rr_atlas *a) { assert(a);return 1; }
rr_tex *rr_atlas_page(const rr_atlas *a,int p) { assert(p==0);return (rr_tex*)a; }
size_t rr_tex_bytes(const rr_tex *t) { assert(t);return atlas_bytes; }
rr_tex *rr_tex_thumbnail(const rr_frame *f,unsigned edge){assert(edge==64);return f?(rr_tex*)&renderer:NULL;}
void rr_tex_free(rr_tex *t){if(t)++freed_compact;}
rr_frame rr_tex_frame(rr_tex *t){return (rr_frame){.tex=t,.w=64,.h=64,.src_w=64,.src_h=64};}
int main(void)
{
    assert(football_render_init(&renderer));
    assert(!football_render_init(&renderer));
    rr_frame f={.tex=(rr_tex*)&renderer,.w=20,.h=30,.off_x=4,.off_y=2,.src_w=40,.src_h=40};
    assert(football_render_draw_sprite(&f,100,100,1,false));
    assert(last.pos.x==84 && last.pos.w==20 && uv.left==0 && uv.right==1);
    assert(football_render_draw_sprite(&f,100,100,1,true));
    assert(last.pos.x==96 && last.pos.w==20 && uv.left==1 && uv.right==0);
    assert(!football_render_draw_sprite(&f,100,100,-1,false));
    assert(!football_render_draw_sprite(&f,NAN,100,1,false));
    /* A plain draw keeps citro2d's top-left origin; a posed one stretches, and a turn pivots about (x, y). */
    assert(football_render_draw_sprite(&f,100,100,1,false) && last.center.x==0 && last.center.y==0 && last.angle==0);
    assert(football_render_draw_sprite_pose(&f,100,100,1,false,2,.5f,0) && last.pos.x==68 && last.pos.y==91 && last.pos.w==40 && last.pos.h==15 && last.angle==0 && last.center.x==0);
    assert(football_render_draw_sprite_pose(&f,100,100,1,false,1,1,.5f) && last.pos.x==100 && last.pos.y==100 && last.center.x==16 && last.center.y==18 && last.angle==.5f && last.pos.w==20 && uv.left==0);
    assert(football_render_draw_sprite_pose(&f,100,100,1,true,1,1,-.5f) && last.pos.x==100 && last.center.x==4 && last.center.y==18 && last.angle==-.5f && uv.left==1 && uv.right==0);
    assert(!football_render_draw_sprite_pose(&f,100,100,1,false,0,1,0) && !football_render_draw_sprite_pose(&f,100,100,1,false,1,-1,0) && !football_render_draw_sprite_pose(&f,100,100,1,false,1,1,NAN) && !football_render_draw_sprite_pose(NULL,100,100,1,false,1,1,0));
    menu_mon mon={0};
    for (unsigned i=0;i<4;++i) assert(football_render_load_front(&renderer,i,&mon));
    assert(renderer.total_sprite_bytes==FB_RENDER_TEXTURE_BUDGET);
    assert(!football_render_load_front(&renderer,4,&mon));
    assert(renderer.total_sprite_bytes==FB_RENDER_TEXTURE_BUDGET && !renderer.sprites[4].atlas);
    assert(!football_render_load_front(&renderer,FB_RENDER_ACTOR_CAP,&mon));
    unsigned prior_loads=loads;
    FootballRenderScene scene={0};
    scene.actor_count=100; /* Renderer clamps untrusted count to its fixed array. */
    scene.actors[0]=(FootballRenderActor){.front_frame=&f,.x_yards=60,.y_yards=20,.away=true};
    FootballPlaycall ui;
    football_playcall_reset(&ui);
    keys_down=KEY_A;
    football_playcall_poll(&ui);
    assert(ui.state==FB_CALL_FORMATION);
    keys_down=KEY_TOUCH; touch=(touchPosition){20,124};
    football_playcall_poll(&ui);
    assert(ui.state==FB_CALL_TYPE && ui.formation==FB_FORM_UNDER_CENTER);
    keys_down=0;
    football_playcall_poll(&ui);
    assert(ui.state==FB_CALL_TYPE);
    keys_down=KEY_TOUCH; touch.py=60;
    football_playcall_poll(&ui);
    assert(ui.state==FB_CALL_PLAY);
    football_render_bottom(&renderer,&ui);
    football_playcall_poll(&ui);
    PlayInfo play;
    assert(football_playcall_take(&ui,&play) && play.id==FB_RUN_DIVE);
    scene.football_visible=true;scene.football_x_yards=60;scene.football_y_yards=20;scene.football_height=.5f;
    unsigned prior_images=images;
    football_render_frame(&renderer,&scene,&ui);
    assert(begins==1 && ends==1 && targets==2 && rectangles>30);
    assert(images==prior_images+1 && uv.left==1 && loads==prior_loads);
    scene.actors[0].movement_x=1;
    football_render_frame(&renderer,&scene,&ui);
    assert(uv.left==0); /* Movement overrides away-team default. */
    FootballDefenseMenu defense={.selected=FB_DEF_BLITZ};
    football_render_defense_bottom(&renderer,&defense);
    FootballEvolutionMenu evolution;
    const uint16_t options[]={SPECIES_ID_NINJASK,SPECIES_ID_SHEDINJA};
    assert(football_evolution_open(&evolution,options,2));
    football_render_evolution_bottom(&renderer,&evolution);
    assert(saw_ninjask && saw_shedinja && saw_cancel && !evolution.ready);
    football_render_clear_sprites(&renderer);compact_mode=true;atlas_bytes=8192;
    for(unsigned i=0;i<22;++i){assert(football_render_load_compact(&renderer,i,&mon));assert(football_render_front_frame(&renderer,i,0));}
    assert(renderer.total_sprite_bytes==22*8192 && freed_compact==0);
    football_render_clear_sprites(&renderer);assert(freed_compact==22 && renderer.total_sprite_bytes==0);
    football_render_free(&renderer);
    assert(renderer.total_sprite_bytes==0 && !renderer.text);
    football_render_free(&renderer);
    puts("football rendering: signed-X flip, trim pivot, budget, front assets and dual-screen submission passed");
}
