#!/usr/bin/env python3
"""Real hit-count ability turns, delegated over the unchanged Speed Boost scaffold."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text().replace('SPEEDBOOST','MULTIHITABIL').replace('SpeedBoost','AddSecondStrike').replace('Speed Boost','Multi-hit abilities')
def replace(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b,1)
replace('import { AddSecondStrikeAbAttr }','import { AddSecondStrikeAbAttr, MaxMultiHitAbAttr }')
replace('import { BattleScene }','import { applyAbAttrs } from "#abilities/apply-ab-attrs";\nimport { NumberHolder } from "#utils/common";\nimport { PokemonType } from "#enums/pokemon-type";\nimport { BattleScene }')
replace('N < 20','N < 96 || N > 100000')
replace('must be an integer >=20','must be an integer in [96,100000]')
replace('    let seed = Number(process.env.MULTIHITABIL_SEED ?? 12345);','    let seed = Number(process.env.MULTIHITABIL_SEED ?? 12345);\n    if(!Number.isSafeInteger(seed)||seed<0||seed>0xffffffff)throw new Error("invalid seed");')
a=s.index('    let recordingTurn=false,state=0;');b=s.index('    const out:string[]=[];',a)
s=s[:a]+'''    let recordingTurn=false,state=0;
    const stateCounts=Array(48).fill(0),sideCounts=[0,0],applies=Array(48).fill(0),refusals=Array(48).fill(0);
    const observed=[AddSecondStrikeAbAttr,MaxMultiHitAbAttr];
    const originals=observed.map(cls=>[cls.prototype.canApply,cls.prototype.apply]);
    observed.forEach((cls,k)=>{
        cls.prototype.canApply=function(params:any){const result=originals[k][0].call(this,params);if(recordingTurn&&!result)refusals[state]++;return result;};
        cls.prototype.apply=function(params:any){const result=originals[k][1].call(this,params);if(recordingTurn)applies[state]++;return result;};
    });
''' +s[b:]
replace('state=Math.floor(i/2)%30;','state=Math.floor(i/2)%48;\n        const family=Math.floor(state/24),mode=state%24;')
a=s.index('        holder.summonData.ability=AbilityId.SPEED_BOOST;');b=s.index('        let before: string[];',a)
s=s[:a]+'''        const ability=family?AbilityId.SKILL_LINK:AbilityId.PARENTAL_BOND;
        holder.summonData.ability=ability;
        holder.level=50;holder.ivs.fill(15);holder.nature=0;holder.calculateStats();holder.hp=holder.getMaxHp();
        other.species=speciesDataRegistry.getSpecies(SpeciesId.SNORLAX);other.formIndex=0;other.generateName();
        other.level=100;other.ivs.fill(15);other.nature=0;other.calculateStats();other.hp=other.getMaxHp();
        let move=family?MoveId.DOUBLE_SLAP:MoveId.TACKLE;
        if(mode===1)holder.summonData.abilitySuppressed=true;
        if(mode===2){holder.summonData.ability=AbilityId.BALL_FETCH;holder.passive=true;holder.customPokemonData.passive=ability;}
        if(mode===3)move=MoveId.SPLASH;
        if(mode===4)move=MoveId.DOUBLE_KICK;
        if(mode===5)move=MoveId.TRIPLE_KICK;
        if(mode===6)move=MoveId.TACKLE;
        if(mode===7)move=MoveId.POLLEN_PUFF;
        if(mode===8)move=MoveId.HYPER_VOICE;
        if(mode===12)other.hp=1;
        if(mode===13){other.addTag(BattlerTagType.SUBSTITUTE,0,MoveId.SUBSTITUTE,other.id);pm.clearAllPhases();}
        if(mode===14)other.summonData.ability=AbilityId.STAMINA;
        if(mode===15)other.summonData.ability=AbilityId.WEAK_ARMOR;
        if(mode===16)holder.hp=1;
        if(mode===17){holder.summonData.ability=AbilityId.PARENTAL_BOND;holder.passive=true;holder.customPokemonData.passive=AbilityId.SKILL_LINK;move=MoveId.DOUBLE_SLAP;}
        if(mode===18){holder.summonData.ability=AbilityId.PARENTAL_BOND;holder.passive=true;holder.customPokemonData.passive=AbilityId.SKILL_LINK;move=MoveId.TACKLE;}
        if(mode===19){holder.isTerastallized=true;holder.teraType=PokemonType.NORMAL;move=MoveId.TACKLE;}
        if(mode===20)move=MoveId.DRAGON_RAGE;
        if(mode===21)holder.summonData.ability=AbilityId.BALL_FETCH;
        if(mode===22)move=MoveId.DIVE;
        if(mode===23)move=MoveId.ENDEAVOR;
        commandMove(holder,move);commandMove(other,mode===9?MoveId.PROTECT:MoveId.SPLASH);
        if([10,11].includes(mode)){
            const mod=modifierTypes.MULTI_LENS().withIdFromFunc(modifierTypes.MULTI_LENS).newModifier(holder) as any;
            mod.stackCount=mode===10?2:1;if(mode===11)mod.pokemonId=other.id;
            (holder.isPlayer()?scene.modifiers:scene.enemyModifiers).push(mod);
        }
''' +s[b:]
a=s.index('    if(N>=2000 &&');b=s.index('    fs.writeFileSync(OUT,',a)
s=s[:a]+'''    if(N>=2000 && (![0,2,6,7,8,10,11,12,13,14,15,16,18,19,20].every(k=>applies[k]>0)||![24,26,34,35,36,37,38,39,40,41].every(k=>applies[k]>0)||![3,4,5,17,23].every(k=>refusals[k]>0)))throw new Error("missing actual multi-hit ability coverage");
    observed.forEach((cls,k)=>{cls.prototype.canApply=originals[k][0];cls.prototype.apply=originals[k][1];});
''' +s[b:]
replace('    const out:string[]=[];','''    if(process.env.MULTIHITABIL_HELPERS==="1"){
        const rows:string[]=[];
        const moves=[MoveId.TACKLE,MoveId.SPLASH,MoveId.DOUBLE_SLAP,MoveId.DIVE,MoveId.FLING,MoveId.ROLLOUT,MoveId.ICE_BALL,MoveId.ENDEAVOR,MoveId.EXPLOSION,MoveId.HYPER_VOICE,MoveId.POLLEN_PUFF];
        for(const p of [A,D])for(const ability of [AbilityId.PARENTAL_BOND,AbilityId.SKILL_LINK])for(const simulated of [false,true])for(const move of moves)for(const input of [-1,0,1,3,19]){
            p.resetSummonData();p.resetBattleAndWaveData();p.summonData.ability=ability;p.passive=false;p.hp=p.getMaxHp();p.isTerastallized=false;
            const value=new NumberHolder(input),rngBefore=[Phaser.Math.RND.state(),battle.battleSeedState].join("/");
            applyAbAttrs(ability===AbilityId.PARENTAL_BOND?"AddSecondStrikeAbAttr":"MaxMultiHitAbAttr",{pokemon:p,opponent:p===A?D:A,move:allMoves[move],simulated,hitCount:value,hits:value});
            if(rngBefore!==[Phaser.Math.RND.state(),battle.battleSeedState].join("/"))throw new Error("helper advanced RNG");
            rows.push(`H ${rows.length} ${p.isPlayer()?0:1} ${ability} ${move} ${+simulated} ${input} ${value.value} ${p.waveData.abilitiesApplied.size} ${p.summonData.abilitiesApplied.size}`);
        }
        fs.writeFileSync(OUT,`MULTIHITABIL_HELPERS 1 ${rows.length}\\n`+rows.join("\\n")+`\\nMULTIHITABIL_HELPERS_COMPLETE ${rows.length}\\n`);
        console.log(`MULTIHITABIL actual dispatch helpers=${rows.length}`);return;
    }
    const out:string[]=[];''')
Path(sys.argv[2]).write_text(s)
