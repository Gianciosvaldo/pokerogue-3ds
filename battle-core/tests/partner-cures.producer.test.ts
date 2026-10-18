/* Actual pinned leaves, queued constructors and native queue/phase execution. */
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

it("actual partner cures and Pollen Puff with queued phases",async()=>{
 const N=Number(process.env.PARTNERCURES_N??12288),SEED=Number(process.env.PARTNERCURES_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<12288)throw Error("Node22 and N>=12288 required");
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager;
 const player:any[]=[...scene.getPlayerField()],enemy:any[]=[...scene.getEnemyField()];if(player.length!==2||enemy.length!==2)throw Error("natural double startup required");
 for(let k=2;k<6;k++){player.push(scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30));enemy.push(scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30,TrainerSlot.NONE));}
 const actors=[...player,...enemy],ids=actors.map(p=>p.id),moves=[MoveId.HEAL_BELL,MoveId.AROMATHERAPY,MoveId.POLLEN_PUFF,MoveId.HEAL_PULSE];
 const attrs=moves.map((m,k)=>allMoves[m].getAttrs(k===3?"HealAttr":k===2?"HealOnAllyAttr":"PartyStatusCureAttr")[0]);if(attrs.some(x=>!x))throw Error("actual owning attributes required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"setFrameRate").mockImplementation(()=>{});}
 vi.spyOn(CommonBattleAnim.prototype,"play").mockImplementation((_reverse:any,done:any)=>{done?.();});
 vi.spyOn(scene.abilityBar,"showAbility").mockImplementation(async()=>{scene.abilityBar.setVisible(true);});
 vi.spyOn(scene.abilityBar,"hide").mockImplementation(async()=>{scene.abilityBar.setVisible(false);});
 const tagTypes=[BattlerTagType.PROTECTED,BattlerTagType.HEAL_BLOCK,BattlerTagType.CONFUSED,BattlerTagType.NIGHTMARE];
 const extras=(p:any)=>[tagTypes.reduce((mask,t,k)=>mask+(p.getTag(t)?1<<k:0),0),p.status?.toxicTurnCount??-1,p.status?.sleepTurnsRemaining??-1,p.status?.freezeTurnsRemaining??-1];
 const abilities=[AbilityId.NONE,AbilityId.BALL_FETCH,AbilityId.SOUNDPROOF,AbilityId.MEGA_LAUNCHER];
 const fd=fs.openSync(process.env.PARTNERCURES_OUT??"/tmp/partner-cures.txt","w");const write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`PARTNERCURES 1 ${N+512} ${SEED}`]);
 const outcomes=[[0,0],[0,0],[0,0],[0,0]],displays=[0,0],resets=[0,0],heals=[0,0];
 const queue=()=>{const out:string[]=[];const emit=(scope:string,level:number,j:number,p:any)=>{
  const name=p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName;let person=-1,a=-1,b=-1,c=-1,d=-1,text="-";
  if(p.phaseName==="ResetStatusPhase"){person=actors.indexOf(p.pokemon);a=+p.affectConfusion;b=+p.reloadAssets;}
  if(p.phaseName==="PokemonHealPhase"){a=p.hpHealed;b=(p.message?1:0)+(!p.showFullHpMessage?2:0)+(p.skipAnim?4:0)+(p.revive?8:0)+(p.healStatus?16:0)+(p.preventFullHeal?32:0)+(p.fullRestorePP?64:0);text=hex(p.message??"");}
  if(p.phaseName==="ShowAbilityPhase"){a=+p.passive;b=allAbilities.find(x=>x?.name===p.abilityName)?.id??-1;c=+p.pokemonOnField;}
  if(p.phaseName==="MessagePhase")text=hex(p.text);
  if(p.phaseName==="CommonAnimPhase"){a=p.anim;b=p.targetIndex??-1;}
  out.push(`Q ${scope} ${level} ${j} ${name} ${p.battlerIndex??-1} ${person} ${a} ${b} ${c} ${d} ${text}`);
 };
 pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,j:number)=>emit("L",l,j,p)));
 [...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a.localeCompare(b)).forEach(([,q]:any,k:number)=>q.queue.forEach((p:any,j:number)=>emit("D",k,j,p)));
 return [`QCOUNT ${out.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...out];};
 const frame=(prefix:string)=>actors.flatMap((p,k)=>[`${prefix} ${k} ${snapshot(p).join(" ")}`,`${prefix}X ${k} ${extras(p).join(" ")}`]);
 for(let i=0;i<N+512;i++){
  const extra=i>=N,q=i-N;
  const mode=extra?(q<256?3:1):i%3,double=extra?true:Math.floor(i/3)%4!==0,n0=extra?6:1+rnd(6),n1=extra?6:1+rnd(6),side=extra?q%2:rnd(2),user=extra?side*6:(side?6:0)+rnd(side?n1:n0),target=extra?side*6+1:rnd(12),swap=i%17===0;
  battle.double=double;scene.getPlayerParty().splice(0,scene.getPlayerParty().length,...player.slice(0,n0));battle.enemyParty=enemy.slice(0,n1);
  for(let k=0;k<12;k++){
   const p=actors[k];p.id=ids[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);p.formIndex=0;p.fusionSpecies=null;p.fusionCustomPokemonData=null;p.bossSegments=0;p.bossSegmentIndex=0;
   p.customPokemonData.ability=abilities[rnd(4)];p.customPokemonData.passive=rnd(2)?AbilityId.SOUNDPROOF:AbilityId.NONE;p.passive=!!rnd(2);p.summonData.abilitySuppressed=!!rnd(2);
   p.level=30+rnd(21);p.nature=rnd(25);p.ivs=Array.from({length:6},()=>rnd(32));p.friendship=70;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.status=null;p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.stats[Stat.HP]=[15,16,17,31,32,127][rnd(6)];p.hp=p.getMaxHp();p.generateName();
   if(scene.field.getIndex(p)>=0)scene.field.remove(p);
  }
  for(const p of [...scene.getPlayerField(),...scene.getEnemyField()])scene.field.add(p);
  if(i%13===0){const party=side?enemy:player;if((side?n1:n0)>1)party[user%6===0?1:0].id=actors[user].id;}
  for(const p of actors){const st=rnd(8);p.status=st?new Status(st,1,2,3):null;p.hp=st===StatusEffect.FAINT?0:[0,1,Math.max(1,Math.floor(p.getMaxHp()/2)),p.getMaxHp()][rnd(4)];
   for(let t=0;t<4;t++)if(rnd(t===3?5:4)===0)p.addTag(tagTypes[t],1,t===3?MoveId.NIGHTMARE:t===2?MoveId.CONFUSE_RAY:t===1?MoveId.HEAL_BLOCK:MoveId.PROTECT,p.id);
   p.switchOutStatus=!!(rnd(5)===0);
  }
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  if(extra){
   const p=actors[mode===3?user:target],t=actors[target],abilityMode=Math.floor(q/2)%4,ability=mode===3?AbilityId.MEGA_LAUNCHER:AbilityId.SAP_SIPPER;
   p.customPokemonData.ability=abilityMode===0||abilityMode===2?ability:AbilityId.BALL_FETCH;
   p.customPokemonData.passive=abilityMode===1||abilityMode===2?ability:AbilityId.NONE;
   p.passive=Math.floor(q/8)%2===0;p.summonData.abilitySuppressed=Math.floor(q/16)%2===1;p.hp=q%7===0?0:p.getMaxHp();p.switchOutStatus=false;
   t.status=null;t.hp=q%11===0?0:q%5===0?t.getMaxHp():1;t.switchOutStatus=false;t.summonData.tags=[];
  }
  const gas=extra?(Math.floor(q/32)%2):rnd(3);if(gas){const tag:any=new SuppressAbilitiesTag(actors[user].id);tag.sourceCount=gas;scene.arena.tags=[tag];}
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();scene.abilityBar.setVisible(false);battle.lastPlayerInvolved=-1;battle.lastEnemyInvolved=-1;
  battle.battleSeed=`partner-cures-${SEED}-${i}`;battle.battleSeedState=null;battle.turn=1;scene.waveSeed=`partner-cures-wave-${SEED}`;Phaser.Math.RND.sow([`partner-cures-global-${SEED}-${i}`]);battle.randSeedInt(2);
  const rows=[`C ${i} ${mode} ${user} ${target} ${+double} ${n0} ${n1} ${+swap}`,`G ${gas} ${actors[user].id}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...frame("I")];
  if(mode===2)rows.push(`COND ${+attrs[2].getCondition()(actors[user],actors[target],allMoves[moves[mode]])}`);
  const result=attrs[mode].apply(actors[user],actors[target],allMoves[moves[mode]],[]);outcomes[mode][+result]++;
  rows.push(`RESULT ${+result}`,...frame("O"),...queue());
  const root=pm.phaseQueue.levels.flat();displays[+(root.some((p:any)=>p.phaseName==="ShowAbilityPhase"))]++;resets[+(root.some((p:any)=>p.phaseName==="ResetStatusPhase"))]++;heals[+(root.some((p:any)=>p.phaseName==="PokemonHealPhase"))]++;
  if(swap){const party=side?battle.enemyParty:scene.getPlayerParty();if(party.length>1)[party[0],party[1]]=[party[1],party[0]];}
  const texts:string[]=[];let runs=0;
  for(;;){let p=pm.phaseQueue.getNextPhase();if(!p)break;if(p.phaseName==="DynamicPhaseMarker")p=pm.dynamicQueueManager.popNextPhase(p.phaseType);if(!p)throw Error("empty dynamic marker");if(++runs>64)throw Error("phase bound");
   if(p.phaseName==="MessagePhase")texts.push(hex(p.text));else{pm.currentPhase=p;p.start();for(let t=0;t<4;t++)await Promise.resolve();}
  }
  rows.push(...frame("Z"),`WORLD ${+scene.abilityBar.isVisible()} ${battle.lastPlayerInvolved??-1} ${battle.lastEnemyInvolved??-1}`,`TEXTS ${texts.length}`,...texts.map(s=>`T ${s}`),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);vi.clearAllMocks();
 }
 if(outcomes.some(a=>a.some(n=>!n))||!displays[1]||!resets[1]||!heals[1])throw Error("branch breadth missing");
 write([`PARTNERCURES_COMPLETE ${N+512}`]);fs.closeSync(fd);console.log(`PARTNERCURES outcomes=${JSON.stringify(outcomes)} displays=${displays} resets=${resets} heals=${heals}`);
},600000);
