/* Native received-hit query owners with actual damage-owner record templates.
 * Complete long lists and zero/status records are explicit raw API inputs;
 * this producer does not label them complete legal command turns. */
import { BattleScene } from "#app/battle-scene";
import { allMoves,modifierTypes } from "#data/data-lists";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { BattlerTagLapseType } from "#enums/battler-tag-lapse-type";
import { BerryType } from "#enums/berry-type";
import { HitResult } from "#enums/hit-result";
import { MoveId } from "#enums/move-id";
import { PokemonType } from "#enums/pokemon-type";
import { SpeciesId } from "#enums/species-id";
import { TrainerSlot } from "#enums/trainer-slot";
import { MoveEffectPhase } from "#phases/move-effect-phase";
import { MovePhase } from "#phases/move-phase";
import { PokemonMove } from "#moves/pokemon-move";
import { NumberHolder } from "#utils/common";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it,vi } from "vitest";
it("native complete received-hit searches and Rage oldest hit",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.MEW,SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 g.override.moveset([]).enemyMoveset([]);
 const s:any=g.scene,b:any=s.currentBattle,pm:any=s.phaseManager;
 const players:any[]=[...s.getPlayerParty()],enemies:any[]=[...s.getEnemyParty()];
 players.push(s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.MEW),50));enemies.push(s.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP),50,TrainerSlot.NONE));
 const actors:any[]=[players[0],players[1],enemies[0],enemies[1],players[2],enemies[2]];
 const ids=[0,4294967295,2147483648,7,11,21];actors.forEach((p,k)=>{p.id=ids[k];p.level=50;p.ivs=Array(6).fill(15);p.nature=0;p.friendship=70;p.calculateStats();p.hp=p.getMaxHp();p.status=null;p.summonData.types=[PokemonType.GRASS];p.moveset=[new PokemonMove(MoveId.FOCUS_PUNCH),new PokemonMove(MoveId.SPLASH)];vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});});
 s.waveSeed="received-history-native";b.turn=1;b.battleSeed="received-history-battle";
 Phaser.Math.RND.sow(["received-history-global"]);const globalRng=Phaser.Math.RND.state();
 Phaser.Math.RND.sow(["received-history-initial-battle"]);b.battleSeedState=Phaser.Math.RND.state();Phaser.Math.RND.state(globalRng);
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});const insert=vi.spyOn(pm,"unshiftNew");
 const raw=(r:any)=>({move:r.move,result:r.result,damage:r.damage,critical:r.critical,sourceId:r.sourceId,sourceBattlerIndex:r.sourceBattlerIndex});
 const templates:any[]=[];
 const owner=(user:any,target:any,move:number,effectiveness:number)=>{
  pm.clearAllPhases();target.hp=target.getMaxHp();
  const phase:any=new MoveEffectPhase(user.getBattlerIndex(),[target.getBattlerIndex()],allMoves[move],1);
  const previous=target.turnData.attacksReceived.length;
  const result=phase.applyMoveDamage(user,target,effectiveness);
  expect(result[1]).toBeGreaterThan(0);expect(target.turnData.attacksReceived).toHaveLength(previous+1);
  const record=raw(target.turnData.attacksReceived[0]);expect(record.damage).toBe(result[1]);expect(record.critical).toBe(result[2]);expect(record.sourceId).toBe(user.id);
  target.hp=target.getMaxHp();return record;
 };
 for(const defender of [0,2])for(const attacker of [0,1,2,3])for(const [move,effectiveness]of [[MoveId.PECK,2],[MoveId.EMBER,2],[MoveId.TACKLE,1],[MoveId.CONFUSION,1]]){
  actors[defender].resetTurnData();const record=owner(actors[attacker],actors[defender],move,effectiveness);templates.push({defender,attacker,move,effectiveness,record});
 }
 const template=(defender:number,attacker:number,move:number)=>{const row=templates.find(t=>t.defender===defender&&t.attacker===attacker&&t.move===move);expect(row).toBeDefined();return {...row.record};};
 const counterMoves=[MoveId.COUNTER,MoveId.MIRROR_COAT,MoveId.METAL_BURST,MoveId.COMEUPPANCE];
 const focus:any=allMoves[MoveId.FOCUS_PUNCH].getAttrs("PreUseInterruptAttr")[0];expect(focus).toBeDefined();
 const rows:any[]=[];
 for(const subject of [0,2])for(const context of [0,1,2])for(const length of [0,1,31,32,33,64,65535,65536])for(let pattern=0;pattern<8;pattern++){
  const u=actors[subject],party=subject===0?players:enemies,other=actors[subject===0?2:0],enemy=subject===0?2:0,enemyOther=enemy+1,ally=subject+1;
  const ordered=party.filter(p=>p!==u);ordered.splice(context,0,u);
  if(subject===0)s.getPlayerParty().splice(0,3,...ordered);else b.enemyParty=ordered;
  // Restore the other side's natural indices; only the subject's context changes.
  if(subject===0)b.enemyParty=enemies.slice();else s.getPlayerParty().splice(0,3,...players);
  for(const p of actors)s.field.remove(p);
  for(const p of s.getField())if(p)s.field.add(p);
  expect(u.getBattlerIndex()).toBe(context===2?-1:subject+context);
  expect(u.isOnField()).toBe(context!==2);
  u.resetSummonData();u.resetTurnData();u.summonData.types=[PokemonType.GRASS];u.hp=u.getMaxHp();u.addTag(BattlerTagType.RAGE,1,MoveId.RAGE,u.id);
  let first=template(subject,enemy,pattern===1?MoveId.EMBER:MoveId.PECK);
  let filler=template(subject,pattern===2?enemyOther:pattern===1?ally:enemy,pattern===1||pattern===2?MoveId.TACKLE:MoveId.CONFUSION);
  if(pattern===3)first={...first,damage:0};
  if(pattern===4||pattern===5||pattern===6||pattern===7)filler={...filler,move:MoveId.GROWL,damage:pattern===7?1:0,result:HitResult.EFFECTIVE};
  if(pattern===5)first={...filler};
  if(pattern===6)first={...first,damage:-1};
  // Explicit complete raw TurnData input; subsequent append stages use the actual owner.
  u.turnData.attacksReceived=Array.from({length},(_,k)=>({...((k===length-1)?first:filler)}));
  const berryType:any=generateModifierType(modifierTypes.BERRY,[BerryType.ENIGMA]);const berry:any=berryType.newModifier(u);
  const query=()=>{
   const rng=[Phaser.Math.RND.state(),b.battleSeedState];pm.clearAllPhases();
   const counters=counterMoves.map(move=>{
    const damage=new NumberHolder(0),target=new NumberHolder(-2);
    const a:any=allMoves[move].getAttrs("CounterDamageAttr")[0],r:any=allMoves[move].getAttrs("CounterRedirectAttr")[0];
    expect(a).toBeDefined();expect(r).toBeDefined();const result=a.apply(u,other,allMoves[move],[damage]);const redirected=r.apply(u,other,allMoves[move],[target]);
    return {move,result,damage:damage.value,redirected,target:target.value,condition:allMoves[move].applyConditions(u,other,3)};
   });
   const powers=actors.map(t=>{const holder=new NumberHolder(1);const attr:any=allMoves[MoveId.REVENGE].getAttrs("TurnDamagedDoublePowerAttr")[0];const result=attr.apply(u,t,allMoves[MoveId.REVENGE],[holder]);return {result,value:holder.value};});
   const enigma=berry.shouldApply(u);
   pm.clearAllPhases();const current:any=new MovePhase(u,[other.getBattlerIndex()],u.moveset[0],1);pm.currentPhase=current;const interrupted=focus.apply(u,other,allMoves[MoveId.FOCUS_PUNCH]);const cancelled=!!current.cancelled;
   pm.clearAllPhases();const before=insert.mock.calls.length;const rageResult=u.getTag(BattlerTagType.RAGE).lapse(u,BattlerTagLapseType.AFTER_HIT);const added=insert.mock.calls.slice(before).map(([name,options]:any)=>({name,battlerIndex:options.battlerIndex,source:options.sourcePokemon===u,changes:options.changes}));
   const rageQueue={levels:pm.phaseQueue.levels.map((level:any[])=>level.map((p:any)=>p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName)),dynamic:[...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).map(([name,q]:any)=>({name,phases:q.queue.map((p:any)=>p.phaseName)}))};
   expect([Phaser.Math.RND.state(),b.battleSeedState]).toEqual(rng);
   return {counters,powers,enigma,interrupted,cancelled,rageResult,added,rageQueue,rng};
  };
  const stages:any[]=[query()];
  const incoming=owner(actors[enemy],u,MoveId.TACKLE,1);stages.push(query());
  for(let i=0;i<33;i++)u.turnData.attacksReceived.unshift({...filler,move:MoveId.GROWL,damage:0,result:HitResult.EFFECTIVE});stages.push(query());
  u.resetTurnData();stages.push(query());
  const fresh=owner(actors[enemy],u,MoveId.EMBER,2);stages.push(query());
  rows.push({subject,context,length,pattern,first,filler,incoming,fresh,stages});
 }
 expect(rows).toHaveLength(384);fs.writeFileSync(process.env.RECEIVED_HISTORY_OUT!,JSON.stringify({version:2,ids,templates,rows})+"\n");
},300000);
