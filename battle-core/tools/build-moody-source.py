#!/usr/bin/env python3
"""Additive real-turn oracle over the unchanged Speed Boost source scaffold."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text().replace('SPEEDBOOST','MOODY').replace('SpeedBoost','Moody').replace('Speed Boost','Moody').replace('speed-boost','moody')
def replace(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b,1)
replace('N < 20','N < 20 || N > 100000')
replace('    let seed = Number(process.env.MOODY_SEED ?? 12345);','    let seed = Number(process.env.MOODY_SEED ?? 12345);\n    if(!Number.isSafeInteger(seed)||seed<0||seed>0xffffffff)throw new Error("invalid MOODY_SEED");')
s=s.replace('Array(30).fill(0)','Array(20).fill(0)')
replace('state=Math.floor(i/2)%30;','state=Math.floor(i/2)%20;')
a=s.index('        holder.summonData.ability=AbilityId.SPEED_BOOST;')
b=s.index('        let before: string[];',a)
s=s[:a]+'''        holder.summonData.ability=AbilityId.MOODY;
        if(state===1)holder.summonData.statStages.fill(6);
        if(state===2)holder.summonData.statStages.fill(-6);
        if(state===3){holder.summonData.statStages[Stat.ATK-1]=6;holder.summonData.statStages[Stat.SPD-1]=-6;}
        if(state===4){holder.summonData.statStages.fill(6);holder.summonData.statStages[Stat.ATK-1]=5;}
        if(state===5){holder.summonData.statStages.fill(-6);holder.summonData.statStages[Stat.SPD-1]=-5;}
        if(state===6)holder.summonData.abilitySuppressed=true;
        if([7,8,9].includes(state)){
            holder.summonData.ability=state===8?AbilityId.SIMPLE:state===9?AbilityId.CONTRARY:AbilityId.BALL_FETCH;
            holder.passive=true;holder.customPokemonData.passive=AbilityId.MOODY;
        }
        if([10,11].includes(state)){holder.passive=true;holder.customPokemonData.passive=state===10?AbilityId.SIMPLE:AbilityId.CONTRARY;}
        if(state===12)other.summonData.ability=AbilityId.MOODY;
        if(state===13)holder.summonData.ability=AbilityId.BALL_FETCH;
        if(state===14){holder.hp=0;holder.status=new Status(StatusEffect.FAINT);}
        if(state===15){holder.hp=1;other.level=100;other.calculateStats();other.hp=other.getMaxHp();}
        if(state===16){holder.tempSummonData.turnCount=0;holder.tempSummonData.waveTurnCount=0;}
        if(state===17){holder.turnData.switchedInThisTurn=true;holder.turnData.failedRunAway=true;}
        commandMove(other,[15,19].includes(state)?MoveId.TACKLE:MoveId.SPLASH);
        commandMove(holder,state===18?MoveId.PROTECT:state===19?MoveId.TACKLE:MoveId.SPLASH);
'''+s[b:]
replace('    if(N>=2000 && (![0,5,6,7,8,9,10,11,12,13,14,15,16,17,18,24,28,29].every(k=>applies[k]>0)||![1,2,3,26,27].every(k=>refusals[k]>0)))throw new Error("missing actual Moody coverage");','    if(N>=2000 && (![0,1,2,3,4,5,7,8,9,10,11,12,16,17,18,19].every(k=>applies[k]>0)||![6,13,14,15].every(k=>applies[k]===0)))throw new Error("missing actual Moody coverage");')
Path(sys.argv[2]).write_text(s)
