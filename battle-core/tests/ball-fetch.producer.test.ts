import { applyAbAttrs } from "#abilities/apply-ab-attrs";
import { getArenaTag } from "#data/arena-tag";
import { ArenaTagType } from "#enums/arena-tag-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { Status } from "#data/status-effect";
import { StatusEffect } from "#enums/status-effect";
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves, allAbilities } from "#data/data-lists";
import { MoveUseMode } from "#enums/move-use-mode";
import { getBattlerTag } from "#data/battler-tags";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
const rng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
const hex=(s:string)=>s?Buffer.from(s,"utf8").toString("hex"):"-";
function snapshot(p: any): number[] {
  const sd = p.summonData, form = p.getSpeciesForm(), transformed = p.isTransformed();
  const formIndex = Math.max(0, speciesDataRegistry.getSpecies(form.speciesId).forms.indexOf(form));
  const moves = (ms: any[]) => [ms.length, ...Array.from({ length: 4 }, (_, i) => ms[i]?.moveId ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUsed ?? 0), ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUp ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.maxPpOverride ?? -1)];
  const types = p.getTypes();
  return [p.species.speciesId, p.formIndex, p.gender, p.id, p.level, p.nature, p.hp, p.status?.effect ?? 0,
    +p.shiny, p.getAbility(true).id, p.getAbility().id, p.hasPassive() ? p.getPassiveAbility().id : 0, +p.hasPassive(),
    p.customPokemonData.passive, p.fusionSpecies?.speciesId ?? 0, sd.illusion?.species ?? 0,
    +p.isOnField(), +p.switchOutStatus,
    ...p.stats, ...Array.from({ length: 6 }, (_, s) => sd.stats[s] ?? 0),
    ...[Stat.ATK, Stat.DEF, Stat.SPATK, Stat.SPDEF, Stat.SPD, Stat.ACC, Stat.EVA].map(s => p.getStatStage(s)),
    ...moves(p.getMoveset()), sd.types?.length ?? 0, ...Array.from({ length: 4 }, (_, i) => sd.types?.[i] ?? 0),
    sd.addedType ?? 0, +p.isTerastallized, p.teraType, +transformed,
    transformed ? sd.speciesForm.speciesId : 0, transformed ? formIndex : 0, transformed ? sd.gender ?? 0 : 0,
    ...(transformed ? moves(p.moveset) : [0, ...Array(12).fill(0), ...Array(4).fill(0)]),
    +!!p.getTag(BattlerTagType.POWER_TRICK), +!!p.getTag(BattlerTagType.SUBSTITUTE),
    form.speciesId, formIndex, p.getGender(), ...Array.from({ length: 6 }, (_, s) => p.getStat(s, false)),
    types.length, ...Array.from({ length: 5 }, (_, i) => types[i] ?? 0), p.variant, p.getSpriteScale(), +p.isMega(), +p.isMax(), ...p.ivs, p.friendship, +sd.abilitySuppressed, sd.abilitiesApplied.size, ...Array.from({length:4},(_,i)=>[...sd.abilitiesApplied][i]??0), p.waveData.abilitiesApplied.size, ...Array.from({length:4},(_,i)=>[...p.waveData.abilitiesApplied][i]??0), p.tempSummonData.turnCount, p.tempSummonData.waveTurnCount, sd.ability??-1];
}

it("native Ball Fetch dispatch and shared inventory",async()=>{
 const N=16384,SEED=Number(process.env.BALLFETCH_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager;
 const actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];if(actors.length!==4)throw Error("natural doubles required");
 const applied=vi.spyOn(allAbilities[AbilityId.BALL_FETCH].getAttrs("FetchBallAbAttr")[0],"apply");
 const fd=fs.openSync(process.env.BALLFETCH_OUT??"/tmp/ball-fetch.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`BALLFETCH 1 ${N} ${SEED}`]);let successes=0,passives=0,repeated=0;
 const dump=()=>{
  const rows:string[]=[];
  pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,j:number)=>{
   let a=-1,b=-1,text="-";
   if(p.phaseName==="ShowAbilityPhase"){a=+p.passive;b=allAbilities.find(x=>x?.name===p.abilityName)?.id??-1;}
   else if(p.phaseName==="MessagePhase")text=hex(p.text);
   else if(p.phaseName!=="HideAbilityPhase")throw Error(`unexpected phase ${p.phaseName}`);
   rows.push(`P ${l} ${j} ${p.phaseName} ${p.battlerIndex??-1} ${a} ${b} ${text}`);
  }));
  const dyn=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue);if(dyn.length)throw Error("unexpected dynamic phase");
  return [`Q ${rows.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive} ${pm.phaseQueue.currentLevel}`,...rows];
 };
 for(let i=0;i<N;i++){
  const user=i%4,only=Math.floor(i/4)%3,layout=Math.floor(i/12)%6,simulated=!!(Math.floor(i/72)%2),last=Math.floor(i/144)%7-1,buddy=Math.floor(i/8064)%2;
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.waveIndex=201;battle.turn=1;battle.double=true;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=layout===1?AbilityId.NONE:AbilityId.BALL_FETCH;p.summonData.ability=p.customPokemonData.ability;p.customPokemonData.passive=[1,2].includes(layout)?AbilityId.BALL_FETCH:AbilityId.NONE;p.passive=[1,2].includes(layout);p.summonData.abilitySuppressed=layout===3;p.nature=0;p.ivs=Array(6).fill(15);p.level=Math.floor(i/1008)%2?1000000:50;p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=layout===5?0:p.getMaxHp();p.status=p.hp?null:new Status(StatusEffect.FAINT);p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.generateName();if(layout===4)p.waveData.abilitiesApplied.add(AbilityId.BALL_FETCH);
  }
  const counts=[0,1,98,99,255,65535,2147483647,9007199254740991],before=counts.map((_,k)=>counts[(Math.floor(i/1008)+k)%counts.length]);scene.pokeballCounts=Object.fromEntries(before.map((n,k)=>[k,n]));battle.lastUsedPokeball=last<0?null:last;
  pm.clearAllPhases();pm.currentPhase=pm.create("TurnEndPhase");battle.battleSeed=`fetch-${SEED}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`fetch-${SEED}-${i}`]);battle.randSeedInt(2);applied.mockClear();
  const rows=[`C ${i} ${user} ${only} ${+simulated} ${buddy}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,`INV ${last} ${before.join(" ")}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)];
  for(let step=0;step<2;step++){
   const k=step&&buddy?(user^1):user;
   applyAbAttrs("FetchBallAbAttr",{pokemon:actors[k],simulated,...(only?{passive:only===2}:{})});
   rows.push(`STEP ${step}`,`OUTINV ${battle.lastUsedPokeball??-1} ${Array.from({length:8},(_,k)=>scene.pokeballCounts[k]).join(" ")}`,...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...dump(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`);
  }
  successes+=applied.mock.calls.length;passives+=applied.mock.calls.filter(([x]:any)=>x.passive).length;repeated+=+(applied.mock.calls.length>1);rows.push("E");write(rows);
 }
 if(successes<100||passives<100||repeated)throw Error(`coverage ${successes}/${passives}/${repeated}`);
 write([`BALLFETCH_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`BALLFETCH success=${successes} passive=${passives} repeat=${repeated}`);
},600000);
