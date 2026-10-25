#ifndef FOOTBALL_CAMERA_H
#define FOOTBALL_CAMERA_H
#include <stdbool.h>
typedef struct { float x,y;bool ready; } FootballCamera;
#define FB_CAMERA_YARDS 44.0f
#define FB_CAMERA_SCALE (400.0f/FB_CAMERA_YARDS)
#define FB_CAMERA_SQUASH .62f
void football_camera_follow(FootballCamera *camera,float x,float y,float vx,float milliseconds);
float football_camera_x(const FootballCamera *camera,float x);
float football_camera_y(const FootballCamera *camera,float y);
#endif
