#!/usr/bin/env python3
"""Native type-changing full turns with existing doubles comparison floors."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:90]
 s=s.replace(a,b,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const observed=[MoveId.REFLECT_TYPE,MoveId.CONVERSION,MoveId.CONVERSION_2].map((m,i)=>vi.spyOn(allMoves[m].getAttrs(["CopyTypeAttr","FirstMoveTypeAttr","ResistLastMoveTypeAttr"][i])[0],"apply"));''')
put('moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER]','moves=[MoveId.REFLECT_TYPE,MoveId.CONVERSION,MoveId.CONVERSION_2,MoveId.TACKLE,MoveId.EMBER,MoveId.WATER_GUN]')
put('import { it,vi } from "vitest";', 'import { it,vi } from "vitest";\nimport { PokemonType } from "#enums/pokemon-type";\nimport { MoveResult } from "#enums/move-result";')
put('  let written = 0;', '  let copyControls=0,resistControls=0;\n  let written = 0;')
put('    // the parties in field order:', r'''    if(i>=8192){
      for(const p of actors){p.summonData.tags=[];p.level=50;p.calculateStats();p.hp=p.getMaxHp();p.summonData.statStages.fill(0);p.moveset=[new PokemonMove(MoveId.SPLASH)];p.summonData.statStages[Stat.SPD-1]=-6;}
      A0.summonData.statStages[Stat.SPD-1]=6;A0.moveset=[new PokemonMove(i<8320?MoveId.REFLECT_TYPE:MoveId.CONVERSION_2)];
      for(const p of [A1,D0,D1])if(i<8320){p.summonData.types=[PokemonType.UNKNOWN];p.summonData.addedType=PokemonType.GHOST;p.isTerastallized=i%2===0;p.teraType=PokemonType.STELLAR;}else p.pushMoveHistory({move:[MoveId.TACKLE,MoveId.EMBER,MoveId.WATER_GUN][i%3],targets:[0],result:MoveResult.SUCCESS,useMode:MoveUseMode.NORMAL});
    }
    const callsBefore=observed.reduce((n,s)=>n+s.mock.calls.length,0);
    // the parties in field order:''')
put('    written++;', '    if(i>=8192){if(observed.reduce((n,s)=>n+s.mock.calls.length,0)-callsBefore<1)throw Error(`missing real callback in type control ${i}`);if(i<8320)copyControls++;else resistControls++;}\n    written++;')
put('''  const results=rawDamage.mock.results.filter(x=>x.type==="return").map(x=>(x.value as any).damage),wide=results.filter(x=>x>2147483647).length;
  if(threwCount||wide<100)throw Error(`wide native combat missing: exceptions ${threwCount}, wide ${wide}`);
  console.log(`WIDE_COMBAT raw=${results.length} aboveInt32=${wide} max=${Math.max(...results)}`);''','''  const applied=observed.map(s=>s.mock.results.filter(x=>x.type==="return"&&x.value===true).length);
  if(threwCount||applied.some(n=>n<100)||copyControls<128||resistControls<128)throw Error(`type turns missing: exceptions ${threwCount}, applied ${applied}, controls ${copyControls}/${resistControls}`);
  console.log(`TYPE_MOVES_TURNS applied=${applied} copyControls=${copyControls} resistControls=${resistControls}`);''')
put('process.env.DOUBLES_N ?? 8192','process.env.DOUBLES_N ?? 8448')
put('it("wide native combat doubles turns"','it("native type-changing doubles turns"')
out.write_text(s)
