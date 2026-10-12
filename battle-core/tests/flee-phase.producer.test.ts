/* Actual AttemptRunPhase boundary; natural singles/doubles, no turn-end RNG reset. */
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { audioManager } from "#app/global-audio-manager";
import { AbilityId } from "#enums/ability-id";
import { ArenaTagType } from "#enums/arena-tag-type";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { StatusEffect } from "#enums/status-effect";
import { Status } from "#data/status-effect";
import { AttemptRunPhase } from "#phases/attempt-run-phase";
import { RunSuccessAbAttr } from "#abilities/ab-attrs";
import { GameManager } from "#test/framework/game-manager";
import { TurnHealModifier, MoneyMultiplierModifier } from "#modifiers/modifier";
import { modifierTypes } from "#data/data-lists";
import * as fs from "node:fs";
import Phaser from "phaser";
import i18next from "i18next";
import { it,vi,beforeAll,afterAll } from "vitest";

const N=Number(process.env.FLEEPHASE_N??2000),SEED=Number(process.env.FLEEPHASE_SEED??777);
let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
const rows=[`FLEE_PHASE 1 ${N}`],states=Array(24).fill(0),outcomes=[0,0];let completed=0;
const rng=(state:string)=>state.replace(/^!rnd,/,"").split(",").join(" ");
const modes=[GameModes.CLASSIC,GameModes.ENDLESS,GameModes.SPLICED_ENDLESS,GameModes.DAILY,GameModes.CHALLENGE];
const waves=[1,2,4,5,9,10,11,19,20,24,25,49,50,51,99,100,199,249];
let phaser:Phaser.Game;
beforeAll(()=>{if(!process.versions.node.startsWith("22."))throw Error("Node22 required");if(!Number.isSafeInteger(N)||N<120||N%2)throw Error("even N>=120 required");phaser=new Phaser.Game({type:Phaser.HEADLESS});});
it.each([false,true])("actual flee phase doubles=%s",async(doubles)=>{
 const original=RunSuccessAbAttr.prototype.apply;let calls:number[]=[];
 RunSuccessAbAttr.prototype.apply=function(params){calls.push(params.pokemon.getBattlerIndex());return original.call(this,params);};
  const game=new GameManager(phaser);game.override.battleStyle(doubles?"double":"single")
   .ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE)
   .enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
  await game.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.PIKACHU);
  const scene:any=game.scene,pm:any=scene.phaseManager,battle:any=scene.currentBattle;
  BattleScene.prototype.randBattleSeedInt=function(range:number,min=0){return(this as any).currentBattle.randSeedInt(range,min);};
  const slots:any[]=[scene.getPlayerField()[0],doubles?scene.getPlayerField()[1]:null,scene.getEnemyField()[0],doubles?scene.getEnemyField()[1]:null];
  vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});vi.spyOn(scene.tweens,"add").mockImplementation((()=>({})) as any);
  for(const p of slots.filter(Boolean)){vi.spyOn(p,"hideInfo").mockImplementation(()=>Promise.resolve());vi.spyOn(p,"destroy").mockImplementation(()=>{});}
  const phase:any=new AttemptRunPhase();vi.spyOn(phase,"end").mockImplementation(()=>{});
  const realT=i18next.t.bind(i18next);let keys:string[]=[],translations:string[]=[];
  vi.spyOn(i18next,"t").mockImplementation(((key:any,o:any)=>{keys.push(String(key));const result=realT(key,o);if(key==="battle:runAwaySuccess"||key==="battle:runAwayCannotEscape")translations.push(String(result));return result;})as any);
  const token=(p:any)=>"battlerIndex"in p?`I${p.battlerIndex}`:"A";
  for(let k=0;k<N/2;k++){
   const i=k+(doubles?N/2:0),state=k%24;states[state]++;calls=[];keys=[];translations=[];
   scene.gameMode=getGameMode(modes[k%modes.length]);battle.waveIndex=waves[Math.floor(k/modes.length)%waves.length];battle.turn=1+rnd(40);
   scene.waveSeed=`flee-wave-${SEED}`;battle.battleSeed=`flee-phase-${SEED}-${i}`;battle.battleSeedState=null;
   battle.escapeAttempts=[0,1,2,4,12][rnd(5)];battle.successfulRun=false;scene.turnCommandManager.setOrder=undefined;
   scene.modifiers.length=0;scene.enemyModifiers.length=0;scene.arena.tags.length=0;
   slots.forEach((p:any,b:number)=>{if(!p)return;p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.stats[Stat.HP]=1000;p.hp=100;
    p.stats[Stat.SPD]=state===0||state===1?100:1+rnd(1000);p.setStatStage(Stat.SPD,state===2?rnd(13)-6:0);
    p.status=state===3?new Status(StatusEffect.PARALYSIS):state===4?new Status(StatusEffect.TOXIC,3):state===5?new Status(StatusEffect.SLEEP,0,2):null;
    p.summonData.ability=AbilityId.BALL_FETCH;p.passive=false;p.customPokemonData.passive=AbilityId.NONE;p.summonData.abilitySuppressed=false;p.switchOutStatus=false;
    if(b>=2)p.bossSegments=state%4===3?3:0;
   });
   if(state>=6&&state<=9){const b=state%4;slots[b]?.summonData&&(slots[b].summonData.ability=AbilityId.RUN_AWAY);}
   if(doubles&&(state===0||state===1||state===2||state===19)){slots[0].summonData.ability=slots[1].summonData.ability=AbilityId.RUN_AWAY;if(state===1)slots[1].status=new Status(StatusEffect.PARALYSIS);}
   if(state===10){slots[0].summonData.ability=AbilityId.RUN_AWAY;slots[0].summonData.abilitySuppressed=true;}
   if(state===11){slots[0].passive=true;slots[0].customPokemonData.passive=AbilityId.RUN_AWAY;}
   if(state===12&&doubles){slots[1].hp=0;slots[1].status=new Status(StatusEffect.FAINT);slots[1].summonData.ability=AbilityId.RUN_AWAY;}
   if(state>=13&&state<=16&&doubles){slots[3].hp=0;slots[3].status=new Status(StatusEffect.FAINT);slots[3].bossSegments=state===14?5:0;slots[3].stats[Stat.SPD]=1200;}
   if(state===17&&doubles){slots[1].stats[Stat.SPD]=slots[0].stats[Stat.SPD];slots[0].summonData.ability=slots[1].summonData.ability=AbilityId.RUN_AWAY;}
   if(state===18&&doubles){slots[1].summonData.ability=AbilityId.RUN_AWAY;slots[1].passive=true;slots[1].customPokemonData.passive=AbilityId.RUN_AWAY;}
   if(state===19){scene.arena.addTag(ArenaTagType.TRICK_ROOM,5,MoveId.TRICK_ROOM,slots[0].id);}
   if(state>=20){scene.enemyModifiers.push(new TurnHealModifier(modifierTypes.LEFTOVERS(),slots[2].id,1));scene.enemyModifiers.push(new MoneyMultiplierModifier(modifierTypes.AMULET_COIN(),1));scene.modifiers.push(new TurnHealModifier(modifierTypes.LEFTOVERS(),slots[0].id,1));}
   if(state===21&&scene.enemyModifiers.length)scene.enemyModifiers[1].virtualStackCount=2;
   if(state===22&&scene.enemyModifiers.length){scene.enemyModifiers[1].stackCount=0;scene.enemyModifiers[1].virtualStackCount=2;}
   if(process.env.FLEEPHASE_TRANSIENT==="1"){if(state===22)slots[2].switchOutStatus=true;if(state===23)slots[0].switchOutStatus=true;}
   pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.currentPhase=phase;
   Phaser.Math.RND.sow([`flee-global-${SEED}-${i}`]);
   rows.push(`C ${i} ${+doubles} ${scene.gameMode.modeId} ${battle.waveIndex} ${battle.turn} ${battle.escapeAttempts} ${state===19?1:0} ${battle.battleSeed} ${scene.waveSeed}`);
   rows.push(`G ${rng(Phaser.Math.RND.state())}`);
   slots.forEach((p:any,b:number)=>{if(!p)return;rows.push(`P ${b} ${p.id} ${p.species.speciesId} ${p.hp} ${p.stats[Stat.SPD]} ${p.getStatStage(Stat.SPD)} ${p.status?.effect??0} ${p.status?.toxicTurnCount??0} ${p.status?.sleepTurnsRemaining??0} ${p.status?.freezeTurnsRemaining??-1} ${p.getAbility().id} ${+p.passive} ${p.getPassiveAbility().id} ${+p.summonData.abilitySuppressed} ${p.bossSegments??0} ${+p.switchOutStatus}`);rows.push(`META ${b} ${p.level} ${p.nature} ${p.ivs.join(" ")} ${p.stats.join(" ")}`);});
   const modRows=(tag:string)=>[scene.modifiers,scene.enemyModifiers].flatMap((list:any[],side:number)=>list.map((m:any,j:number)=>`${tag} ${side} ${j} ${m instanceof TurnHealModifier?0:1} ${m.pokemonId??0} ${m.stackCount} ${m.virtualStackCount}`));
   rows.push(`M ${scene.modifiers.length} ${scene.enemyModifiers.length}`,...modRows("J"));
   const chance=phase.calculateEscapeChance(battle.escapeAttempts);
   phase.start();outcomes[+battle.successfulRun]++;completed++;
   rows.push(`A ${chance} ${battle.escapeAttempts} ${+battle.successfulRun} ${scene.modifiers.length} ${scene.enemyModifiers.length}`,...modRows("K"));
   slots.forEach((p:any,b:number)=>{if(!p)return;rows.push(`O ${b} ${p.hp} ${p.status?.effect??0} ${p.status?.toxicTurnCount??0} ${p.status?.sleepTurnsRemaining??0} ${p.status?.freezeTurnsRemaining??-1} ${+!!p.turnData.failedRunAway} ${+p.waveData.abilitiesApplied.has(AbilityId.RUN_AWAY)} ${+p.summonData.abilitiesApplied.has(AbilityId.RUN_AWAY)}`);rows.push(`OS ${b} ${p.stats.join(" ")}`);});
   rows.push(`R ${rng(Phaser.Math.RND.state())}`);rows.push(`B ${rng(battle.battleSeedState)}`);
   const queue:string[]=[];pm.phaseQueue.levels.forEach((lv:any[],a:number)=>lv.forEach((p:any,j:number)=>queue.push(`Q L ${a} ${j} ${p.phaseName} ${token(p)} ${p.phaseName==="ShowAbilityPhase"?+p.passive:-1}`)));
   if([...pm.dynamicQueueManager.dynamicPhaseMap.values()].some((q:any)=>q.queue.length))throw Error("unexpected dynamic phases");
   rows.push(`QCOUNT ${queue.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...queue);
   const messages=keys.filter(x=>x==="battle:runAwaySuccess"||x==="battle:runAwayCannotEscape");if(messages.length!==1)throw Error("missing flee message");
   if(translations.length!==1)throw Error("missing rendered message");
   rows.push(`T ${messages[0]}`,`TEXT ${Buffer.from(translations[0],"utf8").toString("hex")}`,`CALLS ${calls.length} ${calls.join(" ")}`.trim(),"E");
  }
  vi.restoreAllMocks();
 RunSuccessAbAttr.prototype.apply=original;
},600000);
afterAll(()=>{
 if(completed!==N||states.some(x=>x<2)||outcomes.some(x=>!x))throw Error("missing coverage");
 rows.push(`FLEE_PHASE_COMPLETE ${N}`);fs.writeFileSync(process.env.FLEEPHASE_OUT??"/tmp/flee-phase.txt",rows.join("\n")+"\n");
 console.log(`FLEEPHASE cases=${N} states=${states.join("/")} outcomes=${outcomes.join("/")}`);
});
