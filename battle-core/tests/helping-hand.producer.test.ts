/* Actual HelpingHandTag lifecycle through Pokemon's public tag APIs. */
import { BattleScene } from "#app/battle-scene";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { BattlerTagLapseType } from "#enums/battler-tag-lapse-type";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual Helping Hand lifecycle",async()=>{
 const N=Number(process.env.HELPING_N??8192),SEED=Number(process.env.HELPING_SEED??777);if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<512)throw Error("Node22,N>=512");
 const native=BattleScene.prototype.randBattleSeedInt;const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,pm:any=s.phaseManager,actors=[...s.getPlayerParty(),...s.getEnemyParty()],ids=actors.map(p=>p.id);if(actors.length!==5)throw Error("natural parties required");g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors)vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(console,"warn").mockImplementation(()=>{});
 const rows=[`HELPING_HAND 1 ${N} ${SEED}`];
 const sequences=[["add"],["add","other"],["add","custom"],["add","end"],["add","pre"],["add","faint"],["add","remove"],["custom","add","custom","custom","other"],["add","source_remove"],["add","transfer","other"]];
 for(let i=0;i<N;i++){
  actors.forEach((p,k)=>{p.id=ids[k];p.resetSummonData();p.resetTurnData();p.customPokemonData.ability=AbilityId.NONE;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.hp=p.getMaxHp();});if(i%19===0)actors[Math.floor(i/19)%5].id=0;
  const target=i%5,which=Math.floor(i/5)%7,source=which<5?actors[which].id:which===5?0xffffffff:undefined,other=actors[(i+1)%5].id,turn=[0,1,3,-2][Math.floor(i/35)%4],move=[MoveId.NONE,MoveId.TACKLE,MoveId.HELPING_HAND][Math.floor(i/140)%3];if(i%23===0)actors[target].hp=0;
  pm.clearAllPhases();s.modifiers=[];s.enemyModifiers=[];Phaser.Math.RND.sow([`helping-${SEED}-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  const before=[Phaser.Math.RND.state(),s.currentBattle.battleSeedState],input=actors.map(p=>[p.id,p.species.speciesId,p.formIndex,p.level,p.getNature(),p.hp,p.getAbility().id,+p.isPlayer(),+p.isOnField(),...p.ivs,...p.stats]);
  const snapshots=[];
  for(const op of sequences[Math.floor(i/420)%sequences.length]){
   const p=actors[target];let result:any=null;
   if(op==="add"||op==="other")result=p.addTag(BattlerTagType.HELPING_HAND,turn,move,op==="add"?source:other);
   else if(op==="custom")result=p.lapseTag(BattlerTagType.HELPING_HAND,BattlerTagLapseType.CUSTOM);
   else if(op==="end")p.lapseTags(BattlerTagLapseType.TURN_END);
   else if(op==="pre")p.lapseTags(BattlerTagLapseType.PRE_MOVE);
   else if(op==="faint")p.lapseTags(BattlerTagLapseType.FAINT);
   else if(op==="remove")p.removeTag(BattlerTagType.HELPING_HAND);
   else if(op==="source_remove")p.removeTagsBySourceId(source);
   else if(op==="transfer")p.transferTagsBySourceId(source,other);
   snapshots.push({op,result,tags:p.summonData.tags.map((t:any)=>({type:t.tagType,count:t.turnCount,move:t.sourceMove??0,source:t.sourceId??-1,lapse:t.lapseTypes,baton:+t.isBatonPassable(p)})),queue:pm.phaseQueue.levels.map((q:any[])=>q.map(p=>({name:p.phaseName,text:p.text??""}))),dynamic:[...pm.dynamicQueueManager.dynamicPhaseMap.values()].reduce((n:any,q:any)=>n+q.queue.length,0)});
  }
  rows.push(JSON.stringify({i,target,source:source??-1,other,turn,move,waveSeed:s.waveSeed,input,before,snapshots,after:[Phaser.Math.RND.state(),s.currentBattle.battleSeedState]}));
 }
 rows.push(`HELPING_HAND_COMPLETE ${N}`);fs.writeFileSync(process.env.HELPING_OUT??"/tmp/helping-hand.jsonl",rows.join("\n")+"\n");
},600000);
