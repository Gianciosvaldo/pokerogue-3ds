#!/usr/bin/env python3
"""Observe/delegate the actual pinned defending-field immunity in full turns."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text().replace('SPEEDBOOST','FIELDPRIORITY').replace('SpeedBoost','FieldPriorityMoveImmunity').replace('Speed Boost','Field priority immunity')
def replace(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b,1)
replace('import { BattleScene }','import { PokemonType } from "#enums/pokemon-type";\nimport { Terrain } from "#data/terrain";\nimport { TerrainType } from "#enums/terrain-type";\nimport { BattleScene }')
replace('N < 20','N < 96 || N > 100000')
replace('must be an integer >=20','must be an integer in [96,100000]')
replace('    let seed = Number(process.env.FIELDPRIORITY_SEED ?? 12345);','    let seed = Number(process.env.FIELDPRIORITY_SEED ?? 12345);\n    if(!Number.isSafeInteger(seed)||seed<0||seed>0xffffffff)throw new Error("invalid seed");')
s=s.replace('Array(30).fill(0)','Array(48).fill(0)')
replace('state=Math.floor(i/2)%30;','state=Math.floor(i/2)%48;\n        const family=Math.floor(state/16),mode=state%16;')
a=s.index('        holder.summonData.ability=AbilityId.SPEED_BOOST;');b=s.index('        let before: string[];',a)
s=s[:a]+'''        const ability=[AbilityId.QUEENLY_MAJESTY,AbilityId.DAZZLING,AbilityId.ARMOR_TAIL][family];
        holder.summonData.ability=ability;
        for(const p of [holder,other]){p.level=50;p.ivs.fill(15);p.nature=0;p.calculateStats();p.hp=p.getMaxHp();}
        let move=MoveId.QUICK_ATTACK;
        if(mode===1)move=MoveId.TACKLE;
        if(mode===2)move=MoveId.VITAL_THROW;
        if(mode===3)holder.summonData.abilitySuppressed=true;
        if(mode===4){holder.summonData.ability=AbilityId.BALL_FETCH;holder.passive=true;holder.customPokemonData.passive=ability;}
        if(mode===5)other.summonData.ability=AbilityId.MOLD_BREAKER;
        if([6,7,8,9].includes(mode)){other.summonData.ability=AbilityId.PRANKSTER;move=[MoveId.THUNDER_WAVE,MoveId.GROWL,MoveId.SPLASH,MoveId.PROTECT][mode-6];}
        if(mode===10)scene.arena.terrain=new Terrain(TerrainType.PSYCHIC,5,5);
        if(mode===11){other.summonData.ability=AbilityId.PROTEAN;other.summonData.types=[PokemonType.GRASS,PokemonType.POISON];}
        if(mode===12){move=MoveId.FAKE_OUT;other.tempSummonData.turnCount=1;other.tempSummonData.waveTurnCount=1;}
        if([13,14].includes(mode)){other.summonData.ability=AbilityId.GALE_WINGS;move=MoveId.WING_ATTACK;if(mode===14)other.hp--;}
        if(mode===15){holder.passive=true;holder.customPokemonData.passive=[AbilityId.QUEENLY_MAJESTY,AbilityId.DAZZLING,AbilityId.ARMOR_TAIL][(family+1)%3];}
        commandMove(other,move);commandMove(holder,MoveId.TACKLE);
''' +s[b:]
a=s.index('    if(N>=2000 &&');b=s.index('    FieldPriorityMoveImmunityAbAttr.prototype.canApply=originalCan;',a)
s=s[:a]+'''    if(N>=2000 && (![0,4,6,7,11,12,13,15].every(k=>[0,16,32].every(offset=>applies[k+offset]>0))||![1,2,14,15].every(k=>[0,16,32].every(offset=>refusals[k+offset]>0))))throw new Error("missing actual field priority coverage");
''' +s[b:]
Path(sys.argv[2]).write_text(s)
