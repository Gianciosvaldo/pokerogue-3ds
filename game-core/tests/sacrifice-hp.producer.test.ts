import { BattleScene } from "#app/battle-scene";
import { globalScene } from "#app/global-scene";
import { allMoves } from "#data/data-lists";
import { SacrificialAttr } from "#moves/move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import { expect,it } from "vitest";
import fs from "node:fs";
it("native SacrificialAttr HP on normal and segmented boss users",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));await g.classicMode.startBattle(1);BattleScene.prototype.randBattleSeedInt=native;
 const p=globalScene.getEnemyField()[0],attr=new SacrificialAttr(),out=["SACRIFICE_HP 1 24"];
 for(const move of [120,153,802])for(const boss of [false,true])for(const hp of [0,1,50,100]){
  globalScene.phaseManager.clearPhaseQueue();p.setBoss(boss,4);p.stats[0]=100;p.hp=hp;p.status=null;p.turnData.damageTaken=0;
  expect(p.isBoss()).toBe(boss);
  const applied=attr.apply(p,null!,allMoves[move],[]);
  out.push(`H ${move} ${+boss} ${hp} ${+applied} ${p.hp} ${p.turnData.damageTaken}`);
 }
 out.push("COMPLETE 24");fs.writeFileSync(process.env.SACRIFICE_HP_OUT!,out.join("\n")+"\n");
},120000);
