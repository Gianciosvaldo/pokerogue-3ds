#!/usr/bin/env python3
"""Additive leave-field primal weather proof over unchanged trainer-turn scaffold."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text().replace('TRAINERSW','LEAVE_WEATHER').replace('trainer switch','leave weather')
def replace(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b,1)
replace('import { BattleScene }', 'import { PreLeaveFieldClearWeatherAbAttr } from "#abilities/ab-attrs";\nimport { Weather } from "#data/weather";\nimport { WeatherType } from "#enums/weather-type";\nimport { BattleScene }')
replace('    const FUSION = false;', '    if(N>100000||!process.env.LEAVE_WEATHER_OUT)throw new Error("invalid request");\n    const FUSION = false;')
replace('    let seed = Number(process.env.LEAVE_WEATHER_SEED ?? 12345);', '    let seed = Number(process.env.LEAVE_WEATHER_SEED ?? 12345);\n    if(!Number.isSafeInteger(seed)||seed<0||seed>0xffffffff)throw new Error("invalid seed");')
replace('    const scene = game.scene as any;', '    game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);\n    const scene = game.scene as any;')
replace('    const out: string[] = [];', '''    let recording=false,abilityIndex=0;
    const weatherAbilities=[AbilityId.PRIMORDIAL_SEA,AbilityId.DESOLATE_LAND,AbilityId.DELTA_STREAM];
    const weathers=[WeatherType.HEAVY_RAIN,WeatherType.HARSH_SUN,WeatherType.STRONG_WINDS];
    const reached=[0,0,0],leafApplies=[0,0,0],leafRefusals=[0,0,0];
    const realCan=PreLeaveFieldClearWeatherAbAttr.prototype.canApply,realApply=PreLeaveFieldClearWeatherAbAttr.prototype.apply;
    PreLeaveFieldClearWeatherAbAttr.prototype.canApply=function(this:any,...args:any[]){const result=(realCan as any).apply(this,args);if(recording){reached[abilityIndex]++;if(!result)leafRefusals[abilityIndex]++;}return result;};
    PreLeaveFieldClearWeatherAbAttr.prototype.apply=function(this:any,...args:any[]){const result=(realApply as any).apply(this,args);if(recording)leafApplies[abilityIndex]++;return result;};
    const out: string[] = [];''')
replace('Array(10).fill(0)', 'Array(12).fill(0)')
replace('family=i%4;const state=Math.floor(i/8)%10;', 'family=i%4;const state=Math.floor(i/8)%12;abilityIndex=Math.floor(i/96)%3;')
replace('p.summonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;', 'p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;')
a=s.index('        D.trainerSlot=state===7?');b=s.index('        const attr=allMoves[move]',a)
s=s[:a]+'''        const user=enemyActs?D:A,target=enemyActs?A:D;
        const move=[MoveId.U_TURN,MoveId.BATON_PASS,MoveId.PARTING_SHOT,MoveId.ROAR][family];
        const holder=family===3?target:user,peer=family===3?user:target;
        const ability=weatherAbilities[abilityIndex],weather=weathers[abilityIndex];
        holder.customPokemonData.ability=ability;holder.summonData.ability=ability;
        scene.arena.weather=state===1?null:new Weather(state===2?weathers[(abilityIndex+1)%3]:weather,0,0);
        if(state===3||state===7||state===8){peer.customPokemonData.ability=ability;peer.summonData.ability=ability;}
        if(state===4){peer.customPokemonData.ability=weatherAbilities[(abilityIndex+1)%3];peer.summonData.ability=weatherAbilities[(abilityIndex+1)%3];}
        if(state===5){holder.customPokemonData.ability=AbilityId.BALL_FETCH;holder.summonData.ability=AbilityId.BALL_FETCH;holder.passive=true;holder.customPokemonData.passive=ability;}
        if(state===6){peer.passive=true;peer.customPokemonData.passive=ability;}
        if(state===7)peer.summonData.abilitySuppressed=true;
        if(state===8){peer.hp=0;peer.status=new Status(StatusEffect.FAINT);}
        if(state===9){peer.customPokemonData.ability=AbilityId.CLOUD_NINE;peer.summonData.ability=AbilityId.CLOUD_NINE;}
        if(state===10)holder.summonData.abilitySuppressed=true;
        if(state===11){holder.hp=1;}
''' +s[b:]
replace('        command(A);command(D);commandMove(user,move);', '        command(A);command(D);commandMove(user,move);\n        if(state===11){commandMove(holder,MoveId.SPLASH);commandMove(peer,MoveId.TACKLE);}')
replace('            run = await runTurn();','            recording=true;run = await runTurn();recording=false;')
replace('    fs.writeFileSync(OUT, out.join("\\n") + "\\n");', '''    if(N>=2000&&[...reached,...leafApplies,...leafRefusals].some(n=>!n))throw new Error("missing leave-weather source coverage");
    console.log(`LEAVE_WEATHER leaves reached=${reached.join("/")} applied=${leafApplies.join("/")} refused=${leafRefusals.join("/")}`);
    PreLeaveFieldClearWeatherAbAttr.prototype.canApply=realCan;PreLeaveFieldClearWeatherAbAttr.prototype.apply=realApply;
    fs.writeFileSync(OUT, out.join("\\n") + "\\n");''')
Path(sys.argv[2]).write_text(s)
