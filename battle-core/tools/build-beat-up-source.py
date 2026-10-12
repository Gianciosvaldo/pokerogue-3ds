#!/usr/bin/env python3
"""Observe real Beat Up count/power methods over the unchanged turn scaffold."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text().replace('SPEEDBOOST','BEATUP').replace('Speed Boost','Beat Up')
def replace(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b,1)
replace('import { SpeedBoostAbAttr } from "#abilities/ab-attrs";', 'import { MultiHitAttr, BeatUpAttr } from "#moves/move";\nimport { NumberHolder } from "#utils/common";')
replace('N < 20','N < 48 || N > 100000')
replace('must be an integer >=20','must be an integer in [48,100000]')
replace('    let seed = Number(process.env.BEATUP_SEED ?? 12345);','    let seed = Number(process.env.BEATUP_SEED ?? 12345);\n    if(!Number.isSafeInteger(seed)||seed<0||seed>0xffffffff)throw new Error("invalid seed");')
replace('    const playerBench = benchPool(true);','    const playerBench = benchPool(true);\n    const enemyBench = benchPool(false);')
a=s.index('    let recordingTurn=false,state=0;');b=s.index('    const out:string[]=[];',a)
s=s[:a]+'''    let recordingTurn=false,state=0;
    const stateCounts=Array(24).fill(0),sideCounts=[0,0],counts=Array(24).fill(0),powers=Array(24).fill(0),sizes=Array(7).fill(0);
    const originalCount=MultiHitAttr.prototype.getHitCount,originalPower=BeatUpAttr.prototype.apply;
    MultiHitAttr.prototype.getHitCount=function(...args:any[]){const result=originalCount.apply(this,args);if(recordingTurn&&this===(allMoves[MoveId.BEAT_UP].getAttrs("MultiHitAttr")[0]))counts[state]++;return result;};
    BeatUpAttr.prototype.apply=function(...args:any[]){const result=originalPower.apply(this,args);if(recordingTurn)powers[state]++;return result;};
''' +s[b:]
replace('state=Math.floor(i/2)%30;','state=Math.floor(i/2)%24;')
a=s.index('        holder.summonData.ability=AbilityId.SPEED_BOOST;');b=s.index('        let before: string[];',a)
s=s[:a]+'''        const partyOfHolder=holder.isPlayer()?scene.getPlayerParty():scene.getEnemyParty();
        const pool=holder.isPlayer()?playerBench:enemyBench;
        const size=1+state%6;
        partyOfHolder.length=0;partyOfHolder.push(holder,...pool.slice(0,size-1));sizes[size]++;
        const roster=[SpeciesId.RATTATA,SpeciesId.ONIX,SpeciesId.SLAKING,SpeciesId.SHUCKLE,SpeciesId.ALAKAZAM,SpeciesId.CASTFORM];
        for(let k=0;k<partyOfHolder.length;k++){
            const p=partyOfHolder[k];
            if(k && p.isOnField())scene.field.remove(p);
            p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.switchOutStatus=false;
            p.species=speciesDataRegistry.getSpecies(roster[(k+Math.floor(state/6))%roster.length]);p.formIndex=0;p.fusionSpecies=null;
            p.summonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;
            p.moveset=[new PokemonMove(MoveId.SPLASH)];p.level=50;p.ivs.fill(15);p.nature=0;p.generateName();p.calculateStats();p.hp=p.getMaxHp();p.status=null;
            p.tempSummonData.turnCount=2;p.tempSummonData.waveTurnCount=2;
            if(k && state>=6 && k%2===1)p.status=new Status(StatusEffect.POISON);
            if(k && state>=12 && k%2===0)p.hp=0,p.status=new Status(StatusEffect.FAINT);
            if(k && state>=18 && k%2===0)p.hp=0,p.status=null;
        }
        other.species=speciesDataRegistry.getSpecies(SpeciesId.SNORLAX);other.formIndex=0;other.level=100;other.ivs.fill(15);other.nature=0;other.generateName();other.calculateStats();other.hp=other.getMaxHp();
        if(state===7)holder.status=new Status(StatusEffect.POISON);
        if(state===8)holder.summonData.ability=AbilityId.PARENTAL_BOND;
        if(state===9)holder.summonData.ability=AbilityId.SKILL_LINK;
        if(state===10){const mod=modifierTypes.MULTI_LENS().withIdFromFunc(modifierTypes.MULTI_LENS).newModifier(holder);(holder.isPlayer()?scene.modifiers:scene.enemyModifiers).push(mod);}
        if(state===11)other.hp=1;
        if(state===13)other.summonData.ability=AbilityId.STAMINA;
        if(state===14)other.summonData.ability=AbilityId.WEAK_ARMOR;
        if(state===15){other.addTag(BattlerTagType.SUBSTITUTE,0,MoveId.SUBSTITUTE,other.id);pm.clearAllPhases();}
        if(state===16)holder.species=speciesDataRegistry.getSpecies(SpeciesId.CASTFORM),holder.formIndex=1,holder.generateName(),holder.calculateStats(),holder.hp=holder.getMaxHp();
        if(state===17){holder.fusionSpecies=speciesDataRegistry.getSpecies(SpeciesId.SLAKING);holder.fusionFormIndex=0;holder.fusionAbilityIndex=0;holder.generateName();holder.calculateStats();holder.hp=holder.getMaxHp();}
        if(state===20){holder.summonData.stats=holder.stats.slice();holder.summonData.stats[Stat.ATK]=1;}
        if(state===21){holder.isTerastallized=true;holder.teraType=15;}
        commandMove(holder,MoveId.BEAT_UP);commandMove(other,state===23?MoveId.PROTECT:MoveId.SPLASH);
''' +s[b:]
a=s.index('    console.log(`BEATUP cases=');b=s.index('    fs.writeFileSync(OUT,',a)
s=s[:a]+'''    console.log(`BEATUP cases=${N} counts=${counts.join("/")} powers=${powers.join("/")} states=${stateCounts.join("/")} sides=${sideCounts.join("/")} sizes=${sizes.join("/")}`);
    if(N>=2000 && (!counts.every(v=>v>0)||!powers.slice(0,23).every(v=>v>0)||!sizes.slice(1).every(v=>v>0)))throw new Error("missing actual Beat Up coverage");
    MultiHitAttr.prototype.getHitCount=originalCount;BeatUpAttr.prototype.apply=originalPower;
''' +s[b:]
Path(sys.argv[2]).write_text(s)
