#!/usr/bin/env python3
"""Actual Secret Power full turns with native biome/terrain and rainbow state."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:90]
 s=s.replace(a,b,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");
  const observed=vi.spyOn(allMoves[MoveId.SECRET_POWER].getAttrs("SecretPowerAttr")[0],"apply");''')
put('import { it,vi } from "vitest";', 'import { it,vi } from "vitest";\nimport { BiomeId } from "#enums/biome-id";\nimport { ArenaTagType } from "#enums/arena-tag-type";\nimport { ArenaTagSide } from "#enums/arena-tag-side";\nimport { getArenaTag } from "#data/arena-tag";\nimport { Terrain } from "#data/terrain";')
put('moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER]','moves=[MoveId.SECRET_POWER,MoveId.TACKLE]')
put('      if(i%3!==0){','''      p.customPokemonData.ability=[AbilityId.BALL_FETCH,AbilityId.SERENE_GRACE,AbilityId.SHIELD_DUST,AbilityId.SHEER_FORCE,AbilityId.INNER_FOCUS,AbilityId.CLEAR_BODY,AbilityId.MIRROR_ARMOR,AbilityId.SIMPLE,AbilityId.CONTRARY][(Math.floor(i/32)+k)%9];p.summonData.ability=p.customPokemonData.ability;
      if(i%3!==0){''')
put('    // the parties in field order:',r'''    scene.arena.biomeId=Object.values(BiomeId)[i%Object.values(BiomeId).length];const terrain=Math.floor(i/35)%5;scene.arena.terrain=terrain?new Terrain(terrain,10,10):null;
    if(i%2===0){const tag=getArenaTag(ArenaTagType.WATER_FIRE_PLEDGE,4,MoveId.WATER_PLEDGE,A0.id,i%4===0?ArenaTagSide.PLAYER:ArenaTagSide.ENEMY);if(!tag)throw Error("rainbow factory");scene.arena.tags.push(tag);}
    // the parties in field order:''')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  const applied=observed.mock.results.filter(x=>x.type==="return"&&x.value===true).length;
  if(applied<100)throw Error(`Secret Power owning callbacks missing ${applied}`);
  console.log(`SECRET_POWER_TURNS applied=${applied} calls=${observed.mock.calls.length}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
put('it("wide native combat doubles turns"','it("native Secret Power doubles turns"')
out.write_text(s)
