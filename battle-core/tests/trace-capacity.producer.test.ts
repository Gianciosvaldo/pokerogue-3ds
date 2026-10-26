/* Actual Weak Armor + Stamina phases exceed the diagnostic trace prefix. */
import { BattleScene } from "#app/battle-scene";
import { AbilityId } from "#enums/ability-id";
import { BattlerIndex } from "#enums/battler-index";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { WeatherType } from "#enums/weather-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("forty hits finish beyond the diagnostic trace capacity",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.NATURAL_CURE).weather(WeatherType.SANDSTORM).enemyAbility(AbilityId.WEAK_ARMOR).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.STAMINA).startingLevel(1000).enemyLevel(1000).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.REGIROCK).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.BLISSEY,SpeciesId.BLISSEY);BattleScene.prototype.randBattleSeedInt=native;
 const actors=g.scene.getField(true);
 for(const[i,p]of actors.entries()){
  p.id=11*(i+1);p.ivs=Array(6).fill(15);p.nature=0;p.friendship=70;p.calculateStats();p.hp=p.getMaxHp();p.setStatStage(Stat.SPD,[0,-1,-3,-2][i]);g.move.changeMoveset(p,[MoveId.POPULATION_BOMB,MoveId.INSTRUCT]);
 }
 actors[0].setStatStage(Stat.ATK,-6);actors[0].setStatStage(Stat.ACC,6);
 g.scene.waveSeed="event-delivery-wave";g.scene.currentBattle.battleSeed="event-delivery-battle";
 Phaser.Math.RND.sow(["event-delivery-global"]);const global=Phaser.Math.RND.state();Phaser.Math.RND.sow(["event-delivery-initial-battle"]);g.scene.currentBattle.battleSeedState=Phaser.Math.RND.state();Phaser.Math.RND.state(global);
 const frame=()=>({weather:g.scene.arena.weather?{type:g.scene.arena.weather.weatherType,turns:g.scene.arena.weather.turnsLeft,max:g.scene.arena.weather.maxDuration}:{type:0,turns:0,max:0},terrain:g.scene.arena.terrain?{type:g.scene.arena.terrain.terrainType,turns:g.scene.arena.terrain.turnsLeft,max:g.scene.arena.terrain.maxDuration}:{type:0,turns:0,max:0},actors:actors.map(p=>({id:p.id,species:p.species.speciesId,form:p.formIndex,level:p.level,hp:p.hp,stats:p.stats,stages:p.summonData.statStages,gender:p.getGender(),types:p.getTypes(),ability:p.getAbility().id,passive:p.hasPassive()?p.getPassiveAbility().id:0,moves:p.getMoveset().map(m=>({move:m.moveId,pp:m.ppUsed})),history:p.getLastXMoves(-1)})),global:Phaser.Math.RND.state(),battle:g.scene.currentBattle.battleSeedState});
 const hits:any[]=[];const defender:any=actors[2],damage=defender.damageAndUpdate.bind(defender);
 defender.damageAndUpdate=(amount:number,options:any)=>{const result=damage(amount,options);hits.push({amount:result,hp:defender.hp,critical:!!options?.isCritical,result:options?.result});return result;};
 const before=JSON.parse(JSON.stringify(frame()));const texts:string[]=[];const ui:any=g.scene.ui,show=ui.showText.bind(ui);
 ui.showText=(text:string,...rest:any[])=>{if(!(rest[3]&&String(text).includes("$")))texts.push(String(text));return show(text,...rest);};
 g.phaseInterceptor.clearLogs();g.move.select(MoveId.POPULATION_BOMB,BattlerIndex.PLAYER,BattlerIndex.ENEMY);g.move.select(MoveId.INSTRUCT,BattlerIndex.PLAYER_2,BattlerIndex.PLAYER);
 await g.move.selectEnemyMove(MoveId.INSTRUCT,BattlerIndex.PLAYER);await g.move.selectEnemyMove(MoveId.INSTRUCT,BattlerIndex.PLAYER);// Run every native phase with the framework's unchanged per-phase wait. Its
 // .to() whole-turn 20s limit is shorter than this long presentation sequence.
 const pi:any=g.phaseInterceptor;let steps=0;
 while(!g.scene.phaseManager.getCurrentPhase().is("BerryPhase")){expect(steps++).toBeLessThan(2000);await pi.run(g.scene.phaseManager.getCurrentPhase());}
 await pi.run(g.scene.phaseManager.getCurrentPhase());
 const records=actors[2].turnData.attacksReceived;expect(records).toHaveLength(40);expect(hits).toHaveLength(40);expect(actors.every(p=>p.hp>0)).toBe(true);expect(actors[0].getMoveset()[0].ppUsed).toBe(4);expect(texts.length).toBeGreaterThan(40);
 const locales:any={};for(const name of ["battle","pokemon-info","move","ability","pokemon","weather","terrain","move-trigger"]){const path=`locales/en/${name}.json`;if(fs.existsSync(path))locales[name]=JSON.parse(fs.readFileSync(path,"utf8"));}
 fs.writeFileSync(process.env.TRACE_CAPACITY_OUT!,JSON.stringify({version:1,before,after:frame(),order:g.scene.phaseManager.dynamicQueueManager.getLastTurnOrder().map(p=>actors.indexOf(p)),records,hits,texts,phases:g.phaseInterceptor.phaseLog,locales})+"\n");
},180000);
