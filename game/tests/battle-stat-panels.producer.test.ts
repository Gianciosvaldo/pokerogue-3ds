import { BattleScene } from "#app/battle-scene";
import { globalScene } from "#app/global-scene";
import { BattleInfo } from "#ui/battle-info/battle-info";
import { PlayerBattleInfo } from "#ui/battle-info/player-battle-info";
import { EnemyBattleInfo } from "#ui/battle-info/enemy-battle-info";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import { expect,it,vi } from "vitest";
import * as fs from "node:fs";
it("native BattleInfo stat panel factory geometry and stage frames",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));await g.classicMode.startBattle(1);BattleScene.prototype.randBattleSeedInt=native;
 const meta=JSON.parse(fs.readFileSync(process.env.STAT_ASSETS_META!,"utf8"));expect(meta.source).toBe("87426a79611f9d212c4dc8af58e2834a05b93725");
 const sprites:any[]=[];
 class Sprite {
  width:number;height:number;visible=true;name="";
  constructor(public x:number,public y:number,public texture:string,public frame?:string){this.setFrame(frame??"");sprites.push(this);}
  setFrame(frame:string){this.frame=frame;const size=meta.boxes[this.texture]??(this.texture==="pbinfo_stat_numbers"?meta.numbers[frame]:meta.labels[frame]);expect(size).toBeTruthy();[this.width,this.height]=size;return this;}
  setName(name:string){this.name=name;return this;}setOrigin(){return this;}setVisible(v:boolean){this.visible=v;return this;}
 }
 class Container {x=0;y=0;setPosition(x:number,y:number){this.x=x;this.y=y;return this;}setName(){return this;}setAlpha(){return this;}add(){return this;}}
 vi.spyOn(globalScene.add,"sprite").mockImplementation(((x:number,y:number,key:string,frame:string)=>new Sprite(x,y,key,frame))as any);
 vi.spyOn(globalScene.add,"container").mockImplementation((()=>new Container())as any);
 vi.spyOn(BattleInfo.prototype,"initInfo").mockImplementation(()=>{});
 const stages=[-6,-4,-2,0,2,4,6],out=["BATTLE_STATS 1 3"];
 for(const mode of [0,1,2]){
  const player=mode===0,p:any=Object.create(player?PlayerBattleInfo.prototype:EnemyBattleInfo.prototype);
  p.player=player;p.mini=false;p.boss=mode===2;p.add=()=>p;sprites.length=0;
  p.constructStatContainer({xOffset:player?8:5,paddingX:player?4:2,statOverflow:player?1:0});p.updateStats(stages);
  if(player){p.setHpNumbers=()=>{};p.expMaskRect={};p.initInfo(globalScene.getPlayerField()[0]);}
  out.push(`B ${mode} ${p.statsBox.width} ${p.statsBox.height}`);
  const labels=sprites.filter(s=>s.name.startsWith("icon_stat_label_")&&s.visible);
  expect(labels).toHaveLength(7);
  for(const label of labels){const idx=Number(label.name.split("_").at(-1)),number=p.statNumbers[idx];
   out.push(`L ${label.frame} ${number.frame} ${label.x+p.statValuesContainer.x} ${label.y+p.statValuesContainer.y} ${number.x+p.statValuesContainer.x} ${number.y+p.statValuesContainer.y}`);
  }
 }
 out.push("COMPLETE 3 21");fs.writeFileSync(process.env.STAT_PANELS_OUT!,out.join("\n")+"\n");vi.restoreAllMocks();
},120000);
