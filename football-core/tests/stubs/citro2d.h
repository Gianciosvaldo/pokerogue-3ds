#ifndef FB_TEST_CITRO2D_H
#define FB_TEST_CITRO2D_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint32_t u32;
typedef struct { uint16_t width,height; float left,top,right,bottom; } Tex3DS_SubTexture;
typedef struct { void *tex; const Tex3DS_SubTexture *subtex; } C2D_Image;
typedef struct {
    struct { float x,y,w,h; } pos;
    struct { float x,y; } center;
    float depth,angle;
} C2D_DrawParams;
typedef void *C2D_TextBuf;
typedef struct { unsigned placeholder; } C2D_Text;
#define C2D_WithColor 1
static inline u32 C2D_Color32(unsigned r,unsigned g,unsigned b,unsigned a)
{ return r|(g<<8)|(b<<16)|(a<<24); }
void C3D_FrameSync(void);
C2D_TextBuf C2D_TextBufNew(size_t size);
void C2D_TextBufDelete(C2D_TextBuf buffer);
void C2D_TextBufClear(C2D_TextBuf buffer);
void C2D_TextParse(C2D_Text *text,C2D_TextBuf buffer,const char *value);
void C2D_TextOptimize(C2D_Text *text);
void C2D_DrawText(const C2D_Text *text,unsigned flags,float x,float y,float z,float sx,float sy,u32 color);
bool C2D_DrawImage(C2D_Image image,const C2D_DrawParams *params,const void *tint);
bool C2D_DrawRectSolid(float x,float y,float depth,float w,float h,u32 color);
#endif
