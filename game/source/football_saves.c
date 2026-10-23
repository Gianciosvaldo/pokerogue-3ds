#include "football_saves.h"
#include "save.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
static unsigned char bytes[FB_SAVE_MAX]; /* Never on the 3DS stack. */
FootballFileStatus football_saves_load(FootballMode *mode,gc_run *roster,gc_system *profile,bool *backup)
{
    /* Reject oversized/unreadable files before the shared reader allocates. */
    const char *paths[]={SAVE_DIR "/football.sav",SAVE_DIR "/football.sav.bak"};
    for(unsigned i=0;i<2;++i) {
        struct stat st;
        if(!stat(paths[i],&st)) { if(st.st_size<0 || (uint64_t)st.st_size>FB_SAVE_MAX+12u)return FB_FILE_BAD; }
        else if(errno!=ENOENT)return FB_FILE_BAD;
    }
    char *data=NULL;size_t length=0;
    save_result r=save_read("football",&data,&length,backup);
    if(r==SAVE_NOT_FOUND)return FB_FILE_EMPTY;
    if(r!=SAVE_OK)return FB_FILE_BAD;
    bool ok=football_save_decode(mode,roster,profile,data,length);free(data);
    return ok?FB_FILE_OK:FB_FILE_BAD;
}
bool football_saves_write(const FootballMode *mode)
{
    size_t n=football_save_encode(mode,bytes,sizeof bytes);
    return n && save_write("football",bytes,n)==SAVE_OK;
}
bool football_saves_clear(void)
{
    const char *paths[]={SAVE_DIR "/football.sav",SAVE_DIR "/football.sav.bak",SAVE_DIR "/football.sav.tmp"};
    bool ok=true;
    for(unsigned i=0;i<3;++i)if(remove(paths[i]) && errno!=ENOENT)ok=false;
    return ok;
}
