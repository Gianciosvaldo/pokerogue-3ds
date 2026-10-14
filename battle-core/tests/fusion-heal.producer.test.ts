import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
/* Actual stored healing effects, entry dispatcher and held healing phases. */
import { getPokemonNameWithAffix } from "#app/messages";
import i18next from "i18next";
import { PendingHealTag } from "#data/arena-tag";
import { Status } from "#data/status-effect";
import { AbilityId } from "#enums/ability-id";
import { ArenaTagType } from "#enums/arena-tag-type";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual pending heal and entry",async()=>{
 const N=Number(process.env.PENDING_N??8192),seed=Number(process.env.PENDING_SEED??777);if(!Number.isSafeInteger(N)||N<1024||!process.versions.node.startsWith("22."))throw Error("Node22,N>=1024 required");
 const doubles=process.env.PENDING_DOUBLE==="1";
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.battleStyle(doubles?"double":"single").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await game.classicMode.startBattle(...(doubles?[SpeciesId.BULBASAUR,SpeciesId.SQUIRTLE]:[SpeciesId.BULBASAUR]));game.override.moveset([]).enemyMoveset([]);const s:any=game.scene,pm:any=s.phaseManager,ps:any[]=doubles?s.getField():[game.field.getPlayerPokemon(),game.field.getEnemyPokemon()];
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of ps){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});}
 vi.spyOn(s.damageNumberHandler,"add").mockImplementation(()=>{});
 const rng=(r:string)=>r.replace(/^!rnd,/,"").replaceAll(","," ");
 const snap=(p:any)=>[p.hp,p.getMaxHp(),p.status?.effect??0,p.status?.toxicTurnCount??0,p.status?.sleepTurnsRemaining??-1,p.status?.freezeTurnsRemaining??-1,...p.getMoveset().map((m:any)=>m.ppUsed)];
 const rows=[`PENDING 3 ${N} ${seed} ${ps.length}`];
 for(let i=0;i<N;i++){
  const state=Math.floor(i/ps.length)%64,p=ps[i%ps.length],b=p.getBattlerIndex(),source=ps.find(q=>q.getBattlerIndex()===(b^2)),mode=Math.floor(i/(64*ps.length))%4,simulated=mode===1;
  for(const q of ps){q.resetSummonData();q.resetTurnData();q.resetBattleAndWaveData();q.id=100+q.getBattlerIndex();q.switchOutStatus=false;q.status=null;q.hp=q.getMaxHp();q.moveset=[new PokemonMove(MoveId.TACKLE),new PokemonMove(MoveId.SPLASH)];}
  const forms=[SpeciesId.CHARIZARD,SpeciesId.VENUSAUR,SpeciesId.BLASTOISE,SpeciesId.RAPIDASH,SpeciesId.SLAKING,SpeciesId.KINGLER];
  source.species=speciesDataRegistry.getSpecies(forms[Math.floor(i/4)%forms.length]);source.formIndex=Math.floor(i/24)%Math.max(1,source.species.forms.length);source.fusionSpecies=speciesDataRegistry.getSpecies(forms[Math.floor(i/64)%forms.length]);source.fusionFormIndex=Math.floor(i/384)%Math.max(1,source.fusionSpecies.forms.length);source.generateName();
  p.hp=state%8<4?p.getMaxHp():Math.max(1,p.getMaxHp()-3);if(state%8===5)p.hp=0;
  if(state%8===2||state%8===6)p.status=new Status(StatusEffect.TOXIC,2);
  if(state%8===3||state%8===7)p.moveset[0].ppUsed=2;
  if(state%16>=12)p.addTag(BattlerTagType.HEAL_BLOCK,3,MoveId.HEAL_BLOCK,source.id);
  s.modifiers=[];s.enemyModifiers=[];s.arena.tags=[];pm.clearAllPhases();pm.phaseQueue.deferredActive=false;
  const tag=new PendingHealTag();s.arena.tags.push(tag);const queued:any[]=[];
  for(let j=0;j<(state>=32?0:1+Math.floor(state/8)%3);j++){
   const move=j%2?MoveId.LUNAR_DANCE:MoveId.HEALING_WISH,index=state%16===10?(b^2):b,sid=state%16===6&&j===0?999:source.id,effect={sourceId:sid,moveId:move,restorePP:move===MoveId.LUNAR_DANCE,healMessage:i18next.t(j%2?"moveTriggers:lunarDanceRestore":"moveTriggers:sacrificialFullRestore",{pokemonName:getPokemonNameWithAffix(source)})};
   tag.queueHeal(index,effect);queued.push({index,...effect});
  }
  const dump=()=>Object.entries(tag.pendingHeals).flatMap(([index,es]:any)=>es.map((e:any)=>`${index},${e.sourceId},${e.moveId},${+e.restorePP},${Buffer.from(e.healMessage).toString("hex")}`)).join(";")||"-";
  Phaser.Math.RND.sow([`pending-${seed}-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  rows.push(`C ${i} ${b} ${mode} ${+!!p.getTag(BattlerTagType.HEAL_BLOCK)} ${queued.length}`,`W ${s.waveSeed} ${s.currentBattle.turn}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(s.currentBattle.battleSeedState)}`,`I ${snap(p).join(" ")}`);
  ps.forEach(q=>rows.push(`S ${q.getBattlerIndex()} ${q.id} ${q.species.speciesId} ${q.formIndex} ${q.getMaxHp()} ${q.fusionSpecies?.speciesId??0} ${q.fusionFormIndex??0}`));
  queued.forEach(q=>rows.push(`H ${q.index} ${q.sourceId} ${q.moveId} ${+q.restorePP} ${Buffer.from(q.healMessage).toString("hex")}`));rows.push(`SI ${dump()}`);
  source.species=speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP);source.formIndex=0;source.fusionSpecies=null;source.fusionFormIndex=0;source.generateName();rows.push(`N ${source.species.speciesId} ${source.formIndex} 0 0`);
  let result=-1;if(mode===2){result=+tag.lapse();s.arena.lapseTags();}else if(mode===3){const ph=pm.create("PostSummonPhase",b);pm.currentPhase=ph;ph.start();}else result=+tag.apply(simulated,p);
  const phases=pm.phaseQueue.levels.flat();rows.push(`A ${result} ${snap(p).join(" ")}`,`SA ${dump()}`,`L ${+s.arena.tags.includes(tag)}`,`Q ${phases.length}`);
  for(const ph of phases){if(ph.phaseName!=="PokemonHealPhase")throw Error("unexpected pending phase "+ph.phaseName);rows.push(`P ${ph.battlerIndex} ${ph.hpHealed} ${+ph.showFullHpMessage} ${+ph.skipAnim} ${+ph.healStatus} ${+ph.fullRestorePP} ${Buffer.from(ph.message).toString("hex")}`);pm.currentPhase=ph;await ph.end();}
  for(const ph of pm.phaseQueue.levels.flat().filter((ph:any)=>ph.phaseName==="ResetStatusPhase")){pm.currentPhase=ph;ph.start();}
  const messages=pm.phaseQueue.levels.flat().filter((ph:any)=>ph.phaseName==="MessagePhase").map((ph:any)=>ph.text??ph.message);if(messages.some((m:any)=>typeof m!=="string"))throw Error("unknown message transport");
  rows.push(`O ${snap(p).join(" ")}`,`SO ${dump()}`,`T ${messages.length?Buffer.from(messages.join("\n")).toString("hex"):"-"}`,`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(s.currentBattle.battleSeedState)}`,"E");
 }
 rows.push(`PENDING_COMPLETE ${N}`);fs.writeFileSync(process.env.PENDING_OUT??"/tmp/pending-heal.txt",rows.join("\n")+"\n");
},600000);
