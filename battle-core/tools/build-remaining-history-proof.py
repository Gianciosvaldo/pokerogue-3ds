#!/usr/bin/env python3
from pathlib import Path
import itertools,json,sys
j=json.loads(Path(sys.argv[1]).read_text());rows=j['rows'];assert j['version']==1 and len(rows)==192
expected=list(itertools.product([0,1,7,8,9,64,65535,65536],[0,33,199,170],range(1,7)))
assert [(r['length'],r['move'],r['mode']) for r in rows]==expected
lines=['/* Actual pinned caller observations; no generated expected answers. */','static const struct {int length,move,mode;int expected[10][5];} remaining_cases[192]={']
for r in rows:
 assert len(r['stages'])==10
 stages=[]
 for s in r['stages']:
  assert type(s['conversion'])==bool and len(s['accuracy'])==4 and all(type(v)==int and v in [-1,0,1] for v in s['accuracy'])
  stages.append('{'+','.join(map(str,[int(s['conversion']),*s['accuracy']]))+'}')
 lines.append('{'+','.join(map(str,[r['length'],r['move'],r['mode']]))+',{'+','.join(stages)+'}},')
lines.append('};');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
