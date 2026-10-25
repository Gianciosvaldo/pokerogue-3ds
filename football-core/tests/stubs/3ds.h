#ifndef FB_TEST_3DS_H
#define FB_TEST_3DS_H
#include <stdint.h>
typedef uint32_t u32;
typedef struct { uint16_t px,py; } touchPosition;
enum { KEY_A=1,KEY_B=2,KEY_UP=4,KEY_DOWN=8,KEY_TOUCH=16,KEY_LEFT=32,KEY_RIGHT=64 };
u32 hidKeysDown(void);
void hidTouchRead(touchPosition *position);
typedef struct { int16_t dx,dy; } circlePosition;
#define KEY_X 128u
#define KEY_Y 256u
#define KEY_L 512u
#define KEY_R 1024u
#define KEY_SELECT 2048u
#define KEY_DLEFT KEY_LEFT
#define KEY_DRIGHT KEY_RIGHT
#define KEY_DUP KEY_UP
#define KEY_DDOWN KEY_DOWN
u32 hidKeysHeld(void);
void hidCircleRead(circlePosition *position);
#endif
