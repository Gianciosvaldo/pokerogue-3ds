#!/usr/bin/env python3
"""Additive Battle Bond matrix over the unchanged knockout/Fell Stinger scaffold."""
from pathlib import Path
import subprocess,sys,tempfile
here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as temp:
    intermediate=Path(temp)/'knockout.ts'
    subprocess.run([sys.executable,str(here/'build-knockout-stats-source.py'),str(here.parent/'tests/fell-stinger.producer.test.ts'),str(intermediate)],check=True)
    s=intermediate.read_text()
def replace(a,b):
    global s
    if s.count(a)!=1:raise SystemExit('Battle Bond source anchor changed: '+a)
    s=s.replace(a,b,1)
replace('const K=22, abilities=[AbilityId.MOXIE,AbilityId.CHILLING_NEIGH,AbilityId.GRIM_NEIGH,AbilityId.AS_ONE_GLASTRIER,AbilityId.AS_ONE_SPECTRIER,AbilityId.BEAST_BOOST,AbilityId.EELEVATE,AbilityId.SOUL_HEART];','const K=22, abilities=[AbilityId.BATTLE_BOND];')
replace('if(state===14){holder.summonData.ability=AbilityId.BALL_FETCH;}','if(state===14){holder.summonData.abilitiesApplied.add(AbilityId.BATTLE_BOND);}')
s=s.replace('KNOCKOUT_STATS','BATTLE_BOND_TURN').replace('Knockout stats','Battle Bond turns')
Path(sys.argv[1]).write_text(s)
