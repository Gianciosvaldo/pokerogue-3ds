/* Run the actual pinned EncounterPhase; only presentation/session hooks are stubbed. */
import { BattleScene } from "#app/battle-scene";
import { activeOverrides } from "#app/overrides";
import { getGameMode } from "#app/game-mode";
import { GameModes } from "#enums/game-modes";
import { BattleType } from "#enums/battle-type";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { EnemyPokemon } from "#field/pokemon";
import { EncounterPhase } from "#phases/encounter-phase";
import * as modifierPools from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect,it,vi } from "vitest";
it("actual two-enemy EncounterPhase constructor order",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).enemySpecies(129).enemyMoveset(MoveId.SPLASH);
 await g.classicMode.startBattle(1);BattleScene.prototype.randBattleSeedInt=native;
 const scene:any=g.scene,b:any=scene.currentBattle;
 for(const [k,v] of Object.entries({ENEMY_SPECIES_OVERRIDE:null,ENEMY_LEVEL_OVERRIDE:0,ENEMY_IVS_OVERRIDE:null,ENEMY_NATURE_OVERRIDE:null,ENEMY_GENDER_OVERRIDE:null,ENEMY_FORM_OVERRIDES:{},ENEMY_SHINY_OVERRIDE:null,ENEMY_VARIANT_OVERRIDE:null,ENEMY_FUSION_OVERRIDE:false,ENEMY_ABILITY_OVERRIDE:null,ENEMY_MOVESET_OVERRIDE:[],ENEMY_HEALTH_SEGMENTS_OVERRIDE:0}))vi.spyOn(activeOverrides as any,k,"get").mockReturnValue(v);
 for(const k of ["updateGameInfo","initSession"])vi.spyOn(scene,k).mockImplementation(()=>{});
 vi.spyOn(scene.gameData,"setPokemonSeen").mockImplementation(()=>{});
 vi.spyOn(EnemyPokemon.prototype,"fieldSetup").mockImplementation(()=>{});
 let reached:()=>void;let constructed=0;
 vi.spyOn(scene.ui,"setMode").mockImplementation(()=>new Promise(()=>{}));
 vi.spyOn(scene,"generateEnemyModifiers").mockImplementation(()=>{scene.updateModifiers(false,true);reached();return Promise.resolve();});
 vi.spyOn(EnemyPokemon.prototype,"loadAssets").mockImplementation(function(){constructed++;return Promise.resolve();});
 const rng=()=>Phaser.Math.RND.state().split(",").slice(1).join(" ");
 const mon=(p:any)=>[p.id,p.species.speciesId,p.formIndex,p.level,p.gender,p.nature,p.abilityIndex,+p.shiny,p.variant,p.friendship,p.teraType,p.bossSegments??0,p.fusionSpecies?.speciesId??0,p.fusionFormIndex??0,p.getMoveset().length,...Array.from({length:4},(_,j)=>p.getMoveset()[j]?.moveId??0),...p.ivs,...p.stats,p.hp];
 let boundaryRng="";
 const regenerate=modifierPools.regenerateModifierPoolThresholds;
 vi.spyOn(modifierPools,"regenerateModifierPoolThresholds").mockImplementation((...args:any[])=>{boundaryRng=rng();return regenerate(...args as Parameters<typeof regenerate>);});
 let moves:any[]=[];
 const generate=EnemyPokemon.prototype.generateAndPopulateMoveset;
 vi.spyOn(EnemyPokemon.prototype,"generateAndPopulateMoveset").mockImplementation(function(...args:any[]){
  const before=rng();generate.apply(this,args as any);moves.push([before,rng(),...Array.from({length:4},(_,i)=>this.getMoveset()[i]?.moveId??0)]);
 });
 const out=["DOUBLE_ENCOUNTER 1 120"];
 scene.gameMode=getGameMode(GameModes.ENDLESS);b.gameMode=scene.gameMode;b.battleType=BattleType.WILD;b.trainer=null;b.double=true;
 scene.modifiers.length=scene.enemyModifiers.length=0;scene.gameData.trainerId=123;scene.gameData.secretId=456;
 for(let i=0;i<120;i++){
  for(const p of b.enemyParty)p.destroy();b.enemyParty=[];scene.phaseManager.clearAllPhases();
  const wave=[2,9,21,49,51,75,99,101,249,499][i%10],level=5+i%60;
  scene.seed=`double-native-run-${i}`;scene.waveSeed=`double-native-wave-${i}`;scene.waveCycleOffset=0;scene.offsetGym=false;b.waveIndex=wave;scene.arena.biomeId=0;scene.arena.lastTimeOfDay=undefined;scene.arena.getTimeOfDay();scene.arena.updatePoolsForTimeOfDay();
  b.waveIndex=wave;b.enemyLevels=[level,level+1];b.battleSeed=`double-battle-${i}`;b.battleSeedState=null;b.turn=1;
  Phaser.Math.RND.sow([`double-constructor-${i}`]);const before=rng();constructed=0;moves=[];boundaryRng="";
  const ready=new Promise<void>(resolve=>{reached=resolve;});new EncounterPhase().start();await ready;
  expect(constructed).toBe(2);expect(b.enemyParty).toHaveLength(2);

  out.push(`C ${i} ${wave} ${level} ${scene.seed} ${scene.waveSeed}`,`R ${before}`,...moves.map((m,j)=>`M ${j} ${m.join(" ")}`),`A ${mon(b.enemyParty[0]).join(" ")}`,`B ${mon(b.enemyParty[1]).join(" ")}`,`S ${boundaryRng}`);
 }
 out.push("COMPLETE 120 240");fs.writeFileSync(process.env.DOUBLES_OUT!,out.join("\n")+"\n");
},180000);
