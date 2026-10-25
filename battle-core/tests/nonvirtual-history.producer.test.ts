/* Actual pinned Pokemon history owner; no replacement query or battle RNG. */
import { allMoves } from "#data/data-lists";
import { PokemonMove } from "#moves/pokemon-move";
import { BattleScene } from "#app/battle-scene";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { MoveUseMode } from "#enums/move-use-mode";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("native last nonvirtual queries across complete long summon histories",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await g.classicMode.startBattle(SpeciesId.MEW);
 BattleScene.prototype.randBattleSeedInt=native;
 g.override.moveset([]).enemyMoveset([]);
 const s:any=g.scene,u:any=s.getPlayerPokemon(),other:any=s.getEnemyPokemon(),rows:any[]=[];
 const ownerMoves=[MoveId.DISABLE,MoveId.MIMIC,MoveId.SKETCH,MoveId.SPITE,MoveId.INSTRUCT];
 u.moveset=[MoveId.TACKLE,MoveId.SPLASH,MoveId.STRUGGLE,MoveId.GROWL].map(m=>new PokemonMove(m));
 other.moveset=[MoveId.DISABLE,MoveId.MIMIC,MoveId.SKETCH,MoveId.SPITE].map(m=>new PokemonMove(m));
 const modes=[1,2,3,4,5,6],lengths=[0,1,7,8,9,64,65535,65536,65537];
 const firstMoves=[MoveId.NONE,MoveId.TACKLE,MoveId.STRUGGLE];
 const suffixes=[[MoveId.NONE,1],[MoveId.SPLASH,3],[MoveId.STRUGGLE,4],[MoveId.NONE,4],[MoveId.TACKLE,5],[MoveId.STRUGGLE,3]];
 const projection=(m:any)=>m?{move:m.move,useMode:m.useMode,result:m.result??255,targets:m.targets.slice()}:null;
 for(const length of lengths)for(const mode of modes)for(const first of firstMoves)for(let shape=0;shape<suffixes.length;shape++){
  u.resetSummonData();expect(u.isOnField()).toBe(true);
  for(let i=0;i<length;i++)u.pushMoveHistory({move:i?suffixes[shape][0]:first,useMode:i?suffixes[shape][1]:mode,
   targets:i?[2]:[[],[0],[1,2],[0,1,2,3],[3],[2,0]][mode-1],result:i?4:mode===6?undefined:mode-1,turn:i+1});
  expect(u.getMoveHistory().length).toBe(length);
  const runs:any[]=[];
  for(const entry of u.getMoveHistory()){
   const m=projection(entry),last=runs.at(-1);
   if(last&&JSON.stringify(last.entry)===JSON.stringify(m))last.count++;else runs.push({count:1,entry:m});
  }
  const battleSeed=s.currentBattle.battleSeedState,rng=Phaser.Math.RND.state();
  const query=()=>[false,true].flatMap(a=>[false,true].map(b=>projection(u.getLastNonVirtualMove(a,b))));
  const conditions=()=>ownerMoves.map(m=>allMoves[m].applyConditions(other,u,3));
  const result=[query()],owners=[conditions()];
  const added={move:MoveId.GROWL,useMode:MoveUseMode.NORMAL,targets:[0,1,2,3],result:0};
  u.pushMoveHistory(added);result.push(query());owners.push(conditions());
  for(let k=0;k<9;k++)u.pushMoveHistory({move:MoveId.SPLASH,useMode:MoveUseMode.INDIRECT,targets:[2],result:4});
  result.push(query());owners.push(conditions());
  u.resetSummonData();result.push(query());owners.push(conditions());
  const resetAdded={move:MoveId.STRUGGLE,useMode:MoveUseMode.FOLLOW_UP,targets:[3],result:undefined};
  u.pushMoveHistory(resetAdded);result.push(query());owners.push(conditions());
  for(let k=0;k<9;k++)u.pushMoveHistory({move:MoveId.NONE,useMode:MoveUseMode.NORMAL,targets:[2],result:4});
  result.push(query());owners.push(conditions());
  expect(Phaser.Math.RND.state()).toBe(rng);expect(s.currentBattle.battleSeedState).toBe(battleSeed);
  rows.push({length,mode,first,shape,runs,added:projection(added),resetAdded:projection(resetAdded),result,owners});
 }
 expect(rows.length).toBe(972);
 const aliases:any[]=[];
 for(const prefix of [0,9,65536])for(const mode of [MoveUseMode.NORMAL,MoveUseMode.FOLLOW_UP])for(const move of [MoveId.TACKLE,MoveId.STRUGGLE]){
  u.resetSummonData();
  for(let i=0;i<prefix;i++)u.pushMoveHistory({move:MoveId.NONE,useMode:MoveUseMode.INDIRECT,targets:[],result:4});
  const targets=[2],entry={move,useMode:mode,targets,result:1};u.pushMoveHistory(entry);
  const query=()=>[false,true].flatMap(a=>[false,true].map(b=>projection(u.getLastNonVirtualMove(a,b))));
  const battleSeed=s.currentBattle.battleSeedState,rng=Phaser.Math.RND.state(),result=[query()];
  targets[0]=1;result.push(query());
  for(let i=0;i<9;i++)u.pushMoveHistory({move:MoveId.NONE,useMode:MoveUseMode.INDIRECT,targets:[],result:4});
  result.push(query());targets.push(3);result.push(query());
  u.resetSummonData();const nextTargets=[0];
  u.pushMoveHistory({move:MoveId.GROWL,useMode:MoveUseMode.NORMAL,targets:nextTargets,result:1});
  for(let i=0;i<9;i++)u.pushMoveHistory({move:MoveId.NONE,useMode:MoveUseMode.INDIRECT,targets:[],result:4});
  result.push(query());targets[0]=2;result.push(query());nextTargets[0]=3;result.push(query());
  expect(Phaser.Math.RND.state()).toBe(rng);expect(s.currentBattle.battleSeedState).toBe(battleSeed);
  aliases.push({prefix,mode,move,result});
 }
 expect(aliases.length).toBe(12);
 fs.writeFileSync(process.env.HISTORY_OUT!,JSON.stringify({version:1,rows,aliases})+"\n");
},180000);
