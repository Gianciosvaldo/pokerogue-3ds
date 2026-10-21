#!/usr/bin/env python3
"""Encode actual native Trainer construction/getKey observations; no name inference."""
from pathlib import Path
import json,sys
x=json.loads(Path(sys.argv[1]).read_text());assert x['version']==2 and len(x['rows'])==272
out=['/* Generated from upstream 8555c08 actual Trainer instances; baseline 2024-01-01. */', '#include <stddef.h>', '#include "trainer_art.h"', 'static const struct { int type; const char* keys[3][2]; } trainers[] = {']
last=-1
for r in x['rows']:
 assert r['id']>last;last=r['id'];assert len(r['keys'])==3
 for k in r['keys']:assert len(k)==2 and all(isinstance(s,str) for s in k) and k[0]
 out.append('  {'+str(r['id'])+', {'+', '.join('{'+', '.join(json.dumps(s) for s in k)+'}' for k in r['keys'])+'}},')
out+=['};', '''const char* gm_trainer_sprite_key(int type, int variant, int partner) {
  if (variant<0 || variant>2 || partner<0 || partner>1) return NULL;
  int lo=0,hi=(int)(sizeof trainers/sizeof *trainers)-1;
  while(lo<=hi){int mid=lo+(hi-lo)/2;
    if(trainers[mid].type==type)return trainers[mid].keys[variant][partner];
    if(trainers[mid].type<type)lo=mid+1;else hi=mid-1;
  }
  return NULL;
}''']
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
