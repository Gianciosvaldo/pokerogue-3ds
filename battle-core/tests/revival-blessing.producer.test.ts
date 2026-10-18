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

import { TrainerSlot } from "#enums/trainer-slot";
import { UiMode } from "#enums/ui-mode";
import { PartyUiMode } from "#enums/party-ui-mode";
import { SwitchType } from "#enums/switch-type";
const info=(p:any,actors:any[])=>{
 const neg="-1 -1 -1 -1 -1 -1 -1 -";
 if(p.phaseName==="MessagePhase")return `M -1 -1 -1 -1 -1 -1 ${+!!p.prompt} ${hex(p.text)}`;
 if(p.phaseName==="ResetStatusPhase")return `R ${actors.indexOf(p.pokemon)} ${+p.affectConfusion} ${+p.reloadAssets} -1 -1 -1 -1 -`;
 if(p.phaseName==="RevivalBlessingPhase")return `V ${actors.indexOf(p.user)} -1 -1 -1 -1 -1 -1 -`;
 if(p.phaseName==="DynamicPhaseMarker"){if(!["MovePhase","SwitchSummonPhase"].includes(p.phaseType))throw Error(`unknown marker ${p.phaseType}`);return `K ${p.phaseType==="MovePhase"?0:1} -1 -1 -1 -1 -1 -1 -`;}
 if(p.phaseName==="MovePhase"){const h=p.moveHistoryEntry;return `P ${actors.indexOf(p.pokemon)} ${p.move.moveId} ${h.useMode} ${h.result??255} ${p.timingModifier} ${+p.cancelled} ${h.targets.length} ${h.targets.length?h.targets.join(","):"-"}`;}
 if(p.phaseName==="SwitchSummonPhase")return `S ${p.switchType} ${p.fieldIndex} ${p.slotIndex} ${+p.doReturn} ${+p.player} -1 -1 -`;
 if(p.phaseName==="ToggleDoublePositionPhase")return `T ${+p.double} -1 -1 -1 -1 -1 -1 -`;
 throw Error(`unexpected phase ${p.phaseName}`);
};
it("actual Revival Blessing raw callbacks and player choice phase",async()=>{
 const N=8704,SEED=Number(process.env.REVIVAL_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager;
 const actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField(),...Array.from({length:2},()=>scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),50)),...Array.from({length:2},()=>scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),50,TrainerSlot.TRAINER))];
 if(actors.length!==8)throw Error("natural doubles and real benches required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"setFrameRate").mockImplementation(()=>{});}
 const rolls=actors.map(p=>vi.spyOn(p,"randBattleSeedInt"));
 let callback:any=null,uiField=-9,filter:any=null;vi.spyOn(scene.ui,"setMode").mockImplementation((mode:any,...args:any[])=>{if(mode===UiMode.PARTY){if(args[0]!==PartyUiMode.REVIVAL_BLESSING)throw Error("wrong party mode");uiField=args[1];callback=args[2];filter=args[3];}return Promise.resolve();});
 const fd=fs.openSync(process.env.REVIVAL_OUT??"/tmp/revival.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`REVIVAL 4 ${N} ${SEED}`]);const move=allMoves[MoveId.REVIVAL_BLESSING],attr=move.getAttrs("RevivalBlessingAttr")[0];if(!attr)throw Error("registered callback required");let enemySuccess=0,playerQueues=0,denied=0,choices=0,cancelled=0,reset=0,playerRevived=0,singleton=0;
 const status=(p:any,k:number)=>`${k} ${p.status?.effect??0} ${p.status?.toxicTurnCount??0} ${p.status?.sleepTurnsRemaining??-1} ${p.status?.freezeTurnsRemaining??-1}`;
 const turn=(p:any,k:number)=>`${k} ${p.turnData.damageTaken} ${p.turnData.hitCount} ${p.turnData.pendingStatus??0} ${p.turnData.combiningPledge??0} ${+!!p.turnData.acted} ${p.turnData.formChangeAbilitiesApplied.size}`;
 const boss=(p:any,k:number)=>`${k} ${+p.isBoss()} ${p.bossSegments??0} ${p.bossSegmentIndex??0} ${+p.hasTrainer()}`;
 const dump=(rows:string[],tag:string)=>{
  rows.push(`${tag}Q ${pm.phaseQueue.levels.flat().length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive} ${pm.phaseQueue.currentLevel}`);
  pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,j:number)=>rows.push(`${tag}P ${l} ${j} ${info(p,actors)}`)));
  const dyn:any[]=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue);rows.push(`${tag}DC ${dyn.length}`);dyn.forEach((p:any,j:number)=>rows.push(`${tag}D ${j} ${info(p,actors)}`));
 };
 for(let i=0;i<N;i++){
  const mode=i%2,u=mode?Math.floor(i/2)%2:Math.floor(i/2)%4,pick=Math.floor(i/8)%7-1;
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.waveIndex=201;battle.turn=1;battle.double=true;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  scene.getPlayerParty().splice(0,99,actors[0],actors[1],actors[4],actors[5]);scene.getEnemyParty().splice(0,99,actors[2],actors[3],actors[6],actors[7]);
  for(let k=0;k<8;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP,SpeciesId.PIDGEY,SpeciesId.RATTATA,SpeciesId.CATERPIE,SpeciesId.WEEDLE][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,256,1000000][(Math.floor(i/64)+k)%4];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=(Math.floor(i/32)+k)%3===0?0:p.getMaxHp();p.status=p.hp===0?new Status(StatusEffect.FAINT):null;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.TACKLE)];p.generateName();p.turnData.damageTaken=37;p.turnData.hitCount=3;p.turnData.pendingStatus=StatusEffect.POISON;p.turnData.combiningPledge=MoveId.FIRE_PLEDGE;p.turnData.acted=true;p.turnData.formChangeAbilitiesApplied.add(AbilityId.BALL_FETCH);
   if(!p.isPlayer()){p.trainerSlot=i%5===0?TrainerSlot.NONE:TrainerSlot.TRAINER;p.bossSegments=(i+k)%11===0?3:0;p.bossSegmentIndex=p.bossSegments?2:0;}
  }
  if(i>=8192){for(const p of actors){
   if(i<8320){p.species=speciesDataRegistry.getSpecies(SpeciesId.SHEDINJA);p.customPokemonData.ability=AbilityId.WONDER_GUARD;p.summonData.ability=AbilityId.WONDER_GUARD;p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();if(p.getMaxHp()!==1)throw Error("native Shedinja maximum HP required");p.generateName();}
   else p.setStat(Stat.HP,i%2?2*p.getMaxHp():1,false);
   p.hp=0;p.status=new Status(StatusEffect.FAINT);if(!p.isPlayer()){p.trainerSlot=TrainerSlot.TRAINER;p.bossSegments=0;p.bossSegmentIndex=0;}
  }}
  if(i>=8576)for(const p of actors){p.hp=p.getMaxHp();p.status=null;}
  pm.clearAllPhases();pm.dynamicQueueManager.clearLastTurnOrder();callback=null;uiField=-9;filter=null;
  for(let f=0;f<2;f++)pm.unshiftNew("SwitchSummonPhase",SwitchType.SWITCH,f,2+f,false,false);
  for(const p of actors.slice(0,4))pm.pushNew("MovePhase",p,[p.isPlayer()?2:0],new PokemonMove(MoveId.TACKLE),MoveUseMode.NORMAL);
  if(i>=8448&&i<8576)for(const p of actors.slice(0,4))pm.pushNew("MovePhase",p,[p.isPlayer()?2:0],new PokemonMove(MoveId.TACKLE),MoveUseMode.NORMAL);
  pm.currentPhase=mode?pm.create("RevivalBlessingPhase",actors[u]):pm.create("MoveEffectPhase",u,[u],move,MoveUseMode.NORMAL);
  battle.battleSeed=`revival-${SEED}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`revival-${SEED}-${i}`]);battle.randSeedInt(2);for(const spy of rolls)spy.mockClear();const beforeHP=actors.map(p=>p.hp);
  const rows=[`C ${i} ${mode} ${u} ${pick}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.flatMap((p,k)=>[`I ${k} ${snapshot(p).join(" ")}`,`SI ${status(p,k)}`,`TI ${turn(p,k)}`,`IB ${boss(p,k)}`])];dump(rows,"I");rows.push(`COND ${+move.applyConditions(actors[u],actors[u])}`);
  if(mode){pm.currentPhase.start();if(!callback||!filter)throw Error("source party callback/filter not installed");rows.push(`UI ${uiField} ${scene.getPlayerParty().map((p:any)=>+(filter(p)===null)).join(" ")}`);callback(pick,2);await Promise.resolve();await Promise.resolve();choices+=+(pick>=0&&pick<4);rows.push("RET -1");}
  else {const result=attr.apply(actors[u],actors[u],move,[]);rows.push(`UI -9 0 0 0 0`,`RET ${+result}`);enemySuccess+=+(!actors[u].isPlayer()&&result);playerQueues+=+(actors[u].isPlayer()&&result);denied+=+!result;}
  const observed=rolls.flatMap((spy,k)=>spy.mock.calls.map((args:any[])=>{singleton+=+(args[0]===1);return `CALL ${k} ${args[0]} ${args[1]??0}`;}));rows.push(`CALLS ${observed.length}`,...observed);
  if(mode)playerRevived+=actors.filter((p,k)=>p.isPlayer()&&beforeHP[k]<=0&&p.hp>0).length;
  rows.push(...actors.flatMap((p,k)=>[`O ${k} ${snapshot(p).join(" ")}`,`SO ${status(p,k)}`,`TO ${turn(p,k)}`,`OB ${boss(p,k)}`]));dump(rows,"O");
  const d:any[]=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue);cancelled+=d.filter((p:any)=>p.phaseName==="MovePhase"&&p.cancelled).length;reset+=pm.phaseQueue.levels.flat().filter((p:any)=>p.phaseName==="ResetStatusPhase").length;
  rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(singleton<100||playerRevived<100||enemySuccess<100||playerQueues<100||denied<100||choices<100||cancelled<100||reset<100)throw Error(`missing coverage ${enemySuccess}/${playerQueues}/${denied}/${choices}/${cancelled}/${reset}`);write([`REVIVAL_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`REVIVAL singleton=${singleton} playerRevived=${playerRevived} enemy=${enemySuccess} playerQueue=${playerQueues} denied=${denied} choices=${choices} cancelled=${cancelled} reset=${reset}`);
},600000);
