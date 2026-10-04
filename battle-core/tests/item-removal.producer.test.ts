/* Additive oracle: actual RemoveHeldItemAttr, loseHeldItem and modifier update. */
import { BattleScene } from "#app/battle-scene";
import { globalScene } from "#app/global-scene";
import { modifierTypes, allMoves } from "#data/data-lists";
import { RemoveHeldItemAttr } from "#moves/move";
import { BerryModifierType } from "#modifiers/modifier-type";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import { it, expect, vi } from "vitest";
import * as fs from "node:fs";
it("bounded item removal oracle", async()=>{
 const n=Number(process.env.REMOVE_N??2000),initial=Number(process.env.REMOVE_SEED??777);let seed=initial>>>0;
 const rnd=(k:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%k;};
 const realBattleRoll=BattleScene.prototype.randBattleSeedInt;
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
  .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH).startingWave(5);
 await game.classicMode.startBattle(SpeciesId.MAGIKARP);
 BattleScene.prototype.randBattleSeedInt=realBattleRoll;
 game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 const p:any=game.field.getPlayerPokemon(),enemy:any=game.field.getEnemyPokemon();
 const funcs=[modifierTypes.LEFTOVERS,modifierTypes.SHELL_BELL,modifierTypes.FOCUS_BAND,modifierTypes.QUICK_CLAW,modifierTypes.KINGS_ROCK,modifierTypes.WHITE_HERB];
 let messages:string[]=[];vi.spyOn(globalScene.phaseManager,"queueMessage").mockImplementation((s:any)=>{messages.push(s);});
 const lines=[`V 1 ${n}`],success=[0,0],reject=[0,0];
 for(let i=0;i<n;i++){
  const player=rnd(2),berries=i%2,ability=[AbilityId.BALL_FETCH,AbilityId.STICKY_HOLD,AbilityId.UNBURDEN][rnd(3)],supp=rnd(3)===0?1:0,passive=rnd(4)===0?AbilityId.STICKY_HOLD:0;
  const target=player?p:enemy,user=player?enemy:p;
  for(const q of [p,enemy]){q.resetSummonData();q.resetTurnData();q.hp=100;q.passive=false;q.customPokemonData.passive=-1;q.summonData.ability=AbilityId.BALL_FETCH;}
  target.summonData.ability=ability;target.summonData.abilitySuppressed=!!supp;target.customPokemonData.passive=passive;target.passive=!!passive;
  globalScene.modifiers=[];globalScene.enemyModifiers=[];
  const rows:number[][]=[],mods:any[]=[];
  const count=rnd(7);
  for(let j=0;j<count;j++){
   const kind=rnd(17),stack=1+rnd(3),transfer=rnd(4)!==0?1:0,owner=rnd(5)===0?0:1;
   const type=kind<6?funcs[kind]():new BerryModifierType(kind-6);
   const m:any=type.newModifier(target);m.stackCount=stack;m.isTransferable=!!transfer;m.pokemonId=owner?target.id:user.id;
   (player?globalScene.modifiers:globalScene.enemyModifiers).push(m);rows.push([kind,stack,transfer,owner]);mods.push(m);
  }
  // The opposite holder's list must remain intact (removal never transfers).
  const other:any=modifierTypes.LEFTOVERS().newModifier(user);other.stackCount=2;
  (player?globalScene.enemyModifiers:globalScene.modifiers).push(other);
  Phaser.Math.RND.sow([`removal-global-${initial}-${i}`]);const globalBefore=Phaser.Math.RND.state();
  Phaser.Math.RND.sow([`removal-battle-${initial}-${i}`]);const before=Phaser.Math.RND.state();
  globalScene.currentBattle.battleSeedState=before;Phaser.Math.RND.state(globalBefore);messages=[];
  const ok=new RemoveHeldItemAttr(!!berries).apply(user,target,allMoves[berries?MoveId.INCINERATE:MoveId.KNOCK_OFF],[]);
  const after=globalScene.currentBattle.battleSeedState;
  expect(Phaser.Math.RND.state()).toBe(globalBefore);expect(other.stackCount).toBe(2);
  (ok?success:reject)[berries]++;
  lines.push([player,berries,ability,supp,passive,count,...rows.flat()].join(" "));
  lines.push(before);lines.push(after);
  const remaining=player?globalScene.modifiers:globalScene.enemyModifiers;
  lines.push([Number(ok),Number(!!target.getTag(BattlerTagType.UNBURDEN)),remaining.length,...remaining.flatMap((m:any)=>[mods.indexOf(m),m.stackCount]),messages.length].join(" "));
  for(const message of messages){const b=[...Buffer.from(message,"utf8")];lines.push([b.length,...b].join(" "));}
 }
 for(const x of [...success,...reject])expect(x).toBeGreaterThan(0);
 lines.push(`Z ${n}`);fs.writeFileSync(process.env.REMOVE_OUT??"/tmp/removal.txt",lines.join("\n")+"\n");
 console.log(`Removal ${n} cases; success KnockOff/Incinerate ${success}; rejected ${reject}; global RNG unchanged; thrown/skipped 0`);
},120000);
