import { getArenaTag } from "#data/arena-tag";
import { ArenaTagType } from "#enums/arena-tag-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { Status } from "#data/status-effect";
import { StatusEffect } from "#enums/status-effect";
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves } from "#data/data-lists";
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

import { HitResult } from "#enums/hit-result";


it("actual HP-cost stat boosts before inherited gates",async()=>{
 const N=12928,SEED=Number(process.env.HPCOST_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];if(actors.length!==4)throw Error("natural doubles required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});if(!p.isPlayer())vi.spyOn(p.battleInfo,"updateBossSegments").mockImplementation(()=>{});}
 const moves=[MoveId.BELLY_DRUM,MoveId.CLANGOROUS_SOUL,MoveId.FILLET_AWAY],modes=Array(3).fill(0);
 const fd=fs.openSync(process.env.HPCOST_OUT??"/tmp/hp-cost.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`HP_COST 3 ${N} ${SEED}`]);let denied=0,conditions=0,shields=0,enduring=0,overrides=0,faints=0,animations=0,statPhases=0;
 const boss=(p:any,k:number)=>`${k} ${+p.isBoss()} ${p.bossSegments??0} ${p.bossSegmentIndex??0} ${+p.hasTrainer()}`;
 const tag=(p:any,k:number)=>{if(p.summonData.tags.some((t:any)=>t.tagType!==BattlerTagType.ENDURING)||p.summonData.tags.length>1)throw Error("unexpected native tag");const t=p.summonData.tags[0];return `${k} ${p.summonData.tags.length} ${t?.tagType??0} ${t?.turnCount??0} ${t?.sourceMove??0} ${t?.sourceId??-1}`;};
 for(let i=0;i<N;i++){
  const mode=i%3,move=allMoves[moves[mode]],attr=move.getAttrs("CutHpStatStageBoostAttr")[0];if(!attr)throw Error("registered attribute required");modes[mode]++;
  let u=Math.floor(i/3)%4,t=Math.floor(i/12)%4;
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;battle.waveIndex=201;battle.turn=1;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=[AbilityId.BALL_FETCH,AbilityId.SIMPLE,AbilityId.CONTRARY][Math.floor(i/192)%3];p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.summonData.ability=p.customPokemonData.ability;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,256,1000000][(Math.floor(i/64)+k)%4];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.status=null;p.switchOutStatus=false;p.moveset=[new PokemonMove(move.id)];p.generateName();
   const mx=p.getMaxHp();p.hp=[0,1,mx,Math.max(1,Math.floor(mx/2)),Math.max(1,Math.floor(mx/3)),Math.max(1,mx-1)][(Math.floor(i/48)+k)%6];if(k>=2){p.bossSegments=0;p.bossSegmentIndex=0;}
   for(let stat=1;stat<=7;stat++)p.setStatStage(stat,((Math.floor(i/96)+stat+k)%13)-6);if(i%17===0)for(let stat=1;stat<=7;stat++)p.setStatStage(stat,6);
  }
  if(i>=12288&&i<12416){u=2+i%2;const p=actors[u];p.level=50;p.calculateStats();p.hp=p.getMaxHp();p.bossSegments=[2,3,5,8][Math.floor(i/2)%4];p.bossSegmentIndex=p.getBossSegmentIndex();shields++;}
  if(i>=12416&&i<12544){const p=actors[u];p.hp=1;p.summonData.tags.push(getBattlerTag(BattlerTagType.ENDURING,0,MoveId.ENDURE,p.id));enduring++;}
  if(i>=12544&&i<12672){const p=actors[u];p.hp=p.getMaxHp();p.setStat(Stat.HP,i%2?2*p.getMaxHp():1,false);overrides++;}
  if(i>=12672&&i<12800){u=2+i%2;const p=actors[u];p.level=50;p.calculateStats();p.hp=p.getMaxHp();p.bossSegments=[2,3,5,8][Math.floor(i/2)%4];p.bossSegmentIndex=p.getBossSegmentIndex();p.setStat(Stat.HP,i%2?2*p.getMaxHp():1,false);shields++;overrides++;}
  if(i>=12800){const p=actors[u];p.species=speciesDataRegistry.getSpecies(SpeciesId.SHEDINJA);p.formIndex=0;p.customPokemonData.ability=AbilityId.WONDER_GUARD;p.summonData.ability=AbilityId.WONDER_GUARD;p.calculateStats();p.hp=p.getMaxHp();p.generateName();if(p.getMaxHp()!==1)throw Error("natural Shedinja maximum HP required");}
  pm.clearAllPhases();battle.battleSeed=`hp-cost-${SEED}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`hp-cost-${SEED}-${i}`]);battle.randSeedInt(2);
  const rows=[`C ${i} ${mode} ${u} ${t}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,`QIN ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive} ${pm.phaseQueue.currentLevel}`,...actors.flatMap((p,k)=>[`I ${k} ${snapshot(p).join(" ")}`,`IB ${boss(p,k)}`,`IT ${tag(p,k)}`])];
  const cond=[2,3,4].map(seq=>+move.applyConditions(actors[u],actors[t],seq as any));conditions+=+cond.some(n=>!n);rows.push(`COND ${cond.join(" ")}`);
  const result=attr.apply(actors[u],actors[t],move,[]);denied+=+!result;rows.push(`RET ${+result}`,...actors.flatMap((p,k)=>[`O ${k} ${snapshot(p).join(" ")}`,`OB ${boss(p,k)}`,`OT ${tag(p,k)}`]));
  const flat=pm.phaseQueue.levels.flat();rows.push(`QCOUNT ${flat.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`);
  pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,k:number)=>{
   if(p.phaseName==="DamageAnimPhase"){animations++;rows.push(`Q ${l} ${k} D ${p.battlerIndex} ${p.amount} ${p.damageResult} ${+p.critical}`);}
   else if(p.phaseName==="DynamicPhaseMarker"&&p.phaseType==="StatStageChangePhase")rows.push(`Q ${l} ${k} S -1 0 0 -1`);
   else if(p.phaseName==="FaintPhase"){faints++;rows.push(`Q ${l} ${k} F ${p.battlerIndex} ${+p.preventInstantRevive} ${p.source?actors.indexOf(p.source):-1} -1`);}
   else throw Error(`unexpected native phase ${p.phaseName}`);
  }));
  const dynamic:any[]=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue);rows.push(`DCOUNT ${dynamic.length}`);statPhases+=dynamic.length;
  dynamic.forEach((p:any,k:number)=>{const o=p.options;if(p.phaseName!=="StatStageChangePhase")throw Error("unexpected dynamic payload");rows.push(`D ${k} ${p.battlerIndex} ${o.sourcePokemon?actors.indexOf(o.sourcePokemon):-1} ${+p.selfTarget} ${o.changes.length} ${+!!o.ignoreAbilities} ${+!!o.processed} ${o.source??0} ${+(o.message!==undefined)}`,...o.changes.map((c:any)=>`CHANGE ${c.stat} ${c.stages}`));});
  rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(denied<100||conditions<100||shields<128||enduring<128||overrides<128||faints<100||animations!==N||statPhases<100||modes.some(n=>n<100))throw Error(`HP cost coverage missing ${denied}/${conditions}/${shields}/${enduring}/${overrides}/${faints}/${animations}/${statPhases}/${modes}`);
 write([`HP_COST_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`HP_COST denied=${denied} conditions=${conditions} shields=${shields} enduring=${enduring} overrides=${overrides} faints=${faints} animations=${animations} statPhases=${statPhases} modes=${modes}`);
},600000);
