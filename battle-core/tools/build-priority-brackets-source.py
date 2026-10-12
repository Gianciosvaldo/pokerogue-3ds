#!/usr/bin/env python3
"""Observe actual upstream turn ordering over the unchanged Speed Boost scaffold."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text().replace('SPEEDBOOST','BRACKETS').replace('SpeedBoost','Brackets').replace('Speed Boost','Priority brackets')
def replace(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b,1)
replace('import { BattleScene }','import { applyAbAttrs } from "#abilities/apply-ab-attrs";\nimport { BattleScene }')
replace('import { BracketsAbAttr }', 'import { ChangeMovePriorityInBracketAbAttr, BypassSpeedChanceAbAttr, PreventBypassSpeedChanceAbAttr }')
replace('N < 20','N < 96 || N > 100000')
replace('must be an integer >=20','must be an integer in [96,100000]')
replace('    let seed = Number(process.env.BRACKETS_SEED ?? 12345);','    let seed = Number(process.env.BRACKETS_SEED ?? 12345);\n    if(!Number.isSafeInteger(seed)||seed<0||seed>0xffffffff)throw new Error("invalid seed");')
a=s.index('    let recordingTurn=false,state=0;');b=s.index('    const out:string[]=[];',a)
s=s[:a]+'''    let recordingTurn=false,state=0;
    const stateCounts=Array(48).fill(0),sideCounts=[0,0],applies=Array(3).fill(0),refusals=Array(3).fill(0);
    const observed=[ChangeMovePriorityInBracketAbAttr,BypassSpeedChanceAbAttr,PreventBypassSpeedChanceAbAttr];
    const originals=observed.map(cls=>[cls.prototype.canApply,cls.prototype.apply]);
    observed.forEach((cls,k)=>{
        cls.prototype.canApply=function(params:any){const result=originals[k][0].call(this,params);if(recordingTurn&&!result)refusals[k]++;return result;};
        cls.prototype.apply=function(params:any){const result=originals[k][1].call(this,params);if(recordingTurn)applies[k]++;return result;};
    });
''' +s[b:]
replace('state=Math.floor(i/2)%30;','state=Math.floor(i/2)%48;\n        const family=Math.floor(state/16),mode=state%16;')
a=s.index('        holder.summonData.ability=AbilityId.SPEED_BOOST;');b=s.index('        let before: string[];',a)
s=s[:a]+'''        const ability=[AbilityId.STALL,AbilityId.MYCELIUM_MIGHT,AbilityId.QUICK_DRAW][family];
        holder.summonData.ability=ability;
        for(const p of [holder,other]){p.level=50;p.ivs.fill(15);p.nature=0;p.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);p.calculateStats();p.hp=p.getMaxHp();p.generateName();}
        const slow=mode===1?holder:other;
        if(mode!==14){slow.species=speciesDataRegistry.getSpecies(SpeciesId.SLOWPOKE);slow.calculateStats();slow.hp=slow.getMaxHp();slow.generateName();}
        let holderMove=MoveId.TACKLE;
        if([2,9,10].includes(mode))holderMove=MoveId.SPLASH;
        if(mode===3)holderMove=MoveId.THUNDER_WAVE;
        if(mode===11)holderMove=MoveId.QUICK_ATTACK;
        if(mode===4)holder.summonData.abilitySuppressed=true;
        if(mode===5){holder.summonData.ability=AbilityId.BALL_FETCH;holder.passive=true;holder.customPokemonData.passive=ability;}
        if(mode===6){holder.passive=true;holder.customPokemonData.passive=family===2?AbilityId.STALL:AbilityId.QUICK_DRAW;}
        if(mode===10){holder.summonData.ability=AbilityId.QUICK_DRAW;holder.passive=true;holder.customPokemonData.passive=AbilityId.MYCELIUM_MIGHT;}
        if(mode===12)other.summonData.ability=AbilityId.STALL;
        if(mode===13)other.summonData.ability=AbilityId.QUICK_DRAW;
        if(mode===15){holder.summonData.statStages[Stat.SPD-1]=-6;other.summonData.statStages[Stat.SPD-1]=6;}
        commandMove(other,MoveId.TACKLE);commandMove(holder,holderMove);
        if(mode===7)holder.addTag(BattlerTagType.BYPASS_SPEED);
        if([8,9,10].includes(mode)){
            const mod=modifierTypes.QUICK_CLAW().withIdFromFunc(modifierTypes.QUICK_CLAW).newModifier(holder);
            (holder.isPlayer()?scene.modifiers:scene.enemyModifiers).push(mod);
        }
''' +s[b:]
replace('            before = dumpState(true);','''            const simBefore=[...monLines(A,"0"),...monLines(D,"2"),Phaser.Math.RND.state(),battle.battleSeedState].join("/");
            for(const p of [A,D])applyAbAttrs("BypassSpeedChanceAbAttr",{pokemon:p,simulated:true});
            if(simBefore!==[...monLines(A,"0"),...monLines(D,"2"),Phaser.Math.RND.state(),battle.battleSeedState].join("/"))throw new Error("simulated Quick Draw changed state");
            before = dumpState(true);
            const rngBefore=[Phaser.Math.RND.state(),battle.battleSeedState].join("/");
            for(const p of [A,D]){
                const move=battle.turnCommands[p.getBattlerIndex()].move.move;
                const value=allMoves[move].getPriorityModifier(p);
                before.push(`BRACKET b=${p.getBattlerIndex()} move=${move} value=${value}`);
            }
            if(rngBefore!==[Phaser.Math.RND.state(),battle.battleSeedState].join("/"))throw new Error("priority probe advanced RNG");''')
a=s.index('    if(N>=2000 &&');b=s.index('    fs.writeFileSync(OUT,',a)
s=s[:a]+'''    if(N>=2000 && (!applies.every(v=>v>0)||!refusals.every(v=>v>0)))throw new Error("missing actual priority dispatch coverage");
    observed.forEach((cls,k)=>{cls.prototype.canApply=originals[k][0];cls.prototype.apply=originals[k][1];});
''' +s[b:]
Path(sys.argv[2]).write_text(s)
