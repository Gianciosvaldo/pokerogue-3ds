#ifndef FOOTBALL_SAVES_H
#define FOOTBALL_SAVES_H
#include "football_save.h"
typedef enum { FB_FILE_EMPTY, FB_FILE_OK, FB_FILE_BAD } FootballFileStatus;
FootballFileStatus football_saves_load(FootballMode *mode,gc_run *roster,gc_system *profile,bool *backup);
bool football_saves_write(const FootballMode *mode);
bool football_saves_clear(void);
#endif
