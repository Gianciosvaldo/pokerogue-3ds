#!/usr/bin/env python3
"""Build a dedicated owner matrix from the unchanged doubles turn driver."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
s=(root/'tests/doubles.producer.test.ts').read_text()
s=s.replace('import { allAbilities, allMoves }', 'import { allAbilities, allMoves, modifierTypes }')
s=s.replace('import { it } from "vitest";', 'import { it } from "vitest";\nimport { getModifierType } from "#utils/modifier-utils";')
needle='    // the parties in field order:'
assert s.count(needle)==1
s=s.replace(needle,'''    const actors=[A0,A1,D0,D1];
    for(let k=0;k<4;k++){
      const p=actors[k];
      p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);
      p.formIndex=0;p.fusionSpecies=null;p.level=50;p.nature=0;p.ivs=Array(6).fill(15);
      p.summonData.ability=i%7===0&&k===2?AbilityId.STICKY_HOLD:i%11===0?AbilityId.UNBURDEN:AbilityId.BALL_FETCH;
      p.calculateStats();p.generateName();p.hp=p.getMaxHp();p.summonData.statStages.fill(0);
      p.moveset=[new PokemonMove([MoveId.TACKLE,MoveId.WATER_GUN,MoveId.SPLASH,MoveId.SURF][(i+k)%4])];
      const list=p.isPlayer()?scene.modifiers:scene.enemyModifiers;
      const grip:any=getModifierType(modifierTypes.GRIP_CLAW).newModifier(p);grip.stackCount=1+(i+k)%5;list.push(grip);
      const stock:any=getModifierType(modifierTypes.WHITE_HERB).newModifier(p);stock.stackCount=2;list.push(stock);
    }
    if(i%16===15){A0.moveset=[new PokemonMove(MoveId.EXPLOSION)];A1.hp=D0.hp=D1.hp=1;}
    // Append targeting controls without replacing a single original matrix ID.
    if(i>=2048&&i<2050){
      A0.moveset=[new PokemonMove(MoveId.HYPER_VOICE)];
      for(const p of [A1,D0,D1])p.moveset=[new PokemonMove(MoveId.SPLASH)];
    }
    if(i>=2050){
      for(const p of [A0,A1])p.moveset=[new PokemonMove(MoveId.TACKLE)];
      for(const p of [D0,D1])p.moveset=[new PokemonMove(MoveId.SPLASH)];
      const context=(i-2050)%4;
      if(context===0){
        for(const p of [D0,D1]){p.species=speciesDataRegistry.getSpecies(SpeciesId.GENGAR);p.calculateStats();p.generateName();p.hp=p.getMaxHp();}
      }else if(context===1){
        for(const p of [D0,D1])p.moveset=[new PokemonMove(MoveId.PROTECT)];
      }else if(context===2){
        for(const p of [A0,A1])p.summonData.statStages[Stat.ACC-1]=-6;
        for(const p of [D0,D1])p.summonData.statStages[Stat.EVA-1]=6;
      }else{
        for(const p of [D0,D1]){p.species=speciesDataRegistry.getSpecies(SpeciesId.GENGAR);p.calculateStats();p.generateName();p.hp=p.getMaxHp();p.moveset=[new PokemonMove(MoveId.SUBSTITUTE)];}
        for(const p of [A0,A1])p.moveset=[new PokemonMove(MoveId.WATER_GUN)];
      }
    }
    // the parties in field order:''')
s=s.replace('it("doubles turn producer"','it("Grip Claw actual complete doubles turns"')
Path(sys.argv[1]).write_text(s)
