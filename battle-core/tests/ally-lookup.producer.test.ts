/* Actual Pokemon.getAlly: party ordering, doubles and reserve semantics. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { SpeciesId } from "#enums/species-id";
import { AbilityId } from "#enums/ability-id";
import { TrainerSlot } from "#enums/trainer-slot";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";
it("actual ally getter",async()=>{
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR);
 const s:any=g.scene,pools=[Array.from({length:6},()=>s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),5)),Array.from({length:6},()=>s.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),5,TrainerSlot.NONE))];
 const N=8192,rows=[`ALLY 1 ${N}`];
 for(let i=0;i<N;i++){
  const side=i%2,other=1-side,dbl=!!(i%4>=2),n=Math.floor(i/4)%7,k=Math.floor(i/28)%6;s.currentBattle.double=dbl;
  s.getPlayerParty().length=0;s.currentBattle.enemyParty=[];
  const parties=[s.getPlayerParty(),s.currentBattle.enemyParty];for(const j of [side,other])parties[j].push(...pools[j].slice(0,j===side?n:6));
  pools.forEach(ps=>ps.forEach((p:any,j:number)=>{p.hp=i%3===0?0:p.getMaxHp();p.switchOutStatus=!!(i%5===0);p.id=100+(p.isPlayer()?0:10)+j;}));
  const p=pools[side][k],want=p.getAlly();rows.push(`C ${i} ${side} ${+dbl} ${n} ${k} ${want?Number(want.id):-1}`);
 }
 rows.push(`ALLY_COMPLETE ${N}`);fs.writeFileSync(process.env.ALLY_OUT??"/tmp/ally.txt",rows.join("\n")+"\n");
},600000);
