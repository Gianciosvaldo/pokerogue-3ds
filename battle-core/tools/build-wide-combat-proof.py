#!/usr/bin/env python3
"""Wide-level native full doubles turns; independent of existing random corpus."""
from pathlib import Path
import sys
r=Path(__file__).resolve().parents[1];s=(r/'tests/doubles.producer.test.ts').read_text()
def put(before,after):
 global s
 assert s.count(before)==1,before[:80]
 s=s.replace(before,after)
put('import { it } from "vitest";','import { it,vi } from "vitest";\nimport { getBattlerTag } from "#data/battler-tags";\nimport { getGameMode } from "#app/game-mode";\nimport { GameModes } from "#enums/game-modes";')
put('  let written = 0;','  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");\n  let written = 0;')
put('    // the parties in field order:','''    const actors=[A0,A1,D0,D1],levels=[1,99,100,101,254,255,256,65534,65535,65536,100000,1000000],moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER];
    scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;battle.waveIndex=[201,1000,5850,10000,100000,1000000][Math.floor(i/16)%6];
    for(let k=0;k<4;k++){
      const p=actors[k];p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=AbilityId.BALL_FETCH;p.passive=false;p.customPokemonData.passive=AbilityId.NONE;p.status=null;p.switchOutStatus=false;p.nature=0;p.ivs=Array(6).fill(15);
      p.level=levels[(Math.floor(i/4)+k)%levels.length];if(i%4===0)p.level=k===Math.floor(i/16)%4?1000000:1;if(i%4===1)p.level=k===Math.floor(i/16)%4?1:1000000;
      p.calculateStats();p.generateName();p.hp=p.getMaxHp();p.summonData.statStages.fill(0);p.summonData.statStages[Stat.SPD-1]=k%2===0?6:-6;p.moveset=[new PokemonMove(moves[(Math.floor(i/4)+k)%moves.length])];
      if(i%3!==0){const tag:any=getBattlerTag(BattlerTagType.SUBSTITUTE,0,MoveId.SUBSTITUTE,p.id);tag.hp=Math.max(1,Math.floor(p.getMaxHp()/4));tag.sourceInFocus=false;tag.sprite=scene.addFieldSprite(p.x,p.y,`pkmn${p.isPlayer()?"__back":""}__sub`);p.summonData.tags.push(tag);}
    }
    if(i%16>=13){for(const p of actors){p.summonData.tags=[];p.level=50;p.calculateStats();p.hp=p.getMaxHp();p.moveset=[new PokemonMove(MoveId.SPLASH)];}A0.summonData.statStages[Stat.SPD-1]=6;for(const p of actors.slice(1))p.summonData.statStages[Stat.SPD-1]=-6;A0.moveset=[new PokemonMove([MoveId.HYPER_VOICE,MoveId.SURF,MoveId.EXPLOSION][i%16-13])];if(i%16===15)A1.hp=D0.hp=D1.hp=1;}
    // the parties in field order:''')
put('      b.species = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);','      b.species = speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR);')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  const results=rawDamage.mock.results.filter(x=>x.type==="return").map(x=>(x.value as any).damage),wide=results.filter(x=>x>2147483647).length;
  if(threwCount||wide<100)throw Error(`wide native combat missing: exceptions ${threwCount}, wide ${wide}`);
  console.log(`WIDE_COMBAT raw=${results.length} aboveInt32=${wide} max=${Math.max(...results)}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
s=s.replace('process.env.DOUBLES_N ?? 2000','process.env.DOUBLES_N ?? 8192').replace('it("doubles turn producer"','it("wide native combat doubles turns"')
Path(sys.argv[1]).write_text(s)
