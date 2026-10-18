#!/usr/bin/env python3
"""Owning native Pain Split turns with the established complete doubles comparator."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
subprocess.run([sys.executable,root/'tools/build-wide-combat-proof.py',out],check=True)
s=out.read_text()
def put(old,new):
 global s
 assert s.count(old)==1,old[:100]
 s=s.replace(old,new,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','  const observed=vi.spyOn(allMoves[MoveId.PAIN_SPLIT].getAttrs("HpSplitAttr")[0],"apply");')
put('moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER]','moves=[MoveId.PAIN_SPLIT,MoveId.HEAL_PULSE,MoveId.TACKLE,MoveId.DRAGON_PULSE]')
put('''  const results=rawDamage.mock.results.filter(x=>x.type==="return").map(x=>(x.value as any).damage),wide=results.filter(x=>x>2147483647).length;
  if(threwCount||wide<100)throw Error(`wide native combat missing: exceptions ${threwCount}, wide ${wide}`);
  console.log(`WIDE_COMBAT raw=${results.length} aboveInt32=${wide} max=${Math.max(...results)}`);''','''  const applied=observed.mock.results.filter(x=>x.type==="return"&&x.value===true).length;
  if(threwCount||applied<100)throw Error(`Pain Split turns missing: exceptions ${threwCount}, applied ${applied}`);
  console.log(`PAIN_SPLIT_TURNS applied=${applied}`);''')
put('      if(i%3!==0){','      p.hp=[1,p.getMaxHp(),Math.max(1,Math.floor(p.getMaxHp()/2))][(Math.floor(i/16)+k)%3];\n      if(i%3!==0){')
put('it("wide native combat doubles turns"' ,'it("native Pain Split doubles turns"')
out.write_text(s)
