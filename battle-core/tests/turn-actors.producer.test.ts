/* Complete order arrays and history edits are explicit API inputs; the separate
 * Dancer/Instruct command producer exercises the natural move-pop owner. */
import { BattleScene } from "#app/battle-scene";
import { allMoves } from "#data/data-lists";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { MoveResult } from "#enums/move-result";
import { MoveUseMode } from "#enums/move-use-mode";
import { SpeciesId } from "#enums/species-id";
import { TrainerSlot } from "#enums/trainer-slot";
import { MovePhase } from "#phases/move-phase";
import { PokemonMove } from "#moves/pokemon-move";
import { NumberHolder } from "#utils/common";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("native turn-order object identities",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.MEW,SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,pm:any=s.phaseManager,b:any=s.currentBattle;
 const players:any[]=[...s.getPlayerParty()],enemies:any[]=[...s.getEnemyParty()];
 while(players.length<6)players.push(s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.MEW),50));
 while(enemies.length<6)enemies.push(s.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP),50,TrainerSlot.NONE));
 s.getPlayerParty().splice(0,s.getPlayerParty().length,...players);b.enemyParty=enemies;
 const actors=[...players,...enemies],ids=[0,4294967295,2147483648,7,11,21,31,41,51,61,71,81];
 actors.forEach((p,i)=>{p.id=ids[i];g.move.changeMoveset(p,MoveId.SPLASH);p.level=50;p.ivs=Array(6).fill(15);p.nature=0;p.calculateStats();p.hp=p.getMaxHp();s.field.remove(p);});
 for(const p of [players[0],players[1],enemies[0],enemies[1]])s.field.add(p);
 s.waveSeed="turn-order-api-wave";b.turn=1;b.battleSeed="turn-order-api-battle";
 Phaser.Math.RND.sow(["turn-order-api-global"]);const globalRng=Phaser.Math.RND.state();Phaser.Math.RND.sow(["turn-order-api-initial-battle"]);b.battleSeedState=Phaser.Math.RND.state();Phaser.Math.RND.state(globalRng);
 const queue:any=pm.dynamicQueueManager.getMovePhaseQueue();const rows:any[]=[];
 const query=()=>{const rng=[Phaser.Math.RND.state(),b.battleSeedState];const values=[MoveId.FUSION_FLARE,MoveId.FUSION_BOLT].map(move=>{const holder=new NumberHolder(1);const attr:any=allMoves[move].getAttrs("LastMoveDoublePowerAttr")[0];const applied=attr.apply(actors[0],actors[6],allMoves[move],[holder]);return {move,applied,power:holder.value};});expect([Phaser.Math.RND.state(),b.battleSeedState]).toEqual(rng);return {values,rng};};
 const pop=()=>{const phase=new MovePhase(actors[0],[2],actors[0].getMoveset()[0],MoveUseMode.NORMAL);queue.push(phase);expect(queue.pop()).toBe(phase);};
 for(const variant of [0,1,2])for(const length of [0,1,2,8,9,19,256])for(let pattern=0;pattern<4;pattern++)for(const selected of [0,1,6,11]){
  s.getPlayerParty().splice(0,s.getPlayerParty().length,...players);b.enemyParty=[...enemies];
  const rowIds=ids.map((id,i)=>variant===2?957185811:variant===1&&i<2?957185811:id);actors.forEach((p,i)=>p.id=rowIds[i]);
  pm.clearAllPhases();actors.forEach(p=>{p.resetSummonData();p.resetTurnData();});
  const filler=(selected+1)%12;
  const initial=actors.map((p,i)=>({move:i===selected?(pattern%2?MoveId.FUSION_BOLT:MoveId.FUSION_FLARE):MoveId.TACKLE,result:i===selected||pattern===2&&i===filler?MoveResult.SUCCESS:MoveResult.FAIL,useMode:MoveUseMode.NORMAL,targets:[2]}));
  actors.forEach((p,i)=>p.getMoveHistory().push({...initial[i]}));
  const order=Array.from({length},(_,i)=>pattern===0?(i===0?selected:filler):pattern===1?(i>=length-2?selected:filler):pattern===2?(i%2?filler:selected):(i+selected)%12);
  queue.lastTurnOrder=order.map(i=>actors[i]);const stages=[query()];
  const update={move:MoveId.FUSION_FLARE,result:[MoveResult.PENDING,MoveResult.MISS,MoveResult.OTHER][pattern%3],useMode:MoveUseMode.NORMAL,targets:[2]};actors[selected].getMoveHistory().push({...update});stages.push(query());
  actors.forEach((p,i)=>p.id=(rowIds[i]^0x80000000)>>>0);stages.push(query());
  const party=selected<6?s.getPlayerParty():s.getEnemyParty();party.splice(party.indexOf(actors[selected]),1);stages.push(query());
  pm.dynamicQueueManager.clearLastTurnOrder();stages.push(query());
  rows.push({variant,ids:rowIds,length,pattern,selected,order,initial,update,stages});
 }
 expect(rows).toHaveLength(336);fs.writeFileSync(process.env.TURN_ACTORS_OUT!,JSON.stringify({version:1,globalRng,battleRng:rows[0].stages[0].rng[1],rows})+"\n");
},300000);
