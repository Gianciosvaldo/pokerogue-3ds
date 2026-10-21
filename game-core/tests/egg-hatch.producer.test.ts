import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { Egg } from "#data/egg";
import { speciesEggMoves } from "#balance/moves/egg-moves";
import { EggSourceType } from "#enums/egg-source-types";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import { expect,it } from "vitest";
import * as fs from "node:fs";
it("actual EggLapsePhase hatch generation",async()=>{
 const nativeRng=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 await g.classicMode.startBattle(SpeciesId.BULBASAUR);
 BattleScene.prototype.randBattleSeedInt=nativeRng;
 g.override.playerIVs(null).nature(null);
 const s:any=g.scene,pm:any=s.phaseManager;
 const phase:any=pm.create("EggLapsePhase");pm.currentPhase=phase;
 const ids=Object.keys(speciesEggMoves).map(Number).filter(id=>speciesDataRegistry.getSpecies(id));
 const rows:any[]=[];let i=0;
 for(const species of ids)for(let mode=0;mode<4;mode++){
  const input={id:((i+1)*2654435761)>>>0,species,sourceType:mode===3?EggSourceType.SAME_SPECIES_EGG:mode,
   isShiny:mode===2,variantTier:mode===2?2:0,eggMoveIndex:i%4,overrideHiddenAbility:mode===1,hatchWaves:0};
  const egg=new Egg(input);
  const before=Phaser.Math.RND.state(),battle=s.currentBattle.battleSeedState;
  const data=phase.generatePokemon(egg);const p:any=data.pokemon;
  expect(Phaser.Math.RND.state()).toBe(before);expect(s.currentBattle.battleSeedState).toBe(battle);
  rows.push({i:i++,egg:{...input,species:egg.species,isShiny:egg.isShiny,variantTier:egg.variantTier},
   pokemon:{id:p.id,species:p.species.speciesId,form:p.formIndex,gender:p.gender,ability:p.abilityIndex,nature:p.nature,
    ivs:p.ivs,shiny:p.shiny,variant:p.variant,tera:p.teraType}});
  p.destroy();
 }
 expect(new Set(rows.map(r=>r.pokemon.nature)).size).toBeGreaterThan(10);
 expect(rows.some(r=>r.pokemon.ivs.some((v:number)=>v<31))).toBe(true);
 fs.writeFileSync(process.env.EGG_HATCH_OUT!,JSON.stringify({version:2,rows})+'\n');
},600000);
