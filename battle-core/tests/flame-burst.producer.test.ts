import { BattleScene } from "#app/battle-scene";
import { allMoves, modifierTypes } from "#data/data-lists";
import { Weather } from "#data/weather";
import { WeatherType } from "#enums/weather-type";
/* Actual Flame Burst override and downstream damage callbacks; no owning turn claim. */
import { audioManager } from "#app/global-audio-manager";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { BaseStatModifier } from "#modifiers/modifier";
import { PokemonMove } from "#moves/pokemon-move";
import { applyAbAttrs } from "#abilities/apply-ab-attrs";
import { SuppressAbilitiesTag } from "#data/arena-tag";
import { ArenaTagType } from "#enums/arena-tag-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it, vi } from "vitest";


const N=Number(process.env.FLAMEBURST_N??16384),SEED=Number(process.env.FLAMEBURST_SEED??777);
let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
const stateRng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
const rows=[`FLAMEBURST 1 ${N} ${SEED}`];
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

it("actual Flame Burst override and indirect damage", async () => {
 if (!process.versions.node.startsWith("22.") || !Number.isSafeInteger(N) || N < 16384) throw Error("Node22 and N>=16384 required");
 const native = BattleScene.prototype.randBattleSeedInt;
 const game = new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
  .passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP)
  .moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await game.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);
 BattleScene.prototype.randBattleSeedInt = native;
 game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=game.scene, pm:any=scene.phaseManager, battle:any=scene.currentBattle;
 const slots:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];
 if(slots.length!==4 || slots.some(p=>!p))throw Error("natural doubles required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of slots){vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});}

 const attr=allMoves[MoveId.FLAME_BURST].getAttrs("FlameBurstAttr")[0];if(!attr)throw Error("missing actual attribute");
 const abilities=[AbilityId.BALL_FETCH,AbilityId.MAGIC_GUARD,AbilityId.STAMINA,AbilityId.BERSERK,AbilityId.ANGER_SHELL,AbilityId.COLOR_CHANGE,AbilityId.WIMP_OUT,AbilityId.EMERGENCY_EXIT];
 const passives=[AbilityId.NONE,AbilityId.MAGIC_GUARD,AbilityId.BERSERK,AbilityId.WIMP_OUT];
 const hps=[1,15,16,17,31,32,127,128,257,1000];
 const breadth=Array(8).fill(0),outcomes=[0,0],bosses=Array(6).fill(0),shields=[0,0],faints=[0,0];
 for(let i=0;i<N;i++){
  const target=i%4,holder=Math.floor(i/4)%4,double=Math.floor(i/16)%4!==0;
  battle.double=double;
  for(const p of slots){
   p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();
   p.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);p.formIndex=0;p.fusionSpecies=null;p.fusionCustomPokemonData=null;
   p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;
   p.level=30+rnd(21);p.nature=rnd(25);p.ivs=Array.from({length:6},()=>rnd(32));p.status=null;
   p.friendship=70;p.switchOutStatus=false;p.turnData.hitCount=2;p.turnData.hitsLeft=2;p.moveset=[new PokemonMove(MoveId.SPLASH)];
   if(!p.isPlayer()){p.bossSegments=0;p.bossSegmentIndex=0;}
   p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=p.getMaxHp();p.generateName();
  }
  const actualAlly=slots[target].getAlly(),ally=actualAlly??slots[target^1];
  const ai=rnd(abilities.length);ally.customPokemonData.ability=abilities[ai];breadth[ai]++;
  ally.customPokemonData.passive=passives[rnd(passives.length)];ally.passive=!!rnd(2);
  ally.summonData.abilitySuppressed=!!rnd(2);
  const maximum=hps[rnd(hps.length)];ally.stats[Stat.HP]=maximum;ally.hp=maximum;
  if(rnd(2))ally.addTag(BattlerTagType.ENDURING,1,MoveId.ENDURE,ally.id);
  if(rnd(2))ally.addTag(BattlerTagType.SUBSTITUTE,0,MoveId.SUBSTITUTE,ally.id);
  const hpMode=rnd(6);ally.hp=[0,1,Math.max(1,Math.floor(maximum/2)),Math.max(1,maximum-1),maximum,Math.min(maximum,Math.ceil(maximum/2))][hpMode];
  if(!ally.isPlayer()){ally.bossSegments=[0,2,3,5][rnd(4)];ally.bossSegmentIndex=ally.getBossSegmentIndex();bosses[ally.bossSegments]++;}
  for(let stat=Stat.ATK;stat<=Stat.SPD;stat++)ally.setStatStage(stat,i%11===0?6:i%11===1?(stat%2?6:5):0);
  ally.switchOutStatus=!!rnd(2);
  if(rnd(3)===0)slots[holder].hp=0;if(rnd(3)===0)slots[target].hp=0;
  // Keep bosses' cached shield index consistent with the final supplied HP.
  for(const p of slots)if(!p.isPlayer())p.bossSegmentIndex=p.getBossSegmentIndex();
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.terrain=null;scene.arena.weather=null;
  if(i%7===0){const mt=modifierTypes.LEFTOVERS();scene.enemyModifiers=[new BaseStatModifier(mt,slots[2].id,Stat.ATK,1),new BaseStatModifier(mt,slots[3].id,Stat.DEF,2)];scene.modifiers=[new BaseStatModifier(mt,slots[0].id,Stat.SPATK,1)];}
  const modRows=()=>[...scene.modifiers.map((m:any)=>`M 0 ${m.pokemonId} ${m.stackCount} ${m.stat}`),...scene.enemyModifiers.map((m:any)=>`M 1 ${m.pokemonId} ${m.stackCount} ${m.stat}`)];
  const initial=rnd(4),sid=slots[holder].id;if(initial){const tag:any=new SuppressAbilitiesTag(sid);tag.sourceCount=initial;scene.arena.tags=[tag];}
  scene.arena.playerFaints=0;battle.enemyFaints=0;battle.playerFaintsHistory=[];battle.enemyFaintsHistory=[];
  battle.playerParticipantIds=new Set([slots[0].id,slots[1].id]);battle.postBattleLoot=[];battle.battleScore=0;
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();scene.abilityBar.setVisible(false);
  battle.battleSeed=`flame-burst-${SEED}-${i}`;battle.battleSeedState=null;battle.turn=1;scene.waveSeed=`flame-burst-wave-${SEED}`;
  Phaser.Math.RND.sow([`flame-burst-global-${SEED}-${i}`]);battle.randSeedInt(2);
  const extras=(p:any)=>[p.bossSegments??0,p.bossSegmentIndex??0,+!!p.getTag(BattlerTagType.ENDURING),p.getTag(BattlerTagType.SUBSTITUTE)?.hp??-1,p.turnData.hitCount,p.turnData.hitsLeft];
  rows.push(`C ${i} ${holder} ${target} ${+double}`,`G ${initial} ${sid}`,`MODS ${scene.modifiers.length+scene.enemyModifiers.length}`,...modRows(),`R ${stateRng(Phaser.Math.RND.state())}`,`B ${stateRng(battle.battleSeedState)}`,...slots.flatMap((p,b)=>[`I ${b} ${snapshot(p).join(" ")}`,`IX ${b} ${extras(p).join(" ")}`]));
  const previousShield=ally.bossSegmentIndex??0,previousHP=ally.hp;
  const result=attr.apply(slots[holder],slots[target],allMoves[MoveId.FLAME_BURST],[]);outcomes[+result]++;
  shields[+(previousShield!==(ally.bossSegmentIndex??0))]++;faints[+(previousHP>0&&ally.hp===0)]++;
  rows.push(`RESULT ${+result}`);
  const gas:any=scene.arena.getTag(ArenaTagType.NEUTRALIZING_GAS),participants=[...battle.playerParticipantIds].sort((a:any,b:any)=>a-b);
  rows.push(`WORLD ${scene.arena.tags.length} ${gas?.sourceCount??0} ${+(gas?.beingRemoved??false)} ${scene.arena.playerFaints} ${battle.enemyFaints} ${battle.battleScore} ${battle.postBattleLoot.length} ${+scene.abilityBar.isVisible()} ${participants.length} ${participants[0]??0} ${participants[1]??0}`);
  rows.push(`MODS ${scene.modifiers.length+scene.enemyModifiers.length}`,...modRows());
  rows.push(...slots.flatMap((p,b)=>[`O ${b} ${snapshot(p).join(" ")}`,`OX ${b} ${extras(p).join(" ")}`]),`RO ${stateRng(Phaser.Math.RND.state())}`,`BO ${stateRng(battle.battleSeedState)}`);
  const queue:string[]=[];
  const emit=(scope:string,level:number,j:number,p:any)=>{
   const name=p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName;
   const text=p.phaseName==="MessagePhase"?Buffer.from(p.text,"utf8").toString("hex"):"-";
   const stages=p.phaseName==="StatStageChangePhase"?p.options.changes.map((x:any)=>`${x.stat}:${x.stages}`).join(",")||"-":"-";
   queue.push(`Q ${scope} ${level} ${j} ${name} ${p.battlerIndex??-1} ${p.phaseName==="ShowAbilityPhase"?+p.passive:-1} ${text} ${stages}`);
   const meta=p.phaseName==="SwitchPhase"?[p.switchType,p.fieldIndex,+p.isModal,+p.doReturn]:p.phaseName==="DamageAnimPhase"?[p.amount,p.damageResult,+p.critical,-1]:p.phaseName==="FaintPhase"?[+p.preventInstantRevive,p.source?slots.indexOf(p.source):-1,0,-1]:p.phaseName==="StatStageChangePhase"?[p.options.sourcePokemon?slots.indexOf(p.options.sourcePokemon):-1,+!!p.options.ignoreAbilities,+p.selfTarget,-1]:[-1,-1,-1,-1];
   queue.push(`P ${meta.join(" ")}`);
  };
  pm.phaseQueue.levels.forEach((lv:any[],level:number)=>lv.forEach((p:any,j:number)=>emit("L",level,j,p)));
  [...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a.localeCompare(b)).forEach(([,q]:any,k:number)=>q.queue.forEach((p:any,j:number)=>emit("D",k,j,p)));
  rows.push(`QCOUNT ${queue.length/2} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...queue,"E");
 }
 if(breadth.some(n=>!n)||outcomes.some(n=>!n)||[0,2,3,5].some(n=>!bosses[n])||!shields[1]||!faints[1])throw Error("missing branch breadth");
 console.log(`FLAMEBURST breadth=${breadth} outcomes=${outcomes} bosses=${bosses} shields=${shields} faints=${faints}`);
 rows.push(`FLAMEBURST_COMPLETE ${N}`);fs.writeFileSync(process.env.FLAMEBURST_OUT??"/tmp/flame-burst.txt",rows.join("\n")+"\n");
},600000);
