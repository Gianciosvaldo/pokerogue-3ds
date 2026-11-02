#include "pr/engine.h"
#include <stdio.h>
int main(void){for(int i=0;i<PR_MOVE_COUNT;i++){
 const pr_move *m=&pr_moves[i];printf("M %d %d\n",i,m->unimplemented);
 for(int charge=0;charge<2;charge++){int first=charge?m->cattr0:m->attr0,n=charge?m->ncattr:m->nattr;
  for(int k=0;k<n;k++){int cls=pr_attrs[first+k].cls;printf("A %d %d %d %s",i,charge,k,pr_attr_class_name[cls]);for(int c=0;c<PR_AC_COUNT_;c++)if(pr_attr_is(cls,c))printf(" %s",pr_attr_class_name[c]);puts("");}
 }
}return 0;}
