#!/usr/bin/env python3
"""Actual owning damage turns: native gains stay queued during the same hit."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
subprocess.run([sys.executable,root/'tools/build-wide-combat-proof.py',out],check=True)
s=out.read_text()
def put(old,new):
 global s
 assert s.count(old)==1,old[:100]
 s=s.replace(old,new,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','  const observed=vi.spyOn(allMoves[MoveId.SPECTRAL_THIEF].getAttrs("SpectralThiefAttr")[0],"apply");')
put('moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER]','moves=[MoveId.SPECTRAL_THIEF,MoveId.SPECTRAL_THIEF,MoveId.TACKLE,MoveId.SPLASH]')
put('      if(i%3!==0){','''      p.customPokemonData.ability=[AbilityId.BALL_FETCH,AbilityId.SIMPLE,AbilityId.CONTRARY][Math.floor(i/16)%3];p.summonData.ability=p.customPokemonData.ability;
      p.summonData.statStages[Stat.ATK-1]=[-6,0,3,6][(Math.floor(i/4)+k)%4];p.summonData.statStages[Stat.DEF-1]=[-6,0,3,6][(Math.floor(i/32)+k)%4];
      if(i%3!==0){''')
put('''  const results=rawDamage.mock.results.filter(x=>x.type==="return").map(x=>(x.value as any).damage),wide=results.filter(x=>x>2147483647).length;
  if(threwCount||wide<100)throw Error(`wide native combat missing: exceptions ${threwCount}, wide ${wide}`);
  console.log(`WIDE_COMBAT raw=${results.length} aboveInt32=${wide} max=${Math.max(...results)}`);''','''  const applied=observed.mock.results.filter(x=>x.type==="return"&&x.value===true).length;
  if(threwCount||applied<100)throw Error(`Spectral Thief turns missing: exceptions ${threwCount}, applied ${applied}`);
  console.log(`SPECTRAL_THIEF_TURNS applied=${applied}`);''')
put('import { it,vi } from "vitest";', 'import { it,vi } from "vitest";\nimport { modifierTypes } from "#modifiers/modifier-type";')
put('  let written = 0;', '  let multiLensControls=0;\n  let written = 0;')
put('    // the parties in field order:', r'''    if(i>=8192){
      for(const p of actors){p.summonData.tags=[];p.level=50;p.calculateStats();p.hp=p.getMaxHp();p.summonData.statStages.fill(0);p.moveset=[new PokemonMove(MoveId.SPLASH)];p.summonData.statStages[Stat.SPD-1]=-6;}
      A0.customPokemonData.ability=[AbilityId.BALL_FETCH,AbilityId.SIMPLE,AbilityId.CONTRARY][i%3];A0.summonData.ability=A0.customPokemonData.ability;A0.summonData.statStages[Stat.SPD-1]=6;A0.summonData.statStages[Stat.ATK-1]=-3;A0.moveset=[new PokemonMove(MoveId.SPECTRAL_THIEF)];
      for(const p of actors.slice(1)){p.customPokemonData.ability=[AbilityId.CLEAR_BODY,AbilityId.MIRROR_ARMOR,AbilityId.DEFIANT][i%3];p.summonData.ability=p.customPokemonData.ability;p.summonData.statStages[Stat.ATK-1]=3;p.summonData.statStages[Stat.DEF-1]=6;p.summonData.statStages[Stat.SPD-1]=2;}
      const factory=modifierTypes.MULTI_LENS,mod:any=factory().withIdFromFunc(factory).newModifier(A0);mod.stackCount=1+i%2;scene.modifiers.push(mod);
    }
    const spectralBefore=observed.mock.results.length;
    // the parties in field order:''')
put('    written++;', '    if(i>=8192){if(observed.mock.results.length-spectralBefore<2)throw Error("Multi Lens control did not repeat Spectral Thief");multiLensControls++;}\n    written++;')
put('  if(threwCount||applied<100)', '  if(threwCount||applied<100||multiLensControls<128)')
put('`SPECTRAL_THIEF_TURNS applied=${applied}`','`SPECTRAL_THIEF_TURNS applied=${applied} multiLensControls=${multiLensControls}`')
put('process.env.DOUBLES_N ?? 8192','process.env.DOUBLES_N ?? 8320')
put('it("wide native combat doubles turns"','it("native Spectral Thief doubles turns"')
out.write_text(s)
