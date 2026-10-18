#!/usr/bin/env python3
"""Additive matrix on the unchanged complete doubles driver."""
from pathlib import Path
import sys
r=Path(__file__).resolve().parents[1];s=(r/'tests/doubles.producer.test.ts').read_text()
def put(before,after):
 global s
 assert s.count(before)==1,before[:80]
 s=s.replace(before,after)
put('import { it } from "vitest";','import { it,vi } from "vitest";\nimport { PostTurnHurtIfSleepingAbAttr } from "#abilities/ab-attrs";')
put('  let written = 0;','  const leaf=vi.spyOn(PostTurnHurtIfSleepingAbAttr.prototype,"apply");\n  const positive=[0,0,0,0];\n  let written = 0;')
put('    // the parties in field order:','''    const actors=[A0,A1,D0,D1],holder=Math.floor(i/32)%4,mode=Math.floor(i/128)%16;
    for(let k=0;k<4;k++){
      const p=actors[k];p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.level=50;p.nature=0;p.ivs=Array(6).fill(15);
      p.customPokemonData.ability=-1;p.summonData.ability=AbilityId.BALL_FETCH;p.passive=false;p.customPokemonData.passive=AbilityId.NONE;
      if(k===holder){p.summonData.ability=mode%4===1?AbilityId.BALL_FETCH:mode%4===3?AbilityId.MAGIC_GUARD:AbilityId.BAD_DREAMS;p.customPokemonData.passive=mode%4===3||mode%4===1||mode%4===2?AbilityId.BAD_DREAMS:AbilityId.NONE;p.passive=p.customPokemonData.passive!==AbilityId.NONE;if(mode===6){p.customPokemonData.passive=AbilityId.MAGIC_GUARD;p.passive=true;}p.summonData.abilitySuppressed=mode===7;}
      else {const v=(mode+k)%8;if(v===1||v===5)p.summonData.ability=AbilityId.COMATOSE;else if(v===2)p.summonData.ability=AbilityId.MAGIC_GUARD;else if(v===3||v===6){p.customPokemonData.passive=v===3?AbilityId.MAGIC_GUARD:AbilityId.COMATOSE;p.passive=true;}p.summonData.abilitySuppressed=mode===8;}
      p.status=(i+k)%5===0?null:new Status(StatusEffect.SLEEP);if(p.status)p.status.sleepTurnsRemaining=i%8===0?1:10;p.switchOutStatus=false;p.calculateStats();p.generateName();p.hp=(Math.floor(i/8)+k)%4===0?1:p.getMaxHp();
      p.summonData.statStages.fill(0);p.summonData.statStages[Stat.SPD-1]=i%3===0?0:k%2===0?6:-6;p.moveset=[new PokemonMove(MoveId.SPLASH)];
    }
    // Retain normal ally/spread/multiple-faint coverage requirements.
    if(i%16>=12){for(const p of actors){p.status=null;p.summonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.summonData.abilitySuppressed=false;p.hp=p.getMaxHp();}A0.summonData.statStages[Stat.SPD-1]=6;for(const p of actors.slice(1))p.summonData.statStages[Stat.SPD-1]=-6;A0.moveset=[new PokemonMove([MoveId.TACKLE,MoveId.HYPER_VOICE,MoveId.SURF,MoveId.EXPLOSION][i%16-12])];if(i%16===15)A1.hp=D0.hp=D1.hp=1;}
    // the parties in field order:''')
put('      b.species = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);','      b.species = speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR);')
# Gameplay calls are observed without replacing the leaf.
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  for(const call of leaf.mock.calls){const role=[A0,A1,D0,D1].indexOf(call[0].pokemon);if(role>=0)positive[role]++;}
  if(positive.some(n=>n<100)||threwCount)throw Error(`Bad Dreams coverage ${positive}, exceptions ${threwCount}`);
  console.log(`BAD_DREAMS_DOUBLES calls=${positive} total=${leaf.mock.calls.length}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
s=s.replace('process.env.DOUBLES_N ?? 2000','process.env.DOUBLES_N ?? 8192').replace('it("doubles turn producer"','it("Bad Dreams complete doubles turns"')
Path(sys.argv[1]).write_text(s)
