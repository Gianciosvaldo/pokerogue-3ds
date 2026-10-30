/* Canonical inventory and actual strike factors in contexts still guarded for
 * whole-Scene/stat parity. Source pin8555c08, Node22. */
import {BattleScene} from "#app/battle-scene";
import {modifierTypes} from "#data/data-lists";
import {speciesDataRegistry} from "#app/globals/global-species-data-registry";
import {AbilityId} from "#enums/ability-id";
import {MoveId} from "#enums/move-id";
import {SpeciesId} from "#enums/species-id";
import {BaseStatBoosterModifierType} from "#modifiers/modifier-type";
import {PokemonMultiHitModifier} from "#modifiers/modifier";
import {GameManager} from "#test/framework/game-manager";
import {NumberHolder} from "#utils/common";
import Phaser from "phaser";
import {expect,it} from "vitest";
import * as fs from "node:fs";
it("Multi Lens fallback inventory and strike factors",async()=>{
 expect(process.versions.node.split('.')[0]).toBe('22');
 const nativeBattleRng=BattleScene.prototype.randBattleSeedInt;
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle('single').startingWave(5).ability(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);BattleScene.prototype.randBattleSeedInt=nativeBattleRng;
 const scene=game.scene as any,p=scene.getPlayerParty()[0],records:any[]=[];
 const type=()=>{const f=modifierTypes.MULTI_LENS;return f().withIdFromFunc(f);};
 for(let context=0;context<8;context++)for(let stack=0;stack<3;stack++)for(let oldArg=-1;oldArg<=0;oldArg++)for(let incomingArg=-1;incomingArg<=0;incomingArg++)for(let storage=0;storage<2;storage++){
  scene.modifiers=[];p.fusionSpecies=context===7?speciesDataRegistry.getSpecies(SpeciesId.RATTATA):null;
  if(context<6){const t=new BaseStatBoosterModifierType(context);t.id='BASE_STAT_BOOSTER';scene.modifiers.push(t.newModifier(p));}
  else if(context===6){const f=modifierTypes.SOUL_DEW;scene.modifiers.push(f().withIdFromFunc(f).newModifier(p));}
  if(stack){const m=type().newModifier(p);m.stackCount=stack;scene.modifiers.push(m);}
  const before=Phaser.Math.RND.state(),ok=type().newModifier(p).add(scene.modifiers,false);expect(Phaser.Math.RND.state()).toBe(before);
  const lenses=scene.modifiers.filter((m:any)=>m instanceof PokemonMultiHitModifier),count=new NumberHolder(1);
  scene.applyModifiers(PokemonMultiHitModifier,true,p,MoveId.TACKLE,count);
  const factors=[];for(let ordinal=0;ordinal<count.value;ordinal++){
   p.turnData.hitCount=count.value;p.turnData.hitsLeft=count.value-ordinal;const d=new NumberHolder(1);
   scene.applyModifiers(PokemonMultiHitModifier,true,p,MoveId.TACKLE,null,d);factors.push(d.value);
  }
  records.push({context,stack,oldArg,incomingArg,storage,ok,lenses:lenses.map((m:any)=>m.stackCount),hits:count.value,factors});
 }
 expect(records.length).toBe(192);
 fs.writeFileSync(process.env.MULTI_FALLBACK_OUT??'/tmp/multi-fallback.json',JSON.stringify(records)+'\n');
},120000);
