import { getArenaTag } from "#data/arena-tag";
import { ArenaTagType } from "#enums/arena-tag-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { Status } from "#data/status-effect";
import { StatusEffect } from "#enums/status-effect";
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves,allAbilities } from "#data/data-lists";
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

import { Terrain } from "#data/terrain";
import { BiomeId } from "#enums/biome-id";

it("actual Secret Power terrain and biome callbacks",async()=>{
 const N=12288,seed=Number(process.env.SECRETPOWER_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];if(actors.length!==4)throw Error("natural doubles required");vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors)vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});
 const move=allMoves[MoveId.SECRET_POWER],attr:any=move.getAttrs("SecretPowerAttr")[0],biomes=Object.values(BiomeId),primary=[AbilityId.BALL_FETCH,AbilityId.SERENE_GRACE,AbilityId.SHEER_FORCE,AbilityId.SHIELD_DUST,AbilityId.INNER_FOCUS,AbilityId.CLEAR_BODY,AbilityId.MIRROR_ARMOR,AbilityId.SIMPLE,AbilityId.CONTRARY];
 const rollSpies=actors.map(p=>vi.spyOn(p,"randBattleSeedInt"));
 const biomeSpy=vi.spyOn(attr,"determineBiomeEffect"),terrainSpy=vi.spyOn(attr,"determineTerrainEffect");
 const variants=[...new Set([StatusEffect.PARALYSIS,StatusEffect.SLEEP,StatusEffect.FREEZE,StatusEffect.BURN].map(x=>`P${x}`).concat([Stat.ATK,Stat.DEF,Stat.SPATK,Stat.SPD,Stat.ACC].map(x=>`S${x}`),["F"]))],positive:any=Object.fromEntries(variants.map(x=>[x,0]));const seenBiomes=new Set(),seenTerrains=new Set(),positiveBiomes=new Set(),positiveTerrains=new Set();
 const fd=fs.openSync(process.env.SECRETPOWER_OUT??"/tmp/secret-power.txt","w"),write=(r:string[])=>fs.writeSync(fd,r.join("\n")+"\n");write([`SECRET_POWER 2 ${N} ${seed}`]);let denied=0,success=0,rainbows=0,faintTargets=0;
 const rng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
 const status=(p:any,k:number)=>`${k} ${p.status?.effect??0} ${p.status?.toxicTurnCount??0} ${p.status?.sleepTurnsRemaining??-1} ${p.status?.freezeTurnsRemaining??-1} ${p.turnData.pendingStatus??0}`;
 const tags=(p:any,k:number)=>{if(p.summonData.tags.length>1||p.summonData.tags.some((t:any)=>t.tagType!==BattlerTagType.FLINCHED))throw Error("unexpected battler tag");const t=p.summonData.tags[0];return `${k} ${p.summonData.tags.length} ${t?.tagType??0} ${t?.turnCount??0} ${t?.sourceMove??0} ${t?.sourceId??-1}`;};
 const arena=()=>{if(scene.arena.tags.length>1||scene.arena.tags.some((t:any)=>t.tagType!==ArenaTagType.WATER_FIRE_PLEDGE))throw Error("unexpected arena tag");const t=scene.arena.tags[0];return `${scene.arena.tags.length} ${t?.tagType??0} ${t?.side??0} ${t?.turnCount??0} ${t?.maxDuration??0} ${t?.sourceMove??0} ${t?.sourceId??-1}`;};
 for(let i=0;i<N;i++){
  const u=Math.floor(i/175)%4,t=Math.floor(i/700)%4,biome=biomes[i%biomes.length],terrain=Math.floor(i/biomes.length)%5;seenBiomes.add(biome);seenTerrains.add(terrain);
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;battle.waveIndex=201;battle.turn=1;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.biomeId=biome;scene.arena.weather=null;scene.arena.terrain=terrain?new Terrain(terrain,10,10):null;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=primary[(Math.floor(i/2800)+k)%primary.length];p.summonData.ability=p.customPokemonData.ability;p.customPokemonData.passive=(Math.floor(i/5600)+k)%3===1?AbilityId.SERENE_GRACE:AbilityId.NONE;p.passive=p.customPokemonData.passive!==AbilityId.NONE;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,256,1000000][(Math.floor(i/64)+k)%4];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=(Math.floor(i/32)+k)%4===0?0:p.getMaxHp();p.status=(Math.floor(i/192)+k)%4===0?new Status(StatusEffect.POISON,i%4):null;p.switchOutStatus=false;p.moveset=[new PokemonMove(move.id)];p.generateName();for(let stat=1;stat<=7;stat++)p.setStatStage(stat,((Math.floor(i/128)+k+stat)%13)-6);
   if(i%23===0)p.summonData.tags.push(getBattlerTag(BattlerTagType.FLINCHED,0,MoveId.FAKE_OUT,p.id));
  }
  if(i%2===0){const tag=getArenaTag(ArenaTagType.WATER_FIRE_PLEDGE,4,MoveId.WATER_PLEDGE,actors[u].id,u<2?ArenaTagSide.PLAYER:ArenaTagSide.ENEMY);if(!tag||tag.tagType!==ArenaTagType.WATER_FIRE_PLEDGE)throw Error("real rainbow factory required");scene.arena.tags.push(tag);rainbows++;}
  pm.clearAllPhases();biomeSpy.mockClear();terrainSpy.mockClear();battle.battleSeed=`secret-power-${seed}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`secret-power-${seed}-${i}`]);battle.randSeedInt(2);faintTargets+=+(actors[t].hp===0);
  const rows=[`C ${i} ${u} ${t}`,`W ${scene.waveSeed}`,`WORLD ${scene.arena.biomeId} ${scene.arena.terrainType} ${scene.arena.terrain?.turnsLeft??0} ${scene.arena.terrain?.maxDuration??0}`,`AI ${arena()}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,`QIN ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive} ${pm.phaseQueue.currentLevel}`,...actors.flatMap((p,k)=>[`I ${k} ${snapshot(p).join(" ")}`,`SI ${status(p,k)}`,`TI ${tags(p,k)}`])];
  rollSpies[u].mockClear();
  const result=attr.apply(actors[u],actors[t],move,[]);denied+=+!result;success+=+result;
  const generated=[...biomeSpy.mock.results,...terrainSpy.mock.results].map((r:any)=>r.value);if(generated.length>1)throw Error("multiple native children");const child=generated[0];let kind="N";if(child){if(child.constructor.name==="StatusEffectAttr")kind=`P${child.effect}`;else if(child.constructor.name==="StatStageChangeAttr"){if(child.stats.length!==1||child.stages!==-1)throw Error("unexpected stat child");kind=`S${child.stats[0]}`;}else if(child.constructor.name==="AddBattlerTagAttr"&&child.tagType===BattlerTagType.FLINCHED)kind="F";else throw Error(`unexpected child ${child.constructor.name}`);}if(result){positiveTerrains.add(terrain);if(terrain===0)positiveBiomes.add(biome);if(!(kind in positive))throw Error("untracked positive effect");positive[kind]++;}
  rows.push(`CHILD ${kind}`,`RET ${+result}`,`CALLS ${rollSpies[u].mock.calls.length}`,...rollSpies[u].mock.calls.map((a:any[])=>`CALL ${a[0]} ${a[1]??0}`),...actors.flatMap((p,k)=>[`O ${k} ${snapshot(p).join(" ")}`,`SO ${status(p,k)}`,`TO ${tags(p,k)}`]),`AO ${arena()}`);
  const flat=pm.phaseQueue.levels.flat();rows.push(`QCOUNT ${flat.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`);pm.phaseQueue.levels.forEach((q:any[],l:number)=>q.forEach((p:any,k:number)=>{
   if(p.phaseName==="MessagePhase")rows.push(`Q ${l} ${k} M -1 -1 -1 -1 ${hex(p.text)}`);
   else if(p.phaseName==="ObtainStatusEffectPhase")rows.push(`Q ${l} ${k} O ${p.battlerIndex} ${p.statusEffect} ${p.sourcePokemon?actors.indexOf(p.sourcePokemon):-1} ${p.sleepTurnsRemaining??-1} ${hex(p.statusMessage)}`);
   else if(p.phaseName==="DynamicPhaseMarker"&&p.phaseType==="StatStageChangePhase")rows.push(`Q ${l} ${k} S -1 -1 -1 -1 -`);
   else if(p.phaseName==="DynamicPhaseMarker"&&p.phaseType==="ObtainStatusEffectPhase")rows.push(`Q ${l} ${k} T -1 -1 -1 -1 -`);
   else if(p.phaseName==="ShowAbilityPhase"){const a=allAbilities.filter(a=>a.name===p.abilityName);if(a.length!==1)throw Error("ambiguous cached ability");rows.push(`Q ${l} ${k} A ${p.battlerIndex} ${a[0].id} ${+p.passive} ${+p.pokemonOnField} ${hex(p.pokemonName)}`);}
   else if(p.phaseName==="HideAbilityPhase")rows.push(`Q ${l} ${k} H -1 -1 -1 -1 -`);
   else throw Error(`unexpected phase ${p.phaseName}:${p.phaseType} at ${i}`);
  }));
  const dyn:any[]=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue);rows.push(`DCOUNT ${dyn.length}`);dyn.forEach((p:any,k:number)=>{if(p.phaseName==="StatStageChangePhase"){const o=p.options;rows.push(`D ${k} S ${p.battlerIndex} ${o.sourcePokemon?actors.indexOf(o.sourcePokemon):-1} ${+p.selfTarget} ${o.changes.length} ${+!!o.ignoreAbilities} ${+!!o.processed} ${o.source??0} ${+(o.message!==undefined)}`,...o.changes.map((c:any)=>`CHANGE ${c.stat} ${c.stages}`));}else if(p.phaseName==="ObtainStatusEffectPhase")rows.push(`D ${k} O ${p.battlerIndex} ${p.statusEffect} ${p.sourcePokemon?actors.indexOf(p.sourcePokemon):-1} ${p.sleepTurnsRemaining??-1} ${hex(p.statusMessage)}`);else throw Error(`unexpected dynamic ${p.phaseName}`);});
  rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(denied<100||success<100||rainbows<100||faintTargets<100||seenBiomes.size!==biomes.length||seenTerrains.size!==5||positiveBiomes.size!==biomes.length||positiveTerrains.size!==5||Object.values(positive).some(n=>Number(n)<1))throw Error(`coverage missing ${denied}/${success}/${rainbows}/${faintTargets}/${JSON.stringify(positive)}`);
 write([`SECRET_POWER_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`SECRET_POWER denied=${denied} success=${success} rainbows=${rainbows} faintTargets=${faintTargets} positives=${JSON.stringify(positive)}`);
},600000);
