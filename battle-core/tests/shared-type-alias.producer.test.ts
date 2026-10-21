import { BattleScene } from "#app/battle-scene";
import { allMoves,modifierTypes } from "#data/data-lists";
import { getModifierType } from "#utils/modifier-utils";
import { StealHeldItemChanceAttr } from "#moves/move";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect,it,vi } from "vitest";
it("native partial-transfer type alias survives later theft on another holder",async()=>{
 const nativeRoll=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE);
 BattleScene.prototype.randBattleSeedInt=nativeRoll;
 const s:any=g.scene,actors:any[]=s.getField();expect(actors.filter(Boolean)).toHaveLength(4);
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 vi.spyOn(s.phaseManager,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});p.customPokemonData.ability=AbilityId.NONE;if(p.isEnemy())p.trainerSlot=1;}
 const rows:any[]=[];
 for(const key of ["SCOPE_LENS","LEFTOVERS","SHELL_BELL","KINGS_ROCK"]) for(const enemy of [false,true]) {
  s.modifiers=[];s.enemyModifiers=[];s.phaseManager.clearAllPhases();
  const owner=enemy?2:0,recipient=enemy?0:2,thief=enemy?3:1;
  const type:any=getModifierType((modifierTypes as any)[key]);
  const original:any=type.newModifier(actors[owner]);original.stackCount=2;
  (enemy?s.enemyModifiers:s.modifiers).push(original);
  const before=type.tier??null;
  expect(s.tryTransferHeldItemModifier(original,actors[recipient],false,1,true,true)).toBe(true);
  const clone:any=(enemy?s.modifiers:s.enemyModifiers).find((m:any)=>m.pokemonId===actors[recipient].id);
  expect(original.stackCount).toBe(1);expect(clone.type).toBe(original.type);
  const attr=new StealHeldItemChanceAttr(1);
  expect(attr.apply(actors[thief],actors[recipient],allMoves[MoveId.THIEF],[])).toBe(true);
  const expectedTier=enemy?3:4;
  expect(original.type.tier).toBe(expectedTier);
  const beforeSecond=original.type.tier;
  expect(attr.apply(actors[recipient],actors[owner],allMoves[MoveId.THIEF],[])).toBe(true);
  const returned:any=(enemy?s.modifiers:s.enemyModifiers).find((m:any)=>m.pokemonId===actors[recipient].id);
  expect(returned.type).toBe(type);expect(returned.type.tier).toBe(expectedTier);
  rows.push({key,enemy,owner,recipient,thief,before,sharedAfterPartial:clone.type===original.type,beforeSecond,afterSecond:returned.type.tier});
 }
 fs.writeFileSync(process.env.ALIAS_OUT??"/tmp/type-alias.json",JSON.stringify({version:1,source:"8555c08c823b856cbec4eb99ca84ea52a955836d",rows},null,2));
},60000);
