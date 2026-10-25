#!/usr/bin/env python3
"""Audit the generated pinned factory catalogue, not arbitrary raw constructors."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
data=(root/'reward-core/gen/out/reward_data.c').read_text()
def records(name):
 match=re.search(r'const \w+ '+name+r'\[.*?\] = \{(.*?)\};',data,re.S)
 assert match,name
 return [list(map(int,row.split(','))) for row in re.findall(r'\{([^{}]+)\}',match.group(1))]
items=records('pr_items');generated=records('pr_gen_types')
held=side=0
for i,(kind,flags,*_) in enumerate(items):
 count=sum(g[0]==i for g in generated) if kind else 1
 if flags&3:held+=count  # Include every form-change identity even if mutually exclusive.
 elif flags&8 or kind==3:side+=count  # Includes generated X items.
assert len(items)==109 and held==233 and side==43,(len(items),held,side)
# Eviolite alone changes concrete held class through clone; reserve one extra per holder.
factory_bound=6*held+side+6
limit=int(re.search(r'#define PR_MAX_MODIFIERS (\d+)',(root/'battle-core/include/pr/inventory_limits.h').read_text()).group(1))
assert factory_bound<=limit<=65535,(factory_bound,limit)
print(f'Pinned catalogue: {held} held/form identities per holder, {side} side identities; '
      f'six holders plus Eviolite clone classes <= {factory_bound}; storage {limit}')
print('This does not bound detached transient objects or arbitrary raw constructor parameters.')
