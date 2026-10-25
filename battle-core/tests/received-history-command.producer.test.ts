/* Reachability probe: four actual command actions, natural speed order and
 * +6 accuracy are fixture inputs. No history injection or mid-turn HP/PP repair. */
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
it("four command actions can produce forty received records",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).startingLevel(100).enemyLevel(1000).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.ORANGURU).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.MAUSHOLD,SpeciesId.ORANGURU);
 BattleScene.prototype.randBattleSeedInt=native;
 const actors=g.scene.getField(true),a=actors[0],defender=actors[2];
 for(const [i,p] of actors.entries()){p.id=(i+1)*11;p.ivs=Array(6).fill(15);p.nature=0;p.friendship=70;p.calculateStats();p.hp=p.getMaxHp();}
 g.move.changeMoveset(a,MoveId.POPULATION_BOMB);
 for(const p of actors.slice(1))g.move.changeMoveset(p,MoveId.INSTRUCT);
 a.setStatStage(Stat.ACC,6);
 actors.forEach((p,i)=>p.setStatStage(Stat.SPD,[6,6,-6,-5][i]));
 g.scene.waveSeed="received-command-wave";
 g.scene.currentBattle.battleSeed="received-command-battle";
 Phaser.Math.RND.sow(["received-command-global"]);const global=Phaser.Math.RND.state();
 Phaser.Math.RND.sow(["received-command-initial-battle"]);g.scene.currentBattle.battleSeedState=Phaser.Math.RND.state();Phaser.Math.RND.state(global);
 const before={speedStages:actors.map(p=>p.getStatStage(Stat.SPD)),actors:actors.map(p=>({id:p.id,species:p.species.speciesId,level:p.level,stats:p.stats,ivs:p.ivs,nature:p.nature})),levels:actors.map(p=>p.level),hp:defender.hp,pp:a.getMoveset()[0].ppUsed,global,battle:g.scene.currentBattle.battleSeedState};
 g.move.select(MoveId.POPULATION_BOMB,BattlerIndex.PLAYER,BattlerIndex.ENEMY);
 g.move.select(MoveId.INSTRUCT,BattlerIndex.PLAYER_2,BattlerIndex.PLAYER);
 await g.move.selectEnemyMove(MoveId.INSTRUCT,BattlerIndex.PLAYER);
 await g.move.selectEnemyMove(MoveId.INSTRUCT,BattlerIndex.PLAYER);
 await g.phaseInterceptor.to("BerryPhase");
 const order=g.scene.phaseManager.dynamicQueueManager.getLastTurnOrder().map(p=>p.id);
 expect(order).toEqual([11,22,11,44,11,33,11]);
 const records=defender.turnData.attacksReceived;
 expect(records).toHaveLength(40);expect(records.every(r=>r.damage>0&&r.move===MoveId.POPULATION_BOMB)).toBe(true);
 expect(a.getMoveset()[0].ppUsed).toBe(4);expect(defender.hp).toBeGreaterThan(0);
 fs.writeFileSync(process.env.RECEIVED_COMMAND_OUT!,JSON.stringify({order,before,after:{hp:defender.hp,pp:a.getMoveset()[0].ppUsed,global:Phaser.Math.RND.state(),battle:g.scene.currentBattle.battleSeedState},records})+"\n");
},120000);
