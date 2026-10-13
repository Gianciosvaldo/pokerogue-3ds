/* Pinned Pokemon.getAbility results, including ignoreOverride. No local oracle. */
#include "pr/battle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char line[1024];static FILE *fp;
static void bad(void){fputs("malformed ability lookup trace\n",stderr);exit(2);}
static void read_line(void){if(!fgets(line,sizeof line,fp)||!strchr(line,'\n'))bad();}
static bool valid_form(int species,int form){pr_pokemon p={0};p.species=species;const pr_species *s=pr_pokemon_species(&p);return s&&form>=0&&form<s->nforms;}
static bool ability(int a){return a>=-1&&a<PR_ABILITY_COUNT;}
int main(int argc,char **argv){
 if(argc!=2||(fp=fopen(argv[1],"r"))==NULL)return 2;
 int n;char extra;read_line();if(sscanf(line,"ABILITY_LOOKUP 1 %d %c",&n,&extra)!=1||n<64||n>100000)bad();
 int fail=0;unsigned coverage=0;
 for(int i=0;i<n;i++){
  int v[18];read_line();
  if(sscanf(line,"C %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %c",v,v+1,v+2,v+3,v+4,v+5,v+6,v+7,v+8,v+9,v+10,v+11,v+12,v+13,v+14,v+15,v+16,v+17,&extra)!=18)bad();
  if(v[0]!=i||!valid_form(v[1],v[2])||v[3]<0||v[3]>2||v[6]<0||v[6]>2)bad();
  for(int k=7;k<=10;k++)if(!ability(v[k]))bad();
  for(int k=15;k<=17;k++)if(!ability(v[k])||(k>15&&v[k]<0))bad();
  if(v[4]?!valid_form(v[4],v[5]):v[5]!=0)bad();
  if(v[11]?!valid_form(v[11],v[12]):v[12]!=0)bad();
  if(v[13]?(!v[4]||!valid_form(v[13],v[14])):v[14]!=0)bad();
  pr_pokemon p={0};p.species=v[1];p.form=v[2];p.ability_lookup_known=true;p.ability_index=v[3];
  p.fusion_species=v[4];p.fusion_form=v[5];p.fusion_ability_index=v[6];p.custom_ability=v[7];p.fusion_custom_ability=v[8];
  p.lookup_ability_override=v[9];p.ability_override=v[10];p.transformed=v[11]!=0;p.transform_species=v[11];p.transform_form=v[12];
  p.temporary_fusion_species=v[13];p.temporary_fusion_form=v[14];p.daily_boss_ability=v[15];
  pr_pokemon before=p;int got=pr_get_ability(&p,false),base=pr_get_ability(&p,true);
  if(memcmp(&p,&before,sizeof p))bad();
  if(got!=v[16]||base!=v[17]){if(fail<20)fprintf(stderr,"case %d effective %d/%d permanent %d/%d\n",i,got,v[16],base,v[17]);fail++;}
  pr_refresh_ability(&p);if(p.ability!=got||p.base_ability!=base)bad();
  coverage|=1u<<((i/2)%32);
 }
 read_line();int footer;if(sscanf(line,"ABILITY_LOOKUP_COMPLETE %d %c",&footer,&extra)!=1||footer!=n||fgetc(fp)!=EOF||coverage!=0xffffffffu)bad();
 printf("Ability lookup cases%d comparisons%d mismatches%d all32states\n",n,2*n,fail);return fail?1:0;
}
