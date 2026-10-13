#!/usr/bin/env python3
"""Native form owning turns using the existing Transform turn transport."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    base=Path(tmp)/'base'
    subprocess.run([sys.executable,str(root/'tools/build-transform-turn-proof.py'),sys.argv[1],str(base)],check=True)
    s=base.read_text()
if sys.argv[1]=='producer':
    s=s.replace('TRANSFORMTURN','NATIVEFORMTURN')
    s=s.replace('    game.override.moveset([]).enemyMoveset([]);','    game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);')
    old='        const slot = rnd(p.moveset.length);\n        p.moveset[slot] = new PokemonMove(move);'
    assert s.count(old)==1
    s=s.replace(old,'        const slot = p.getMoveset().findIndex((m:any)=>m.moveId===move);\n        if(slot<0)throw Error("selected move absent");')
    a=s.index('        holder.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);')
    b=s.index('        let before: string[];',a)
    s=s[:a]+'''        const native=[[SpeciesId.DARMANITAN,AbilityId.ZEN_MODE],[SpeciesId.GALAR_DARMANITAN,AbilityId.ZEN_MODE],[SpeciesId.MINIOR,AbilityId.SHIELDS_DOWN],[SpeciesId.WISHIWASHI,AbilityId.SCHOOLING],[SpeciesId.MORPEKO,AbilityId.HUNGER_SWITCH],[SpeciesId.EISCUE,AbilityId.ICE_FACE],[SpeciesId.MIMIKYU,AbilityId.DISGUISE],[SpeciesId.ZYGARDE,AbilityId.POWER_CONSTRUCT]][state%8];
        holder.species=speciesDataRegistry.getSpecies(native[0]);holder.formIndex=rnd(holder.species.forms.length);
        holder.customPokemonData.ability=native[1];holder.summonData.ability=undefined;
        other.customPokemonData.ability=AbilityId.BALL_FETCH;other.summonData.ability=undefined;
        for(const p of [holder,other]){p.level=18+rnd(5);p.moveset=[new PokemonMove(MoveId.SPLASH)];p.calculateStats();p.hp=p.getMaxHp();p.generateName();}
        const max=holder.getMaxHp(),hp=[1,Math.max(1,Math.floor(max/4)-1),Math.floor(max/4),Math.floor(max/4)+1,Math.floor(max/2),Math.floor(max/2)+1,max];
        holder.hp=hp[rnd(hp.length)];holder.isTerastallized=!!rnd(2);
        if(holder.getAbility().id!==native[1])throw Error("native ability fixture overridden");
        commandMove(holder,MoveId.SPLASH);commandMove(other,MoveId.SPLASH);
'''+s[b:]
    a=s.index('            if(state===12||state===23){')
    b=s.index('            before = dumpState(true);',a)
    s=s[:a]+s[b:]
    s=s.replace('actual Transform full move turns','actual native form full move turns')
    s=s.replace('    const stateCounts=Array(24).fill(0), sideCounts=[0,0];','    const stateCounts=Array(24).fill(0), sideCounts=[0,0], nativeChanges=Array(8).fill(0);')
    s=s.replace('            texts = shown;','            texts = shown;\n            if(run.trace.some((x:string)=>x.startsWith("QuietFormChangePhase")))nativeChanges[state%8]++;')
    anchor='    console.log(`NATIVEFORMTURN cases='
    index=s.index(anchor)
    s=s[:index]+'    if(N>=512&&[0,1,2,3,4,7].some(k=>nativeChanges[k]===0))throw Error(`unreached native changes ${nativeChanges}`);\n    console.log(`native changes ${nativeChanges.join("/")}`);\n'+s[index:]
elif sys.argv[1]!='replay':
    raise SystemExit('producer or replay required')
Path(sys.argv[2]).write_text(s)
