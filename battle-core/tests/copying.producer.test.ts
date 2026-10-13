/* Actual pinned copying conditions/apply, Pokemon.setMove, PP use and summon reset. */
import { GameManager } from "#test/framework/game-manager";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves } from "#data/data-lists";
import { PokemonMove } from "#moves/pokemon-move";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { MoveResult } from "#enums/move-result";
import { MoveUseMode } from "#enums/move-use-mode";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";
const N=Number(process.env.COPYING_N??4000),SEED=Number(process.env.COPYING_SEED??777);
let state=SEED;const rnd=(n:number)=>{state=(Math.imul(state,1103515245)+12345)>>>0;return(state>>>8)%n;};
const rng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
function snap(p:any){
 const temp=p.summonData.moveset,eff=p.getMoveset();let aliases=0;
 if(temp)for(let i=0;i<temp.length;i++)if(temp[i]&&temp[i]===p.moveset[i])aliases|=1<<i;
 const moves=(ms:any[])=>[ms.length,...Array.from({length:4},(_,i)=>ms[i]?.moveId??0),...Array.from({length:4},(_,i)=>ms[i]?.ppUsed??0),...Array.from({length:4},(_,i)=>ms[i]?.ppUp??0),...Array.from({length:4},(_,i)=>ms[i]?.maxPpOverride??-1)];
 return[p.species.speciesId,p.formIndex,+p.isTransformed(),+!!temp,aliases,...moves(p.moveset),...moves(eff),p.hp];
}
it("unchanged copying APIs and object sharing",async()=>{
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<192)throw Error("Node22 and complete states required");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.SMEARGLE);
 game.override.moveset([]).enemyMoveset([]);
 const scene:any=game.scene,pm:any=scene.phaseManager,p:any=game.field.getPlayerPokemon(),t:any=game.field.getEnemyPokemon();
 const rows=[`COPYING 1 ${N} ${SEED}`];const successes=Array(6).fill(0);
 for(let c=0;c<N;c++){
  const st=c%32,kind=Math.floor(c/32)%6,move=kind===1||kind===4?MoveId.MIMIC:MoveId.SKETCH;
  p.resetSummonData();t.resetSummonData();p.hp=[6,13,22,29].includes(st)?0:p.getMaxHp();p.moveset=[move,MoveId.TACKLE,MoveId.SPLASH,MoveId.REST].map(id=>new PokemonMove(id));
  if(kind===4)p.moveset=[MoveId.SKETCH,MoveId.MIMIC,MoveId.TACKLE,MoveId.REST].map(id=>new PokemonMove(id));
  p.moveset.forEach((m:any)=>{m.ppUp=rnd(4);m.ppUsed=rnd(m.getMovePp()+1);});
  if(st>=8&&st<16){p.summonData.moveset=p.moveset.slice();p.summonData.moveset[1]=new PokemonMove(MoveId.LEER);}
  if(st>=16){p.summonData.speciesForm=speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP);p.summonData.moveset=p.moveset.map((m:any)=>new PokemonMove(m.moveId,0,0,5));}
  if(st>=28)p.setMove(2,MoveId.TAIL_WHIP);
  if(st===23){p.moveset=[new PokemonMove(MoveId.TRANSFORM)];p.summonData.moveset=[MoveId.SPLASH,MoveId.TACKLE,MoveId.REST,move].map(id=>new PokemonMove(id));}
  if(kind===5){p.moveset=p.moveset.map((m:any)=>m.moveId===MoveId.SKETCH?new PokemonMove(MoveId.GROWL):m);if(p.summonData.moveset)p.summonData.moveset=p.summonData.moveset.map((m:any)=>m.moveId===MoveId.SKETCH?new PokemonMove(MoveId.GROWL):m);}
  const last=[MoveId.NONE,MoveId.STRUGGLE,MoveId.SKETCH,MoveId.TACKLE,MoveId.SPLASH,MoveId.GROWL,MoveId.SOLAR_BEAM,MoveId.SOLAR_BEAM][st%8];
  const result=st%8===6?MoveResult.OTHER:st%8===5?MoveResult.FAIL:MoveResult.SUCCESS;
  const history:any[]=[];
  if(st%8===0&&st>=8)history.push({move:MoveId.GROWL,targets:[0],useMode:MoveUseMode.NORMAL,result:MoveResult.SUCCESS});
  history.push({move:last,targets:[0],useMode:st%8===0?MoveUseMode.INDIRECT:MoveUseMode.NORMAL,result});
  t.summonData.moveHistory=history;
  let slot=p.getMoveset().findIndex((m:any)=>m?.moveId===move);if(slot<0)slot=0;if(kind===2||kind===3)slot=st%4;
  const old=p.getMoveset()[slot];if(!old)throw Error("effective slot missing");
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;Phaser.Math.RND.sow([`copying-${SEED}-${c}`]);const amount=1+rnd(8);
  rows.push(`C ${c} ${kind} ${slot} ${amount} ${history.length}`,`I ${snap(p).join(" ")}`,...history.map(h=>`H ${h.move} ${h.useMode} ${h.result}`),`R ${rng(Phaser.Math.RND.state())}`);
  let condition=true,applied=true;
  if(kind===2)p.setMove(slot,MoveId.GROWL);
  else if(kind===3)p.moveset[slot]=new PokemonMove(MoveId.LEER);
  else{
   condition=allMoves[move].applyConditions(p,t);applied=false;
   if(condition)applied=allMoves[move].getAttrs(kind===1||kind===4?"MovesetCopyMoveAttr":"SketchAttr")[0].apply(p,t,allMoves[move],[]);
   if(kind===4){t.summonData.moveHistory.push({move:MoveId.LEER,targets:[0],useMode:MoveUseMode.NORMAL,result:MoveResult.SUCCESS});const next=allMoves[MoveId.SKETCH];if(next.applyConditions(p,t))next.getAttrs("SketchAttr")[0].apply(p,t,next,[]);}
  }
  rows.push(`A ${+condition} ${+applied}`,`O ${snap(p).join(" ")}`);
  successes[kind]+=+applied;
  if(p.getMoveset()[slot].getMovePp()!==-1)p.getMoveset()[slot].usePp(amount);rows.push(`P ${snap(p).join(" ")}`,`OLD ${old.moveId} ${old.ppUsed} ${old.ppUp} ${old.maxPpOverride??-1}`);
  const texts=pm.phaseQueue.levels.flat().filter((ph:any)=>ph.phaseName==="MessagePhase").map((ph:any)=>Buffer.from(ph.text,"utf8").toString("hex"));
  rows.push(`TEXT ${texts.length}`,...texts.map((s:string)=>`T ${s}`),`RO ${rng(Phaser.Math.RND.state())}`);
  p.resetSummonData();rows.push(`Z ${snap(p).join(" ")}`,"E");
 }
 if(successes.slice(0,5).some(n=>n===0)||successes[5]!==0)throw Error(`unreached copying branches: ${successes}`);
 console.log(`COPYING success ${successes.join("/")}`);
 rows.push(`COPYING_COMPLETE ${N}`);fs.writeFileSync(process.env.COPYING_OUT??"/tmp/copying.txt",rows.join("\n")+"\n");
},600000);
