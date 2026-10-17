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

it("native wide Substitute applyMoveDamage intermediate state",async()=>{
 const AI=process.env.WIDESUB_AI==="1";
 const N=8192,SEED=Number(process.env.WIDESUB_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP).mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER);BattleScene.prototype.randBattleSeedInt=native;g.override.levelCap(0).ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,actors:any[]=scene.getField();if(actors.length!==4||actors.some(p=>!p))throw Error("natural double field");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors)vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});
 const fd=fs.openSync(process.env.WIDESUB_OUT??"/tmp/wide-substitute.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`WIDE_SUBSTITUTE ${AI?3:2} ${N} ${SEED}`]);
 const levels=[1,99,100,101,254,255,256,65535,65536,1000000,10000000],moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.ERUPTION,MoveId.STORED_POWER];let wide=0,broken=0,criticals=0;
 for(let i=0;i<N;i++){
  const u=i%4,t=(u+1+Math.floor(i/4)%3)%4,move=moves[Math.floor(i/16)%moves.length],eff=[.5,1,2][Math.floor(i/8)%3];
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;battle.waveIndex=[201,1000,5850,100000][Math.floor(i/64)%4];battle.turn=1;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.summonData.ability=AbilityId.BALL_FETCH;p.nature=0;p.ivs=Array(6).fill(15);p.level=levels[(Math.floor(i/4)+k)%levels.length];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=p.getMaxHp();p.status=null;p.switchOutStatus=false;p.moveset=[new PokemonMove(move)];p.generateName();
  }
  const target=actors[t],user=actors[u],tag:any=getBattlerTag(BattlerTagType.SUBSTITUTE,0,MoveId.SUBSTITUTE,target.id);tag.hp=i%2?1:Math.max(1,Math.floor(target.getMaxHp()/4));tag.sourceInFocus=false;target.summonData.tags.push(tag);
  pm.clearAllPhases();battle.battleSeed=`wide-sub-${SEED}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`wide-sub-${SEED}-${i}`]);battle.randSeedInt(2);
  const rows=[`C ${i} ${u} ${t} ${move} ${eff} ${battle.waveIndex} ${scene.getMaxExpLevel()}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`),`H ${tag.hp}`];
  const phase:any=pm.create("MoveEffectPhase",user.getBattlerIndex(),[target.getBattlerIndex()],allMoves[move],MoveUseMode.NORMAL);pm.currentPhase=phase;
  if(AI){
   const ignoreAbility=!!(i%2),ignoreAllyAbility=!!(Math.floor(i/2)%2),isCritical=!!(Math.floor(i/4)%2),prior=actors.map(p=>snapshot(p).join(" "));
   const simulated=target.getAttackDamage({source:user,move:allMoves[move],simulated:true,ignoreAbility,ignoreAllyAbility,isCritical});
   rows.push(`SIM ${+ignoreAbility} ${+ignoreAllyAbility} ${+isCritical} ${+simulated.cancelled} ${simulated.result} ${simulated.damage}`);
   if(rng(Phaser.Math.RND.state())!==rows[2].slice(2)||rng(battle.battleSeedState)!==rows[3].slice(2)||actors.some((p,k)=>snapshot(p).join(" ")!==prior[k])||pm.phaseQueue.levels.flat().length)throw Error("simulated fixture changed state/RNG/queues");
  }
  const result=phase.applyMoveDamage(user,target,eff);rows.push(`RESULT ${result[0]} ${result[1]} ${+result[2]}`,`HOUT ${tag.hp} ${user.turnData.totalDamageDealt} ${user.turnData.singleHitDamageDealt} ${target.turnData.damageTaken}`,...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`));
  const queued=pm.phaseQueue.levels.flat();if(queued.some((p:any)=>p.phaseName!=="MessagePhase"))throw Error("unexpected native queue");rows.push(`QCOUNT ${queued.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`);pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,k:number)=>rows.push(`Q ${l} ${k} ${hex(p.text)}`)));rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);wide+=+(tag.hp<-2147483648);broken+=+(tag.hp<=0);criticals+=+result[2];
 }
 if(wide<100||broken<100||criticals<100)throw Error(`coverage missing ${wide}/${broken}`);write([`WIDE_SUBSTITUTE_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`WIDE_SUBSTITUTE wide=${wide} broken=${broken}`);
},600000);
