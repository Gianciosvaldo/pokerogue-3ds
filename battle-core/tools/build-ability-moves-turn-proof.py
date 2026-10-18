#!/usr/bin/env python3
"""Native ability-changing moves, including real gain/loss callbacks."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
subprocess.run([sys.executable,root/'tools/build-wide-combat-proof.py',out],check=True)
s=out.read_text()
def put(old,new):
 global s
 assert s.count(old)==1,old[:100]
 s=s.replace(old,new,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','  const observed=[MoveId.WORRY_SEED,MoveId.SIMPLE_BEAM,MoveId.ROLE_PLAY,MoveId.DOODLE,MoveId.ENTRAINMENT,MoveId.SKILL_SWAP].map((m,i)=>vi.spyOn(allMoves[m].getAttrs(["AbilityChangeAttr","AbilityChangeAttr","AbilityCopyAttr","AbilityCopyAttr","AbilityGiveAttr","SwitchAbilitiesAttr"][i])[0],"apply"));')
put('moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER]','moves=[MoveId.WORRY_SEED,MoveId.SIMPLE_BEAM,MoveId.ROLE_PLAY,MoveId.DOODLE,MoveId.ENTRAINMENT,MoveId.SKILL_SWAP]')
put('      if(i%3!==0){','      p.customPokemonData.ability=[AbilityId.BALL_FETCH,AbilityId.SIMPLE,AbilityId.CONTRARY,AbilityId.INTIMIDATE,AbilityId.DOWNLOAD,AbilityId.INTREPID_SWORD,AbilityId.DRIZZLE][(Math.floor(i/32)+k)%7];p.summonData.ability=p.customPokemonData.ability;\n      if(i%3!==0){')
put('''  const results=rawDamage.mock.results.filter(x=>x.type==="return").map(x=>(x.value as any).damage),wide=results.filter(x=>x>2147483647).length;
  if(threwCount||wide<100)throw Error(`wide native combat missing: exceptions ${threwCount}, wide ${wide}`);
  console.log(`WIDE_COMBAT raw=${results.length} aboveInt32=${wide} max=${Math.max(...results)}`);''','''  const applied=observed.map(spy=>spy.mock.results.filter(x=>x.type==="return"&&x.value===true).length);
  if(threwCount||applied.some(n=>n<100))throw Error(`ability move turns missing: exceptions ${threwCount}, applied ${applied}`);
  console.log(`ABILITY_MOVE_TURNS applied=${applied}`);''')
put('it("wide native combat doubles turns"','it("native ability-changing doubles turns"')
put('import { it,vi } from "vitest";', 'import { it,vi } from "vitest";\nimport { Weather } from "#data/weather";\nimport { WeatherType } from "#enums/weather-type";')
put('  let written = 0;', '  const loss=vi.spyOn(allAbilities[AbilityId.PRIMORDIAL_SEA].getAttrs("PreLeaveFieldClearWeatherAbAttr")[0],"apply");\n  let lossControls=0;\n  let written = 0;')
put('    // the parties in field order:', r'''    if(i>=8192){
      for(const p of actors){p.summonData.tags=[];p.level=50;p.calculateStats();p.hp=p.getMaxHp();p.summonData.statStages.fill(0);p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=AbilityId.BALL_FETCH;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.summonData.statStages[Stat.SPD-1]=-6;}
      A0.customPokemonData.ability=AbilityId.PRIMORDIAL_SEA;A0.summonData.ability=AbilityId.PRIMORDIAL_SEA;A0.summonData.statStages[Stat.SPD-1]=6;A0.moveset=[new PokemonMove([MoveId.ROLE_PLAY,MoveId.DOODLE,MoveId.SKILL_SWAP][i%3])];scene.arena.weather=new Weather(WeatherType.HEAVY_RAIN,0);
    }
    const lossBefore=loss.mock.calls.length;
    // the parties in field order:''')
put('    written++;', '    if(i>=8192){if(loss.mock.calls.length-lossBefore<1)throw Error("missing real primal weather loss in owning control");lossControls++;}\n    written++;')
put('  if(threwCount||applied.some(n=>n<100))', '  if(threwCount||applied.some(n=>n<100)||lossControls<128)')
put('`ABILITY_MOVE_TURNS applied=${applied}`', '`ABILITY_MOVE_TURNS applied=${applied} lossControls=${lossControls} lossCalls=${loss.mock.calls.length}`')
put('process.env.DOUBLES_N ?? 8192','process.env.DOUBLES_N ?? 8320')

out.write_text(s)
