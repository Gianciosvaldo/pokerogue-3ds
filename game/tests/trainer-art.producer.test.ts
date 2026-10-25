import { BattleScene } from "#app/battle-scene";
import { trainerConfigs } from "#data/trainers/trainer-config";
import { SpeciesId } from "#enums/species-id";
import { Trainer } from "#field/trainer";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import { expect,it,vi } from "vitest";
import * as fs from "node:fs";
it("native baseline trainer sprite keys",async()=>{
 vi.setSystemTime(new Date("2024-01-01T12:00:00Z"));
 const nativeRng=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 await g.classicMode.startBattle(SpeciesId.BULBASAUR);
 BattleScene.prototype.randBattleSeedInt=nativeRng;
 const rows=Object.entries(trainerConfigs).map(([id,c])=>({id:+id,keys:[0,1,2].map(variant=>{
  const trainer=new Trainer(+id,variant,0,"trainerNames:rival","trainerNames:rivalFemale");
  const keys=[trainer.getKey(),trainer.getSprites().length>1?trainer.getKey(true):""];
  trainer.destroy();return keys;
 })})).sort((a,b)=>a.id-b.id);
 expect(rows.length).toBeGreaterThan(100);

 fs.writeFileSync(process.env.TRAINER_ART_OUT!,JSON.stringify({version:2,clock:"2024-01-01T12:00:00Z",rows})+'\n');
},240000);
