#!/usr/bin/env python3
"""Delegate real pinned type-change handlers over an unchanged turn scaffold."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text().replace('SPEEDBOOST','PROTEAN').replace('SpeedBoost','PokemonTypeChange').replace('Speed Boost','Protean / Libero')
def replace(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b,1)
replace('import { BattleScene }','import { applyAbAttrs } from "#abilities/apply-ab-attrs";\nimport { PokemonType } from "#enums/pokemon-type";\nimport { Terrain } from "#data/terrain";\nimport { TerrainType } from "#enums/terrain-type";\nimport { BattleScene }')
replace('N < 20','N < 80 || N > 100000')
replace('must be an integer >=20','must be an integer in [80,100000]')
replace('    let seed = Number(process.env.PROTEAN_SEED ?? 12345);','    let seed = Number(process.env.PROTEAN_SEED ?? 12345);\n    if(!Number.isSafeInteger(seed)||seed<0||seed>0xffffffff)throw new Error("invalid seed");')
s=s.replace('Array(30).fill(0)','Array(40).fill(0)')
replace('state=Math.floor(i/2)%30;','state=Math.floor(i/2)%40;\n        const family=Math.floor(state/20),mode=state%20;')
a=s.index('        holder.summonData.ability=AbilityId.SPEED_BOOST;');b=s.index('        let before: string[];',a)
s=s[:a]+'''        const ability=family?AbilityId.LIBERO:AbilityId.PROTEAN;
        holder.summonData.ability=ability;
        for(const p of [holder,other]){p.level=50;p.ivs.fill(15);p.nature=0;p.calculateStats();p.hp=p.getMaxHp();}
        let move=MoveId.WATER_GUN;
        if(mode===1)move=MoveId.TACKLE;
        if(mode===2)move=MoveId.THUNDER_WAVE;
        if(mode===3)holder.summonData.abilitySuppressed=true;
        if(mode===4){holder.summonData.ability=AbilityId.BALL_FETCH;holder.passive=true;holder.customPokemonData.passive=ability;}
        if(mode===5){holder.isTerastallized=true;holder.teraType=PokemonType.GRASS;}
        if(mode===6){move=MoveId.STRUGGLE;holder.summonData.types=[PokemonType.GRASS,PokemonType.POISON];}
        if(mode===7){move=MoveId.NATURE_POWER;holder.summonData.types=[PokemonType.GRASS,PokemonType.POISON];}
        if(mode===8){move=MoveId.COPYCAT;battle.lastMove=MoveId.WATER_GUN;}
        if(mode===9)holder.summonData.types=[PokemonType.WATER];
        if(mode===10)holder.summonData.types=[PokemonType.GRASS,PokemonType.POISON];
        if(mode===11)holder.summonData.addedType=PokemonType.GRASS;
        if(mode===12){holder.summonData.abilitiesApplied.add(ability);holder.waveData.abilitiesApplied.add(ability);}
        if(mode===13){holder.passive=true;holder.customPokemonData.passive=family?AbilityId.PROTEAN:AbilityId.LIBERO;}
        if(mode===14){holder.summonData.ability=AbilityId.AERILATE;holder.passive=true;holder.customPokemonData.passive=ability;move=MoveId.TACKLE;}
        if(mode===15){holder.summonData.ability=AbilityId.NORMALIZE;holder.passive=true;holder.customPokemonData.passive=ability;}
        if(mode===16)other.summonData.ability=AbilityId.COLOR_CHANGE;
        if(mode===17){holder.summonData.types=[PokemonType.GRASS,PokemonType.POISON];move=MoveId.QUICK_ATTACK;scene.arena.terrain=new Terrain(TerrainType.PSYCHIC,5,5);}
        if(mode===18){move=MoveId.FORESTS_CURSE;other.summonData.types=[PokemonType.GRASS];}
        if(mode===19)move=MoveId.DIVE;
        commandMove(other,MoveId.TACKLE);commandMove(holder,move);
''' +s[b:]
a=s.index('    if(N>=2000 &&');b=s.index('    PokemonTypeChangeAbAttr.prototype.canApply=originalCan;',a)
s=s[:a]+'''    if(N>=2000 && (![0,2,4,7,8,10,11,12,13,14,16,17,18,19].every(k=>applies[k]>0&&applies[k+20]>0)||![1,5,6,7,9,15].every(k=>refusals[k]>0&&refusals[k+20]>0)))throw new Error("missing actual type-change coverage");
''' +s[b:]
replace('    const out:string[]=[];','''    if(process.env.PROTEAN_HELPERS==="1"){
        const rows:string[]=[];
        for(const p of [A,D])for(const ability of [AbilityId.PROTEAN,AbilityId.LIBERO])for(const simulated of [false,true])for(const mode of [0,1,2,3,4,5,6,7,8,9]){
            p.resetSummonData();p.resetBattleAndWaveData();p.summonData.ability=ability;p.passive=false;p.hp=p.getMaxHp();p.isTerastallized=false;
            p.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);p.formIndex=0;p.fusionSpecies=null;
            let move=MoveId.WATER_GUN;
            if(mode===1)move=MoveId.TACKLE;
            if(mode===2)move=MoveId.STRUGGLE;
            if(mode===3)move=MoveId.NATURE_POWER;
            if(mode===4)move=MoveId.COPYCAT;
            if([2,3,4].includes(mode))p.summonData.types=[PokemonType.GRASS,PokemonType.POISON];
            if(mode===5)p.isTerastallized=true,p.teraType=PokemonType.GRASS;
            if(mode===6)p.summonData.types=[PokemonType.WATER];
            if(mode===7)p.summonData.types=[PokemonType.GRASS,PokemonType.POISON];
            if(mode===8)p.summonData.addedType=PokemonType.GRASS;
            if(mode===9)p.summonData.abilitiesApplied.add(ability),p.waveData.abilitiesApplied.add(ability);
            const input=L(p.summonData.types),rngBefore=[Phaser.Math.RND.state(),battle.battleSeedState].join("/");
            applyAbAttrs("PokemonTypeChangeAbAttr",{pokemon:p,opponent:p===A?D:A,move:allMoves[move],simulated});
            if(rngBefore!==[Phaser.Math.RND.state(),battle.battleSeedState].join("/"))throw new Error("type-change advanced RNG");
            rows.push(`H ${rows.length} ${p.isPlayer()?0:1} ${ability} ${move} ${+simulated} ${mode} ${input} ${L(p.summonData.types)} ${p.waveData.abilitiesApplied.size} ${p.summonData.abilitiesApplied.size}`);
        }
        fs.writeFileSync(OUT,`PROTEAN_HELPERS 1 ${rows.length}\\n`+rows.join("\\n")+`\\nPROTEAN_HELPERS_COMPLETE ${rows.length}\\n`);
        console.log(`PROTEAN actual dispatch helpers=${rows.length}`);return;
    }
    const out:string[]=[];''')
Path(sys.argv[2]).write_text(s)
