#!/usr/bin/env python3
"""Actual Transform first, then a complete copied-ability damage/status turn."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 base=Path(tmp)/'base';subprocess.run([sys.executable,str(root/'tools/build-transform-turn-proof.py'),sys.argv[1],str(base)],check=True);s=base.read_text()
if sys.argv[1]=='producer':
 s=s.replace('TRANSFORMTURN','FORMINTERTURN').replace('actual Transform full move turns','actual copied native ability full move turns')
 s=s.replace('    game.override.moveset([]).enemyMoveset([]);','    game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);')
 a=s.index('        holder.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);');b=s.index('        let before: string[];',a)
 s=s[:a]+'''        holder.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);holder.formIndex=0;holder.gender=-1;
        const entries=[[SpeciesId.MORPEKO,AbilityId.HUNGER_SWITCH],[SpeciesId.MIMIKYU,AbilityId.DISGUISE],[SpeciesId.MINIOR,AbilityId.SHIELDS_DOWN],[SpeciesId.EISCUE,AbilityId.ICE_FACE],[SpeciesId.DARMANITAN,AbilityId.ZEN_MODE],[SpeciesId.WISHIWASHI,AbilityId.SCHOOLING],[SpeciesId.ZYGARDE,AbilityId.POWER_CONSTRUCT],[SpeciesId.AEGISLASH,AbilityId.STANCE_CHANGE]],native=entries[state%8];
        other.species=speciesDataRegistry.getSpecies(native[0]);other.formIndex=rnd(other.species.forms.length);
        for(const p of [holder,other]){p.summonData.ability=undefined;p.level=30+rnd(21);p.moveset=[new PokemonMove(MoveId.TRANSFORM),new PokemonMove(MoveId.SPLASH),new PokemonMove(MoveId.WATER_GUN),new PokemonMove(MoveId.GROWL)];p.calculateStats();p.hp=p.getMaxHp();p.generateName();}
        holder.customPokemonData.ability=AbilityId.BALL_FETCH;other.customPokemonData.ability=native[1];
        if(other.getAbility().id!==native[1])throw Error("native target overridden");
        if(state%4===3){holder.passive=true;holder.customPokemonData.passive=native[1];}
        commandMove(holder,MoveId.TRANSFORM);commandMove(other,MoveId.SPLASH);
'''+s[b:]
 a=s.index('            if(state===12||state===23){');b=s.index('            before = dumpState(true);',a)
 s=s[:a]+'''            await runTurn();
            if(!holder.isTransformed())throw Error("actual first Transform failed");
            if(holder.getAbility().id!==native[1])throw Error("actual native ability not copied");
            if(state>=12)holder.hp=1;
            commandMove(holder,MoveId.SPLASH);commandMove(other,state%3===0?MoveId.GROWL:MoveId.WATER_GUN);
            pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();
'''+s[b:]
 old='        const slot = rnd(p.moveset.length);\n        p.moveset[slot] = new PokemonMove(move);'
 assert s.count(old)==1;s=s.replace(old,'        const slot=p.getMoveset().findIndex((m:any)=>m.moveId===move);if(slot<0)throw Error("selected move absent");')
elif sys.argv[1]!='replay':raise SystemExit('producer or replay required')
Path(sys.argv[2]).write_text(s)
