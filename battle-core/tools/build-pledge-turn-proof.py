#!/usr/bin/env python3
"""Observe actual paired Pledges through native complete doubles turns."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:90]
 s=s.replace(a,b,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");
  const pledges=[MoveId.FIRE_PLEDGE,MoveId.WATER_PLEDGE,MoveId.GRASS_PLEDGE];
  const waiting=pledges.map(m=>vi.spyOn(allMoves[m].getAttrs("AwaitCombinedPledgeAttr")[0],"apply"));
  const effects=pledges.map(m=>allMoves[m].getAttrs("AddPledgeEffectAttr").map(a=>vi.spyOn(a,"apply"))).flat();''')
put('moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER]','moves=[MoveId.FIRE_PLEDGE,MoveId.WATER_PLEDGE,MoveId.FIRE_PLEDGE,MoveId.GRASS_PLEDGE,MoveId.WATER_PLEDGE,MoveId.GRASS_PLEDGE]')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  const combined=waiting.map(s=>s.mock.results.filter(x=>x.type==="return"&&x.value===true).length),added=effects.map(s=>s.mock.results.filter(x=>x.type==="return"&&x.value===true).length);
  if(combined.some(n=>n<100)||added.some(n=>n<25))throw Error(`Pledge coverage missing: combined ${combined}, effects ${added}`);
  console.log(`PLEDGE_TURNS combined=${combined} effects=${added}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
put('it("wide native combat doubles turns"','it("native combined Pledge doubles turns"')
out.write_text(s)
