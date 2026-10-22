import { BattleScene } from "#app/battle-scene";
import { globalScene } from "#app/global-scene";
import { allMoves } from "#data/data-lists";
import { MoveResult } from "#enums/move-result";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import { expect,it } from "vitest";
import fs from "node:fs";
it("native consecutive-power closures, history wave retention and summon reset",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));await g.classicMode.startBattle(1);BattleScene.prototype.randBattleSeedInt=native;
 const p=globalScene.getPlayerField()[0],target=globalScene.getEnemyField()[0],out=["CHAIN_POWER 1 256"];
 for(const move of [205,210,301,497])for(let n=0;n<8;n++)for(let mode=0;mode<8;mode++){
  p.resetSummonData();const history:any[]=[];
  for(let k=0;k<n;k++)history.push({move:mode===3&&k===n-1?33:mode===4&&k===0?111:move,result:(mode===1&&k===n-1)||(mode===2&&k===0)?MoveResult.FAIL:MoveResult.SUCCESS});
  history.push({move,result:MoveResult.SUCCESS});p.summonData.moveHistory=history;
  const original=Phaser.Math.RND.state();
  // This is the real native wave-data reset, which must retain summon history.
  if(mode===5)p.resetWaveData();
  if(mode===6)p.resetBattleAndWaveData();
  if(mode===7){p.resetSummonData();p.summonData.moveHistory.push({move,result:MoveResult.SUCCESS} as any);}
  const power={value:allMoves[move].power};
  const attrs=allMoves[move].getAttrs("ConsecutiveUsePowerMultiplierAttr");expect(attrs.length).toBe(1);
  attrs[0].apply(p,target,allMoves[move],[power]);expect(Phaser.Math.RND.state()).toBe(original);
  out.push(`C ${move} ${n} ${mode} ${power.value}`);
 }
 out.push("COMPLETE 256");fs.writeFileSync(process.env.CHAIN_POWER_OUT!,out.join("\n")+"\n");
},120000);
