#!/usr/bin/env python3
"""Dedicated Baton inventory owner matrix on the established doubles driver."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
s=(root/'tests/doubles.producer.test.ts').read_text()
s=s.replace('import { allAbilities, allMoves }', 'import { allAbilities, allMoves, modifierTypes }')
s=s.replace('import { it } from "vitest";', 'import { it } from "vitest";\nimport { getModifierType } from "#utils/modifier-utils";')
s=s.replace('playerBench.slice(0, rnd(playerBench.length + 1))','playerBench.slice(0, 2)')
needle='    battle.playerParticipantIds.add(A0.id);'
assert s.count(needle)==1
s=s.replace(needle,'''    const actors=[A0,A1,D0,D1];
    for(let k=0;k<4;k++){
      const p=actors[k];p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);
      p.formIndex=0;p.fusionSpecies=null;p.level=50;p.nature=0;p.ivs=Array(6).fill(15);
      p.customPokemonData.nature=-1;p.summonData.ability=AbilityId.UNBURDEN;
      p.calculateStats();p.generateName();p.hp=p.getMaxHp();p.summonData.statStages.fill(0);
      p.moveset=[new PokemonMove(MoveId.TACKLE)];
    }
    const passer=i%2;
    actors[passer].moveset=[new PokemonMove(MoveId.BATON_PASS)];
    actors[passer].summonData.statStages[Stat.ATK-1]=2;
    actors[passer].summonData.statStages[Stat.SPD-1]=-1;
    const addBaton=(p:any,locked:boolean)=>{
      const m:any=getModifierType(modifierTypes.BATON).newModifier(p);m.isTransferable=!locked;
      (p.isPlayer()?scene.modifiers:scene.enemyModifiers).push(m);
    };
    if(i%3!==0)addBaton(actors[passer],i%5===0);
    addBaton(D0,i%7===0);addBaton(D1,false);
    for(const [j,b] of party.slice(2).entries()){
      b.species=speciesDataRegistry.getSpecies(SpeciesId.PIDGEY);b.formIndex=0;b.fusionSpecies=null;
      b.level=50;b.nature=0;b.customPokemonData.nature=-1;b.ivs=Array(6).fill(15);
      b.customPokemonData.ability=AbilityId.UNBURDEN;b.customPokemonData.passive=AbilityId.NONE;b.passive=false;
      b.calculateStats();b.generateName();b.hp=b.getMaxHp();b.status=null;
      if(Math.floor(i/6)%3===1||(Math.floor(i/6)%3===2&&j===0))addBaton(b,i%11===0);
    }
    if(i%16===15){A0.moveset=[new PokemonMove(MoveId.EXPLOSION)];A1.hp=D0.hp=D1.hp=1;}
    if(i>=2048){A0.moveset=[new PokemonMove(MoveId.HYPER_VOICE)];}
    battle.playerParticipantIds.add(A0.id);''')
s=s.replace('it("doubles turn producer"','it("Baton actual complete doubles turns"')
Path(sys.argv[1]).write_text(s)
