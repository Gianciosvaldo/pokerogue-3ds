/* Actual Last Resort condition owner over long histories and changed movesets. */
import { BattleScene } from "#app/battle-scene";
import { allMoves } from "#data/data-lists";
import { PokemonMove } from "#moves/pokemon-move";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { MoveUseMode } from "#enums/move-use-mode";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("native Last Resort complete summon-history membership",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await g.classicMode.startBattle(SpeciesId.MEW);
 BattleScene.prototype.randBattleSeedInt=native;
 g.override.moveset([]).enemyMoveset([]);
 const s:any=g.scene,u:any=s.getPlayerPokemon(),t:any=s.getEnemyPokemon(),rows=[];
 const modes=[MoveUseMode.NORMAL,MoveUseMode.IGNORE_PP,MoveUseMode.INDIRECT,MoveUseMode.FOLLOW_UP,MoveUseMode.REFLECTED,MoveUseMode.DELAYED_ATTACK];
 const lengths=[0,1,7,8,9,10,64,256,1024];
 const shapes=[
  [MoveId.TACKLE,MoveId.LAST_RESORT],[MoveId.LAST_RESORT],[MoveId.TACKLE],
  [MoveId.TACKLE,MoveId.TACKLE,MoveId.LAST_RESORT],[MoveId.TACKLE,MoveId.GROWL,MoveId.LAST_RESORT],
  [MoveId.TACKLE,MoveId.SPLASH,MoveId.LAST_RESORT],[MoveId.MALIGNANT_CHAIN,MoveId.LAST_RESORT],
  [MoveId.TACKLE,MoveId.LAST_RESORT],
 ];
 for(const length of lengths)for(const mode of modes)for(let shape=0;shape<shapes.length;shape++){
  u.resetSummonData();u.moveset=shapes[shape].map(m=>new PokemonMove(m));
  if(shape===7){u.moveset=[new PokemonMove(MoveId.SPLASH),new PokemonMove(MoveId.LAST_RESORT)];u.summonData.moveset=shapes[shape].map(m=>new PokemonMove(m));}
  expect(u.getMoveset().map((m:any)=>m.moveId)).toEqual(shapes[shape]);
  const first=shape===6?MoveId.MALIGNANT_CHAIN:MoveId.TACKLE;
  for(let i=0;i<length;i++)u.pushMoveHistory({move:i===0?first:MoveId.SPLASH,targets:[2],result:1,turn:i+1,useMode:i===0?mode:MoveUseMode.NORMAL});
  expect(u.getMoveHistory().length).toBe(length);
  const history=u.getMoveHistory().map((m:any)=>({...m,targets:m.targets.slice()}));
  const query=()=>allMoves[MoveId.LAST_RESORT].applyConditions(u,t,3);
  const beforeBattle=s.currentBattle.battleSeedState,beforeRng=Phaser.Math.RND.state(),result=[query()];
  u.summonData.moveset=undefined;u.moveset=[new PokemonMove(MoveId.LAST_RESORT),new PokemonMove(MoveId.SPLASH)];
  result.push(query());
  u.moveset=[new PokemonMove(MoveId.LAST_RESORT),new PokemonMove(first)];result.push(query());
  u.moveset=[new PokemonMove(MoveId.LAST_RESORT),new PokemonMove(MoveId.SPLASH)];u.resetSummonData();result.push(query());
  u.pushMoveHistory({move:MoveId.SPLASH,targets:[2],useMode:MoveUseMode.NORMAL});result.push(query());
  expect(Phaser.Math.RND.state()).toBe(beforeRng);expect(s.currentBattle.battleSeedState).toBe(beforeBattle);
  rows.push({length,mode,shape,moves:shapes[shape],active:shape===7,futureMove:first,history,result});
 }
 expect(rows.length).toBe(432);expect(rows.flatMap(r=>r.result).length).toBe(2160);
 fs.writeFileSync(process.env.HISTORY_OUT!,JSON.stringify({version:2,rows})+"\n");
},120000);
