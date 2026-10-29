import { BattleScene } from "#app/battle-scene";
import { allMoves,allAbilities } from "#data/data-lists";
import { getArenaTag } from "#data/arena-tag";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
import { ArenaTagType } from "#enums/arena-tag-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerSlot } from "#enums/trainer-slot";
import { Stat } from "#enums/stat";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { gzipSync } from "node:zlib";
import { expect,it,vi } from "vitest";
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

it("native arena storage through public owners",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,pm:any=s.phaseManager,players:any[]=[...s.getPlayerField()],enemies:any[]=[...s.getEnemyField()];
 players.push(s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30));enemies.push(s.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30,TrainerSlot.NONE));
 const actors=[...players,...enemies],move=allMoves[MoveId.COURT_CHANGE],attr=move.getAttrs("SwapArenaTagsAttr")[0];expect(attr).toBeTruthy();expect(vi.isMockFunction(attr.apply)).toBe(false);
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockResolvedValue(undefined);vi.spyOn(p,"loadAssets").mockResolvedValue(undefined);vi.spyOn(p,"setFrameRate").mockImplementation(()=>{});}
 const rows:string[]=["ARENA_CAPACITY 1 32",`W ${s.waveSeed}`],rng=(k:string,v:string)=>`${k} ${v.replace(/^!rnd,/,"").replaceAll(","," ")}`,hex=(v:string)=>v?Buffer.from(v).toString("hex"):"-";
 const frame=(prefix:string)=>actors.flatMap((p:any,k:number)=>{const t=p.getTag(BattlerTagType.CHARGED);return [`${prefix} ${k} ${snapshot(p).join(" ")}`,`${prefix}X ${k} ${+!!t} ${t?.turnCount??0} ${t?.sourceMove??0} ${t?.sourceId??-1}`];});
 const queue=()=>{const out:string[]=[];const emit=(scope:string,l:number,j:number,p:any)=>{
  const name=p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName;
  const changes=p.phaseName==="StatStageChangePhase"?p.options.changes.map((v:any)=>`${v.stat}:${v.stages}`).join(","):"-";
  out.push(`Q ${scope} ${l} ${j} ${name} ${p.battlerIndex??-1} ${p.phaseName==="ShowAbilityPhase"?+p.passive:-1} ${p.phaseName==="ShowAbilityPhase"?(allAbilities.find((a:any)=>a?.name===p.abilityName)?.id??-1):-1} ${p.phaseName==="MessagePhase"?hex(p.text):"-"} ${changes} ${p.phaseName==="StatStageChangePhase"?actors.indexOf(p.options.sourcePokemon):-1}`);
 };
 pm.phaseQueue.levels.forEach((q:any[],l:number)=>q.forEach((p:any,j:number)=>emit("L",l,j,p)));
 [...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a.localeCompare(b)).forEach(([,q]:any,l:number)=>q.queue.forEach((p:any,j:number)=>emit("D",l,j,p)));
 return [`QCOUNT ${out.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...out];};
 const names=Object.values(ArenaTagType),types=names.filter(t=>t!==ArenaTagType.NONE);
 const shared=types.filter(t=>getArenaTag(t,5,MoveId.SPLASH,101,ArenaTagSide.PLAYER)!.side===ArenaTagSide.BOTH);
 expect(types.length).toBe(28);expect(shared.length).toBe(8);
 const operations=(order:number,sideOrder:number)=>{
  let ts=[...types];if(order===1)ts.reverse();if(order===2)ts=[...ts.slice(14),...ts.slice(0,14)];if(order===3)ts.sort((a,b)=>((names.indexOf(a)*11)%29)-((names.indexOf(b)*11)%29));
  return ts.flatMap(t=>(shared.includes(t)?[ArenaTagSide.BOTH]:(sideOrder?[ArenaTagSide.ENEMY,ArenaTagSide.PLAYER]:[ArenaTagSide.PLAYER,ArenaTagSide.ENEMY])).map(side=>({t,side})));
 };
 const tags=()=>[`TAGS ${s.arena.tags.length}`,...s.arena.tags.map((t:any)=>`TAG ${names.indexOf(t.tagType)} ${t.side} ${t.turnCount} ${t.maxDuration} ${t.sourceMove??0} ${t.sourceId??-1} ${t.layers??1} ${t.sourceCount??0} ${+!!t.beingRemoved} ${t.pendingHeals?.length??0}`)];
 const battlers=()=>actors.flatMap((p:any,k:number)=>[`BTAGS ${k} ${p.summonData.tags.length}`,...p.summonData.tags.map((t:any)=>`BTAG ${Object.values(BattlerTagType).indexOf(t.tagType)} ${t.turnCount} ${t.sourceMove??0} ${t.sourceId??-1}`)]);
 let i=0;
 for(const order of [0,1,2,3])for(const sideOrder of [0,1])for(const mode of [0,1,2,3]){
  s.party=players;s.currentBattle.enemyParty=enemies;s.modifiers=[];s.enemyModifiers=[];s.arena.tags=[];s.arena.weather=null;s.arena.terrain=null;
  for(let k=0;k<6;k++){
   const p=actors[k];p.id=101+k;p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.level=30;p.nature=0;p.customPokemonData.nature=-1;p.ivs=[31,31,31,31,31,31];p.gender=0;p.shiny=false;p.variant=0;p.teraType=0;p.fusionSpecies=null;p.formIndex=0;p.status=null;p.friendship=70;p.passive=false;p.customPokemonData.ability=AbilityId.NONE;p.customPokemonData.passive=AbilityId.NONE;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=p.getMaxHp();p.stats[Stat.SPD]=10+k*10;p.bossSegments=0;p.bossSegmentIndex=0;if(s.field.getIndex(p)>=0)s.field.remove(p);
  }
  for(const p of [players[0],players[1],enemies[0],enemies[1]])s.field.add(p);
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;Phaser.Math.RND.sow([`arena-capacity-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  rows.push(`C ${i} ${order} ${sideOrder} ${mode}`,rng('R',Phaser.Math.RND.state()),rng('B',s.currentBattle.battleSeedState??'!rnd,1,0,0,0'),...frame('I'));
  const ops=operations(order,sideOrder);for(const op of [...ops]){if(op.t===ArenaTagType.SPIKES)ops.push(op,op);if(op.t===ArenaTagType.TOXIC_SPIKES)ops.push(op);}
  expect(ops.length).toBe(54);
  rows.push(`OPS ${ops.length}`);
  const seenKeys=new Set<string>();
  for(const {t,side} of ops){const source=side===ArenaTagSide.ENEMY?104:101;const result=s.arena.addTag(t,5,MoveId.SPLASH,source,side,true);rows.push(`ADD ${names.indexOf(t)} ${side} ${source} ${+result}`);expect(result).toBe(!seenKeys.has(`${t}:${side}`));seenKeys.add(`${t}:${side}`);}
  expect(s.arena.tags.length).toBe(48);rows.push(...tags());
  if(mode===1)expect(attr.apply(actors[0],actors[0],move,[])).toBe(true);
  if(mode===2)s.arena.removeTagsOnSide(types,ArenaTagSide.PLAYER,true);
  if(mode===3)s.arena.removeAllTags();
  rows.push(...frame('O'),...battlers(),...tags(),rng('RO',Phaser.Math.RND.state()),rng('BO',s.currentBattle.battleSeedState??'!rnd,1,0,0,0'),...queue(),'E');i++;
 }
 expect(i).toBe(32);
 fs.writeFileSync(process.env.ARENA_CAPACITY_LOCALES_OUT!,gzipSync(JSON.stringify(Object.fromEntries(['battle','pokemon-info','battler-tags','arena-tag','ability-trigger','move-trigger','pokemon','ability','move'].map(name=>[name,fs.readFileSync(`locales/en/${name}.json`,'utf8')])))));
 rows.push(`ARENA_CAPACITY_COMPLETE ${i}`);fs.writeFileSync(process.env.ARENA_CAPACITY_OUT!,gzipSync(rows.join('\n')+'\n'));
},240000);
