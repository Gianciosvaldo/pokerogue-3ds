#!/usr/bin/env python3
"""Actual weather moves, lapse, form callbacks, faint and recall turns."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 base=Path(tmp)/'base';subprocess.run([sys.executable,str(root/'tools/build-transform-turn-proof.py'),sys.argv[1],str(base)],check=True);s=base.read_text()
if sys.argv[1]=='producer':
 s=s.replace('TRANSFORMTURN','WEATHERTURN').replace('actual Transform full move turns','actual weather form full move turns')
 s=s.replace('    game.override.moveset([]).enemyMoveset([]);','    game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);')
 a=s.index('        holder.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);');b=s.index('        let before: string[];',a)
 s=s[:a]+'''        holder.species=speciesDataRegistry.getSpecies(state%2?SpeciesId.CHERRIM:SpeciesId.CASTFORM);holder.formIndex=rnd(holder.species.forms.length);holder.gender=-1;
        other.species=speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP);other.formIndex=0;
        for(const p of [holder,other]){p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=undefined;p.level=30+rnd(21);p.moveset=[new PokemonMove(MoveId.SPLASH),new PokemonMove(MoveId.SUNNY_DAY),new PokemonMove(MoveId.RAIN_DANCE),new PokemonMove(MoveId.HAIL)];p.calculateStats();p.hp=p.getMaxHp();p.generateName();}
        const native=state%2?AbilityId.FLOWER_GIFT:AbilityId.FORECAST;
        holder.customPokemonData.ability=state%8===3?AbilityId.BALL_FETCH:native;
        if(state%8===4){holder.customPokemonData.ability=AbilityId.BALL_FETCH;holder.passive=true;holder.customPokemonData.passive=native;}
        holder.summonData.abilitySuppressed=state%8===5;
        if(state%8===6)holder.summonData.ability=AbilityId.NONE;
        if(state%8===7)other.customPokemonData.ability=AbilityId.CLOUD_NINE;
        const weather=[WeatherType.SUNNY,WeatherType.RAIN,WeatherType.HAIL,WeatherType.SANDSTORM][Math.floor(state/2)%4];
        scene.arena.weather=new Weather(weather);scene.arena.weather.turnsLeft=state>=8&&state<16?1:5;
        if(state>=16){holder.hp=1;holder.setStat(Stat.SPD,1,false);other.setStat(Stat.SPD,1000,false);}
        commandMove(holder,state<8?[MoveId.SUNNY_DAY,MoveId.RAIN_DANCE,MoveId.HAIL][state%3]:MoveId.SPLASH);
        commandMove(other,state>=16?MoveId.WATER_GUN:MoveId.SPLASH);
'''+s[b:]
 a=s.index('            if(state===12||state===23){');b=s.index('            before = dumpState(true);',a);s=s[:a]+s[b:]
elif sys.argv[1]!='replay':raise SystemExit('producer or replay required')
Path(sys.argv[2]).write_text(s)
