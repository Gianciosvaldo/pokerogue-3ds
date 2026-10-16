/* Actual pinned leaves, queued constructors and native queue/phase execution. */
import * as AbilityCalls from "#abilities/apply-ab-attrs";
import { BattleScene } from "#app/battle-scene";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { getModifierType } from "#utils/modifier-utils";
import { StatStageChangePhase } from "#phases/stat-stage-change-phase";
import { applyMoveAttrs } from "#moves/apply-attrs";
import { allMoves,allAbilities,modifierTypes } from "#data/data-lists";
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

it("native berry move leaves and queued phase execution",async()=>{
 const N=Number(process.env.BERRYMOVES_N??16384),SEED=Number(process.env.BERRYMOVES_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<16384)throw Error("Node22,N>=16384");
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,actors:any[]=scene.getField();if(actors.length!==4||actors.some(p=>!p))throw Error("natural double field");
 const callbackSpy=vi.spyOn(AbilityCalls,"applyAbAttrs");
 const callbackNames=["PostItemLostAbAttr","HealFromBerryUseAbAttr","BlockItemTheftAbAttr","DoubleBerryEffectAbAttr"];
 const ids=actors.map(p=>p.id),moves=[MoveId.STUFF_CHEEKS,MoveId.TEATIME,MoveId.BUG_BITE,MoveId.PLUCK];
 const attrs=moves.map((m,k)=>allMoves[m].getAttrs(k<2?"EatBerryAttr":"StealEatBerryAttr")[0]);if(attrs.some(x=>!x))throw Error("native owning attrs");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});vi.spyOn(pm,"startCurrentPhase").mockImplementation(()=>{});
 vi.spyOn(StatStageChangePhase.prototype as any,"playStatChangeAnimation").mockImplementation(async()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"setFrameRate").mockImplementation(()=>{});}
 vi.spyOn(CommonBattleAnim.prototype,"play").mockImplementation((_reverse:any,done:any)=>{done?.();});
 vi.spyOn(scene.abilityBar,"showAbility").mockImplementation(async()=>{scene.abilityBar.setVisible(true);});vi.spyOn(scene.abilityBar,"hide").mockImplementation(async()=>{scene.abilityBar.setVisible(false);});
 const tags=[BattlerTagType.HEAL_BLOCK,BattlerTagType.CONFUSED,BattlerTagType.NIGHTMARE,BattlerTagType.CRIT_BOOST,BattlerTagType.UNBURDEN];
 const history=(v:number[])=>{if(v.length>8)throw Error("history projection overflow");return[v.length,...Array.from({length:8},(_,k)=>v[k]??0)];};
 const extras=(p:any)=>[tags.reduce((m,t,k)=>m+(p.getTag(t)?1<<k:0),0),...tags.flatMap(t=>{const tag=p.getTag(t);return[tag?.turnCount??0,tag?.sourceMove??0,tag?.sourceId??-1];}),p.status?.toxicTurnCount??-1,p.status?.sleepTurnsRemaining??-1,p.status?.freezeTurnsRemaining??-1,+p.battleData.hasEatenBerry,...history(p.battleData.berriesEaten),...history(p.turnData.berriesEaten),...history(p.summonData.berriesEatenLast)];
 const frame=(prefix:string)=>actors.flatMap((p,k)=>[`${prefix} ${k} ${snapshot(p).join(" ")}`,`${prefix}X ${k} ${extras(p).join(" ")}`]);
 const inventory=(prefix:string)=>[scene.modifiers,scene.enemyModifiers].flatMap((list:any[],side:number)=>[`${prefix} ${side} ${list.length}`,...list.map(m=>`M ${m.type.id} ${actors.findIndex(p=>p.id===m.pokemonId)} ${m.berryType??-1} ${m.stackCount} ${m.virtualStackCount} ${+(m.isTransferable??false)}`)]);
 const queue=()=>{const out:string[]=[];const emit=(scope:string,level:number,j:number,p:any)=>{
  const name=p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName;let person=-1,a=-1,b=-1,c=-1,d=-1,text="-";
  if(p.phaseName==="ResetStatusPhase"){person=actors.indexOf(p.pokemon);a=+p.affectConfusion;b=+p.reloadAssets;}
  if(p.phaseName==="PokemonHealPhase"){a=p.hpHealed;b=(p.message?1:0)+(!p.showFullHpMessage?2:0)+(p.skipAnim?4:0)+(p.revive?8:0)+(p.healStatus?16:0)+(p.preventFullHeal?32:0)+(p.fullRestorePP?64:0);text=hex(p.message??"");}
  if(p.phaseName==="StatStageChangePhase"){const o=p.options;if(o.changes.length!==1)throw Error("unexpected native stat request count");person=actors.indexOf(o.sourcePokemon);a=o.changes[0].stat;b=o.changes[0].stages;c=o.changes.length;d=(o.message?1:0)+(+!!o.ignoreAbilities)*2+(+!!o.processed)*4+(+p.selfTarget)*8;if(o.sourceEffectType!=null)throw Error("unexpected stat source type");}
  if(p.phaseName==="ShowAbilityPhase"){a=+p.passive;b=allAbilities.find(x=>x?.name===p.abilityName)?.id??-1;c=+p.pokemonOnField;}
  if(p.phaseName==="MessagePhase")text=hex(p.text);if(p.phaseName==="CommonAnimPhase"){a=p.anim;b=p.targetIndex??-1;}
  out.push(`Q ${scope} ${level} ${j} ${name} ${p.battlerIndex??-1} ${person} ${a} ${b} ${c} ${d} ${text}`);
 };
 pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,j:number)=>emit("L",l,j,p)));
 [...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a.localeCompare(b)).forEach(([,q]:any,k:number)=>q.queue.forEach((p:any,j:number)=>emit("D",k,j,p)));
 return[`QCOUNT ${out.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...out];};
 const abilities=[AbilityId.NONE,AbilityId.STICKY_HOLD,AbilityId.UNBURDEN,AbilityId.RIPEN,AbilityId.CHEEK_POUCH,AbilityId.SIMPLE,AbilityId.CONTRARY];
 const fd=fs.openSync(process.env.BERRYMOVES_OUT??"/tmp/berry-moves.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`BERRYMOVES 3 ${N+512} ${SEED}`]);
 const outcomes=Array.from({length:4},()=>[0,0]),eaten=Array(11).fill(0);let runsTotal=0,virtualSeen=0;
 for(let i=0;i<N+512;i++){
  const mode=i%4,call=Math.floor(i/16)%2,user=Math.floor(i/4)%4,target=mode===0&&i%29!==0?user:(user+Math.floor(i/32)%4)%4,owner=mode===0?user:target;
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;pm.clearAllPhases();
  for(let k=0;k<4;k++){
   const p=actors[k];p.id=ids[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.customPokemonData.ability=abilities[rnd(abilities.length)];p.customPokemonData.passive=abilities[rnd(abilities.length)];p.passive=!!rnd(2);p.summonData.abilitySuppressed=!!rnd(2);p.level=5+rnd(46);p.nature=rnd(25);p.ivs=Array.from({length:6},()=>rnd(32));p.friendship=70;p.switchOutStatus=false;p.status=null;p.stats=Array(6).fill(0);p.hp=undefined;
   p.moveset=[MoveId.SPLASH,MoveId.TACKLE,MoveId.GROWL,MoveId.EMBER].map(m=>new PokemonMove(m));for(const m of p.moveset){m.ppUp=rnd(4);m.ppUsed=rnd(m.getMovePp()+1);}if(i%3===0){const full=p.moveset[rnd(4)];full.ppUsed=full.getMovePp();}
   p.calculateStats();p.hp=[0,1,Math.max(1,Math.floor(p.getMaxHp()/2)),p.getMaxHp()][rnd(4)];const status=rnd(8);p.status=status?new Status(status,1,2,3):null;if(status===StatusEffect.FAINT)p.hp=0;
   for(const stat of [Stat.ATK,Stat.DEF,Stat.SPATK,Stat.SPDEF,Stat.SPD,Stat.ACC,Stat.EVA])p.setStatStage(stat,[-6,0,5,6][rnd(4)]);
   for(let t=0;t<5;t++)if(rnd(6)===0)p.addTag(tags[t],t===1?3:0,MoveId.SPLASH,p.id);
   p.battleData.hasEatenBerry=!!rnd(2);p.battleData.berriesEaten=Array.from({length:rnd(5)},()=>rnd(11));p.turnData.berriesEaten=Array.from({length:rnd(3)},()=>rnd(11));p.summonData.berriesEatenLast=Array.from({length:rnd(3)},()=>rnd(11));p.generateName();
  }
  if(i%101===0)actors[Math.floor(i/101)%4].id=0;
  const addBerry=(role:number,type:number)=>{const t:any=generateModifierType(modifierTypes.BERRY,[type]);if(!t)throw Error("native berry factory");const m:any=t.newModifier(actors[role]);m.stackCount=1+rnd(m.getMaxStackCount());m.isTransferable=i%7!==0;(role<2?scene.modifiers:scene.enemyModifiers).push(m);};
  if(i%17!==0)for(let j=0,n=1+rnd(5);j<n;j++)addBerry(owner,(Math.floor(i/64)+j)%11);
  for(let k=0;k<4;k++)if(k!==owner&&rnd(3)===0)addBerry(k,rnd(11));
  for(let side=0;side<2;side++){const stacks=rnd(4);if(stacks){const t:any=getModifierType(modifierTypes.BERRY_POUCH),m=t.newModifier();m.stackCount=stacks;(side?scene.enemyModifiers:scene.modifiers).push(m);}}
  scene.updateModifiers(true,true);scene.updateModifiers(false,true);
  if(i>=N)for(const m of [...scene.modifiers,...scene.enemyModifiers])if(m.stackCount<m.getMaxStackCount()){m.virtualStackCount=1;virtualSeen++;}
  const gas=rnd(3);if(gas){const tag:any=new SuppressAbilitiesTag(actors[user].id);tag.sourceCount=gas;scene.arena.tags=[tag];}
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();scene.abilityBar.setVisible(false);battle.lastPlayerInvolved=-1;battle.lastEnemyInvolved=-1;
  battle.battleSeed=`berry-moves-${SEED}-${i}`;battle.battleSeedState=null;battle.turn=1;scene.waveSeed=`berry-moves-wave-${SEED}`;Phaser.Math.RND.sow([`berry-moves-global-${SEED}-${i}`]);battle.randSeedInt(2);
  const rows=[`C ${i} ${mode} ${call} ${user} ${target}`,`G ${gas} ${actors[user].id}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...frame("I"),...inventory("MI")];
  const stuff=allMoves[MoveId.STUFF_CHEEKS],selection=actors[user].isMoveSelectable(MoveId.STUFF_CHEEKS);if(stuff.conditionsSeq3.length!==1||stuff.restrictions.length!==1)throw Error("native Stuff Cheeks closure shape");
  rows.push(`CHECK ${+selection[0]} ${+stuff.conditionsSeq3[0].apply(actors[user],actors[target],stuff)} ${+stuff.restrictions[0].func(actors[user],stuff)} ${hex(selection[1])}`);
  callbackSpy.mockClear();let result:any=null;if(call===0)result=attrs[mode].apply(actors[user],actors[target],allMoves[moves[mode]],[]);else applyMoveAttrs(mode<2?"EatBerryAttr":"StealEatBerryAttr",actors[user],actors[target],allMoves[moves[mode]]);
  if(result!=null)outcomes[mode][+result]++;if(result&&attrs[mode].chosenBerry)eaten[attrs[mode].chosenBerry.berryType]++;
  const calls=callbackSpy.mock.calls.filter(([name]:any)=>callbackNames.includes(name)).map(([name,params]:any)=>[callbackNames.indexOf(name),actors.indexOf(params.pokemon)]);if(calls.some(c=>c[1]<0))throw Error("native callback identity");
  rows.push(`RESULT ${result==null?-1:+result}`,`CALLS ${calls.length}`,...calls.map(c=>`AB ${c.join(" ")}`),...frame("O"),...inventory("MO"),...queue());
  const texts:string[]=[];let runs=0;
  for(;;){let p=pm.phaseQueue.getNextPhase();if(!p)break;if(p.phaseName==="DynamicPhaseMarker")p=pm.dynamicQueueManager.popNextPhase(p.phaseType);if(!p)throw Error("empty dynamic marker");if(++runs>128)throw Error("phase bound");if(p.phaseName==="MessagePhase")texts.push(hex(p.text));else{pm.currentPhase=p;p.start();for(let t=0;t<4;t++)await Promise.resolve();}}
  runsTotal+=runs;rows.push(...frame("Z"),...inventory("MZ"),`WORLD ${+scene.abilityBar.isVisible()} ${battle.lastPlayerInvolved??-1} ${battle.lastEnemyInvolved??-1}`,`TEXTS ${texts.length}`,...texts.map(t=>`T ${t}`),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);vi.clearAllMocks();
 }
 if(outcomes.some(a=>a.some(n=>!n))||eaten.some(n=>!n)||!runsTotal||virtualSeen<64)throw Error("native berry-move coverage missing");write([`BERRYMOVES_COMPLETE ${N+512}`]);fs.closeSync(fd);console.log(`BERRYMOVES outcomes=${JSON.stringify(outcomes)} berries=${eaten} phases=${runsTotal} virtual=${virtualSeen}`);
},600000);
