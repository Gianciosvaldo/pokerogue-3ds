#!/usr/bin/env python3
"""Dedicated berry-move matrix on the unchanged native doubles turn driver."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
s=(root/'tests/doubles.producer.test.ts').read_text()
def put(before,after):
 global s
 assert s.count(before)==1,before[:80]
 s=s.replace(before,after)
put('import { allAbilities, allMoves } from "#data/data-lists";','import { allAbilities, allMoves, modifierTypes } from "#data/data-lists";\nimport { generateModifierType } from "#mystery-encounters/encounter-phase-utils";\nimport { getModifierType } from "#utils/modifier-utils";')
put('    // the parties in field order:', '''    const actors=[A0,A1,D0,D1],berryMoves=[MoveId.STUFF_CHEEKS,MoveId.TEATIME,MoveId.BUG_BITE,MoveId.PLUCK];
    const berryAbilities=[AbilityId.BALL_FETCH,AbilityId.RIPEN,AbilityId.CHEEK_POUCH,AbilityId.STICKY_HOLD,AbilityId.UNBURDEN,AbilityId.SIMPLE,AbilityId.CONTRARY];
    for(let k=0;k<4;k++){
      const p=actors[k];p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.level=50;p.nature=0;p.ivs=Array(6).fill(15);
      p.customPokemonData.ability=-1;p.summonData.ability=berryAbilities[(Math.floor(i/44)+k)%berryAbilities.length];p.passive=false;p.customPokemonData.passive=AbilityId.NONE;p.status=null;p.switchOutStatus=false;p.calculateStats();p.generateName();p.hp=p.getMaxHp();
      for(let st=0;st<7;st++)p.summonData.statStages[st]=[0,5,6,-6][(Math.floor(i/11)+st+k)%4];
      p.moveset=[new PokemonMove(berryMoves[(i+k)%4])];
    }
    // Retain the established comparator's spread-hit and simultaneous-faint floors.
    if(i%16===15){A0.moveset=[new PokemonMove(MoveId.EXPLOSION)];A1.hp=D0.hp=D1.hp=1;}
    else if(i%16===14)A0.moveset=[new PokemonMove(MoveId.SURF)];
    if(i>=2048)for(let k=0;k<4;k++){
      const p=actors[k];p.summonData.ability=AbilityId.BALL_FETCH;p.summonData.statStages.fill(0);p.summonData.statStages[Stat.SPD-1]=k===0?6:-6;p.moveset=[new PokemonMove(k===0?MoveId.HYPER_VOICE:MoveId.SPLASH)];
    }
    // the parties in field order:''')
put('    battle.playerParticipantIds.add(A0.id);', '''    for(let k=0;k<4;k++)if(i%19!==0){
      const p=[A0,A1,D0,D1][k],list=k<2?scene.modifiers:scene.enemyModifiers;
      for(let j=0;j<2;j++){const type:any=generateModifierType(modifierTypes.BERRY,[(Math.floor(i/64)+k+j)%11]);if(!type)throw Error("berry type missing");const m:any=type.newModifier(p);m.stackCount=1+Math.floor(i/704)%2;list.push(m);}
    }
    for(let side=0;side<2;side++){const stack=(Math.floor(i/11)+side)%4;if(stack){const type:any=getModifierType(modifierTypes.BERRY_POUCH),m:any=type.newModifier();m.stackCount=stack;(side?scene.enemyModifiers:scene.modifiers).push(m);}}
    scene.updateModifiers(true,true);scene.updateModifiers(false,true);
    battle.playerParticipantIds.add(A0.id);''')
s=s.replace('process.env.DOUBLES_N ?? 2000','process.env.DOUBLES_N ?? 2050')
s=s.replace('it("doubles turn producer"','it("complete doubles turns with berry moves"')
Path(sys.argv[1]).write_text(s)
