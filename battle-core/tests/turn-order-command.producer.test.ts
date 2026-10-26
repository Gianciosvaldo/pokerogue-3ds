/* Actual four-action Dancer/Instruct reachability probe. */
import { allMoves } from "#data/data-lists";
import { NumberHolder } from "#utils/common";
import { BattleScene } from "#app/battle-scene";
import { AbilityId } from "#enums/ability-id";
import { BattlerIndex } from "#enums/battler-index";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("Dancer and Instruct exceed eight recorded movers",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.DANCER).enemyAbility(AbilityId.DANCER).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).startingLevel(1000).enemyLevel(1000).moveset([MoveId.FIERY_DANCE,MoveId.INSTRUCT]).enemyMoveset([MoveId.FIERY_DANCE,MoveId.INSTRUCT]).enemySpecies(SpeciesId.BLISSEY).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.BLISSEY,SpeciesId.BLISSEY);BattleScene.prototype.randBattleSeedInt=native;
 const actors=g.scene.getField(true);for(const p of actors)g.move.changeMoveset(p,[MoveId.FIERY_DANCE,MoveId.INSTRUCT]);
 for(const [i,p]of actors.entries()){p.id=(i+1)*11;p.ivs=Array(6).fill(15);p.nature=0;p.friendship=70;p.calculateStats();p.hp=p.getMaxHp();p.setStatStage(Stat.SPD,[0,-1,-3,-2][i]);}
 g.scene.waveSeed="turn-order-wave";g.scene.currentBattle.battleSeed="turn-order-battle";
 Phaser.Math.RND.sow(["turn-order-global"]);const global=Phaser.Math.RND.state();Phaser.Math.RND.sow(["turn-order-initial-battle"]);g.scene.currentBattle.battleSeedState=Phaser.Math.RND.state();Phaser.Math.RND.state(global);
 const frame=()=>({actors:actors.map(p=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,hp:p.hp,stats:p.stats,stages:p.summonData.statStages,moves:p.getMoveset().map(m=>({move:m.moveId,pp:m.ppUsed})),history:p.getLastXMoves(-1)})),global:Phaser.Math.RND.state(),battle:g.scene.currentBattle.battleSeedState});
 const before=JSON.parse(JSON.stringify(frame()));
 g.move.select(MoveId.FIERY_DANCE,BattlerIndex.PLAYER,BattlerIndex.ENEMY);g.move.select(MoveId.INSTRUCT,BattlerIndex.PLAYER_2,BattlerIndex.PLAYER);
 await g.move.selectEnemyMove(MoveId.INSTRUCT,BattlerIndex.PLAYER);await g.move.selectEnemyMove(MoveId.INSTRUCT,BattlerIndex.PLAYER);
 await g.phaseInterceptor.to("BerryPhase");
 const order=g.scene.phaseManager.dynamicQueueManager.getLastTurnOrder().map(p=>p.id);
 const rng=[Phaser.Math.RND.state(),g.scene.currentBattle.battleSeedState];
 const consumers=[MoveId.FUSION_FLARE,MoveId.FUSION_BOLT].map(move=>{const holder=new NumberHolder(1);const attr:any=allMoves[move].getAttrs("LastMoveDoublePowerAttr")[0];expect(attr).toBeDefined();const applied=attr.apply(actors[0],actors[2],allMoves[move],[holder]);return {move,applied,power:holder.value};});
 expect([Phaser.Math.RND.state(),g.scene.currentBattle.battleSeedState]).toEqual(rng);
 fs.writeFileSync(process.env.TURN_ORDER_COMMAND_OUT!,JSON.stringify({version:1,before,after:frame(),order,consumers})+"\n");
 expect(order).toHaveLength(19);expect(actors.every(p=>p.hp>0)).toBe(true);
},120000);
