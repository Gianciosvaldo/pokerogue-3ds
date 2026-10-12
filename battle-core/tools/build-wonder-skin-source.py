#!/usr/bin/env python3
"""Additive real-turn and dispatch oracle over the unchanged Speed Boost scaffold."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text().replace('SPEEDBOOST','WONDERSKIN').replace('SpeedBoost','WonderSkin').replace('Speed Boost','Wonder Skin')
def replace(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b,1)
replace('import { BattleScene }','import { applyAbAttrs } from "#abilities/apply-ab-attrs";\nimport { NumberHolder } from "#utils/common";\nimport { ArenaTagSide } from "#enums/arena-tag-side";\nimport { ArenaTagType } from "#enums/arena-tag-type";\nimport { getArenaTag } from "#data/arena-tag";\nimport { BattleScene }')
replace('N < 20','N < 20 || N > 100000')
replace('    let seed = Number(process.env.WONDERSKIN_SEED ?? 12345);','    let seed = Number(process.env.WONDERSKIN_SEED ?? 12345);\n    if(!Number.isSafeInteger(seed)||seed<0||seed>0xffffffff)throw new Error("invalid seed");')
s=s.replace('Array(30).fill(0)','Array(20).fill(0)')
replace('state=Math.floor(i/2)%30;','state=Math.floor(i/2)%20;')
replace('                    + (m.berryType !== undefined', '                    + (m.constructor.name === "PokemonMoveAccuracyBoosterModifier" ? ` amount=${m.accuracyAmount} virtual=${m.virtualStackCount}` : "")\n                    + (m.berryType !== undefined')
a=s.index('        holder.summonData.ability=AbilityId.SPEED_BOOST;');b=s.index('        let before: string[];',a)
s=s[:a]+'''        holder.summonData.ability=AbilityId.WONDER_SKIN;
        let move=MoveId.THUNDER_WAVE;
        if(state===1){move=MoveId.DARK_VOID;other.species=speciesDataRegistry.getSpecies(SpeciesId.DARKRAI);other.generateName();other.calculateStats();other.hp=other.getMaxHp();}
        if(state===2)move=MoveId.TACKLE;
        if(state===3)move=MoveId.SWIFT;
        if(state===4)move=MoveId.TEARFUL_LOOK;
        if(state===5)holder.summonData.abilitySuppressed=true;
        if(state===6){holder.summonData.ability=AbilityId.BALL_FETCH;holder.passive=true;holder.customPokemonData.passive=AbilityId.WONDER_SKIN;}
        if([7,8,9].includes(state)){holder.summonData.statStages[Stat.EVA-1]=state===7?6:-6;other.summonData.statStages[Stat.ACC-1]=state===9?6:-6;}
        if([10,12].includes(state))scene.arena.weather=new Weather(WeatherType.FOG,5,5);
        if([11,12].includes(state))scene.arena.tags.push(getArenaTag(ArenaTagType.GRAVITY,5,MoveId.GRAVITY,other.id,ArenaTagSide.BOTH));
        if([13,14,15].includes(state)){
            const mod=modifierTypes.WIDE_LENS().withIdFromFunc(modifierTypes.WIDE_LENS).newModifier(other) as any;
            mod.stackCount=state===14?2:1;
            if(state===15)mod.pokemonId=holder.id;
            (other.isPlayer()?scene.modifiers:scene.enemyModifiers).push(mod);
        }
        if(state===16)holder.summonData.ability=AbilityId.BALL_FETCH;
        if(state===17)move=MoveId.SCREECH;
        if(state===18)move=MoveId.SING;
        if(state===19)holder.passive=true,holder.customPokemonData.passive=AbilityId.WONDER_SKIN;
        commandMove(other,move);commandMove(holder,MoveId.SPLASH);
''' +s[b:]
replace('    if(N>=2000 && (![0,5,6,7,8,9,10,11,12,13,14,15,16,17,18,24,28,29].every(k=>applies[k]>0)||![1,2,3,26,27].every(k=>refusals[k]>0)))throw new Error("missing actual Wonder Skin coverage");', '    if(N>=2000 && (![0,1,6,7,8,9,10,11,12,13,14,15,17,18,19].every(k=>applies[k]>0)||![2,3,4].every(k=>refusals[k]>0)))throw new Error("missing actual Wonder Skin coverage");')
replace('            before = dumpState(true);', '            before = dumpState(true);\n            const rngBefore=[Phaser.Math.RND.state(),battle.battleSeedState].join("/");\n            for(const [p,target] of [[A,D],[D,A]]){\n                const accuracy=allMoves[move].calculateBattleAccuracy(p,target,true);\n                before.push(`ACCURACY b=${p.getBattlerIndex()} move=${move} value=${accuracy}`);\n            }\n            if(rngBefore!==[Phaser.Math.RND.state(),battle.battleSeedState].join("/"))throw new Error("accuracy probe advanced RNG");')
replace('    const out:string[]=[];', '''    if(process.env.WONDERSKIN_HELPERS==="1"){
        const rows:string[]=[];
        for(const p of [A,D])for(const move of [MoveId.THUNDER_WAVE,MoveId.TACKLE])for(const simulated of [false,true])for(const value of [-1,0,49,49.9,50,50.1,100]){
            p.resetSummonData();p.resetBattleAndWaveData();p.summonData.ability=AbilityId.WONDER_SKIN;p.passive=false;p.hp=p.getMaxHp();
            const accuracy=new NumberHolder(value);
            applyAbAttrs("WonderSkinAbAttr",{pokemon:p,opponent:p===A?D:A,move:allMoves[move],simulated,accuracy});
            rows.push(`H ${rows.length} ${p.isPlayer()?0:1} ${move} ${+simulated} ${value} ${accuracy.value} ${p.waveData.abilitiesApplied.size} ${p.summonData.abilitiesApplied.size}`);
        }
        fs.writeFileSync(OUT,`WONDERSKIN_HELPERS 1 ${rows.length}\\n`+rows.join("\\n")+`\\nWONDERSKIN_HELPERS_COMPLETE ${rows.length}\\n`);
        console.log(`WONDERSKIN actual dispatch helpers=${rows.length}`);return;
    }
    const out:string[]=[];''')
Path(sys.argv[2]).write_text(s)
