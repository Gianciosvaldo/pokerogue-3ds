/* Complete actual-source getter capture, including reserves in singles. */
#include "pr/engine.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=2)return 2;FILE *f=fopen(argv[1],"r");if(!f)return 2;char row[256],extra;int n,fail=0;
 if(!fgets(row,sizeof row,f)||sscanf(row,"ALLY 1 %d %c",&n,&extra)!=1||n!=8192)return 2;
 for(int i=0;i<n;i++){
  int ix,side,dbl,count,k,want;if(!fgets(row,sizeof row,f)||sscanf(row,"C %d %d %d %d %d %d %c",&ix,&side,&dbl,&count,&k,&want,&extra)!=6||ix!=i||side!=i%2||dbl!=(i%4>=2)||count!=(i/4)%7||k!=(i/28)%6)return 2;
  pr_battle b={0};pr_pokemon ps[2][6]={0};b.double_battle=dbl;
  for(int s=0;s<2;s++){b.n_party[s]=s==side?count:6;for(int j=0;j<6;j++){ps[s][j].id=100+s*10+j;ps[s][j].is_player=!s;ps[s][j].hp=i%3?100:0;ps[s][j].switch_out_status=i%5==0;b.party[s][j]=&ps[s][j];}for(int j=0;j<(dbl?2:1)&&j<b.n_party[s];j++)b.field[s*2+j]=b.party[s][j];}
  const pr_pokemon *got=pr_get_ally(&b,&ps[side][k]);if((got?(int)got->id:-1)!=want)fail++;
 }
 int footer;if(!fgets(row,sizeof row,f)||sscanf(row,"ALLY_COMPLETE %d %c",&footer,&extra)!=1||footer!=n||fgetc(f)!=EOF)return 2;
 printf("Ally lookup: %d complete, %d exact, %d failures\n",n,n-fail,fail);return fail?1:0;
}
