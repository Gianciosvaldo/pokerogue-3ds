#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "optional-phase-map.inc"
static int guarded_printf(const char *,...);static int parse_bad,stat_bad,turn_bad,text_bad,covered;
#define printf guarded_printf
#define main original_turn_main
#include "optional-phase-replay.inc"
#undef main
#undef printf
static int guarded_printf(const char *format,...){char out[65536];va_list a;va_start(a,format);int n=vsnprintf(out,sizeof out,format,a);va_end(a);
 if(!strncmp(out,"cases ",6)){int total,skip;sscanf(out,"cases %d (skipped %d: records not read yet), parse mismatches %d, stats mismatches %d",&total,&skip,&parse_bad,&stat_bad);parse_bad+=skip;}
 if(!strncmp(out,"covered ",8)){int pass,uncovered;sscanf(out,"covered %d: pass %d, fail %d; not covered %d",&covered,&pass,&turn_bad,&uncovered);}
 if(!strncmp(out,"texts of covered turns ",23)){int total,same,diff,missing;sscanf(out,"texts of covered turns %d: identical %d, differ %d, a message with no text yet %d",&total,&same,&diff,&missing);text_bad=diff+missing;}
 fputs(out,stdout);return n;}
static bool valid_token(const char *s){if(!strcmp(s,"A")||!strcmp(s,"U")||!strcmp(s,"N"))return true;if(*s!='I')return false;char *end;long v=strtol(s+1,&end,10);return end!=s+1&&!*end&&v>=-1&&v<=3;}
static bool valid_queue(const char *s){char copy[4096];if(strlen(s)>=sizeof copy)return false;strcpy(copy,s);char *dyn=strstr(copy," dyn=");if(!dyn)return false;dyn+=5;if(!*dyn)return false;if(!strcmp(dyn,"-"))return true;char previous[96]={0};
 for(char *group=strtok(dyn,",");group;group=strtok(NULL,",")){char *colon=strchr(group,':');if(!colon)return false;*colon++=0;if(strlen(group)>=sizeof previous||(*previous&&strcmp(previous,group)>=0))return false;strcpy(previous,group);int type=-1;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(group,pr_phase_name[t]))type=t;if(type<0||!pr_phase_is_dynamic(type))return false;char *token=colon;while(1){char *slash=strchr(token,'/');if(slash)*slash=0;if(!valid_token(token))return false;if(!slash)break;token=slash+1;}}
 return true;}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES"))return 2;
 FILE *f=fopen(argv[1],"r");if(!f)return 2;char s[4096],rest;int version,wanted,cases=0,ends=0,queues=0,ran=0,footer=0;
 if(!fgets(s,sizeof s,f)||sscanf(s,"# OPTIONAL_PHASE %d %d %c",&version,&wanted,&rest)!=2||version!=1||wanted<120||wanted>100000){fclose(f);return 2;}
 while(fgets(s,sizeof s,f)){if(!strchr(s,'\n')||footer){fclose(f);return 2;}s[strcspn(s,"\n")]=0;
  if(!strncmp(s,"CASE ",5)){int id;if(sscanf(s,"CASE %d %c",&id,&rest)!=1||id!=cases||cases!=ends){fclose(f);return 2;}cases++;queues=ran=0;}
  else if(!strcmp(s,"RUN")){if(cases!=ends+1||ran++){fclose(f);return 2;}}
  else if(!strncmp(s,"QUEUE ",6)){if(!ran||queues++||!valid_queue(s)){fclose(f);return 2;}}
  else if(!strncmp(s,"END ",4)){if(!ran||queues!=1||cases!=ends+1){fclose(f);return 2;}ends++;}
  else if(!strncmp(s,"# OPTIONAL_COMPLETE ",20)){int n;if(sscanf(s,"# OPTIONAL_COMPLETE %d %c",&n,&rest)!=1||n!=wanted||cases!=wanted||ends!=wanted){fclose(f);return 2;}footer=1;}
 }
 fclose(f);if(!footer)return 2;int rc=original_turn_main(argc,argv);if(rc||parse_bad||stat_bad||turn_bad||text_bad||!covered)return 1;
 printf("Optional phase typed queue requested%d sourceComplete%d coveredControls%d, zero covered differences; uncovered remain excluded\n",wanted,cases,covered);return 0;
}
