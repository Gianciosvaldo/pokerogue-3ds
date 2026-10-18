/* Actual Arena.removeAllTags: standard tag callbacks, list order and text. */
import { ArenaTagType } from "#enums/arena-tag-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { getArenaTag } from "#data/arena-tag";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";
it("actual bulk arena removal",async()=>{
 const N=Number(process.env.ARENAREMOVE_N??2000),seed=Number(process.env.ARENAREMOVE_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<64)throw Error("Node22,N>=64 required");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.RATTATA);
 const scene:any=game.scene,pm:any=scene.phaseManager,actors=[game.field.getPlayerPokemon(),game.field.getEnemyPokemon()];
 const names=Object.values(ArenaTagType),types=names.filter(t=>![ArenaTagType.NONE,ArenaTagType.NEUTRALIZING_GAS,ArenaTagType.IMPRISON,ArenaTagType.PENDING_HEAL].includes(t));
 let state=seed;const rnd=(n:number)=>{state=(Math.imul(state,1103515245)+12345)>>>0;return(state>>>8)%n;};
 const rows=[`ARENA_REMOVE 1 ${N}`];
 for(let i=0;i<N;i++){
  const n=1+rnd(24);scene.arena.tags=[];
  rows.push(`C ${i} ${actors[0].id} ${actors[1].id} ${n}`);
  for(let j=0;j<n;j++){
   const type=types[(i+j)%types.length],side=[ArenaTagSide.PLAYER,ArenaTagSide.ENEMY,ArenaTagSide.BOTH][rnd(3)],sourceId=actors[rnd(2)].id;
   const tag:any=getArenaTag(type,1+rnd(5),MoveId.SPLASH,sourceId,side);if(!tag)throw Error("tag missing");scene.arena.tags.push(tag);
   rows.push(`A ${names.indexOf(tag.tagType)} ${tag.side} ${tag.turnCount} ${tag.maxDuration} ${tag.sourceMove??0} ${tag.sourceId??-1}`);
  }
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;
  const before=Phaser.Math.RND.state(),battleBefore=scene.currentBattle.battleSeedState;
  scene.arena.removeAllTags();
  if(scene.arena.tags.length||before!==Phaser.Math.RND.state()||battleBefore!==scene.currentBattle.battleSeedState)throw Error("bulk removal state/RNG");
  const phases=pm.phaseQueue.levels.flat();if(phases.some((ph:any)=>ph.phaseName!=="MessagePhase"))throw Error("unexpected non-message callback");
  rows.push(`Q ${phases.length}`,...phases.map((ph:any)=>`T ${Buffer.from(ph.text,"utf8").toString("hex")}`),"E");
 }
 rows.push(`ARENA_REMOVE_COMPLETE ${N}`);fs.writeFileSync(process.env.ARENAREMOVE_OUT??"/tmp/arena-remove.txt",rows.join("\n")+"\n");
},600000);
