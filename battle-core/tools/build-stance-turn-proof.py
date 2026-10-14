#!/usr/bin/env python3
"""Actual Stance Change turns on the established Transform turn transport."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 base=Path(tmp)/'base';subprocess.run([sys.executable,str(root/'tools/build-transform-turn-proof.py'),sys.argv[1],str(base)],check=True);s=base.read_text()
if sys.argv[1]=='producer':
 s=s.replace('TRANSFORMTURN','STANCETURN').replace('actual Transform full move turns','actual Stance Change full move turns')
 s=s.replace('    game.override.moveset([]).enemyMoveset([]);','    game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);')
 a=s.index('        holder.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);');b=s.index('        let before: string[];',a)
 s=s[:a]+'''        holder.species=speciesDataRegistry.getSpecies(SpeciesId.AEGISLASH);holder.formIndex=state%2;holder.gender=-1;
        other.species=speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP);other.formIndex=0;
        for(const p of [holder,other]){p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=undefined;p.level=30+rnd(21);p.moveset=[new PokemonMove(MoveId.SPLASH),new PokemonMove(MoveId.TACKLE),new PokemonMove(MoveId.KINGS_SHIELD),new PokemonMove(MoveId.SLEEP_TALK)];p.calculateStats();p.hp=p.getMaxHp();p.generateName();}
        holder.customPokemonData.ability=state%8===3?AbilityId.BALL_FETCH:AbilityId.STANCE_CHANGE;
        if(state%8===4){holder.customPokemonData.ability=AbilityId.BALL_FETCH;holder.passive=true;holder.customPokemonData.passive=AbilityId.STANCE_CHANGE;}
        holder.summonData.abilitySuppressed=state%8===5;
        const selected=state%8===6?MoveId.SLEEP_TALK:[MoveId.TACKLE,MoveId.KINGS_SHIELD,MoveId.SPLASH][state%3];
        if(selected===MoveId.SLEEP_TALK)holder.status=new Status(StatusEffect.SLEEP,0,4);
        if(state>=12){holder.hp=1;holder.setStat(Stat.SPD,1,false);other.setStat(Stat.SPD,1000,false);}
        commandMove(holder,selected);commandMove(other,state>=12?MoveId.WATER_GUN:MoveId.SPLASH);
'''+s[b:]
 a=s.index('            if(state===12||state===23){');b=s.index('            before = dumpState(true);',a);s=s[:a]+s[b:]
elif sys.argv[1]!='replay':raise SystemExit('producer or replay required')
Path(sys.argv[2]).write_text(s)
