/* Real waiting override, repeated calls and native queued owner selection. */
import { BattleScene } from "#app/battle-scene";
import { allMoves } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { MoveUseMode } from "#enums/move-use-mode";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import { BooleanHolder } from "#utils/common";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual Pledge waiting callback and dynamic queue",async()=>{
 const n=12544,seed=Number(process.env.PLEDGE_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=game.scene,pm=s.phaseManager,actors:any[]=[...s.getPlayerField(),...s.getEnemyField()];if(actors.length!==4)throw Error("natural double field required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 const pledges=[MoveId.FIRE_PLEDGE,MoveId.WATER_PLEDGE,MoveId.GRASS_PLEDGE],choices=[0,...pledges,MoveId.TACKLE];
 const fd=fs.openSync(process.env.PLEDGE_OUT??"/tmp/pledge.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`PLEDGE 2 ${n} ${seed}`]);let waited=0,declined=0,already=0,faintedWait=0,separateWait=0;const modes=[0,0,0];
 const rng=(k:string,v:string)=>`${k} ${v.replace(/^!rnd,/,"").replaceAll(","," ")}`;
 for(let i=0;i<n;i++){
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;s.getPlayerParty().splice(0,2,actors[0],actors[1]);s.getEnemyParty().splice(0,2,actors[2],actors[3]);
  const u=i%4,mode=Math.floor(i/4)%3,move=allMoves[pledges[mode]],count=i>=12288?4:Math.floor(i/12)%9,ops=1+Math.floor(i/108)%3,swap=Math.floor(i/324)%4;
  const rows=[`C ${i} ${u} ${move.id} ${count} ${ops} ${swap}`];modes[mode]++;
  for(let k=0;k<4;k++){const p=actors[k];p.resetSummonData();p.resetTurnData();p.hp=(Math.floor(i/1296)+k)%2?0:p.getMaxHp();p.turnData.acted=(Math.floor(i/2592)+k)%2===1;p.turnData.combiningPledge=i>=12288?0:choices[(Math.floor(i/5184)+k)%choices.length];rows.push(`MON ${p.species.speciesId} ${p.formIndex} ${p.hp} ${+p.turnData.acted} ${p.turnData.combiningPledge}`);}
  Phaser.Math.RND.sow([`pledge-${seed}-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  for(let j=0;j<count;j++){const who=i>=12288?[u^2,u^1,u^3,u][j]:(i+j*3)%4,timing=(i+j)%3,m=i>=12288?(i>=12416&&j===1?MoveId.TACKLE:pledges[(mode+1)%3]):[...pledges,MoveId.TACKLE][(Math.floor(i/36)+j)%4];pm.pushNew("MovePhase",actors[who],[u],new PokemonMove(m),MoveUseMode.NORMAL,timing);rows.push(`MOVE ${who} ${timing} ${m}`);}
  if(swap&1)s.getPlayerParty().splice(0,2,actors[1],actors[0]);if(swap&2)s.getEnemyParty().splice(0,2,actors[3],actors[2]);rows.push(rng("R",Phaser.Math.RND.state()),rng("B",s.currentBattle.battleSeedState));
  for(let op=0;op<ops;op++){
   const holder=new BooleanHolder((i+op)%2===1),input=holder.value,had=!!actors[u].turnData.combiningPledge;
   const result=move.getAttrs("AwaitCombinedPledgeAttr")[0].apply(actors[u],actors[(u+op)%4],move,[holder]);if(i>=12288&&i<12416&&result)separateWait++;waited+=+result;declined+=+!result;already+=+had;faintedWait+=+(result&&actors[u].hp===0);
   rows.push(`OP ${+input} ${+result} ${+holder.value}`,`LEVELS ${pm.phaseQueue.levels.length}`);
   for(const [l,q] of pm.phaseQueue.levels.entries()){rows.push(`L ${l} ${q.length}`);for(const p of q){if(p.phaseName==="DynamicPhaseMarker"&&p.phaseType==="MovePhase")rows.push("MARKER MovePhase");else if(p.phaseName==="MessagePhase")rows.push(`MSG ${Buffer.from(p.text).toString("hex")}`);else throw Error(`unexpected phase ${p.phaseName}`);}}
   const dynamic:any[]=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue);rows.push(`DYNAMIC ${dynamic.length}`);for(const p of dynamic){if(p.phaseName!=="MovePhase")throw Error("unexpected dynamic phase");rows.push(`D ${actors.indexOf(p.pokemon)} ${p.timingModifier} ${p.move.moveId}`);}
   for(const p of actors)rows.push(`AFTER ${p.hp} ${+p.turnData.acted} ${p.turnData.combiningPledge??0}`);
  }
  rows.push(rng("RO",Phaser.Math.RND.state()),rng("BO",s.currentBattle.battleSeedState));write(rows);
 }
 if(separateWait<100||waited<100||declined<100||already<100||faintedWait<100||modes.some(n=>n<100))throw Error(`coverage missing ${waited}/${declined}/${already}/${faintedWait}/${modes}`);
 write([`PLEDGE_COMPLETE ${n}`]);fs.closeSync(fd);console.log(`PLEDGE waited=${waited} declined=${declined} already=${already} faintedWait=${faintedWait} separateWait=${separateWait} modes=${modes}`);
},600000);
