/* Source-produced selection/caller decisions and English text, strict transport. */
#include "pr/engine.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *locale_get(void *ctx, const char *file, int fallback, size_t *len)
{
    static char *texts[16];static char names[16][64];static size_t lengths[16];static int n;
    if(fallback)return NULL;
    for(int i=0;i<n;i++)if(!strcmp(names[i],file)){*len=lengths[i];return texts[i];}
    if(n==16)return NULL;
    char path[1024];snprintf(path,sizeof path,"%s/%s.json",(const char *)ctx,file);
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long size=ftell(f);rewind(f);
    if(size<0){fclose(f);return NULL;}char *p=malloc((size_t)size+1);if(!p){fclose(f);return NULL;}
    size_t got=fread(p,1,(size_t)size,f);fclose(f);if(got!=(size_t)size){free(p);return NULL;}
    p[got]=0;snprintf(names[n],sizeof names[n],"%s",file);texts[n]=p;lengths[n++]=got;*len=got;return p;
}
static int number(const char *s,int low,int high,int *v)
{
    if(!s||!*s)return 0;
    const char *p=s;if(*p=='-')p++;
    if(!*p)return 0;
    for(;*p;p++)if(*p<'0'||*p>'9')return 0;
    char *end;errno=0;long n=strtol(s,&end,10);
    if(errno||*end||n<low||n>high)return 0;
    *v=(int)n;return 1;
}
static int hex_digit(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
static int decode(const char *s,char *out,size_t cap)
{
    if(!strcmp(s,"-")){out[0]=0;return 1;}size_t n=strlen(s);
    if(n%2||n/2>=cap)return 0;
    for(size_t i=0;i<n;i+=2){int a=hex_digit(s[i]),b=hex_digit(s[i+1]);if(a<0||b<0||!(a*16+b))return 0;out[i/2]=(char)(a*16+b);}
    out[n/2]=0;return 1;
}
static int compare(int id,const char *which,int actual,int expected,const pr_msg *msg,const char *hex,const pr_text_source *text)
{
    char wanted[1024],got[1024];if(!decode(hex,wanted,sizeof wanted))return 2;
    if(msg->key&&*msg->key)pr_msg_format(msg,text,got,sizeof got);else got[0]=0;
    if(actual!=expected||strcmp(got,wanted)){fprintf(stderr,"FAIL %d %s: %d/%d text [%s]/[%s]\n",id,which,actual,expected,got,wanted);return 1;}return 0;
}
int main(int argc,char **argv)
{
    const char *locales=getenv("TURNTEST_LOCALES");if(argc!=2||!locales)return 2;
    FILE *f=fopen(argv[1],"r");if(!f)return 2;
    pr_text_source text={locale_get,(void *)locales,"en"};char line[8192];int count=0,footer=0,wanted=0,failed=0;
    while(fgets(line,sizeof line,f)){
        if(!strchr(line,'\n')||footer){fclose(f);return 2;}line[strcspn(line,"\n")]=0;
        char *tokens[16];int nt=0;for(char *p=strtok(line," ");p;p=strtok(NULL," ")){if(nt==16){fclose(f);return 2;}tokens[nt++]=p;}
        if(!wanted){if(nt!=3||strcmp(tokens[0],"HISTORY_CONTRACT")||strcmp(tokens[1],"1")||!number(tokens[2],672,672,&wanted)){fclose(f);return 2;}continue;}
        if(nt==2&&!strcmp(tokens[0],"HISTORY_COMPLETE")){int n;if(!number(tokens[1],wanted,wanted,&n)||count!=wanted){fclose(f);return 2;}footer=1;continue;}
        int id,move,control,nh,sel,usable,runtime,fight;
        if(nt!=14||strcmp(tokens[0],"C")||!number(tokens[1],count,count,&id)||!number(tokens[2],1,PR_MOVE_COUNT-1,&move)
            ||(move!=MOVE_ID_GIGATON_HAMMER&&move!=MOVE_ID_BLOOD_MOON)||!number(tokens[3],0,3,&control)||!number(tokens[4],0,33,&nh)
            ||!number(tokens[6],0,1,&sel)||!number(tokens[8],0,1,&usable)||!number(tokens[10],0,1,&runtime)||!number(tokens[12],-1,PR_MOVE_COUNT-1,&fight)){fclose(f);return 2;}
        pr_battle bt={0};pr_pokemon p={0};p.id=1;p.species=SPECIES_ID_RATTATA;p.ability=ABILITY_ID_BALL_FETCH;p.is_player=true;p.on_field=true;p.hp=100;bt.field[0]=&p;
        p.n_moves=control==2?1:2;p.moves[0]=move;p.moves[1]=MOVE_ID_TACKLE;
        p.pp_used[0]=control==1||control==3?99:0;p.pp_used[1]=control==3?99:0;
        char *h=tokens[5];if(!nh&&strcmp(h,"-")){fclose(f);return 2;}
        for(int j=0;j<nh;j++){
            char *end=strchr(h,';');if(end)*end=0;char *a=strchr(h,','),*b=a?strchr(a+1,','):NULL;
            int hm,mode,result;if(!a||!b){fclose(f);return 2;}*a++=0;*b++=0;
            if(!number(h,0,PR_MOVE_COUNT-1,&hm)||!number(a,1,6,&mode)||!number(b,0,4,&result)||((j==nh-1)!=(end==NULL))){fclose(f);return 2;}
            pr_push_move_history(&p,(pr_turn_move){.move=hm,.use_mode=mode,.result=result,.targets={2},.n_targets=1});h=end?end+1:NULL;
        }
        pr_battle before=bt;pr_pokemon mon=p;pr_msg why={0};int status;
        int actual=pr_move_selectable(&bt,&p,move,&why);status=compare(id,"selectable",actual,sel,&why,tokens[7],&text);if(status==2){fclose(f);return 2;}failed+=status;
        actual=pr_move_usable(&bt,&p,0,control==3,true,&why);status=compare(id,"trySelectMove",actual,usable,&why,tokens[9],&text);if(status==2){fclose(f);return 2;}failed+=status;
        actual=pr_move_usable(&bt,&p,0,control==3,false,&why);status=compare(id,"runtime",actual,runtime,&why,tokens[11],&text);if(status==2){fclose(f);return 2;}failed+=status;
        actual=pr_fight_move(&bt,&p,0,control==3,&why);status=compare(id,"CommandPhase fight",actual,fight,&why,tokens[13],&text);if(status==2){fclose(f);return 2;}failed+=status;
        if(memcmp(&bt,&before,sizeof bt)||memcmp(&p,&mon,sizeof p)){fprintf(stderr,"FAIL %d selection changed state\n",id);failed++;}count++;
    }
    fclose(f);if(!footer||count!=wanted)return 2;
    printf("consecutive selection cases %d: %d failures; four source decisions/texts per case\n",count,failed);return failed?1:0;
}
