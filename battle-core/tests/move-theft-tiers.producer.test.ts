/* Native pool inference and ModifierData reconstruction, no derived answers. */
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { modifierTypes } from "#data/data-lists";
import { getModifierPoolForType, getModifierType } from "#utils/modifier-utils";
import { ModifierData } from "#system/modifier-data";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";
it("native held type tier inference and reconstruction",async()=>{
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE);
 const actors:any[]=g.scene.getField();if(actors.length!==4)throw Error("native field required");
 const keys=["LEFTOVERS","SHELL_BELL","FOCUS_BAND","QUICK_CLAW","KINGS_ROCK","WHITE_HERB","MINI_BLACK_HOLE","BERRY","WIDE_LENS","MULTI_LENS","SCOPE_LENS","TOXIC_ORB","FLAME_ORB","SOUL_DEW","GOLDEN_PUNCH","ATTACK_TYPE_BOOSTER","REVIVER_SEED"];
 const make=(key:string)=>{const f:any=(modifierTypes as any)[key];return key==="BERRY"?generateModifierType(f,[0]):key==="ATTACK_TYPE_BOOSTER"?generateModifierType(f,[0]):getModifierType(f);};
 const rows=[`THEFT_TIERS 1 ${keys.length}`];let resets=0;
 for(const key of keys){
  const tiers:number[]=[],pools:number[]=[];
  for(let pool=0;pool<3;pool++){
   const type:any=make(key);tiers.push(type.getOrInferTier(pool));
   const data:any=getModifierPoolForType(pool);let mask=0;for(const tier of Object.keys(data))if(data[tier].some((w:any)=>w.modifierType.id===key))mask|=1<<Number(tier);pools.push(mask);
  }
  rows.push(`T ${key} ${tiers.join(" ")} ${pools.join(" ")}`);
  for(const player of [true,false])for(let tier=0;tier<6;tier++){
   const type:any=make(key);type.setTier(tier);const m:any=type.newModifier(actors[player?0:2]);
   const restored:any=new ModifierData(m,player).toModifier(m.constructor);if(!restored)throw Error("native reconstruction failed");
   rows.push(`S ${key} ${+player} ${tier} ${restored.type.tier??-1}`);resets++;
  }
 }
 rows.push(`THEFT_TIERS_COMPLETE ${keys.length} ${resets}`);fs.writeFileSync(process.env.THEFT_TIERS_OUT??"/tmp/theft-tiers.txt",rows.join("\n")+"\n");
},600000);
