#ifndef FB_TEST_3DS_H
#define FB_TEST_3DS_H
#include <stdint.h>
typedef uint32_t u32;
typedef struct { uint16_t px,py; } touchPosition;
enum { KEY_A=1,KEY_B=2,KEY_UP=4,KEY_DOWN=8,KEY_TOUCH=16,KEY_LEFT=32,KEY_RIGHT=64 };
u32 hidKeysDown(void);
void hidTouchRead(touchPosition *position);
#endif
