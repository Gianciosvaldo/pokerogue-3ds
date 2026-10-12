#!/usr/bin/env python3
"""Additive knockout ability oracle over the unchanged Fell Stinger turn scaffold."""
from pathlib import Path
import sys
s = Path(sys.argv[1]).read_text().replace('FELL_STINGER', 'KNOCKOUT_STATS').replace('Fell Stinger', 'Knockout stats')
def replace(a, b):
    global s
    assert s.count(a) == 1, a
    s = s.replace(a, b, 1)
replace('import { PostVictoryStatStageChangeAttr } from "#moves/move";', 'import { PostVictoryStatStageChangeAbAttr, PostKnockOutStatStageChangeAbAttr } from "#abilities/ab-attrs";\nimport { Nature } from "#enums/nature";')
replace('N < 20)', 'N < 352 || N > 100000)')
replace('    const FUSION = false;', '    if(Number(process.env.KNOCKOUT_STATS_SEED)>0xffffffff)throw new Error("invalid seed");\n    const FUSION = false;')
a = s.index('    let recordingTurn=false;')
b = s.index('    for (let i = 0; i < N; i++) {', a)
s = s[:a] + '''    let recordingTurn=false;
    const K=22, abilities=[AbilityId.MOXIE,AbilityId.CHILLING_NEIGH,AbilityId.GRIM_NEIGH,AbilityId.AS_ONE_GLASTRIER,AbilityId.AS_ONE_SPECTRIER,AbilityId.BEAST_BOOST,AbilityId.EELEVATE,AbilityId.SOUL_HEART];
    const applied=abilities.map(()=>[0,0]),moveCounts=abilities.map(()=>0),stateCounts=Array(K).fill(0),sideCounts=[0,0],partyCounts=Array(6).fill(0);
    let currentMove=0,currentState=0;
    const reached=abilities.map(()=>Array(K).fill(0));
    const originals=[PostVictoryStatStageChangeAbAttr,PostKnockOutStatStageChangeAbAttr].map(cls=>cls.prototype.apply);
    [PostVictoryStatStageChangeAbAttr,PostKnockOutStatStageChangeAbAttr].forEach((cls,k)=>{
        cls.prototype.apply=function(this:any,...args:any[]){const result=(originals[k] as any).apply(this,args);if(recordingTurn){applied[currentMove][1]++;reached[currentMove][currentState]++;}return result;};
    });
    const out:string[]=[],thrown:Record<string,number>={};
''' + s[b:]
replace('const side=i%2;currentMove=0;', 'const side=i%2;currentMove=Math.floor(i/2)%abilities.length;')
replace('const state=Math.floor(i/2)%K,', 'const state=Math.floor(i/(2*abilities.length))%K,')
replace('        const player=scene.getPlayerParty(),enemy=scene.getEnemyParty();', '        currentState=state;\n        const player=scene.getPlayerParty(),enemy=scene.getEnemyParty();')
a = s.index('        holder.summonData.statStages[Stat.ATK-1]=rnd(13)-6;other.hp=1;')
b = s.index('        let before: string[];', a)
s = s[:a] + '''        holder.summonData.ability=abilities[currentMove];other.hp=1;
        // Legal calculated stats exercise all raw winners and the stable ATK tie.
        const species=[SpeciesId.RAMPARDOS,SpeciesId.REGIROCK,SpeciesId.ALAKAZAM,SpeciesId.REGICE,SpeciesId.ELECTRODE,SpeciesId.MEW];
        holder.species=speciesDataRegistry.getSpecies(species[state%6]);holder.generateName();holder.nature=Nature.HARDY;
        holder.ivs.fill(15);holder.calculateStats();holder.hp=holder.getMaxHp();
        if(state===6){other.level=100;other.calculateStats();other.hp=other.getMaxHp();holder.level=1;holder.calculateStats();holder.hp=holder.getMaxHp();}
        if(state===7){holder.passive=true;holder.customPokemonData.passive=AbilityId.SIMPLE;}
        if(state===8){holder.passive=true;holder.customPokemonData.passive=AbilityId.CONTRARY;}
        if(state===9)holder.summonData.statStages.fill(6);
        if(state===10)holder.summonData.statStages.fill(-6);
        if(state===11)holder.summonData.abilitySuppressed=true;
        if(state===12){holder.passive=true;holder.customPokemonData.passive=abilities[currentMove];holder.summonData.ability=AbilityId.BALL_FETCH;}
        if(state===13){holder.summonData.statStages[Stat.ATK-1]=6;holder.summonData.statStages[Stat.DEF-1]=-6;}
        if(state===14){holder.summonData.ability=AbilityId.BALL_FETCH;}
        if(state===15){holder.hp=1;other.level=100;other.calculateStats();other.hp=other.getMaxHp();}
        if(state===16){holder.summonData.statStages[Stat.ACC-1]=-6;other.summonData.statStages[Stat.EVA-1]=6;}
        if(state===17){other.status=new Status(StatusEffect.POISON);}
        if(state===19){other.summonData.ability=AbilityId.SOUL_HEART;}
        if(state===20){holder.passive=true;holder.customPokemonData.passive=abilities[currentMove];holder.summonData.ability=AbilityId.SIMPLE;}
        if(state===21){holder.passive=true;holder.customPokemonData.passive=abilities[currentMove];holder.summonData.ability=AbilityId.CONTRARY;}
        pm.clearAllPhases();commandMove(holder,[15,17].includes(state)?MoveId.SPLASH:MoveId.TACKLE);commandMove(other,state===15?MoveId.TACKLE:state===18?MoveId.PROTECT:MoveId.SPLASH);
''' + s[b:]
replace('    if(N>=2000&&(applied.some(a=>!a[1])||[...stateCounts,...sideCounts].some(n=>!n)))throw new Error("missing fell stinger coverage");', '    if(applied.some(a=>!a[1])||[...moveCounts,...stateCounts,...sideCounts].some(n=>!n))throw new Error("missing knockout stats coverage");')
replace('    PostVictoryStatStageChangeAttr.prototype.applyPostVictory=originalApply;', '    [PostVictoryStatStageChangeAbAttr,PostKnockOutStatStageChangeAbAttr].forEach((cls,k)=>{cls.prototype.apply=originals[k];});')
replace('    fs.writeFileSync(OUT, out.join("\\n") + "\\n");', '''    if(N>=2000)for(let a=0;a<abilities.length;a++){
        if(![0,1,2,3,4,5,7,8,9,10,12,13,19,20,21].every(k=>reached[a][k]>0))throw new Error("missing loaded ability scenario");
        if(![6,14,15,18].every(k=>reached[a][k]===0))throw new Error("unexpected ability trigger control");
        if(allAbilities[abilities[a]].suppressable?reached[a][11]!==0:reached[a][11]===0)throw new Error("ability suppression control");
        if(a===7?reached[a][17]===0:reached[a][17]!==0)throw new Error("indirect knockout trigger");
    }
    console.log("KNOCKOUT_STATS reached="+JSON.stringify(reached));
    fs.writeFileSync(OUT, out.join("\\n") + "\\n");''')
Path(sys.argv[2]).write_text(s)
