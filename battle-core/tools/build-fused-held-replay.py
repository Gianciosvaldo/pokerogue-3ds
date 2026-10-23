#!/usr/bin/env python3
"""Retain the strict turn comparator and add the existing critical-item fields."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
s=(root/'tests/effective-stat-item-replay.inc').read_text()
crit=(root/'tests/critical-item-replay.inc').read_text()
a=crit.index('            if (m->cls == PR_MOD_CRIT_BOOSTER || m->cls == PR_MOD_SPECIES_CRIT_BOOSTER) {')
b=crit.index('            if (m->cls == PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE)',a)
parser=crit[a:b].replace('crititem_integer','statitem_integer')
needle='            if (m->cls == PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE)'
assert s.count(needle)==1
s=s.replace(needle,parser+needle)
a=crit.index('            else if (m->cls == PR_MOD_CRIT_BOOSTER || m->cls == PR_MOD_SPECIES_CRIT_BOOSTER) {')
b=crit.index('            else if (m->cls == PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE)',a)
needle='            else if (m->cls == PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE)'
assert s.count(needle)==1
s=s.replace(needle,crit[a:b]+needle)
Path(sys.argv[1]).write_text(s)
