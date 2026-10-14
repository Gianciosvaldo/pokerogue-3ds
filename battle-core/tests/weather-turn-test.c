/* Owning full-turn guard; the existing comparison/tolerances are unchanged. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int turns,covered,passed,failed,uncovered,parse_fail,stat_fail,text_count,same,differ,no_text;
static int report(const char *format,...);
#define printf report
#define main replay_main
#include "weather-turn-replay.inc"
#undef main
#undef printf
static int report(const char *format,...)
{
    char s[65536];va_list args;va_start(args,format);int n=vsnprintf(s,sizeof s,format,args);va_end(args);
    if(!strncmp(s,"cases ",6)){int skipped;sscanf(s,"cases %d (skipped %d: records not read yet), parse mismatches %d, stats mismatches %d",&turns,&skipped,&parse_fail,&stat_fail);if(skipped)parse_fail++;}
    if(!strncmp(s,"covered ",8))sscanf(s,"covered %d: pass %d, fail %d; not covered %d",&covered,&passed,&failed,&uncovered);
    if(!strncmp(s,"texts of covered turns ",23))sscanf(s,"texts of covered turns %d: identical %d, differ %d, a message with no text yet %d",&text_count,&same,&differ,&no_text);
    fputs(s,stdout);return n;
}
int main(int argc,char **argv)
{
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"r");if(!f)return 2;
    char line[8192],extra;int n=0,cases=0,ends=0,footer=0;
    if(!fgets(line,sizeof line,f)||sscanf(line,"# WEATHERTURN_CONTRACT 1 %d %c",&n,&extra)!=1||n<20||n>100000){fclose(f);return 2;}
    while(fgets(line,sizeof line,f)){
        if(!strncmp(line,"CASE ",5)){int c;if(sscanf(line,"CASE %d %c",&c,&extra)!=1||c!=cases++){fclose(f);return 2;}}
        if(!strncmp(line,"END stop=",9))ends++;
        if(!strncmp(line,"# WEATHERTURN_COMPLETE ",strlen("# WEATHERTURN_COMPLETE "))){if(sscanf(line,"# WEATHERTURN_COMPLETE %d %c",&footer,&extra)!=1||fgetc(f)!=EOF){fclose(f);return 2;}}
    }
    fclose(f);if(n!=cases||n!=ends||n!=footer)return 2;
    int result=replay_main(argc,argv);
    return result||turns!=n||covered!=n||passed!=n||failed||uncovered||parse_fail||stat_fail||text_count!=n||same!=n||differ||no_text?1:0;
}
