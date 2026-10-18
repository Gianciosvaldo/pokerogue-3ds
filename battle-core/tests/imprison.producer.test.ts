/* Actual pinned arena/battler Imprison methods and PRE_MOVE cancellation. */
import { ArenaTagType } from "#enums/arena-tag-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { BattlerTagType } from "#enums/battler-tag-type";
import { BattlerTagLapseType } from "#enums/battler-tag-lapse-type";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { MoveUseMode } from "#enums/move-use-mode";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";
it("actual Imprison lifecycle",async()=>{
 const N=Number(process.env.IMPRISON_N??8192);if(!Number.isSafeInteger(N)||N<512||!process.versions.node.startsWith("22."))throw Error("Node22,N>=512");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.RATTATA);
 game.override.moveset([]).enemyMoveset([]);
 const s:any=game.scene,pm:any=s.phaseManager,ps:any[]=[game.field.getPlayerPokemon(),game.field.getEnemyPokemon()];const rows=[`IMPRISON 1 ${N}`,`W ${s.waveSeed}`];
 for(let i=0;i<N;i++){
  const state=i%64,kind=Math.floor(i/64)%8,side=state%3,quiet=!!(state&8),source=ps[0],target=ps[1];
  for(const p of ps){p.resetSummonData();p.hp=p.getMaxHp();p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];if(!p.isOnField())s.field.add(p);}
  source.moveset=[new PokemonMove(state&1?MoveId.TACKLE:MoveId.SPLASH)];if(state&2)source.summonData.moveset=[new PokemonMove(MoveId.SPLASH)];
  if(state&4)source.hp=0;if(state&16)source.switchOutStatus=true;if(state&32)target.hp=0;
  s.arena.tags=[];pm.clearAllPhases();pm.phaseQueue.deferredActive=false;
  const id=state%7===0?undefined:source.id;
  rows.push(`C ${i} ${kind} ${side} ${+quiet} ${source.id} ${target.id} ${source.hp} ${target.hp} ${+source.switchOutStatus} ${+(!!source.summonData.moveset)} ${source.moveset[0].moveId} ${id??-1}`);
  const r=Phaser.Math.RND.state(),b=s.currentBattle.battleSeedState;
  const rng=(x:string)=>x.replace(/^!rnd,/," ").replaceAll(","," ");rows.push(`R ${rng(r)}`);
  s.arena.addTag(ArenaTagType.IMPRISON,0,MoveId.IMPRISON,id,side,true);
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;
  let cancelled=false;
  if(kind===0){s.arena.removeAllTags();s.arena.addTag(ArenaTagType.IMPRISON,0,MoveId.IMPRISON,id,side,quiet);}
  if(kind===1)s.arena.removeAllTags();
  if(kind===2)s.arena.removeTagOnSide(ArenaTagType.IMPRISON,ArenaTagSide.BOTH,quiet);
  if(kind===3)s.arena.lapseTags();
  if(kind===4||kind===5){const phase:any=pm.create("MovePhase",target,[0],new PokemonMove(MoveId.SPLASH),MoveUseMode.NORMAL);pm.currentPhase=phase;target.lapseTag(BattlerTagType.IMPRISON,kind===4?BattlerTagLapseType.PRE_MOVE:BattlerTagLapseType.AFTER_MOVE);cancelled=!!phase.cancelled;}
  if(kind===6){target.removeTag(BattlerTagType.IMPRISON);s.arena.getTag(ArenaTagType.IMPRISON)?.apply(false,target);}
  if(kind===7){s.arena.addTag(ArenaTagType.IMPRISON,0,MoveId.IMPRISON,id,side,quiet);}
  const tags=ps.map(p=>p.getTag(BattlerTagType.IMPRISON));
  rows.push(`RO ${rng(Phaser.Math.RND.state())}`);
  rows.push(`O ${s.arena.tags.length} ${+cancelled} ${tags.map(t=>`${+!!t} ${t?.turnCount??0} ${t?.sourceId??-1}`).join(" ")}`);
  const qs=pm.phaseQueue.levels.flat();if(qs.some((p:any)=>p.phaseName!=="MessagePhase"))throw Error("unexpected phase");rows.push(`Q ${qs.length}`,...qs.map((p:any)=>`T ${Buffer.from(p.text).toString("hex")}`),"E");
  if(r!==Phaser.Math.RND.state()||b!==s.currentBattle.battleSeedState)throw Error("unexpected RNG mutation");
 }
 rows.push(`IMPRISON_COMPLETE ${N}`);fs.writeFileSync(process.env.IMPRISON_OUT??"/tmp/imprison.txt",rows.join("\n")+"\n");
},600000);
