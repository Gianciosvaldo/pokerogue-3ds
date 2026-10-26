/* Real Fusion turn; native constructors separately witness an allowed ID collision. */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { randSeedInt } from "#utils/common";
import { AbilityId } from "#enums/ability-id";
import { BattlerIndex } from "#enums/battler-index";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("Fusion follows actor objects when constructor IDs collide",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).startingLevel(1000).enemyLevel(1000).moveset([MoveId.FUSION_FLARE,MoveId.FUSION_BOLT,MoveId.SPLASH]).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.BLISSEY).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.BLISSEY,SpeciesId.BLISSEY);BattleScene.prototype.randBattleSeedInt=native;
 Phaser.Math.RND.sow(["turn-actor-constructor-collision"]);
 const seen=new Map<number,{index:number,state:string}>();let witness:any;
 for(let index=0;index<400000;index++){const state=Phaser.Math.RND.state(),id=randSeedInt(4294967296),prior=seen.get(id);if(prior){witness={id,indices:[prior.index,index],states:[prior.state,state]};break;}seen.set(id,{index,state});}
 expect(witness).toBeDefined();seen.clear();
 const constructed=witness.states.map((state:string)=>{Phaser.Math.RND.state(state);return g.scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.BLISSEY),1000,0,0,1,false,0,Array(6).fill(15),0);});
 expect(constructed[0]).not.toBe(constructed[1]);expect(constructed.map((p:any)=>p.id)).toEqual([witness.id,witness.id]);
 const actors=g.scene.getField(true);for(const [i,p]of actors.entries()){
  g.move.changeMoveset(p,[MoveId.FUSION_FLARE,MoveId.FUSION_BOLT,MoveId.SPLASH]);p.id=i<2?witness.id:11*(i+1);p.ivs=Array(6).fill(15);p.nature=0;p.friendship=70;p.calculateStats();p.hp=p.getMaxHp();p.setStatStage(Stat.SPD,[-1,0,-3,-2][i]);
 }
 g.scene.waveSeed="turn-actor-wave";g.scene.currentBattle.battleSeed="turn-actor-battle";
 Phaser.Math.RND.sow(["turn-actor-global"]);const global=Phaser.Math.RND.state();Phaser.Math.RND.sow(["turn-actor-initial-battle"]);g.scene.currentBattle.battleSeedState=Phaser.Math.RND.state();Phaser.Math.RND.state(global);
 const frame=()=>({actors:actors.map(p=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,hp:p.hp,stats:p.stats,stages:p.summonData.statStages,moves:p.getMoveset().map(m=>({move:m.moveId,pp:m.ppUsed})),history:p.getLastXMoves(-1)})),global:Phaser.Math.RND.state(),battle:g.scene.currentBattle.battleSeedState});
 const before=JSON.parse(JSON.stringify(frame()));
 g.move.select(MoveId.FUSION_FLARE,BattlerIndex.PLAYER,BattlerIndex.ENEMY);g.move.select(MoveId.FUSION_BOLT,BattlerIndex.PLAYER_2,BattlerIndex.ENEMY);
 await g.move.selectEnemyMove(MoveId.SPLASH);await g.move.selectEnemyMove(MoveId.SPLASH);
 await g.phaseInterceptor.to("BerryPhase");const order=g.scene.phaseManager.dynamicQueueManager.getLastTurnOrder().map(p=>actors.indexOf(p));expect(order).toEqual([1,0,3,2]);expect(actors.every(p=>p.hp>0)).toBe(true);
 fs.writeFileSync(process.env.TURN_ACTOR_COMMAND_OUT!,JSON.stringify({version:1,witness,before,after:frame(),order})+"\n");
},180000);
