/* Additive actual Scene.addModifier and BattleEndPhase timer proof. Presentation
 * refresh/destroy/end are headless boundaries, not compared lifecycle claims. */
import { audioManager } from "#app/global-audio-manager";
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TempCritBoosterModifier, TempStatStageBoosterModifier } from "#modifiers/modifier";
import { TempStatStageBoosterModifierType } from "#modifiers/modifier-type";
import { BattleEndPhase } from "#phases/battle-end-phase";
import { ModifierData } from "#system/modifier-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect,it,vi } from "vitest";
it("temporary Scene and BattleEnd caller",async()=>{
 const N=Number(process.env.GC_TEMP_N??2000),SEED=Number(process.env.GC_TEMP_SEED??777);
 expect(process.versions.node.split(".")[0]).toBe("22");expect(Number.isSafeInteger(N)&&N>=120).toBe(true);
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return (seed>>>8)%n;};
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").startingWave(5).ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 const scene=game.scene as any,lines=[`GC_TEMP_CALLER 1 ${N}`];let message="",errors=0;
 vi.spyOn(scene.ui,"playError").mockImplementation(()=>{errors++;});
 vi.spyOn(scene.ui,"clearText").mockImplementation(()=>{});
 vi.spyOn(scene.ui,"setMode").mockImplementation(()=>{});
 vi.spyOn(scene.ui,"getHandler").mockImplementation(()=>({updateCostText(){}}) as any);
 vi.spyOn(scene.phaseManager,"shiftPhase").mockImplementation(()=>{});
 vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});
 vi.spyOn(scene,"updateModifiers").mockImplementation(()=>{});
 vi.spyOn(scene.phaseManager,"queueMessage").mockImplementation((text:any)=>{message=String(text);});
 for(const p of scene.getEnemyParty())vi.spyOn(p,"destroy").mockImplementation(()=>{});
 const rng=()=>Phaser.Math.RND.state();
 for(let ci=0;ci<N;ci++) {
  const family=ci%7,arg=family===6?0:family+1,mode=Math.floor(ci/7)%4,tier=rnd(5),ticks=1+rnd(7),victory=ci%2===0;
  const create=()=>{if(family===6)return modifierTypes.DIRE_HIT().withIdFromFunc(modifierTypes.DIRE_HIT);const t=new TempStatStageBoosterModifierType(arg);t.id="TEMP_STAT_STAGE_BOOSTER";return t;};
  const type=create();type.tier=tier;scene.modifiers.length=0;message="";errors=0;const paid=ci%3===0;scene.money=9999+ci;
  const stack=mode?1+rnd(2):0,duration=mode===2?5:mode?1+rnd(4):0;
  if(mode)scene.modifiers.push(family===6?new TempCritBoosterModifier(type,5,duration,stack):new TempStatStageBoosterModifier(type,arg,5,duration,stack));
  for(let j=0;j<5;j++)scene.pokeballCounts[j]=j?0:5;
  const before=Array.from({length:5},(_,j)=>scene.pokeballCounts[j]),beforeRng=rng();
  (new SelectModifierPhase() as any).applyModifier(type.newModifier(),paid?42:-1,false);
  const item=scene.modifiers.find((m:any)=>m instanceof TempStatStageBoosterModifier||m instanceof TempCritBoosterModifier);
  expect(item).toBeDefined();expect(item.getBattleCount()).toBe(5);
  const data=new ModifierData(item,true),restored=new ModifierData(JSON.parse(JSON.stringify(data)),true).toModifier(family===6?TempCritBoosterModifier:TempStatStageBoosterModifier);
  expect(restored.getBattleCount()).toBe(5);expect(restored.stackCount).toBe(item.stackCount);
  lines.push(`C ${ci} ${family} ${arg} ${tier} ${stack} ${duration} ${ticks} ${+victory} ${+paid}`);
  lines.push(`A ${item.stackCount} ${item.getBattleCount()} ${+!!message} ${Array.from({length:5},(_,j)=>scene.pokeballCounts[j]-before[j]).join(" ")} ${scene.money-(9999+ci)} ${errors}`);
  if(message)expect(message).toContain("The stack for ");
  for(let tick=0;tick<ticks;tick++) {
   // A queued duplicate is culled by the actual phase; duration drops once.
   scene.phaseManager.removeAllPhasesOfType("SelectModifierPhase");
   scene.phaseManager.pushNew("BattleEndPhase",victory);scene.phaseManager.pushNew("BattleEndPhase",victory);
   scene.currentBattle.successfulRun=!victory;scene.currentBattle.moneyScattered=0;
   const phase=new BattleEndPhase(victory);vi.spyOn(phase,"end").mockImplementation(()=>{});phase.start();
   expect(scene.phaseManager.hasPhaseOfType("BattleEndPhase")).toBe(false);
   const left=scene.modifiers.find((m:any)=>m instanceof TempStatStageBoosterModifier||m instanceof TempCritBoosterModifier);
   if(left){const data=new ModifierData(left,true),restored=new ModifierData(JSON.parse(JSON.stringify(data)),true).toModifier(family===6?TempCritBoosterModifier:TempStatStageBoosterModifier);expect(restored.getBattleCount()).toBe(left.getBattleCount());expect(restored.stackCount).toBe(left.stackCount);}
   lines.push(`T ${tick} ${left?.stackCount??0} ${left?.getBattleCount()??0}`);
  }
  expect(rng()).toBe(beforeRng);lines.push(`E ${ci}`);
 }
 lines.push(`Z ${N}`);fs.writeFileSync(process.env.GC_TEMP_OUT??"/tmp/game-temp-caller.txt",lines.join("\n")+"\n");
 console.log(`GC_TEMP actual Scene/BattleEnd cases=${N}, duplicate phases culled, RNG unchanged`);
},240000);
