/* Actual CheckInterlude -> selected TurnEnd; duration lapse remains active. */
import { MistTag } from "#data/arena-tag";
import { Terrain } from "#data/terrain";
import { Weather } from "#data/weather";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { TerrainType } from "#enums/terrain-type";
import { WeatherType } from "#enums/weather-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual interlude end effects",async()=>{
 const N=Number(process.env.INTERLUDE_N??8192),seed=Number(process.env.INTERLUDE_SEED??777);if(!Number.isSafeInteger(N)||N<512||!process.versions.node.startsWith("22."))throw Error("Node22,N>=512");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);await g.classicMode.startBattle(SpeciesId.BULBASAUR);
 const s:any=g.scene,pm:any=s.phaseManager,p:any=g.field.getPlayerPokemon(),enemy:any=g.field.getEnemyPokemon();vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const q of [p,enemy])vi.spyOn(q,"updateInfo").mockImplementation(async()=>{});
 const rows=[`INTERLUDEEND 1 ${N} ${seed}`],rs=(r:string|null)=>r?r.replace(/^!rnd,/,"").replaceAll(","," "):"-",hex=(s:string)=>Buffer.from(s).toString("hex")||"-";
 const snap=()=>[s.currentBattle.turn,p.hp,p.tempSummonData.turnCount,p.tempSummonData.waveTurnCount,enemy.tempSummonData.turnCount,enemy.tempSummonData.waveTurnCount,s.arena.weather?.weatherType??0,s.arena.weather?.turnsLeft??0,s.arena.terrain?.terrainType??0,s.arena.terrain?.turnsLeft??0,s.arena.tags.length,s.arena.tags[0]?.turnCount??0];
 for(let i=0;i<N;i++){
  for(const q of [p,enemy]){q.resetSummonData();q.resetTurnData();q.resetBattleAndWaveData();q.status=null;q.hp=q.getMaxHp();q.tempSummonData.turnCount=2;q.tempSummonData.waveTurnCount=3;}
  const wave=[9,10,20,200][i%4],alive=Math.floor(i/4)%2,weather=Math.floor(i/8)%2?WeatherType.SUNNY:WeatherType.NONE,terrain=Math.floor(i/16)%2?TerrainType.GRASSY:TerrainType.NONE,turns=1+Math.floor(i/32)%3;
  p.hp=Math.max(1,p.getMaxHp()-4);enemy.hp=alive?enemy.getMaxHp():0;s.currentBattle.waveIndex=wave;s.currentBattle.turn=1+i%7;s.modifiers=[];s.enemyModifiers=[];s.arena.tags=[new MistTag(turns,ArenaTagSide.BOTH)];s.arena.weather=weather?new Weather(weather,turns):null;s.arena.terrain=terrain?new Terrain(terrain,turns):null;
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;const end=pm.create("TurnEndPhase");pm.phaseQueue.levels[0]=[pm.create("WeatherEffectPhase",0),pm.create("BerryPhase"),pm.create("CheckStatusEffectPhase"),pm.create("PositionalTagPhase"),end];
  Phaser.Math.RND.sow([`interlude-end-${seed}-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  rows.push(`C ${i} ${wave} ${alive} ${weather} ${terrain} ${turns}`,`W ${s.waveSeed}`,`S ${p.id} ${enemy.id} ${p.stats.join(" ")} ${enemy.stats.join(" ")}`,`I ${snap().join(" ")}`,`R ${rs(Phaser.Math.RND.state())}`,`B ${rs(s.currentBattle.battleSeedState)}`);
  const check=pm.create("CheckInterludePhase");pm.currentPhase=check;check.start();rows.push(`A ${+end.upcomingInterlude}`);pm.phaseQueue.levels[0]=pm.phaseQueue.levels[0].filter((q:any)=>q!==end);pm.currentPhase=end;end.start();
  rows.push(`O ${snap().join(" ")}`);const queue=pm.phaseQueue.levels.flat();rows.push(`Q ${queue.length}`);
  for(const ph of queue){if(!["WeatherEffectPhase","BerryPhase","CheckStatusEffectPhase","PositionalTagPhase","PokemonHealPhase","MessagePhase","HideAbilityPhase"].includes(ph.phaseName))throw Error("unexpected phase "+ph.phaseName);rows.push(`P ${ph.phaseName} ${ph.battlerIndex??-1} ${ph.hpHealed??0} ${hex(ph.message??ph.text??"")}`);}
  rows.push(`RO ${rs(Phaser.Math.RND.state())}`,`BO ${rs(s.currentBattle.battleSeedState)}`,"E");
 }
 rows.push(`INTERLUDEEND_COMPLETE ${N}`);fs.writeFileSync(process.env.INTERLUDE_OUT??"/tmp/interlude-end.txt",rows.join("\n")+"\n");
},600000);
