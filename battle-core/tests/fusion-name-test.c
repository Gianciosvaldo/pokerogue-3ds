/* Replay unchanged actual-source UTF-8 results; reject incomplete captures. */
#include "pr/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int decode(const char *s,char *out,size_t cap) {
 size_t n=strlen(s); if(n%2 || n/2>=cap)return 0;
 for(size_t i=0;i<n;i+=2){unsigned v=0;for(int j=0;j<2;j++){unsigned c=(unsigned char)s[i+j];if(c>='0'&&c<='9')c-='0';else if(c>='a'&&c<='f')c=c-'a'+10;else return 0;v=v*16+c;}if(!v)return 0;out[i/2]=(char)v;}
 out[n/2]=0;return 1;
}
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 FILE *f=fopen(argv[1],"r");if(!f)return 2;
 char line[8192],extra;unsigned version,n,seed;
 if(!fgets(line,sizeof line,f)||sscanf(line,"FUSENAME %u %u %u %c",&version,&n,&seed,&extra)!=3||version!=1||n<1024||n>1000000)return 2;
 unsigned failures=0;
 for(unsigned i=0;i<n;i++) {
  unsigned index;char ah[2048],bh[2048],eh[4096],a[1024],b[1024],e[2048],out[4096];
  if(!fgets(line,sizeof line,f)||sscanf(line,"C %u %2047s %2047s %4095s %c",&index,ah,bh,eh,&extra)!=4||index!=i||!decode(ah,a,sizeof a)||!decode(bh,b,sizeof b)||!decode(eh,e,sizeof e))return 2;
  size_t need=pr_fused_species_name(a,b,out,sizeof out);
  if(need!=strlen(e)||strcmp(out,e)){if(failures<12)fprintf(stderr,"case %u: %s + %s expected %s got %s\n",i,a,b,e,out);failures++;}
  for(size_t cap=0;cap<12;cap++) {
   char tiny[16];memset(tiny,0x55,sizeof tiny);size_t got=pr_fused_species_name(a,b,tiny,cap);
   size_t used=cap?(strlen(e)<cap-1?strlen(e):cap-1):0;
   if(got!=need||(cap&&(memcmp(tiny,e,used)||tiny[used]!=0))||(unsigned char)tiny[cap]!=0x55)return 1;
  }
 }
 unsigned completed;if(!fgets(line,sizeof line,f)||sscanf(line,"FUSENAME_COMPLETE %u %c",&completed,&extra)!=1||completed!=n||fgetc(f)!=EOF)return 2;
 fclose(f);printf("fusion names: %u cases, %u failures (seed %u)\n",n,failures,seed);return failures?1:0;
}
