#define _GNU_SOURCE
#include "football_saves.h"
#include "save.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static gc_run run,loaded;
static FootballMode mode,restored;
static void damage(const char *path){FILE *f=fopen(path,"wb");assert(f);fputs("broken",f);fclose(f);}
int main(void)
{
    char folder[]="/tmp/football-save-test-XXXXXX";assert(mkdtemp(folder));assert(!chdir(folder));assert(!mkdir("sdmc:",0700));assert(save_init());
    bool backup=false;
    assert(football_saves_load(&restored,&loaded,NULL,&backup)==FB_FILE_EMPTY);
    assert(football_mode_new(&mode,&run,NULL,123));gc_starter team={.species=SPECIES_ID_MAWILE,.tera_type=-1};
    assert(football_mode_draft(&mode,&team,1,NULL));
    assert(save_write("session_0","classic",7)==SAVE_OK);
    assert(football_saves_write(&mode));mode.season.wallet=123;assert(football_saves_write(&mode));
    assert(football_saves_load(&restored,&loaded,NULL,&backup)==FB_FILE_OK && !backup && restored.season.wallet==123);
    /* A failed temp-file write must preserve the committed checkpoint. */
    assert(!mkdir(SAVE_DIR "/football.sav.tmp",0700));mode.season.wallet=456;assert(!football_saves_write(&mode));
    assert(!rmdir(SAVE_DIR "/football.sav.tmp"));
    assert(football_saves_load(&restored,&loaded,NULL,&backup)==FB_FILE_OK && restored.season.wallet==123);
    damage(SAVE_DIR "/football.sav");
    assert(football_saves_load(&restored,&loaded,NULL,&backup)==FB_FILE_OK && backup && restored.season.wallet==0);
    damage(SAVE_DIR "/football.sav.bak");
    assert(football_saves_load(&restored,&loaded,NULL,&backup)==FB_FILE_BAD);
    struct stat st;assert(!stat(SAVE_DIR "/football.sav",&st) && st.st_size==6); /* Unreadable bytes are preserved. */
    assert(football_saves_clear());assert(football_saves_load(&restored,&loaded,NULL,&backup)==FB_FILE_EMPTY);
    FILE *large=fopen(SAVE_DIR "/football.sav","wb");assert(large);
    assert(!ftruncate(fileno(large),FB_SAVE_MAX+13u));fclose(large);
    assert(football_saves_load(&restored,&loaded,NULL,&backup)==FB_FILE_BAD);
    assert(football_saves_clear());
    char *classic=NULL;size_t len=0;assert(save_read("session_0",&classic,&len,&backup)==SAVE_OK && len==7 && !memcmp(classic,"classic",7));free(classic);
    assert(!unlink(SAVE_DIR "/session_0.sav"));
    puts("football save files: dedicated path, atomic write failure, backup recovery, corruption preservation and explicit clear passed");
}
