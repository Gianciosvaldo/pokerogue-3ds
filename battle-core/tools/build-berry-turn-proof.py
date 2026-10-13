#!/usr/bin/env python3
"""Additive owning berry lifecycle projection; unchanged full-turn comparator."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
def overlay(s,a,b):
 if s.count(a)!=1: raise SystemExit('Berry proof anchor changed: '+a[:90])
 return s.replace(a,b,1)
if sys.argv[1] in ('producer','producer-first'):
 s=(root/'tests/speed-boost.producer.test.ts').read_text().replace('SPEEDBOOST','BERRYLIFE').replace('bounded Speed Boost move turn producer','actual Harvest and Cud Chew full turns')
 s=overlay(s,'import { SpeedBoostAbAttr } from "#abilities/ab-attrs";','''import { SpeedBoostAbAttr } from "#abilities/ab-attrs";
import { BerryModifier, PreserveBerryModifier } from "#modifiers/modifier";
import { BerryModifierType } from "#modifiers/modifier-type";
import { BerryType } from "#enums/berry-type";''')
 a=s.index('    const originalCan=SpeedBoostAbAttr.prototype.canApply;');b=s.index('    const out:string[]=[];',a);s=s[:a]+s[b:]
 s=s.replace('state=Math.floor(i/2)%30;','state=Math.floor(i/2)%40;')
 s=overlay(s,'                    + (m.berryType !== undefined ? ` btype=${m.berryType}` : "")','                    + (m.berryType !== undefined ? ` btype=${m.berryType}` : "")\n                    + (["BerryModifier","PreserveBerryModifier"].includes(m.constructor.name)?` virtual=${m.virtualStackCount}`:"")')
 s=overlay(s,'const stateCounts=Array(30).fill(0), sideCounts=[0,0], applies=Array(30).fill(0),refusals=Array(30).fill(0);','const stateCounts=Array(40).fill(0), sideCounts=[0,0];')
 a=s.index('        holder.summonData.ability=AbilityId.SPEED_BOOST;');b=s.index('        let before: string[];',a)
 s=s[:a]+'''        const harvest=state<20, k=state%20, berry=rnd(11);
        holder.summonData.ability=harvest?AbilityId.HARVEST:AbilityId.CUD_CHEW;
        const list=holder.isPlayer()?scene.modifiers:scene.enemyModifiers;
        const addBerry=(type:number,stack:number,p=holder)=>list.push(new BerryModifier(new BerryModifierType(type),p.id,type,stack));
        holder.battleData.berriesEaten=harvest?[berry,berry,rnd(11)]:[];
        holder.summonData.berriesEatenLast=harvest?[]:[berry,rnd(11)];
        if(k===0){holder.battleData.berriesEaten=[];holder.summonData.berriesEatenLast=[];}
        if(k===1)holder.summonData.abilitySuppressed=true;
        if(k===2){holder.summonData.ability=AbilityId.BALL_FETCH;holder.passive=true;holder.customPokemonData.passive=harvest?AbilityId.HARVEST:AbilityId.CUD_CHEW;}
        if(k===3){holder.hp=0;holder.status=new Status(StatusEffect.FAINT);}
        if(k===4||k===5)scene.arena.weather=new Weather(k===4?WeatherType.SUNNY:WeatherType.HARSH_SUN,5);
        if(k===6){scene.arena.weather=new Weather(WeatherType.SUNNY,5);other.summonData.ability=AbilityId.CLOUD_NINE;}
        if(k===7){addBerry(berry,berry<=BerryType.ENIGMA||berry===BerryType.LEPPA?1:2);list.at(-1).virtualStackCount=1;holder.battleData.berriesEaten=[berry];}
        if(k===8){addBerry(BerryType.LUM,2);holder.battleData.berriesEaten=[BerryType.LUM,BerryType.SITRUS];}
        if(k===9)addBerry(berry,1);
        if(k===10){holder.hp=1;addBerry(BerryType.SITRUS,2);}
        if(k===11){holder.hp=1;addBerry(BerryType.SITRUS,1);list.push(new PreserveBerryModifier(modifierTypes.BERRY_POUCH(),3));}
        if(k===12){holder.hp=1;addBerry(BerryType.SITRUS,1);const pouch=new PreserveBerryModifier(modifierTypes.BERRY_POUCH(),1);pouch.virtualStackCount=1;list.push(pouch);}
        if(k===13){holder.hp=1;addBerry(BerryType.SITRUS,1);other.summonData.ability=AbilityId.UNNERVE;}
        if(k===14){holder.passive=true;holder.customPokemonData.passive=AbilityId.RIPEN;holder.hp=1;}
        if(k===15){holder.passive=true;holder.customPokemonData.passive=AbilityId.CHEEK_POUCH;holder.hp=1;}
        if(k===16){holder.moveset=rnd(2)?[new PokemonMove(MoveId.SPLASH,0),new PokemonMove(MoveId.GROWL,allMoves[MoveId.GROWL].pp)]:[new PokemonMove(MoveId.GROWL,0),new PokemonMove(MoveId.SPLASH,3)];holder.summonData.berriesEatenLast=[BerryType.LEPPA];}
        if(k===17){holder.status=new Status(StatusEffect.PARALYSIS);holder.summonData.berriesEatenLast=[BerryType.LUM];}
        if(k===18){holder.summonData.statStages.fill(6);holder.summonData.berriesEatenLast=[BerryType.STARF,BerryType.LIECHI];}
        if(k===19){holder.summonData.berriesEatenLast=[BerryType.SITRUS];holder.hp=1;}
        commandMove(holder,MoveId.SPLASH);commandMove(other,MoveId.SPLASH);
''' +s[b:]
 # Histories are always part of owning projection, including empty values.
 s=overlay(s,'                    + (p.battleData.hasEatenBerry ? " eb=1" : "")','''                    + (p.battleData.hasEatenBerry ? " eb=1" : "")
                    + ` bb=${L(p.battleData.berriesEaten)} tb=${L(td.berriesEaten)} lb=${L(sd.berriesEatenLast)}`''')
 # Selected cases follow the owning repeat through a second actual turn.
 s=overlay(s,'            before = dumpState(true);','''            pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();
            if([10,11,12,19].includes(k) && !harvest){
                await runTurn();battle.turn++;battle.turnCommands={};battle.preTurnCommands={};
                for(const p of [holder,other])p.resetTurnData();
                commandMove(holder,MoveId.SPLASH);commandMove(other,MoveId.SPLASH);
                pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();
            }
            before = dumpState(true);''')
 a=s.index('    console.log(`BERRYLIFE cases=');b=s.index('    fs.writeFileSync(OUT',a);s=s[:a]+'    console.log(`BERRYLIFE cases=${N} states=${stateCounts.join("/")} sides=${sideCounts.join("/")}`);\n'+s[b:]
 if sys.argv[1]=='producer-first':
  s=overlay(s,'            if([10,11,12,19].includes(k) && !harvest){','            if(false){')
elif sys.argv[1]=='replay':
 s=(root/'tests/speed-boost-replay.inc').read_text()
 s=overlay(s,'            p->has_eaten_berry = field(l, "eb") && inum(l, "eb") != 0;','''            p->has_eaten_berry = field(l, "eb") && inum(l, "eb") != 0;
            const char *keys[]={"bb","tb","lb"};
            uint8_t *arrays[]={p->battle_berries,p->turn_berries,p->last_berries};
            uint16_t *counts[]={&p->n_battle_berries,&p->n_turn_berries,&p->n_last_berries};
            for(int k=0;k<3;k++){
                char entries[PR_MAX_BERRY_HISTORY+1][64];
                int n=split(need(l,keys[k]),',',entries,PR_MAX_BERRY_HISTORY+1);
                if(n>PR_MAX_BERRY_HISTORY){fprintf(stderr,"berry history overflow\\n");exit(2);}
                *counts[k]=(uint16_t)n;
                for(int j=0;j<n;j++){char *end;long value=strtol(entries[j],&end,10);if(*end||value<0||value>BERRY_TYPE_LEPPA){fprintf(stderr,"invalid berry history\\n");exit(2);}arrays[k][j]=(uint8_t)value;}
            }''')
 s=overlay(s,'            m->stack_count = (int32_t)inum(l, "stack");','            m->stack_count = (int32_t)inum(l, "stack");\n            if(m->cls==PR_MOD_BERRY || m->cls==PR_MOD_PRESERVE_BERRY){char *end;long v=strtol(need(l,"virtual"),&end,10);if(*end||v<0||v>3){fprintf(stderr,"invalid virtual berry stack\\n");exit(2);}m->virtual_stack_count=(int32_t)v;}')
 s=overlay(s,'"MOD side=%d cls=%s pid=%lld stack=%d btype=%d", side,\n                         pr_mod_class_name[m->cls], (long long)m->pokemon_id, m->stack_count, m->berry_type);','"MOD side=%d cls=%s pid=%lld stack=%d btype=%d virtual=%d", side,\n                         pr_mod_class_name[m->cls], (long long)m->pokemon_id, m->stack_count, m->berry_type, m->virtual_stack_count);')
 s=overlay(s,'            else if (m->cls == PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE)','            else if(m->cls == PR_MOD_PRESERVE_BERRY)\n                add_line(L,"MOD side=%d cls=%s pid=%lld stack=%d virtual=%d",side,pr_mod_class_name[m->cls],(long long)m->pokemon_id,m->stack_count,m->virtual_stack_count);\n            else if (m->cls == PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE)')
 s=overlay(s,'    if (p->has_eaten_berry) sb(&s, " eb=1");','''    if (p->has_eaten_berry) sb(&s, " eb=1");
    sb(&s," bb=");LIST(&s,p->n_battle_berries,',',"%d",p->battle_berries[i]);
    sb(&s," tb=");LIST(&s,p->n_turn_berries,',',"%d",p->turn_berries[i]);
    sb(&s," lb=");LIST(&s,p->n_last_berries,',',"%d",p->last_berries[i]);''')
else: raise SystemExit('producer or replay required')
Path(sys.argv[2]).write_text(s)
