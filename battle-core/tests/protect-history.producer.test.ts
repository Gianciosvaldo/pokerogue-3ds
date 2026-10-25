/* Complete source-owned Protect condition, including Number-sized RNG ranges. */
import { MovePhase } from "#phases/move-phase";
import { BattleScene } from "#app/battle-scene";
import { allMoves } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("native Protect complete streak and seeded condition owner",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await g.classicMode.startBattle(SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,u:any=s.getPlayerPokemon(),t:any=s.getEnemyPokemon(),b=s.currentBattle;
 const protect=allMoves.filter(m=>m?.hasAttr("ProtectAttr")).map(m=>m.id);
 const historyMoves=[...protect,MoveId.QUICK_GUARD,MoveId.WIDE_GUARD];
 const lengths=[0,1,7,8,9,19,20,33,34,40,100,645,646,647,648,65535,65536];
 const rows:any[]=[];
 for(const length of lengths)for(const historyMove of historyMoves)for(let mode=1;mode<=6;mode++){
  u.resetSummonData();expect(u.isOnField()).toBe(true);
  for(let i=0;i<length;i++)u.pushMoveHistory({move:historyMove,useMode:mode,result:1,targets:[0]});
  const history=u.getMoveHistory();expect(history.length).toBe(length);
  expect(history.every((m:any)=>m.move===historyMove&&m.useMode===mode&&m.result===1&&m.targets.length===1&&m.targets[0]===0)).toBe(true);
  const stages:any[]=[];
  for(let stage=0;stage<5;stage++){
   if(stage===1)u.pushMoveHistory({move:historyMove,useMode:mode,result:2,targets:[0]});
   if(stage===2)u.pushMoveHistory({move:MoveId.QUICK_GUARD,useMode:mode,result:1,targets:[0]});
   if(stage===3)u.pushMoveHistory({move:MoveId.SPLASH,useMode:mode,result:1,targets:[0]});
   if(stage===4)u.resetSummonData();
   const observations:any[]=[];
   for(const move of protect)for(let rng=0;rng<4;rng++){
    b.turn=13;b.battleSeed="protect-history-native";
    Phaser.Math.RND.sow(["protect-history-global"]);const global=Phaser.Math.RND.state();
    Phaser.Math.RND.sow(["protect-history-initial"]);const initialized=Phaser.Math.RND.state();Phaser.Math.RND.state(global);
    b.battleSeedState=rng===0?null:rng===1?initialized:rng===2?"!rnd,0,0,0,0":"!rnd,0,1.1102230246251565e-16,0,0";
    const before=b.battleSeedState;
    const result=allMoves[move].applyConditions(u,t,4);
    expect(Phaser.Math.RND.state()).toBe(global);
    observations.push({move,rng,before,after:b.battleSeedState,result});
   }
   stages.push(observations);
  }
  rows.push({length,historyMove,mode,stages});
 }
 u.resetSummonData();expect(allMoves[MoveId.PROTECT].applyConditions(u,t,4)).toBe(true);
 const gates:any[]=[];
 for(const queued of [false,true]){
  s.phaseManager.clearAllPhases();
  if(queued)s.phaseManager.pushPhase(new MovePhase(t,[0],t.getMoveset()[0],1));
  expect(s.phaseManager.hasPhaseOfType("MovePhase")).toBe(queued);
  const before=[Phaser.Math.RND.state(),b.battleSeedState];
  for(const move of historyMoves)gates.push({queued,move,result:allMoves[move].applyConditions(u,t,3)});
  expect([Phaser.Math.RND.state(),b.battleSeedState]).toEqual(before);
 }
 expect(rows.some(r=>r.stages[0].some((x:any)=>x.result))).toBe(true);
 expect(rows.some(r=>r.stages[0].some((x:any)=>!x.result))).toBe(true);
 expect(rows.some(r=>r.stages[0].some((x:any)=>x.before!==x.after))).toBe(true);
 expect(protect.length).toBeGreaterThan(0);expect(new Set(protect).size).toBe(protect.length);
 expect(rows.length).toBe(lengths.length*historyMoves.length*6);
 fs.writeFileSync(process.env.PROTECT_OUT!,JSON.stringify({version:1,protect,historyMoves,lengths,battleSeed:"protect-history-native",turn:13,gates,rows})+"\n");
},300000);
