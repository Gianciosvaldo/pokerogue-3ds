/* Pinned charge owner and encounter retention; explicit user-requested cleanup policy. */
import { BattleScene } from "#app/battle-scene";
import { MoveChargeAnim } from "#data/battle-anims";
import { allMoves } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import { SpeciesId } from "#enums/species-id";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it,vi,afterAll } from "vitest";
const native=BattleScene.prototype.randBattleSeedInt;
const rows:any[]=[];
 const moves=[MoveId.DIG,MoveId.FLY,MoveId.PHANTOM_FORCE,MoveId.DIVE,MoveId.BOUNCE,MoveId.SHADOW_FORCE,MoveId.SKY_DROP];
 const semi=[BattlerTagType.FLYING,BattlerTagType.UNDERGROUND,BattlerTagType.UNDERWATER,BattlerTagType.HIDDEN];
 for(const move of moves)for(const slot of [0,1])for(const target of [2,3])it(`charge ${move} slot ${slot} target ${target}`,async()=>{
  const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
  g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP).disableTrainerWaves().mysteryEncounterChance(0);
  await g.classicMode.startBattle(SpeciesId.MEW,SpeciesId.MEW);
  BattleScene.prototype.randBattleSeedInt=native;
  g.override.moveset([]).enemyMoveset([]);
  const s:any=g.scene,pm:any=s.phaseManager,u:any=s.getPlayerField()[slot],enemies:any[]=s.getEnemyParty().slice();
  vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
  vi.spyOn(MoveChargeAnim.prototype,"play").mockImplementation((_skip:any,callback:any)=>callback?.());
  vi.spyOn(s.tweens,"addCounter").mockImplementation((config:any)=>{config.onComplete?.();return {} as any;});
  u.resetSummonData();u.moveset=[new PokemonMove(move),new PokemonMove(MoveId.SPLASH)];u.moveset[0].ppUsed=1;
  u.summonData.statStages[1]=2;
  u.pushMoveHistory({move:MoveId.FURY_CUTTER,useMode:1,result:1,targets:[2]});
  const frame=()=>({queue:u.getMoveQueue().map((q:any)=>({move:q.move,mode:q.useMode,targets:q.targets.slice()})),
   tags:u.summonData.tags.map((t:any)=>{expect(t.sourceId==null||t.sourceId===-1||t.sourceId===u.id).toBe(true);return {type:t.tagType,move:t.sourceMove,ownSource:t.sourceId===u.id,turns:t.turnCount};}),
   visible:u.visible,stages:u.summonData.statStages.slice(),pp:u.moveset[0].ppUsed,
   history:u.getMoveHistory().map((m:any)=>({move:m.move,mode:m.useMode,result:m.result??255,targets:m.targets.slice()}))});
  pm.clearAllPhases();const charge=pm.create("MoveChargePhase",slot,target,u.moveset[0],2);pm.currentPhase=charge;charge.start();
  const charged=frame();expect(charged.queue).toHaveLength(1);expect(charged.visible).toBe(false);
  for(const p of enemies)p.hp=0;
  pm.clearAllPhases();const end=pm.create("BattleEndPhase",true);pm.currentPhase=end;end.start();
  const ended=frame();expect(ended.queue).toEqual(charged.queue);expect(ended.visible).toBe(false);
  // Ordinary waves reset waveData, preserving summon history/charge state in the pin.
  u.resetWaveData();const nextWave=frame();expect(nextWave).toEqual(ended);
  const before=[Phaser.Math.RND.state(),s.currentBattle.battleSeedState];
  // This cleanup is the requested game policy, not an upstream BattleEndPhase behavior.
  u.summonData.moveQueue=u.getMoveQueue().filter((q:any)=>!allMoves[q.move].isChargingMove());
  for(const type of [BattlerTagType.CHARGING,...semi])if(u.getTag(type)&&allMoves[u.getTag(type).sourceMove].isChargingMove())u.removeTag(type);
  const cleaned=frame();expect(cleaned.queue).toHaveLength(0);expect(cleaned.tags).toHaveLength(0);expect(cleaned.visible).toBe(true);
  expect(cleaned.stages).toEqual(charged.stages);expect(cleaned.history).toEqual(charged.history);expect(cleaned.pp).toBe(1);
  expect([Phaser.Math.RND.state(),s.currentBattle.battleSeedState]).toEqual(before);
  rows.push({move,slot,target,charged,ended,nextWave,cleaned});
},30000);
afterAll(()=>{
 const chargingIds=allMoves.filter(m=>m?.isChargingMove()).map(m=>m.id);
 expect(rows.length).toBe(28);fs.writeFileSync(process.env.CHARGE_OUT!,JSON.stringify({version:1,chargingIds,rows})+"\n");
});
