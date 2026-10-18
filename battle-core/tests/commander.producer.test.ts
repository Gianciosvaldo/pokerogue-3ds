/* Native Commander, CommandedTag, cached Order Up and queued execution. */
import { BattleScene } from "#app/battle-scene";
import { allMoves,allAbilities } from "#data/data-lists";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { CommonBattleAnim } from "#data/battle-anims";
import { Status } from "#data/status-effect";
import { SuppressAbilitiesTag } from "#data/arena-tag";
import { AbilityId } from "#enums/ability-id";
import { ArenaTagType } from "#enums/arena-tag-type";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { TrainerSlot } from "#enums/trainer-slot";
import { Stat } from "#enums/stat";
import { PokemonMove } from "#moves/pokemon-move";
import { applyAbAttrs } from "#abilities/apply-ab-attrs";
import { PhaseManager } from "#app/phase-manager";
import { MovePhase } from "#phases/move-phase";
import { StatStageChangePhase } from "#phases/stat-stage-change-phase";
import { MoveUseMode } from "#enums/move-use-mode";
import { BattlerTagLapseType } from "#enums/battler-tag-lapse-type";
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

it("Commander and cached Order Up with native phases",async()=>{
 const N=Number(process.env.COMMANDER_N??16384),SEED=Number(process.env.COMMANDER_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<16384)throw Error("Node22 and N>=16384 required");
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const native=BattleScene.prototype.randBattleSeedInt,nativeShift=PhaseManager.prototype.shiftPhase;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager;
 const player:any[]=[...scene.getPlayerField()],enemy:any[]=[...scene.getEnemyField()];if(player.length!==2||enemy.length!==2)throw Error("natural double startup required");
 for(let k=2;k<6;k++){player.push(scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30));enemy.push(scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30,TrainerSlot.NONE));}
 const actors=[...player,...enemy],ids=actors.map(p=>p.id);
 const orderAttr=allMoves[MoveId.ORDER_UP].getAttrs("OrderUpStatBoostAttr")[0];if(!orderAttr)throw Error("actual owning Order Up required");
 vi.spyOn(StatStageChangePhase.prototype as any,"playStatChangeAnimation").mockImplementation(async()=>{});
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 vi.spyOn(pm,"startCurrentPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"setFrameRate").mockImplementation(()=>{});}
 vi.spyOn(CommonBattleAnim.prototype,"play").mockImplementation((_reverse:any,done:any)=>{done?.();});
 vi.spyOn(scene.abilityBar,"showAbility").mockImplementation(async()=>{scene.abilityBar.setVisible(true);});
 vi.spyOn(scene.abilityBar,"hide").mockImplementation(async()=>{scene.abilityBar.setVisible(false);});
 const tagTypes=[BattlerTagType.FLYING,BattlerTagType.UNDERGROUND,BattlerTagType.UNDERWATER,BattlerTagType.HIDDEN];
 const extras=(p:any)=>{const t=p.getTag(BattlerTagType.COMMANDED);return [tagTypes.reduce((mask,x,k)=>mask+(p.getTag(x)?1<<k:0),0),t?1:0,t?.turnCount??0,t?.sourceMove??0,t?.sourceId??-1,hex(t?.tatsugiriFormKey??""),+(scene.field.getIndex(p)>=0)];};
 const statAbilities=[AbilityId.BALL_FETCH,AbilityId.SIMPLE,AbilityId.CONTRARY];
 const queueMoves=[MoveId.SPLASH,MoveId.QUICK_ATTACK,MoveId.ROAR];
 const stats=[Stat.ATK,Stat.DEF,Stat.SPATK,Stat.SPDEF,Stat.SPD,Stat.ACC,Stat.EVA];
 const fd=fs.openSync(process.env.COMMANDER_OUT??"/tmp/commander.txt","w");const write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`COMMANDER 1 ${N+576} ${SEED}`]);
 const outcomes=[[0,0],[0,0],[0,0],[0,0]];let commanded=0,orderPhases=0;
 const queue=()=>{const out:string[]=[];const emit=(scope:string,level:number,j:number,p:any)=>{
  const name=p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName;let person=-1,a=-1,b=-1,c=-1,d=-1,text="-";
  if(p.phaseName==="MovePhase"){person=actors.indexOf(p.pokemon);a=p.move.moveId;b=p.timingModifier;c=p.useMode;}
  if(p.phaseName==="PokemonAnimPhase"){person=actors.indexOf(p.pokemon);a=p.key;b=p.fieldAssets.length;}
  if(p.phaseName==="StatStageChangePhase"){person=actors.indexOf(p.options.sourcePokemon);a=p.options.changes.length;b=+p.selfTarget+(p.options.processed?2:0)+(p.options.ignoreAbilities?4:0)+(p.options.message?8:0);c=p.options.sourceEffectType??0;text=hex(p.options.changes.map((x:any)=>`${x.stat}:${x.stages}`).join(","));}
  if(p.phaseName==="ShowAbilityPhase"){a=+p.passive;b=allAbilities.find(x=>x?.name===p.abilityName)?.id??-1;c=+p.pokemonOnField;}
  if(p.phaseName==="MessagePhase")text=hex(p.text);
  if(p.phaseName==="CommonAnimPhase"){a=p.anim;b=p.targetIndex??-1;}
  out.push(`Q ${scope} ${level} ${j} ${name} ${p.battlerIndex??-1} ${person} ${a} ${b} ${c} ${d} ${text}`);
 };
 pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,j:number)=>emit("L",l,j,p)));
 [...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a.localeCompare(b)).forEach(([,q]:any,k:number)=>q.queue.forEach((p:any,j:number)=>emit("D",k,j,p)));
 return [`QCOUNT ${out.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...out];};
 const frame=(prefix:string)=>actors.flatMap((p,k)=>[`${prefix} ${k} ${snapshot(p).join(" ")}`,`${prefix}X ${k} ${extras(p).join(" ")}`]);
 for(let i=0;i<N+576;i++){
  const extra=i>=N,q=i-N;
  const mode=extra?3:i%3,double=extra?true:Math.floor(i/3)%5!==0,n0=extra?6:2+rnd(5),n1=extra?6:2+rnd(5),side=extra?q%2:rnd(2),user=extra?side*6:(side?6:0)+rnd(side?n1:n0),target=extra?(1-side)*6:rnd(12),simulated=extra?false:i%7===0,lapse=[BattlerTagLapseType.CUSTOM,BattlerTagLapseType.TURN_END,BattlerTagLapseType.FAINT][rnd(3)];
  battle.double=double;scene.getPlayerParty().splice(0,scene.getPlayerParty().length,...player.slice(0,n0));battle.enemyParty=enemy.slice(0,n1);
  for(let k=0;k<12;k++){
   const p=actors[k];p.id=ids[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(rnd(4)?SpeciesId.TATSUGIRI:SpeciesId.RATTATA);p.formIndex=p.species.speciesId===SpeciesId.TATSUGIRI?rnd(3):0;p.fusionSpecies=null;p.fusionCustomPokemonData=null;p.bossSegments=0;p.bossSegmentIndex=0;
   p.customPokemonData.ability=statAbilities[rnd(3)];p.customPokemonData.passive=statAbilities[rnd(3)];p.passive=!!rnd(2);p.summonData.abilitySuppressed=!!rnd(2);
   p.level=30+rnd(21);p.nature=rnd(25);p.ivs=Array.from({length:6},()=>rnd(32));p.friendship=70;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.status=null;p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=rnd(5)?p.getMaxHp():0;p.generateName();
   for(const st of stats)p.setStatStage(st,[-6,0,5,6][rnd(4)]);
   if(scene.field.getIndex(p)>=0)scene.field.remove(p);
  }
  const ally=actors[user].getAlly();if(ally&&(extra||rnd(4))){ally.species=speciesDataRegistry.getSpecies(SpeciesId.DONDOZO);ally.formIndex=0;ally.calculateStats();ally.generateName();}
  actors[user].customPokemonData.ability=rnd(3)?AbilityId.COMMANDER:AbilityId.BALL_FETCH;actors[user].customPokemonData.passive=rnd(2)?AbilityId.COMMANDER:AbilityId.NONE;
  for(const p of [...scene.getPlayerField(),...scene.getEnemyField()])if(rnd(6))scene.field.add(p);
  for(const p of actors){for(let t=0;t<4;t++)if(rnd(8)===0)p.addTag(tagTypes[t],1,[MoveId.FLY,MoveId.DIG,MoveId.DIVE,MoveId.PHANTOM_FORCE][t],p.id);p.switchOutStatus=rnd(5)===0;}
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  const gas=extra?0:rnd(3),trick=extra&&q%3===0;if(gas){const tag:any=new SuppressAbilitiesTag(actors[user].id);tag.sourceCount=gas;scene.arena.tags=[tag];}
  if(trick)scene.arena.addTag(ArenaTagType.TRICK_ROOM,5,MoveId.TRICK_ROOM,actors[user].id);
  const orderUser=mode===0||mode===3?actors.indexOf(ally??actors[user]):mode===1?target:user;
  if(!extra&&rnd(3)===0)actors[orderUser].addTag(BattlerTagType.COMMANDED,9,MoveId.TACKLE,rnd(5)?actors[user].id:4294967295);
  if(extra){
   const p=actors[user],a=actors[orderUser];
   p.species=speciesDataRegistry.getSpecies(q%4===0?SpeciesId.DONDOZO:SpeciesId.TATSUGIRI);p.formIndex=q%4===0?0:q%3;p.calculateStats();p.generateName();
   for(const mon of [p,a]){mon.hp=mon.getMaxHp();mon.switchOutStatus=false;mon.summonData.abilitySuppressed=false;mon.customPokemonData.passive=AbilityId.NONE;mon.passive=false;mon.setStatStage(Stat.SPD,0);if(scene.field.getIndex(mon)<0)scene.field.add(mon);}
   p.customPokemonData.ability=AbilityId.COMMANDER;a.customPokemonData.ability=q%4===0?AbilityId.COMMANDER:AbilityId.BALL_FETCH;
   p.stats[Stat.SPD]=q%2?30:200;a.stats[Stat.SPD]=q%2?200:30;
   if(q%8===1)p.hp=0;
   if(q%8===3)a.hp=0;
   if(q%8===5)p.switchOutStatus=true;
   if(q%8===6)a.switchOutStatus=true;
   if(q>=512){p.hp=1;a.hp=a.getMaxHp();p.switchOutStatus=false;a.switchOutStatus=false;}
  }
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();scene.abilityBar.setVisible(false);battle.lastPlayerInvolved=-1;battle.lastEnemyInvolved=-1;
  battle.battleSeed=`commander-${SEED}-${i}`;battle.battleSeedState=null;battle.turn=1;scene.waveSeed=`commander-wave-${SEED}`;Phaser.Math.RND.sow([`commander-global-${SEED}-${i}`]);battle.randSeedInt(2);
  const sourceId=i%11===0?4294967295:actors[user].id;
  const rows=[`C ${i} ${mode} ${user} ${target} ${+double} ${n0} ${n1} ${+simulated} ${lapse} ${orderUser} ${sourceId}`,`G ${gas} ${actors[user].id} ${+trick}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...frame("I")];
  const candidates=actors.filter(p=>p.getBattlerIndex()>=0),jobs:any[]=[];
  for(let j=0;j<i%6;j++){
   const p=j<2&&actors[user].getBattlerIndex()>=0?actors[user]:candidates[rnd(candidates.length)];
   const kind=rnd(5)===0?2:rnd(2),move=queueMoves[rnd(3)],timing=rnd(3),battler=i%13===0?candidates[rnd(candidates.length)].getBattlerIndex():p.getBattlerIndex();
   const phase:any=new MovePhase(p,[(p.isPlayer()?2:0)],new PokemonMove(move),MoveUseMode.NORMAL);phase.battlerIndex=battler;phase.timingModifier=timing;
   if(kind===2)pm.phaseQueue.addPhase(phase,true);else if(kind===1)pm.unshiftPhase(phase);else pm.pushPhase(phase);
   jobs.push(`J ${kind} ${actors.indexOf(p)} ${battler} ${move} ${timing}`);
  }
  rows.push(`JOBS ${jobs.length}`,...jobs,...queue());
  let result=0;
  if(mode===0){applyAbAttrs("CommanderAbAttr",{pokemon:actors[user],simulated});result=+!!actors[orderUser].getTag(BattlerTagType.COMMANDED);}
  else if(mode===1)result=+actors[target].addTag(BattlerTagType.COMMANDED,9,MoveId.TACKLE,sourceId);
  else if(mode===2)result=+orderAttr.apply(actors[user],actors[target],allMoves[MoveId.ORDER_UP],[]);
  else {const phase=pm.create("PostSummonPhase",actors[user].getBattlerIndex());phase.start();result=+!!actors[orderUser].getTag(BattlerTagType.COMMANDED);}
  outcomes[mode][result]++;if(actors[orderUser].getTag(BattlerTagType.COMMANDED))commanded++;
  rows.push(`RESULT ${result}`,...frame("O"),...queue());
  const newForm=actors[user].species.speciesId===SpeciesId.TATSUGIRI?(actors[user].formIndex+1)%3:actors[user].formIndex;
  rows.push(`FORM ${newForm}`);actors[user].formIndex=newForm;actors[user].calculateStats();actors[user].generateName();
  const before=pm.phaseQueue.levels.flat().filter((p:any)=>p.phaseName==="StatStageChangePhase").length;
  const orderResult=orderAttr.apply(actors[orderUser],actors[target],allMoves[MoveId.ORDER_UP],[]);if(orderResult)orderPhases++;
  rows.push(`ORDER_RESULT ${+orderResult}`,...frame("P"),...queue());
  rows.push(`TRAPPED ${+actors[orderUser].isTrapped([],true)}`);
  actors[orderUser].lapseTags(lapse);
  rows.push(...frame("L"),...queue());
  for(const p of actors)while(pm.tryRemovePhase("MovePhase",(phase:any)=>phase.pokemon===p)){}
  rows.push(...queue());
  const texts:string[]=[];let runs=0;
  for(;;){nativeShift.call(pm);const p=pm.currentPhase;if(p.phaseName==="TurnInitPhase")break;if(++runs>128)throw Error("phase bound");
   if(p.phaseName==="MessagePhase")texts.push(hex(p.text));else if(p.phaseName!=="PokemonAnimPhase"){pm.currentPhase=p;p.start();for(let t=0;t<6;t++)await Promise.resolve();}
  }
  rows.push(...frame("Z"),`WORLD ${+scene.abilityBar.isVisible()} ${battle.lastPlayerInvolved??-1} ${battle.lastEnemyInvolved??-1}`,`TEXTS ${texts.length}`,...texts.map(s=>`T ${s}`),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);vi.clearAllMocks();
 }
 if(outcomes.some(a=>a.some(n=>!n))||!commanded||!orderPhases)throw Error("branch breadth missing");
 write([`COMMANDER_COMPLETE ${N+576}`]);fs.closeSync(fd);console.log(`COMMANDER outcomes=${JSON.stringify(outcomes)} commanded=${commanded} orderPhases=${orderPhases}`);
},600000);
