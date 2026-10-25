#!/usr/bin/env python3
"""Transport actual source lists in their original order, without an effect oracle."""
import json,sys
from pathlib import Path
j=json.loads(Path(sys.argv[1]).read_text())
assert j['version']==1 and j['transferred'] is True and j['highIndex']==107
assert list(map(len,j['before']))==[342,114] and list(map(len,j['after']))==[342,113]
assert len(j['loot'])==113
rows=['# INVENTORY_CAPACITY 1 107']
for stage in ['before','after']:
 for side,items in enumerate(j[stage]):
  rows.append(f'LIST {stage} {side} {len(items)}')
  for m in items:
   assert set(m)=={'key','arg','owner','stack','cls'}
   rows.append('ITEM {key} {arg} {owner} {stack} {cls}'.format(**m))
rows.append('LOOT '+str(len(j['loot']))+' '+' '.join(map(str,j['loot'])))
rows.append('COMPLETE')
Path(sys.argv[2]).write_text('\n'.join(rows)+'\n')
