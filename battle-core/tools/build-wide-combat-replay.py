#!/usr/bin/env python3
"""Retain the established comparator and additionally require every native case."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text();assert s.count('int main(int argc, char **argv)')==1
s=s.replace('int main(int argc, char **argv)','static int complete_doubles_main(int argc, char **argv)',1)
s+=r'''
int main(int argc,char **argv){
 if(argc!=2)return 2;
 FILE *f=fopen(argv[1],"r");if(!f)return 2;char row[256],extra;int version,n;
 if(!fgets(row,sizeof row,f)||sscanf(row,"# DOUBLES_CONTRACT %d %d %c",&version,&n,&extra)!=2||version!=1||n<8192||n>MAX_CASES){fclose(f);return 2;}fclose(f);
 int status=complete_doubles_main(argc,argv);if(status)return status;
 if(reported_covered!=n){fprintf(stderr,"Wide combat requires all %d native turns, compared %d\n",n,reported_covered);return 1;}
 return 0;
}
'''
Path(sys.argv[2]).write_text(s)
