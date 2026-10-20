#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static const char *lookup(void *ctx,const char *file,int fallback,size_t *n){const char *s="{\"value\":\"{{moneyAmount}}\"}";*n=strlen(s);return s;}
int main(int argc,char **argv){
 if(argc!=2)return 2;FILE *f=fopen(argv[1],"r");if(!f)return 2;
 char row[1200],hex[900],extra;int version,n,seed;
 if(!fgets(row,sizeof row,f)||sscanf(row,"MONEY_NUMBER %d %d %d %c",&version,&n,&seed,&extra)!=3||version!=1||n!=100000||seed<0)return 2;
 int failed=0,wide=0;
 for(int i=0;i<n;i++){
  int id;unsigned long long bits;
  if(!fgets(row,sizeof row,f)||sscanf(row,"C %d %llx %899s %c",&id,&bits,hex,&extra)!=3||id!=i||strlen(hex)%2)return 2;
  uint64_t raw=bits;double value;memcpy(&value,&raw,sizeof value);if(isfinite(value)&&value!=trunc(value))return 2;
  char expected[450];size_t len=strlen(hex)/2;if(len>=sizeof expected)return 2;
  for(size_t j=0;j<len;j++){unsigned x;if(sscanf(hex+2*j,"%2x",&x)!=1)return 2;expected[j]=(char)x;}expected[len]=0;
  char out[450];size_t got=pr_money_en_us(value,out,sizeof out);bool ok=got==len&&!strcmp(out,expected);wide+=fabs(value)>INT32_MAX;
  for(size_t cap=0;cap<=8;cap++){memset(out,'?',sizeof out);ok&=pr_money_en_us(value,out,cap)==len;if(cap)ok&=out[(len<cap-1)?len:cap-1]==0&&!strncmp(out,expected,cap-1);}
  pr_msg m=pr_msg1("money:value",pr_arg_money_en_us("moneyAmount",value));pr_text_source src={lookup,NULL,"de"};
  ok&=pr_msg_format(&m,&src,out,sizeof out)==len&&!strcmp(out,expected);
  if(!ok){if(failed<8)fprintf(stderr,"case%d bits%016llx got[%s] want[%s]\n",i,bits,out,expected);failed++;}
 }
 int count;if(!fgets(row,sizeof row,f)||sscanf(row,"MONEY_NUMBER_COMPLETE %d %c",&count,&extra)!=1||count!=n||fgetc(f)!=EOF||wide<90000)return 2;
 printf("Money Number: %d compared, %d exact, %d failures (wide%d)\n",n,n-failed,failed,wide);return failed?1:0;
}
