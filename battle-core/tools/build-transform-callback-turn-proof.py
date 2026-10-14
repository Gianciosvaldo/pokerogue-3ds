#!/usr/bin/env python3
"""Actual Transform move turns copying natural native form abilities."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 base=Path(tmp)/'base';subprocess.run([sys.executable,str(root/'tools/build-transform-turn-proof.py'),sys.argv[1],str(base)],check=True);s=base.read_text()
if sys.argv[1]=='producer':
 s=s.replace('TRANSFORMTURN','TRANSFORMCALLBACKTURN')
 s=s.replace('    game.override.moveset([]).enemyMoveset([]);','    game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);')
 old='        const slot = rnd(p.moveset.length);\n        p.moveset[slot] = new PokemonMove(move);'
 assert s.count(old)==1;s=s.replace(old,'        const slot=p.getMoveset().findIndex((m:any)=>m.moveId===move);if(slot<0)throw Error("selected move absent");')
 a=s.index('        holder.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);');b=s.index('        let before: string[];',a)
 s=s[:a]+'''        const native=[[SpeciesId.DARMANITAN,AbilityId.ZEN_MODE],[SpeciesId.GALAR_DARMANITAN,AbilityId.ZEN_MODE],[SpeciesId.MINIOR,AbilityId.SHIELDS_DOWN],[SpeciesId.WISHIWASHI,AbilityId.SCHOOLING],[SpeciesId.MORPEKO,AbilityId.HUNGER_SWITCH],[SpeciesId.EISCUE,AbilityId.ICE_FACE],[SpeciesId.MIMIKYU,AbilityId.DISGUISE],[SpeciesId.ZYGARDE,AbilityId.POWER_CONSTRUCT]][state%8];
        holder.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);holder.formIndex=0;holder.gender=-1;holder.customPokemonData.ability=-1;holder.abilityIndex=0;
        other.species=speciesDataRegistry.getSpecies(native[0]);
        const choices=other.species.forms.flatMap((f:any,k:number)=>[f.ability1,f.ability2,f.abilityHidden].map((ab:any,index:number)=>({k,index,ab}))).filter((f:any)=>f.ab===native[1]);if(!choices.length)throw Error("native ability unavailable");const chosen=choices[state%choices.length];other.formIndex=chosen.k;other.abilityIndex=chosen.index;other.customPokemonData.ability=-1;
        for(const p of [holder,other]){p.summonData.ability=undefined;p.level=20+rnd(5);p.moveset=[new PokemonMove(MoveId.TRANSFORM),new PokemonMove(MoveId.SPLASH)];p.calculateStats();p.hp=p.getMaxHp();p.generateName();}
        const max=holder.getMaxHp();holder.hp=[1,Math.max(1,Math.floor(max/4)),Math.floor(max/2),max][state%4];holder.isTerastallized=state>=16;holder.teraType=4;
        if(other.getAbility().id!==native[1]||!holder.canTransformInto(other))throw Error("native Transform ownership fixture");
        commandMove(holder,MoveId.TRANSFORM);commandMove(other,MoveId.SPLASH);
'''+s[b:]
 s=s.replace('            if(state===12||state===23){','            if(state>=8){')
 s=s.replace('actual Transform full move turns','actual native callback Transform full move turns')
elif sys.argv[1]!='replay':raise SystemExit('producer or replay required')
Path(sys.argv[2]).write_text(s)
