import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { Egg } from "#data/egg";
import { getSameSpeciesEggCandyCounts } from "#balance/starters";
import { EggSourceType } from "#enums/egg-source-types";
import { variantData } from "#sprites/variant";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import { expect,it,vi } from "vitest";
import * as fs from "node:fs";
it("native same-species prices and Egg constructor",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));await g.classicMode.startBattle(1);
 BattleScene.prototype.randBattleSeedInt=native;
 Object.assign(variantData,JSON.parse(fs.readFileSync("assets/images/pokemon/variant/_masterlist.json","utf8")));
 const state=(r:any)=>r.state().replace(/^!rnd,/,"").split(",").map(Number).join(" ");
 const ids=speciesDataRegistry.getAllStarters().sort((a,b)=>a-b);expect(ids).toHaveLength(572);
 const out=["SPECIES_EGGS 1 572"],meta:string[]=[];let rows=0,shiny=0,variants=0,rare=0;
 const sceneBefore=state(Phaser.Math.RND);
 for(const id of ids){
  const cost=speciesDataRegistry.getStarterCost(id),tier=speciesDataRegistry.getEggTier(id),hasVariants=+speciesDataRegistry.getSpecies(id).hasVariants();
  out.push(`S ${id} ${tier} ${hasVariants}`);meta.push(` {${id},${tier},${hasVariants}},`);
  for(const count of [0,7,8,9,10,14,15,16,19,20,29,30,31,32,39,40,59,60,79,80,100])out.push(`P ${id} ${count} ${getSameSpeciesEggCandyCounts(cost,count)}`);
  for(let k=0;k<4;k++){
   const entropy=new Phaser.Math.RandomDataGenerator([`species-egg-${id}-${k}`]);let draws=0;
   vi.spyOn(Math,"random").mockImplementation(()=>{draws++;return entropy.frac();});
   const before=state(entropy),timestamp=1792627200000;
   const e=new Egg({species:id,sourceType:EggSourceType.SAME_SPECIES_EGG,timestamp});
   expect(state(Phaser.Math.RND)).toBe(sceneBefore);
   out.push(`E ${id} ${before} ${e.id} ${e.hatchWaves} ${e.tier} ${e.species} ${e.sourceType} ${e.variantTier} ${+e.isShiny} ${e.eggMoveIndex} ${+e.overrideHiddenAbility} ${e.timestamp} ${state(entropy)} ${draws}`);
   rows++;shiny+=+e.isShiny;variants+=+(e.variantTier>0);rare+=+(e.eggMoveIndex===3);
   vi.restoreAllMocks();
  }
 }
 expect(rows).toBe(2288);expect(shiny&&variants&&rare).toBeGreaterThan(0);
 out.push(`COMPLETE ${ids.length} ${rows} ${shiny} ${variants} ${rare}`);
 fs.writeFileSync(process.env.SPECIES_EGG_META_OUT??"/tmp/species-egg-info.inc","/* Pinned registry 8555c08; regenerate with species-egg.producer.test.ts. */\nstatic const struct {uint16_t species;uint8_t tier;bool variants;} species_egg_info[]={\n"+meta.join("\n")+"\n};\n");
 fs.writeFileSync(process.env.SPECIES_EGG_OUT??"/tmp/species-eggs.txt",out.join("\n")+"\n");
},120000);
