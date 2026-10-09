/* Pinned registry + actual standard variant asset, no hand-written eligibility. */
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { variantData } from "#sprites/variant";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { createHash } from "node:crypto";
import { expect,it } from "vitest";
it("dump actual egg pool",()=>{
 new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 const asset=fs.readFileSync("assets/images/pokemon/variant/_masterlist.json");
 expect(createHash("sha256").update(asset).digest("hex")).toBe("c776d4ad0eb7fe63f00b76bc1cfc0dde4bf60404195a2a1634ef3b2e3dcf9b50");
 Object.assign(variantData,JSON.parse(asset.toString()));
 const rows:string[]=[];
 for(const tier of [0,1,2,3])for(const s of speciesDataRegistry.getSpeciesForEggTier(tier))rows.push(`{${s.speciesId},${speciesDataRegistry.getStarterCost(s.speciesId)},${tier},${+s.hasVariants()}}`);
 fs.writeFileSync(process.env.DUMP_OUT!,`/* Generated from source8555c08 registry; standard variants SHA256 c776d4ad0eb7fe63f00b76bc1cfc0dde4bf60404195a2a1634ef3b2e3dcf9b50. */\n#include "gc/gacha.h"\nconst int gc_egg_pool_count=${rows.length};\nconst gc_egg_pool_entry gc_egg_pool[${rows.length}]={\n${rows.join(",\n")}\n};\n`);
});
