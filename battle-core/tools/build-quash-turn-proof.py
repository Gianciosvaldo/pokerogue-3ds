#!/usr/bin/env python3
"""Owning native Quash turns with the established complete doubles comparator."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
subprocess.run([sys.executable,root/'tools/build-wide-combat-proof.py',out],check=True)
s=out.read_text()
def put(old,new):
 global s
 assert s.count(old)==1,old[:100]
 s=s.replace(old,new,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','  const observed=vi.spyOn(allMoves[MoveId.QUASH].getAttrs("ForceLastAttr")[0],"apply");')
put('moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER]','moves=[MoveId.QUASH,MoveId.QUICK_ATTACK,MoveId.TACKLE,MoveId.DRAGON_PULSE]')
put('''  const results=rawDamage.mock.results.filter(x=>x.type==="return").map(x=>(x.value as any).damage),wide=results.filter(x=>x>2147483647).length;
  if(threwCount||wide<100)throw Error(`wide native combat missing: exceptions ${threwCount}, wide ${wide}`);
  console.log(`WIDE_COMBAT raw=${results.length} aboveInt32=${wide} max=${Math.max(...results)}`);''','''  const applied=observed.mock.results.filter(x=>x.type==="return"&&x.value===true).length;
  if(threwCount||applied<100)throw Error(`Quash turns missing: exceptions ${threwCount}, applied ${applied}`);
  console.log(`QUASH_TURNS applied=${applied}`);''')
put('it("wide native combat doubles turns"','it("native Quash doubles turns"')
out.write_text(s)
