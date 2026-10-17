/* Native ExpPhase and ShowPartyExpBarPhase; UI transport only is intercepted. */
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { settings } from "#app/global-settings-manager";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { getLevelTotalExp } from "#data/exp";
import { AbilityId } from "#enums/ability-id";
import { ExpNotification } from "#enums/exp-notification";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
const rng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
const hex=(s:string)=>s?Buffer.from(s,"utf8").toString("hex"):"-";
it("native wide experience phase awards and text",async()=>{
 const N=8320,SEED=Number(process.env.WIDEEXP_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP).mysteryEncounterChance(0);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);BattleScene.prototype.randBattleSeedInt=native;game.override.levelCap(0).ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 const scene:any=game.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,p:any=game.field.getPlayerPokemon();
 const species:number[]=Array(6).fill(0);for(const key of Object.keys(speciesDataRegistry.data)){const s=speciesDataRegistry.getSpecies(Number(key));if(!species[s.growthRate]&&scene.gameData.starterData[s.getRootSpeciesId()])species[s.growthRate]=s.speciesId;}if(species.some(n=>!n))throw Error("six growth rates");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});vi.spyOn(p,"showExpGain").mockImplementation(async()=>{});vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});
 const addExp=vi.spyOn(p,"addExp");
 let texts:string[]=[];vi.spyOn(scene.ui,"showText").mockImplementation((text:any,_d:any,cb:any)=>{texts.push(text);cb?.();});settings.general.partyExpNotificationMode=ExpNotification.SKIP;
 const fd=fs.openSync(process.env.WIDEEXP_OUT??"/tmp/wide-exp-phase.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`WIDE_EXP_PHASE 3 ${N} ${SEED}`]);
 const waves=[1,199,200,201,1000,5850,10000,100000],levels=[1,97,99,100,101,254,255,256,65534,65535,65536,100000,1000000],amounts=[0,1,10.75,2147483646,2147483647,2147483648,4294967295,4294967296,1099511627776.5];
 const count=[0,0],growth=Array(6).fill(0);let wide=0,capped=0,changed=0;
 for(let i=0;i<N;i++){
  const op=i%2,mode=Math.floor(i/2)%3,wave=waves[Math.floor(i/6)%waves.length],rate=Math.floor(i/48)%6;
  scene.gameMode=getGameMode(mode);battle.gameMode=scene.gameMode;battle.waveIndex=wave;const cap=scene.getMaxExpLevel();scene.modifiers=[];scene.enemyModifiers=[];
  const stacks=i>=8192||i%32>=30?[0,0,0]:[[0,0,0],[1,0,0],[0,1,0],[0,0,1],[99,30,10],[5,3,2]][Math.floor(i/2)%6],order=Math.floor(i/12)%2?[2,1,0]:[0,1,2];
  for(const j of order)if(stacks[j]){const f:any=modifierTypes[["EXP_CHARM","SUPER_EXP_CHARM","GOLDEN_EXP_CHARM"][j]],type=f().withIdFromFunc(f),m=type.newModifier();m.stackCount=stacks[j];scene.modifiers.push(m);}
  const amount=i>=8192?10.75:i%32>=30?Number.MAX_SAFE_INTEGER:amounts[Math.floor(i/2)%amounts.length];
  p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(species[rate]);p.formIndex=0;p.fusionSpecies=null;p.abilityIndex=0;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.nature=0;p.ivs=Array(6).fill(15);
  p.level=levels[Math.floor(i/2)%levels.length];if(amount>1e9)p.level=Math.max(p.level,cap-3);if(i%13===0)p.level=cap;if(i>=8192)p.level=Math.max(1,cap-1);
  const total=getLevelTotalExp(p.level,rate);p.exp=i%7===0?total+1:total;p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=p.getMaxHp();p.generateName();
  pm.clearAllPhases();texts=[];addExp.mockClear();Phaser.Math.RND.sow([`wide-exp-${SEED}-${i}`]);battle.battleSeed=`wide-exp-${SEED}-${i}`;battle.battleSeedState=null;battle.randSeedInt(2);
  const before=p.exp,old=p.level;
  const rows=[`C ${i} ${op} ${mode} ${wave} ${p.id} ${p.species.speciesId} ${rate} ${p.level} ${p.exp} ${amount} ${cap} ${stacks.join(" ")} ${order.join(" ")}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`];
  const phase=pm.create(op?"ShowPartyExpBarPhase":"ExpPhase",0,amount);pm.currentPhase=phase;await phase.start();await Promise.resolve();
  const requests=pm.dynamicQueueManager.dynamicPhaseMap.get("LevelUpPhase")?.queue??[];const flat=pm.phaseQueue.levels.flat();if(flat.some((q:any)=>q.phaseName!=="DynamicPhaseMarker"&&q.phaseName!=="HidePartyExpBarPhase")||flat.filter((q:any)=>q.phaseName==="DynamicPhaseMarker").length!==requests.length)throw Error("unexpected native phase");
  if(addExp.mock.calls.length!==1)throw Error("native addExp call missing");const award=Number(addExp.mock.calls[0][0]);if(!Number.isSafeInteger(award)||award<0)throw Error("fixture award domain");
  rows.push(`A ${p.level} ${p.exp} ${award}`,`T ${hex(texts.join("\n"))}`,`L ${requests.length} ${requests[0]?.lastLevel??0} ${requests[0]?.level??0}`,`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);count[op]++;growth[rate]++;wide+=+(award>2147483647);capped+=+(p.exp-before<award);changed+=+(p.level!==old);
 }
 if(count.some(n=>n<4096)||growth.some(n=>!n)||wide<1000||!capped||!changed)throw Error("coverage missing");write([`WIDE_EXP_PHASE_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`WIDE_EXP_PHASE ops=${count} growth=${growth} wide=${wide} capped=${capped} changed=${changed}`);
},600000);
