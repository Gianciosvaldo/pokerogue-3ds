/* Native attention owning leaves, tag lifecycle and MovePhase.resolveRedirectTarget. */
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
import { MovePhase } from "#phases/move-phase";
import { MoveUseMode } from "#enums/move-use-mode";
import { BattlerTagLapseType } from "#enums/battler-tag-lapse-type";
import { PokemonType } from "#enums/pokemon-type";
import { CenterOfAttentionTag } from "#data/battler-tags";
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

it("attention owning leaves, redirection and tag lifecycle",async()=>{
 const N=Number(process.env.ATTENTION_N??16384),SEED=Number(process.env.ATTENTION_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<16384)throw Error("Node22 and N>=16384 required");
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager;
 const player:any[]=[...scene.getPlayerField()],enemy:any[]=[...scene.getEnemyField()];if(player.length!==2||enemy.length!==2)throw Error("natural double startup required");
 for(let k=2;k<6;k++){player.push(scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30));enemy.push(scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30,TrainerSlot.NONE));}
 const actors=[...player,...enemy],ids=actors.map(p=>p.id),moves=[MoveId.FOLLOW_ME,MoveId.RAGE_POWDER,MoveId.SPOTLIGHT];
 const attrs=moves.map((m,k)=>allMoves[m].getAttrs("AddBattlerTagAttr")[0]);if(attrs.some(x=>!x))throw Error("actual owning attributes required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"setFrameRate").mockImplementation(()=>{});}
 vi.spyOn(CommonBattleAnim.prototype,"play").mockImplementation((_reverse:any,done:any)=>{done?.();});
 vi.spyOn(scene.abilityBar,"showAbility").mockImplementation(async()=>{scene.abilityBar.setVisible(true);});
 vi.spyOn(scene.abilityBar,"hide").mockImplementation(async()=>{scene.abilityBar.setVisible(false);});
 const tagTypes=[BattlerTagType.PROTECTED];
 const extras=(p:any)=>{const t=p.getTag(CenterOfAttentionTag);return [+!!p.getTag(BattlerTagType.PROTECTED),t?1:0,t?.turnCount??0,t?.sourceMove??0,t?.sourceId??-1,t?+t.powder:0];};
 const abilities=[AbilityId.NONE,AbilityId.BALL_FETCH,AbilityId.LIGHTNING_ROD,AbilityId.STORM_DRAIN,AbilityId.OVERCOAT,AbilityId.STALWART,AbilityId.PROPELLER_TAIL];
 const redirectMoves=[MoveId.TACKLE,MoveId.THUNDER_SHOCK,MoveId.WATER_GUN,MoveId.SNIPE_SHOT,MoveId.EARTHQUAKE,MoveId.HELPING_HAND,MoveId.FOLLOW_ME,MoveId.RAGE_POWDER,MoveId.SPOTLIGHT,MoveId.SPLASH,MoveId.SURF,MoveId.HEAL_PULSE,MoveId.TEARFUL_LOOK];
 const fd=fs.openSync(process.env.ATTENTION_OUT??"/tmp/attention.txt","w");const write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`ATTENTION 1 ${N+384} ${SEED}`]);
 const outcomes=[[0,0],[0,0],[0,0]];let changed=0,tagsAdded=0,tagsRemoved=0;
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
 for(let i=0;i<N+384;i++){
  const extra=i>=N,q=i-N;
  const mode=i%3,double=extra?true:Math.floor(i/3)%4!==0,n0=extra?6:1+rnd(6),n1=extra?6:1+rnd(6),side=extra?q%2:rnd(2),user=extra?side*6:(side?6:0)+rnd(side?n1:n0),target=extra?(1-side)*6:rnd(12),redirectMove=extra?[MoveId.FIRE_PLEDGE,MoveId.WATER_PLEDGE,MoveId.GRASS_PLEDGE][q%3]:redirectMoves[rnd(redirectMoves.length)],lapse=[BattlerTagLapseType.TURN_END,BattlerTagLapseType.AFTER_MOVE,BattlerTagLapseType.FAINT,BattlerTagLapseType.CUSTOM][rnd(4)];
  battle.double=double;scene.getPlayerParty().splice(0,scene.getPlayerParty().length,...player.slice(0,n0));battle.enemyParty=enemy.slice(0,n1);
  for(let k=0;k<12;k++){
   const p=actors[k];p.id=ids[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);p.formIndex=0;p.fusionSpecies=null;p.fusionCustomPokemonData=null;p.bossSegments=0;p.bossSegmentIndex=0;
   p.customPokemonData.ability=abilities[rnd(abilities.length)];p.customPokemonData.passive=abilities[rnd(abilities.length)];p.passive=!!rnd(2);p.summonData.abilitySuppressed=!!rnd(2);
   p.level=30+rnd(21);p.nature=rnd(25);p.ivs=Array.from({length:6},()=>rnd(32));p.friendship=70;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.status=null;p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=rnd(5)?p.getMaxHp():0;p.generateName();
   p.stats[Stat.SPD]=i%7===0?100:1+rnd(200);p.summonData.types=[rnd(3)===0?PokemonType.GRASS:PokemonType.NORMAL];
   if(scene.field.getIndex(p)>=0)scene.field.remove(p);
  }
  for(const p of [...scene.getPlayerField(),...scene.getEnemyField()])if(rnd(6))scene.field.add(p);
  for(const p of actors){if(rnd(5)===0)p.addTag(BattlerTagType.PROTECTED,1,MoveId.PROTECT,p.id);p.switchOutStatus=rnd(5)===0;}
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  const gas=rnd(3);if(gas){const tag:any=new SuppressAbilitiesTag(actors[user].id);tag.sourceCount=gas;scene.arena.tags=[tag];}
  const trick=i%5===0;if(trick)scene.arena.addTag(ArenaTagType.TRICK_ROOM,5,MoveId.TRICK_ROOM,actors[user].id);
  for(const party of [player,enemy])if(rnd(2)){const p=party[rnd(2)];p.addTag(BattlerTagType.CENTER_OF_ATTENTION,9,moves[rnd(3)],actors[user].id);}
  if(i%19===0)for(const p of [player[0],player[1],enemy[0],enemy[1]]){p.summonData.tags=p.summonData.tags.filter((t:any)=>t.tagType!==BattlerTagType.CENTER_OF_ATTENTION);p.summonData.tags.push(new CenterOfAttentionTag(moves[rnd(3)]));}
  if(extra){
   for(const p of actors){p.summonData.tags=[];p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.summonData.abilitySuppressed=false;p.hp=p.getMaxHp();p.switchOutStatus=false;p.summonData.types=[PokemonType.NORMAL];}
   for(const p of [...scene.getPlayerField(),...scene.getEnemyField()])if(scene.field.getIndex(p)<0)scene.field.add(p);
   actors[(1-side)*6+1].summonData.tags.push(new CenterOfAttentionTag(MoveId.FOLLOW_ME));
  }
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();scene.abilityBar.setVisible(false);battle.lastPlayerInvolved=-1;battle.lastEnemyInvolved=-1;
  battle.battleSeed=`attention-${SEED}-${i}`;battle.battleSeedState=null;battle.turn=1;scene.waveSeed=`attention-wave-${SEED}`;Phaser.Math.RND.sow([`attention-global-${SEED}-${i}`]);battle.randSeedInt(2);
  const targets=extra?[(1-side)*2]:Array.from({length:i%9===0?0:i%9===1?2:1},()=>rnd(5)-1);
  const rows=[`C ${i} ${mode} ${user} ${target} ${+double} ${n0} ${n1} ${redirectMove} ${lapse}`,`G ${gas} ${actors[user].id} ${+trick}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...frame("I")];
  const result=attrs[mode].apply(actors[user],actors[target],allMoves[moves[mode]],[]);outcomes[mode][+result]++;
  if(result)tagsAdded++;
  rows.push(`RESULT ${+result}`,...frame("O"),...queue());
  const phase:any=new MovePhase(actors[user],targets.slice(),new PokemonMove(redirectMove),MoveUseMode.NORMAL);
  rows.push(`TARGETS ${targets.length} ${[...targets,...Array(4-targets.length).fill(-2)].join(" ")}`);
  phase.resolveRedirectTarget();if(JSON.stringify(targets)!==JSON.stringify(phase.targets))changed++;
  rows.push(`TARGETS_OUT ${phase.targets.length} ${[...phase.targets,...Array(4-phase.targets.length).fill(-2)].join(" ")}`,...frame("P"),...queue());
  for(const p of actors){const before=!!p.getTag(CenterOfAttentionTag);p.lapseTags(lapse);if(before&&!p.getTag(CenterOfAttentionTag))tagsRemoved++;}
  rows.push(...frame("Z"),...queue(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);vi.clearAllMocks();
 }
 if(outcomes.some(a=>a.some(n=>!n))||!changed||!tagsAdded||!tagsRemoved)throw Error("branch breadth missing");
 write([`ATTENTION_COMPLETE ${N+384}`]);fs.closeSync(fd);console.log(`ATTENTION outcomes=${JSON.stringify(outcomes)} redirects=${changed} added=${tagsAdded} removed=${tagsRemoved}`);
},600000);
