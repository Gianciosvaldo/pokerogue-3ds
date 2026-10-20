/* Native money pickup and MoneyRewardPhase; all legal Endless waves, plus overflow-edge balances. */
import { BattleScene } from "#app/battle-scene";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { ArenaTagType } from "#enums/arena-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { MoneyRewardPhase } from "#phases/money-reward-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("native wide money owners",async()=>{
 const N=23400,seed=Number(process.env.WIDE_MONEY_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 expect(process.versions.node.split('.')[0]).toBe('22');
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle('single').ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR);BattleScene.prototype.randBattleSeedInt=native;
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager;
 scene.updateMoneyText=()=>{};scene.animateMoneyChanged=()=>{};scene.validateAchvs=()=>{};
 const hex=(s:string)=>Buffer.from(s).toString('hex'),rng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(',', ' ');
 const fd=fs.openSync(process.env.WIDE_MONEY_OUT??'/tmp/wide-money.txt','w'),write=(s:string)=>fs.writeSync(fd,s+'\n');
 write(`WIDE_MONEY 1 ${N} ${seed}`);let wide=0,capped=0,zero=0,infinite=0;
 for(let i=0;i<N;i++){
  const wave=i%5850+1,kind=Math.floor(i/5850),reward=kind>=2,hh=!!(i&1),stacks=kind===0?[]:kind===1?[i%6]:[i%6,(i+3)%6],multiplier=[.2,1,2.5,5][i%4];
  battle.waveIndex=wave;scene.modifiers.length=0;scene.arena.tags.length=0;
  for(const stack of stacks){const f=(modifierTypes as any).AMULET_COIN,m=f().withIdFromFunc(f).newModifier();m.stackCount=stack;scene.modifiers.push(m);}
  if(hh)expect(scene.arena.addTag(ArenaTagType.HAPPY_HOUR,5,MoveId.HAPPY_HOUR,0,ArenaTagSide.BOTH,true)).toBe(true);
  const input=kind===0?scene.getWaveMoneyAmount(.2):[0,1,2147483647,9007199254740991,1e18+128,Number.MAX_VALUE][i%6];
  const before=[0,1234,Number.MAX_SAFE_INTEGER-1,Number.MAX_SAFE_INTEGER][i%4];scene.money=before;battle.moneyScattered=input;
  pm.clearAllPhases();battle.battleSeed=`wide-money-${seed}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([battle.battleSeed]);battle.randSeedInt(2);
  write(`C ${i} ${wave} ${+reward} ${+hh} ${stacks.length} ${stacks[0]??0} ${stacks[1]??0} ${multiplier} ${input} ${before}`);
  write(`R ${rng(Phaser.Math.RND.state())}`);write(`B ${rng(battle.battleSeedState)}`);
  let message='',prompt=false,credit:number|undefined;
  const original=scene.addMoney;scene.addMoney=function(amount:number){credit=amount;return original.call(this,amount);};
  if(reward){scene.ui.showText=(text:string,_a:any,_end:any,_b:any,confirm:boolean)=>{message=text;prompt=confirm;};new MoneyRewardPhase(multiplier).start();}
  else {battle.pickUpScatteredMoney();const phases=pm.phaseQueue.levels.flat();expect(phases).toHaveLength(1);const q=phases[0];expect(q.phaseName).toBe('MessagePhase');message=q.text;prompt=q.prompt;}
  scene.addMoney=original;expect(credit).not.toBeUndefined();
  write(`O ${scene.money} ${battle.moneyScattered} ${credit} ${+prompt} ${hex(message)}`);
  write(`RO ${rng(Phaser.Math.RND.state())}`);write(`BO ${rng(battle.battleSeedState)}`);write('E');
  wide+=+(credit!>2147483647);capped+=+(scene.money===Number.MAX_SAFE_INTEGER);zero+=+(credit===0);infinite+=+(credit===Infinity);
 }
 expect(wide).toBeGreaterThan(1000);expect(capped).toBeGreaterThan(1000);expect(zero).toBeGreaterThan(100);expect(infinite).toBeGreaterThan(100);
 write(`WIDE_MONEY_COMPLETE ${N}`);fs.closeSync(fd);console.log(`WIDE_MONEY wide${wide} capped${capped} zero${zero} infinite${infinite}`);
},240000);
