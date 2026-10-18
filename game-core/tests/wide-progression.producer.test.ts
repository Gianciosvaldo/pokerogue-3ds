/* Actual pinned addExp and reward application at wide levels; no legal-run claim. */
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { getLevelTotalExp } from "#data/exp";
import { AbilityId } from "#enums/ability-id";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PlayerPokemon } from "#field/pokemon";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
const rng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
it("wide native experience and candy progression",async()=>{
 const N=Number(process.env.PROGRESSION_N??12288),SEED=Number(process.env.PROGRESSION_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<12288)throw Error("Node22,N>=12288 required");
 const nativeRng=BattleScene.prototype.randBattleSeedInt,nativeAddExp=PlayerPokemon.prototype.addExp;
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.battleStyle("single").startingWave(5).ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP).mysteryEncounterChance(0);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);BattleScene.prototype.randBattleSeedInt=nativeRng;game.override.levelCap(0).ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 const scene:any=game.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,starter=scene.gameData.starterData;
 const species:number[]=Array(6).fill(0);for(const key of Object.keys(speciesDataRegistry.data)){const p=speciesDataRegistry.getSpecies(Number(key));if(!species[p.growthRate]&&starter[p.getRootSpeciesId()])species[p.growthRate]=p.speciesId;}
 if(species.some(n=>!n))throw Error("all six actual growth rates required");
 const pool=Array.from({length:6},()=>scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),5));
 for(const p of pool){vi.spyOn(p,"playSprite").mockImplementation(()=>{});vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});}
 if(scene.candyBar)vi.spyOn(scene.candyBar,"showStarterSpeciesCandy").mockImplementation(()=>{});
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 const levels=[1,97,98,99,100,101,199,200,201,254,255,256,257,32767,32768,65534,65535,65536,65537,100000,999999,1000000];
 const waves=[1,199,200,201,250,255,256,500,1000,2500,5850,10000,100000],modes=[GameModes.CLASSIC,GameModes.ENDLESS,GameModes.SPLICED_ENDLESS];
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const fd=fs.openSync(process.env.PROGRESSION_OUT??"/tmp/wide-progression.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`WIDE_PROGRESSION 1 ${N} ${SEED}`]);
 const counts=[0,0,0],growthSeen=Array(6).fill(0);let changed=0;
 for(let i=0;i<N;i++){
  const op=i%3,mode=modes[Math.floor(i/3)%3],wave=waves[Math.floor(i/9)%waves.length],n=1+rnd(6),target=rnd(n),jars=[0,1,5,25,99][rnd(5)];
  scene.gameMode=getGameMode(mode);battle.gameMode=scene.gameMode;battle.waveIndex=wave;
  const cap=scene.getMaxExpLevel();scene.getPlayerParty().splice(0,scene.getPlayerParty().length,...pool.slice(0,n));
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  for(let k=0;k<n;k++){
   const p=pool[k],rate=(Math.floor(i/9)+k)%6;p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(species[rate]);p.formIndex=0;p.fusionSpecies=null;p.fusionCustomPokemonData=null;p.abilityIndex=0;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.nature=0;p.customPokemonData.nature=-1;p.ivs=Array(6).fill(15);
   p.level=i%11===0?Math.max(1,cap-2+k):levels[(Math.floor(i/3)+k)%levels.length];
   const total=getLevelTotalExp(p.level,rate),next=getLevelTotalExp(p.level+1,rate);p.exp=[total,total-1,next-1,next,total+Math.max(0,next-total)/2,0][rnd(6)];p.friendship=[0,195,199,200,201,255][rnd(6)];
   p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=i%13===0?0:p.getMaxHp();p.generateName();growthSeen[rate]++;
  }
  for(const root of new Set(pool.slice(0,n).map(p=>p.species.getRootSpeciesId()))){starter[root].friendship=rnd(40);starter[root].candyCount=rnd(100);}
  if(jars){const factory:any=modifierTypes.CANDY_JAR,t=factory().withIdFromFunc(factory),m=t.newModifier();m.stackCount=jars;scene.modifiers.push(m);}
  pm.clearAllPhases();battle.battleSeed=`wide-progression-${SEED}-${i}`;battle.battleSeedState=null;scene.waveSeed=`wide-progression-wave-${SEED}`;Phaser.Math.RND.sow([`wide-progression-global-${SEED}-${i}`]);battle.randSeedInt(2);
  const p=pool[target],rate=p.species.growthRate,delta=[0,1,Math.max(0,getLevelTotalExp(p.level+1,rate)-p.exp),Math.max(0,getLevelTotalExp(p.level+3,rate)-p.exp)+1][Math.floor(i/3)%4];
  const rows=[`C ${i} ${op} ${mode} ${wave} ${n} ${target} ${jars} ${cap} ${delta}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`];
  for(let k=0;k<n;k++){const p=pool[k],root=p.species.getRootSpeciesId(),s=starter[root];rows.push(`P ${k} ${p.id} ${p.species.speciesId} ${p.species.growthRate} ${p.level} ${p.exp} ${p.friendship} ${root} ${s.friendship} ${s.candyCount} ${p.hp} ${p.stats.join(" ")}`);}
  const before=pool.slice(0,n).map(p=>p.level);let applied=true,filter=true;
  if(op===0)nativeAddExp.call(p,delta);
  else {const type:any=op===1?modifierTypes.RARE_CANDY():modifierTypes.RARER_CANDY();filter=!type.selectFilter||type.selectFilter(p)===null;if(filter)applied=!!scene.addModifier(op===1?type.newModifier(p):type.newModifier());else applied=false;}
  rows.push(`RESULT ${+filter} ${+applied}`);
  for(let k=0;k<n;k++){const p=pool[k],s=starter[p.species.getRootSpeciesId()];rows.push(`A ${k} ${p.level} ${p.exp} ${p.friendship} ${p.hp} ${p.getMaxHp()} ${s.friendship} ${s.candyCount}`);if(before[k]!==p.level)changed++;}
  const markers=pm.phaseQueue.levels.flat();if(markers.some((q:any)=>q.phaseName!=="DynamicPhaseMarker"||q.phaseType!=="LevelUpPhase"))throw Error(`case ${i} unexpected native phase`);
  const requests=pm.dynamicQueueManager.dynamicPhaseMap.get("LevelUpPhase")?.queue??[];
  if(markers.length!==requests.length)throw Error("native marker/request mismatch");
  rows.push(`LEVELS ${requests.length}`,...requests.map((p:any)=>`L ${p.partyMemberIndex} ${p.lastLevel} ${p.level}`),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);counts[op]++;vi.clearAllMocks();
 }
 if(counts.some(n=>n<4096)||growthSeen.some(n=>!n)||!changed)throw Error("coverage missing");
 write([`WIDE_PROGRESSION_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`WIDE_PROGRESSION counts=${counts} growth=${growthSeen} levelChanges=${changed} species=${species}`);
},600000);
