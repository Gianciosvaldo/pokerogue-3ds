#!/usr/bin/env python3
"""Extend the retained real-turn producer with explicit fusion arrangements."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
s=(root/'tests/effective-stat-item.producer.test.ts').read_text()
def replace(old,new):
 global s
 assert s.count(old)==1,(old,s.count(old))
 s=s.replace(old,new)
replace('    const N = Number(process.env.STATITEM_N ?? 2000);','    const N = 2560;')
s=s.replace('STATITEM','FUSEDHELD').replace('bounded effective stat item turn producer','native fused held-item full turns')
assert 'const nativeBattleRng=BattleScene.prototype.randBattleSeedInt;' in s
assert 'BattleScene.prototype.randBattleSeedInt=nativeBattleRng;' in s
s='import { CritBoosterModifier } from "#modifiers/modifier";\n'+s
replace('    const applications=Array(7).fill(0), refusals=Array(7).fill(0);', '''    const applications=Array(8).fill(0), refusals=Array(8).fill(0);
    const fusionShapes=Array(16).fill(0);
    const nativeCritApply=CritBoosterModifier.prototype.apply;
    CritBoosterModifier.prototype.apply=function(p,stage){
        if(recordingTurn)applications[family]++;
        return nativeCritApply.call(this,p,stage);
    };''')
replace('    const familyCounts = Array(7).fill(0);','    const familyCounts = Array(8).fill(0);')
replace('        family=i%7;\n        const state=Math.floor(i/14)%10;', '        family=i%8;\n        const state=Math.floor(i/16)%10;\n        const fusionShape=Math.floor(i/160)%16;')
replace('"DEEP_SEA_TOOTH"];','"DEEP_SEA_TOOTH","LEEK"];')
replace('SpeciesId.DITTO,SpeciesId.DITTO,SpeciesId.CLAMPERL,SpeciesId.CLAMPERL];', '''SpeciesId.DITTO,SpeciesId.DITTO,SpeciesId.CLAMPERL,SpeciesId.CLAMPERL,
            [SpeciesId.FARFETCHD,SpeciesId.GALAR_FARFETCHD,SpeciesId.SIRFETCHD][Math.floor(i/160)%3]];''')
replace('        holder.generateName();holder.calculateStats();holder.hp=holder.getMaxHp();', '''        const ordinary=[SpeciesId.MEW,SpeciesId.RATTATA,SpeciesId.BULBASAUR,SpeciesId.VENUSAUR];
        if(family===0){
            const pairs=[
                [SpeciesId.BULBASAUR,SpeciesId.IVYSAUR],[SpeciesId.BULBASAUR,SpeciesId.VENUSAUR],
                [SpeciesId.VENUSAUR,SpeciesId.BULBASAUR],[SpeciesId.VENUSAUR,SpeciesId.MEW],
                [SpeciesId.EEVEE,SpeciesId.BULBASAUR],[SpeciesId.EEVEE,SpeciesId.VENUSAUR],
                [SpeciesId.MEW,SpeciesId.EEVEE],[SpeciesId.MEW,SpeciesId.MEW],
                [SpeciesId.IVYSAUR,SpeciesId.BULBASAUR],[SpeciesId.VENUSAUR,SpeciesId.BULBASAUR],
                [SpeciesId.BULBASAUR,SpeciesId.VENUSAUR],[SpeciesId.MEW,SpeciesId.VENUSAUR],
                [SpeciesId.BULBASAUR,SpeciesId.EEVEE],[SpeciesId.VENUSAUR,SpeciesId.EEVEE],
                [SpeciesId.EEVEE,SpeciesId.MEW],[SpeciesId.VENUSAUR,SpeciesId.VENUSAUR],
            ];
            const [primary,secondary]=pairs[fusionShape];
            holder.species=speciesDataRegistry.getSpecies(primary);holder.formIndex=0;
            holder.fusionSpecies=speciesDataRegistry.getSpecies(secondary);holder.fusionFormIndex=0;
        }else{
            holder.species=speciesDataRegistry.getSpecies(fusionShape%4<2?eligible[family]:ordinary[fusionShape%4]);holder.formIndex=0;
            holder.fusionSpecies=speciesDataRegistry.getSpecies(fusionShape%4===1||fusionShape%4===2?eligible[family]:ordinary[Math.floor(fusionShape/4)]);holder.fusionFormIndex=0;
        }
        holder.fusionAbilityIndex=0;
        if(state===1){holder.species=speciesDataRegistry.getSpecies(SpeciesId.MEW);holder.fusionSpecies=speciesDataRegistry.getSpecies(SpeciesId.MEW);}
        if(state===5){
            const fusionMax=fusionShape%2===1;
            const max=speciesDataRegistry.getSpecies(SpeciesId.EEVEE),form=max.forms.findIndex((f:any)=>f.formKey==="gigantamax");
            if(form<0)throw Error("missing max form");
            if(fusionMax){holder.fusionSpecies=max;holder.fusionFormIndex=form;}else{holder.species=max;holder.formIndex=form;}
        }
        holder.generateName();holder.calculateStats();holder.hp=holder.getMaxHp();''')
replace('const type=family===0?modifierTypes.EVIOLITE():new SpeciesStatBoosterModifierType(keys[family] as any);','const type=family===0?modifierTypes.EVIOLITE():family===7?modifierTypes.LEEK():new SpeciesStatBoosterModifierType(keys[family] as any);')
needle='                    + (["EvolutionStatBoosterModifier", "SpeciesStatBoosterModifier"].includes(m.constructor.name)'
s=s.replace(needle,'                    + (m.constructor.name==="SpeciesCritBoosterModifier"?` virtual=${m.virtualStackCount} stage=${m.getArgs()[1]} species=${L(m.species ?? [])}`:"")\n'+needle)
replace('        battle.enemyFaints = 0;', '        battle.enemyFaints = 0;\n        battle.postBattleLoot.length = 0; // independent case, no prior battle objects')
replace('        stateCounts[state]++;','        stateCounts[state]++;fusionShapes[fusionShape]++;')
replace('    StatBoosterModifier.prototype.apply=originalApply;', '    if(fusionShapes.some(n=>n!==160)||familyCounts.some(n=>n!==320))throw Error("missing required fusion arrangement");\n    CritBoosterModifier.prototype.apply=nativeCritApply;\n    StatBoosterModifier.prototype.apply=originalApply;')
Path(sys.argv[1]).write_text(s)
