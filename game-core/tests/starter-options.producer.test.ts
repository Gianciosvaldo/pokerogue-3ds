import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { globalScene } from "#app/global-scene";
import { getPassiveCandyCount } from "#balance/starters";
import { getStarterMoves } from "#ui/utils/starter-select-ui-utils";
import { getLuckString } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect,it } from "vitest";
it("native starter move pools, passive prices and luck grades",async()=>{
 const nativeRoll=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR);
 BattleScene.prototype.randBattleSeedInt=nativeRoll;
 const ids=speciesDataRegistry.getAllStarters().sort((a,b)=>a-b);
 expect(ids).toHaveLength(572);
 const lines=["STARTER_OPTIONS 1 572"],rows:any[]=[];
 for(const id of ids) {
  const sp=speciesDataRegistry.getSpecies(id),cost=speciesDataRegistry.getStarterCost(id);
  const forms=Math.max(1,sp.forms?.length??0);
  lines.push(`S ${id} ${cost} ${getPassiveCandyCount(cost)} ${forms}`);
  for(let form=0;form<forms;form++)for(let mask=0;mask<16;mask++) {
   globalScene.gameData.starterData[id].eggMoves=mask;
   const pool=[...new Set(getStarterMoves(id,form))];
   lines.push(`P ${id} ${form} ${mask} ${pool.length} ${pool.join(" ")}`.trim());
   rows.push({species:id,form,mask,pool});
  }
 }
 for(let luck=0;luck<=14;luck++)lines.push(`L ${luck} ${getLuckString(luck)}`);
 lines.push(`COMPLETE ${ids.length} ${rows.length}`);
 fs.writeFileSync(process.env.STARTER_OPTIONS_OUT??"/tmp/starter-options.txt",lines.join("\n")+"\n");
},120000);
