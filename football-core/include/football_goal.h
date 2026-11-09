#ifndef FOOTBALL_GOAL_H
#define FOOTBALL_GOAL_H
/* The goal posts. A kick is aimed at the plane FB_GOAL_POST_INSET yards inside each end line, at the middle of the field's width; a good
 * kick ends within a yard and a half or so of the middle, a miss starts four yards out (football_live_special.c), and the posts stand
 * exactly there so the picture says what the engine decided: the stadium (game/source/football_stadium.c) draws them from these numbers. */
#define FB_GOAL_POST_INSET 1.0f      /* yards inside each end line */
#define FB_GOAL_MIDDLE 26.67f        /* the middle of the width of the field, in yards from the near sideline */
#define FB_GOAL_HALF_WIDTH 3.1f      /* yards from the middle to each upright (18 ft 6 in apart) */
#endif
