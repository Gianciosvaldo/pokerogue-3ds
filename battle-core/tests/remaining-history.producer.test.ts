/* Actual all-history callers, including virtual entries and shared targets. */
import { BattleScene } from "#app/battle-scene";
import { allMoves } from "#data/data-lists";
import { MoveEffectPhase } from "#phases/move-effect-phase";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("native Conversion 2 and accuracy owner across complete history",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await g.classicMode.startBattle(SpeciesId.MEW,SpeciesId.MEW);
 BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,u:any=s.getPlayerField()[0],t:any=s.getEnemyField()[0],field:any[]=s.getField(),rows:any[]=[];
 const lengths=[0,1,7,8,9,64,65535,65536],moves=[MoveId.NONE,MoveId.TACKLE,MoveId.LOCK_ON,MoveId.MIND_READER];
 const query=()=>{
  const rng=[Phaser.Math.RND.state(),s.currentBattle.battleSeedState];
  // Use the enemy as Conversion's user, observing our subject's history.
  const conversion=allMoves[MoveId.CONVERSION_2].applyConditions(t,u,4);
  const phase=new MoveEffectPhase(0,[2],allMoves[MoveId.TACKLE],1);
  const accuracy=field.map(p=>{const v=phase.checkBypassAccAndInvuln(p);return v===undefined?-1:Number(v);});
  expect([Phaser.Math.RND.state(),s.currentBattle.battleSeedState]).toEqual(rng);
  return {conversion,accuracy};
 };
 for(const length of lengths)for(const move of moves)for(let mode=1;mode<=6;mode++){
  u.resetSummonData();t.resetSummonData();expect(u.isOnField()).toBe(true);
  const shared=[3,2];
  for(let i=0;i<length;i++)u.pushMoveHistory({move:i===0?move:MoveId.NONE,useMode:mode,result:i%5,targets:i===0?shared:[2]});
  expect(u.getMoveHistory()).toHaveLength(length);
  const stages:any[]=[];
  stages.push(query());
  u.addTag(BattlerTagType.IGNORE_ACCURACY,2,MoveId.LOCK_ON,u.id);stages.push(query());
  shared[0]=1;stages.push(query());
  u.pushMoveHistory({move:MoveId.LOCK_ON,useMode:mode,result:2,targets:[0,3]});stages.push(query());
  for(let i=0;i<9;i++)u.pushMoveHistory({move:MoveId.NONE,useMode:mode,result:4,targets:[2]});stages.push(query());
  // Every entry mode counts; a failed lock still controls the last matching search.
  u.pushMoveHistory({move:MoveId.MIND_READER,useMode:mode,result:3,targets:[1]});stages.push(query());
  u.resetSummonData();stages.push(query());
  const fresh=[2];u.pushMoveHistory({move:MoveId.MIND_READER,useMode:mode,result:2,targets:fresh});
  for(let i=0;i<9;i++)u.pushMoveHistory({move:MoveId.NONE,useMode:mode,result:4,targets:[2]});stages.push(query());
  // Re-add the tag after summon reset, then test old versus current shared lists.
  u.addTag(BattlerTagType.IGNORE_ACCURACY,2,MoveId.MIND_READER,u.id);
  shared[0]=0;stages.push(query());fresh[0]=3;stages.push(query());
  rows.push({length,move,mode,stages});
 }
 expect(rows).toHaveLength(192);
 expect(rows.some(r=>r.stages[0].conversion)).toBe(true);
 expect(rows.some(r=>!r.stages[0].conversion)).toBe(true);
 expect(rows.some(r=>r.stages[1].accuracy.some((x:number)=>x===1))).toBe(true);
 fs.writeFileSync(process.env.REMAINING_HISTORY_OUT!,JSON.stringify({version:1,rows})+"\n");
},180000);
