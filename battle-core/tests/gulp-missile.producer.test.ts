import { HitResult } from "#enums/hit-result";
import { CommonBattleAnim } from "#data/battle-anims";
import { getArenaTag } from "#data/arena-tag";
import { ArenaTagType } from "#enums/arena-tag-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { Status } from "#data/status-effect";
import { StatusEffect } from "#enums/status-effect";
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves, allAbilities } from "#data/data-lists";
import { MoveUseMode } from "#enums/move-use-mode";
import { getBattlerTag } from "#data/battler-tags";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
const rng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
const hex=(s:string)=>s?Buffer.from(s,"utf8").toString("hex"):"-";
function snapshot(p: any): number[] {
  const sd = p.summonData, form = p.getSpeciesForm(), transformed = p.isTransformed();
  const formIndex = Math.max(0, speciesDataRegistry.getSpecies(form.speciesId).forms.indexOf(form));
  const moves = (ms: any[]) => [ms.length, ...Array.from({ length: 4 }, (_, i) => ms[i]?.moveId ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUsed ?? 0), ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUp ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.maxPpOverride ?? -1)];
  const types = p.getTypes();
  return [p.species.speciesId, p.formIndex, p.gender, p.id, p.level, p.nature, p.hp, p.status?.effect ?? 0,
    +p.shiny, p.getAbility(true).id, p.getAbility().id, p.hasPassive() ? p.getPassiveAbility().id : 0, +p.hasPassive(),
    p.customPokemonData.passive, p.fusionSpecies?.speciesId ?? 0, sd.illusion?.species ?? 0,
    +p.isOnField(), +p.switchOutStatus,
    ...p.stats, ...Array.from({ length: 6 }, (_, s) => sd.stats[s] ?? 0),
    ...[Stat.ATK, Stat.DEF, Stat.SPATK, Stat.SPDEF, Stat.SPD, Stat.ACC, Stat.EVA].map(s => p.getStatStage(s)),
    ...moves(p.getMoveset()), sd.types?.length ?? 0, ...Array.from({ length: 4 }, (_, i) => sd.types?.[i] ?? 0),
    sd.addedType ?? 0, +p.isTerastallized, p.teraType, +transformed,
    transformed ? sd.speciesForm.speciesId : 0, transformed ? formIndex : 0, transformed ? sd.gender ?? 0 : 0,
    ...(transformed ? moves(p.moveset) : [0, ...Array(12).fill(0), ...Array(4).fill(0)]),
    +!!p.getTag(BattlerTagType.POWER_TRICK), +!!p.getTag(BattlerTagType.SUBSTITUTE),
    form.speciesId, formIndex, p.getGender(), ...Array.from({ length: 6 }, (_, s) => p.getStat(s, false)),
    types.length, ...Array.from({ length: 5 }, (_, i) => types[i] ?? 0), p.variant, p.getSpriteScale(), +p.isMega(), +p.isMax(), ...p.ivs, p.friendship, +sd.abilitySuppressed, sd.abilitiesApplied.size, ...Array.from({length:4},(_,i)=>[...sd.abilitiesApplied][i]??0), p.waveData.abilitiesApplied.size, ...Array.from({length:4},(_,i)=>[...p.waveData.abilitiesApplied][i]??0), p.tempSummonData.turnCount, p.tempSummonData.waveTurnCount, sd.ability??-1];
}

import { TrainerSlot } from "#enums/trainer-slot";
import { BattlerTagLapseType } from "#enums/battler-tag-lapse-type";
import { SpeciesFormChangeAbilityTrigger, SpeciesFormChangeActiveTrigger } from "#data/form-change-triggers";
it("native Gulp Missile move, tag and form boundaries",async()=>{
 const N=16768,SEED=Number(process.env.GULP_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager;
 const actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField(),...Array.from({length:2},()=>scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),50)),...Array.from({length:2},()=>scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),50,TrainerSlot.NONE))];
 if(actors.length!==8)throw Error("natural doubles and real benches required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors)vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});
 vi.spyOn(CommonBattleAnim.prototype,"play").mockImplementation((_r:any,done:any)=>{done?.();});
 const fd=fs.openSync(process.env.GULP_OUT??"/tmp/gulp.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`GULP 3 ${N} ${SEED}`]);
 const attr=allMoves[MoveId.SURF].getAttrs("GulpMissileTagAttr")[0];if(!attr)throw Error("registered Surf attribute required");
 const actor=(p:any)=>actors.indexOf(p),gulps=[BattlerTagType.GULP_MISSILE_ARROKUDA,BattlerTagType.GULP_MISSILE_PIKACHU];
 const dump=(prefix:string)=>actors.flatMap((p,k)=>[
  `${prefix} ${k} ${snapshot(p).join(" ")}`,
  `${prefix}S ${k} ${p.status?.toxicTurnCount??0} ${p.status?.sleepTurnsRemaining??-1} ${p.status?.freezeTurnsRemaining??-1}`,
  `${prefix}X ${k} ${p.turnData.damageTaken} ${p.turnData.hitCount} ${p.turnData.pendingStatus??0} ${p.turnData.combiningPledge??0} ${+!!p.turnData.acted} ${p.turnData.formChangeAbilitiesApplied.size} ${p.turnData.attacksReceived.length} ${+p.isBoss()} ${p.bossSegments??0} ${p.bossSegmentIndex??0} ${+p.hasTrainer()}`,
  `${prefix}T ${k} ${p.summonData.tags.length}`,
  ...p.summonData.tags.map((t:any,j:number)=>`${prefix}G ${k} ${j} ${t.tagType} ${t.turnCount} ${t.sourceMove??0} ${t.sourceId??-1} ${t.hp??0} ${+!!t.sourceInFocus}`)
 ]);
 const queue=()=>{
  const rows:string[]=[];
  const emit=(prefix:string,l:number,j:number,p:any)=>{
   let who=-1,a=-1,b=-1,c=-1,d=-1,f=-1,h=-1,z=-1,text="-";
   switch(p.phaseName){
   case "QuietFormChangePhase":who=actor(p.pokemon);a=p.pokemon.species.forms.findIndex((x:any)=>x.formKey===p.formChange.formKey);break;
   case "MessagePhase":a=+!!p.prompt;text=hex(p.text);break;
   case "ShowAbilityPhase":a=+p.passive;b=allAbilities.find(x=>x?.name===p.abilityName)?.id??-1;c=+p.pokemonOnField;break;
   case "HideAbilityPhase":break;
   case "DynamicPhaseMarker":a=["StatStageChangePhase","ObtainStatusEffectPhase"].indexOf(p.phaseType);if(a<0)throw Error(`unexpected marker ${p.phaseType}`);break;
   case "DamageAnimPhase":a=p.amount;b=p.damageResult;c=+p.critical;break;
   case "FaintPhase":who=actor(p.source);a=+p.preventInstantRevive;break;
   case "StatStageChangePhase":{const x=p.options;who=actor(x.sourcePokemon);if(x.changes.length!==1)throw Error("single Gulp stat change required");a=x.changes[0].stat;b=x.changes[0].stages;c=+!!x.ignoreAbilities;d=+(x.message!=null);f=+!!x.processed;h=+p.selfTarget;z=x.sourceEffectType??0;break;}
   case "ObtainStatusEffectPhase":who=actor(p.sourcePokemon);a=p.statusEffect;b=p.sleepTurnsRemaining??-1;text=hex(p.statusMessage);break;
   default:throw Error(`unexpected queue ${p.phaseName}`);
   }
   rows.push(`${prefix} ${l} ${j} ${p.phaseName} ${p.battlerIndex??-1} ${who} ${a} ${b} ${c} ${d} ${f} ${h} ${z} ${text}`);
  };
  pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,j:number)=>emit("P",l,j,p)));
  const out=[`Q ${rows.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive} ${pm.phaseQueue.currentLevel}`,...rows];rows.length=0;
  const dyn:any[]=[...pm.dynamicQueueManager.dynamicPhaseMap.entries()].sort(([a]:any,[b]:any)=>a.localeCompare(b)).flatMap(([,q]:any)=>q.queue);dyn.forEach((p:any,j:number)=>emit("D",0,j,p));
  return [...out,`DQ ${dyn.length}`,...rows];
 };
 let additions=0,recoils=0,quiet=0,secondary=0,missing=0,under=0,sub=0;
 for(let i=0;i<N;i++){
  const low=i>=16384&&i<16640,late=i>=16640;
  const mode=late?5:low?2:i%5,u=Math.floor(i/5)%4,shape=Math.floor(i/20)%8,tag=late?gulps[1]:gulps[Math.floor(i/160)%2],form=Math.floor(i/320)%3,layout=Math.floor(i/960)%4,fxkind=low?1:late?0:Math.floor(i/3840)%4;
  const attack=fxkind===1?(u<2?4:6):((u+2+Math.floor(i/80)%2)%4),move=[MoveId.SURF,MoveId.DIVE,MoveId.TACKLE,MoveId.HYPER_VOICE][Math.floor(i/40)%4],lapse=[BattlerTagLapseType.HIT,BattlerTagLapseType.CUSTOM,BattlerTagLapseType.FAINT][Math.floor(i/80)%3];
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.waveIndex=201;battle.turn=1;battle.double=true;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  for(let k=0;k<8;k++){if(scene.field.getIndex(actors[k])>=0)scene.field.remove(actors[k]);if(k<4)scene.field.add(actors[k]);}
  scene.getPlayerParty().splice(0,99,actors[0],actors[1],actors[4],actors[5]);scene.getEnemyParty().splice(0,99,actors[2],actors[3],actors[6],actors[7]);
  for(let k=0;k<8;k++){
   const p=actors[k];p.id=1000+k;p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(k===u?(layout===3?SpeciesId.RATTATA:SpeciesId.CRAMORANT):k===attack&&shape===7?SpeciesId.SHEDINJA:[SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k%4]);p.formIndex=k===u&&layout!==3?form:0;p.fusionSpecies=null;p.customPokemonData.ability=k===u?(layout===1?AbilityId.BALL_FETCH:AbilityId.GULP_MISSILE):k===attack?[AbilityId.NONE,AbilityId.MAGIC_GUARD,AbilityId.LIMBER,AbilityId.BALL_FETCH][layout]:AbilityId.BALL_FETCH;p.summonData.ability=p.customPokemonData.ability;p.customPokemonData.passive=k===u&&layout===1?AbilityId.GULP_MISSILE:AbilityId.NONE;p.passive=k===u&&layout===1;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,256,1000000][(Math.floor(i/80)+k)%4];if(!p.isPlayer()){p.bossSegments=Math.floor(i/640)%3===1?2:0;p.bossSegmentIndex=0;p.trainerSlot=TrainerSlot.NONE;}
   p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();const mh=p.getMaxHp();p.hp=k===u?[0,1,Math.max(0,Math.floor(mh/2)-1),Math.floor(mh/2),Math.min(mh,Math.floor(mh/2)+1),mh,mh,mh][shape]:k===attack&&shape===0?1:mh;p.status=p.hp===0?new Status(StatusEffect.FAINT):null;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.generateName();p.turnData.damageTaken=37;p.turnData.hitCount=3;p.turnData.pendingStatus=StatusEffect.NONE;p.turnData.combiningPledge=0;p.turnData.acted=true;
  }
  if(low)actors[attack].id=(i-16384)%4;
  if(late){const holder=actors[u],target=actors[attack];holder.species=speciesDataRegistry.getSpecies(SpeciesId.CRAMORANT);holder.formIndex=2;holder.customPokemonData.ability=AbilityId.GULP_MISSILE;holder.summonData.ability=AbilityId.GULP_MISSILE;holder.hp=Math.max(1,Math.floor(holder.getMaxHp()/4));holder.status=null;target.hp=target.getMaxHp();target.status=null;target.customPokemonData.ability=AbilityId.NONE;target.summonData.ability=AbilityId.NONE;holder.generateName();}
  const p=actors[u];if(!late&&shape===6){const t:any=getBattlerTag(BattlerTagType.UNDERWATER,1,MoveId.DIVE,p.id);p.summonData.tags.push(t);}if(!late&&shape===5){const t:any=getBattlerTag(BattlerTagType.SUBSTITUTE,0,MoveId.SUBSTITUTE,p.id);t.hp=Math.max(1,Math.floor(p.getMaxHp()/4));t.sourceInFocus=false;p.summonData.tags.push(t);}
  if(mode===2||mode===3||mode===5||Math.floor(i/80)%4===0)p.summonData.tags.push(getBattlerTag(tag,0,MoveId.SURF));
  pm.clearAllPhases();pm.dynamicQueueManager.clearLastTurnOrder();
  const sourceIndex=fxkind===1?actors[attack].id:fxkind===2?4294967294:attack,use=fxkind===1||fxkind===2?MoveUseMode.DELAYED_ATTACK:MoveUseMode.NORMAL;
  pm.currentPhase=fxkind===3?pm.create("TurnEndPhase"):pm.create("MoveEffectPhase",sourceIndex,[u],allMoves[move],use);
  const spy=fxkind!==3?vi.spyOn(pm.currentPhase,"getUserPokemon"):null;
  battle.battleSeed=`gulp-${SEED}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`gulp-${SEED}-${i}`]);battle.randSeedInt(2);
  const hp=actors.map(p=>p.hp),nt=p.summonData.tags.length,rows=[`C ${i} ${mode} ${u} ${attack} ${move} ${tag} ${lapse} ${fxkind}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...dump("I"),`IQ ${pm.phaseQueue.levels.flat().length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive} ${pm.phaseQueue.currentLevel}`];
  let ret=-1;if(mode===0)ret=+attr.apply(p,actors[u^2],allMoves[move],[]);else if(mode===1)ret=+p.addTag(tag,0,move);else if(mode===2||mode===5)ret=+p.lapseTag(tag,lapse);else if(mode===3)p.removeTag(tag);else scene.triggerPokemonFormChange(p,Math.floor(i/80)%2?SpeciesFormChangeAbilityTrigger:SpeciesFormChangeActiveTrigger);
  rows.push(`RET ${ret}`,...dump("O"),...queue(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`);
  if(late){
   const status=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue).find((q:any)=>q.phaseName==="ObtainStatusEffectPhase");if(!status)throw Error(`late status missing ${i}`);
   const bench=attack<2?4:6,party=attack<2?scene.getPlayerParty():scene.getEnemyParty(),a=party.indexOf(actors[attack]),b=party.indexOf(actors[bench]);[party[a],party[b]]=[party[b],party[a]];scene.field.remove(actors[attack]);scene.field.add(actors[bench]);
   rows.push(`SWAP ${attack} ${bench}`);status.start();rows.push(...dump("F"),...queue(),`FR ${rng(Phaser.Math.RND.state())}`,`FB ${rng(battle.battleSeedState)}`);
  }
  rows.push("E");write(rows);
  additions+=+(p.summonData.tags.length>nt);recoils+=actors.filter((p,k)=>p.hp<hp[k]).length;quiet+=pm.phaseQueue.levels.flat().filter((p:any)=>p.phaseName==="QuietFormChangePhase").length;secondary+=[...pm.phaseQueue.levels.flat(),...[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue)].filter((p:any)=>["StatStageChangePhase","ObtainStatusEffectPhase"].includes(p.phaseName)).length;missing+=spy?.mock.results.filter((x:any)=>x.type==="return"&&!x.value).length??0;spy?.mockRestore();under+=+(mode===2&&shape===6&&p.getTag(tag)!=null);sub+=+(mode===2&&shape===5&&p.getTag(tag)!=null);
 }
 if([additions,recoils,quiet,secondary,missing,under,sub].some(n=>n<100))throw Error(`coverage ${additions}/${recoils}/${quiet}/${secondary}/${missing}/${under}/${sub}`);
 // Separate exception probe: queueing is defined, executing bench index -1
 // dereferences undefined in pinned ObtainStatusEffectPhase.start.
 pm.clearAllPhases();const bench=actors[5];bench.hp=bench.getMaxHp();bench.status=null;bench.turnData.pendingStatus=0;bench.summonData.ability=AbilityId.NONE;bench.customPokemonData.ability=AbilityId.NONE;
 if(bench.isOnField()||!bench.trySetStatus(StatusEffect.PARALYSIS,actors[0]))throw Error("bench exception setup");
 const absent=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue).find((q:any)=>q.phaseName==="ObtainStatusEffectPhase");
 if(!absent||absent.battlerIndex!==-1)throw Error("bench phase missing");
 let sourceException=false;try{absent.start();}catch(error){if(!(error instanceof TypeError)||!String(error).includes("doSetStatus"))throw error;sourceException=true;}
 if(!sourceException)throw Error("expected pinned bench status exception");
 console.log("GULP separate-native-exception=ObtainStatusEffectPhase(-1):TypeError(doSetStatus)");
 const damage=pm.create("DamageAnimPhase",-1,1,HitResult.INDIRECT);sourceException=false;
 try{damage.start();}catch(error){if(!(error instanceof TypeError))throw error;sourceException=true;console.log(`GULP separate-native-exception=DamageAnimPhase(-1):${String(error)}`);}
 if(!sourceException)throw Error("expected pinned bench damage animation exception");
 console.log("GULP low-id=256 late-status=128");
 write([`GULP_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`GULP added=${additions} recoil=${recoils} quiet=${quiet} secondary=${secondary} missing=${missing} under=${under} sub=${sub}`);
},600000);
