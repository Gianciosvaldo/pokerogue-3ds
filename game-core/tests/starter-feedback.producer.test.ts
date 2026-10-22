import { BattleScene } from "#app/battle-scene";
import { globalScene } from "#app/global-scene";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { getValueReductionCandyCounts } from "#balance/starters";
import { getPokerusStarters } from "#utils/pokemon-utils";
import { randSeedItem } from "#utils/common";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import { expect,it,vi } from "vitest";
import fs from "node:fs";
it("native reduction prices/values and isolated Pokerus selection; requested 3 per 3h variant",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));await g.classicMode.startBattle(1);BattleScene.prototype.randBattleSeedInt=native;
 const ids=speciesDataRegistry.getAllStarters();expect(ids.length).toBe(572);
 const out=["STARTER_FEEDBACK 1 572 64"];
 for(const id of ids){const base=speciesDataRegistry.getStarterCost(id),cost=getValueReductionCandyCounts(base);
  out.push(`C ${id} ${base} ${cost[0]} ${cost[1]} ${globalScene.gameData.getSpeciesStarterValue(id,1)} ${globalScene.gameData.getSpeciesStarterValue(id,2)}`);
 }
 const before=Phaser.Math.RND.state();
 for(let k=0;k<64;k++){
  const ms=1792627200000+k*10800000,chosen:number[]=[];
  globalScene.executeWithSeedOffset(()=>{while(chosen.length<3){const id=randSeedItem(ids);if(!chosen.includes(id))chosen.push(id);}},0,String(ms));
  expect(Phaser.Math.RND.state()).toBe(before);
  if(k%8===0){vi.useFakeTimers({toFake:["Date"]});vi.setSystemTime(ms);expect(getPokerusStarters().slice(0,3)).toEqual(chosen);vi.useRealTimers();}
  out.push(`P ${ms} ${chosen.join(" ")}`);
 }
 out.push("COMPLETE 572 64");fs.writeFileSync(process.env.STARTER_FEEDBACK_OUT!,out.join("\n")+"\n");
},120000);
