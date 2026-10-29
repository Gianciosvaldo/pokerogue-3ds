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

it("native Court Change Tailwind callbacks with retained source",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,pm:any=s.phaseManager,players:any[]=[...s.getPlayerField()],enemies:any[]=[...s.getEnemyField()];
 players.push(s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30));enemies.push(s.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30,TrainerSlot.NONE));
 const actors=[...players,...enemies],move=allMoves[MoveId.COURT_CHANGE],attr=move.getAttrs("SwapArenaTagsAttr")[0];expect(attr).toBeTruthy();expect(vi.isMockFunction(attr.apply)).toBe(false);
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockResolvedValue(undefined);vi.spyOn(p,"loadAssets").mockResolvedValue(undefined);vi.spyOn(p,"setFrameRate").mockImplementation(()=>{});}
 const rows:string[]=["TAILWIND_OWNER 1 2048",`W ${s.waveSeed}`],rng=(k:string,v:string)=>`${k} ${v.replace(/^!rnd,/,"").replaceAll(","," ")}`,hex=(v:string)=>v?Buffer.from(v).toString("hex"):"-";
 const frame=(prefix:string)=>actors.flatMap((p:any,k:number)=>{const t=p.getTag(BattlerTagType.CHARGED);return [`${prefix} ${k} ${snapshot(p).join(" ")}`,`${prefix}X ${k} ${+!!t} ${t?.turnCount??0} ${t?.sourceMove??0} ${t?.sourceId??-1}`];});
 const queue=()=>{const out:string[]=[];const emit=(scope:string,l:number,j:number,p:any)=>{
  const name=p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName;
  const changes=p.phaseName==="StatStageChangePhase"?p.options.changes.map((v:any)=>`${v.stat}:${v.stages}`).join(","):"-";
  out.push(`Q ${scope} ${l} ${j} ${name} ${p.battlerIndex??-1} ${p.phaseName==="ShowAbilityPhase"?+p.passive:-1} ${p.phaseName==="ShowAbilityPhase"?(allAbilities.find((a:any)=>a?.name===p.abilityName)?.id??-1):-1} ${p.phaseName==="MessagePhase"?hex(p.text):"-"} ${changes} ${p.phaseName==="StatStageChangePhase"?actors.indexOf(p.options.sourcePokemon):-1}`);
 };
 pm.phaseQueue.levels.forEach((q:any[],l:number)=>q.forEach((p:any,j:number)=>emit("L",l,j,p)));
 [...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a.localeCompare(b)).forEach(([,q]:any,l:number)=>q.queue.forEach((p:any,j:number)=>emit("D",l,j,p)));
 return [`QCOUNT ${out.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...out];};
 let i=0;
 for(const sourceSide of [0,1])for(const tagSide of [0,1])for(const shape of [0,1,2,3])for(const mode of [0,1])for(const ability of [0,1,2,3])for(const charged of [0,1])for(const tie of [0,1])for(const stage of [-6,0,5,6]){
  const order=[0,1,2,3,4,5];if(shape===1||shape===3){const n=sourceSide*3;[order[n],order[n+1],order[n+2]]=[n+1,n+2,n];}
  s.party=order.slice(0,3).map(k=>actors[k]);s.currentBattle.enemyParty=order.slice(3).map(k=>actors[k]);s.modifiers=[];s.enemyModifiers=[];s.arena.tags=[];s.arena.weather=null;s.arena.terrain=null;
  for(let k=0;k<6;k++){
   const p=actors[k];p.id=101+k;p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.level=30;p.nature=0;p.customPokemonData.nature=-1;p.ivs=[31,31,31,31,31,31];p.gender=0;p.shiny=false;p.variant=0;p.teraType=0;p.fusionSpecies=null;p.formIndex=0;p.status=null;p.friendship=70;p.passive=false;p.customPokemonData.ability=AbilityId.NONE;p.customPokemonData.passive=AbilityId.NONE;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=p.getMaxHp();p.stats[Stat.SPD]=tie?100:10+k*10;p.setStatStage(Stat.ATK,stage);p.bossSegments=0;p.bossSegmentIndex=0;
   if(s.field.getIndex(p)>=0)s.field.remove(p);
  }
  const field=[s.party[0],s.party[1],s.currentBattle.enemyParty[0],s.currentBattle.enemyParty[1]];
  for(let b=0;b<4;b++){const p=field[b];s.field.add(p);const a=ability===0?AbilityId.NONE:ability===3?AbilityId.WIND_RIDER:(b%2?AbilityId.WIND_RIDER:AbilityId.WIND_POWER);if(ability===2){p.passive=true;p.customPokemonData.passive=a;}else p.customPokemonData.ability=a;if(charged)p.addTag(BattlerTagType.CHARGED);}
  if(shape===3)actors[sourceSide*3].hp=0;
  const sourceId=shape===2?999999:actors[sourceSide*3].id;
  if(mode)s.arena.tags=[getArenaTag(ArenaTagType.TAILWIND,4,MoveId.TAILWIND,sourceId,[ArenaTagSide.PLAYER,ArenaTagSide.ENEMY][tagSide])];
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;Phaser.Math.RND.sow([`tailwind-owner-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  rows.push(`C ${i} ${sourceSide} ${tagSide} ${shape} ${mode} ${ability} ${charged} ${tie} ${stage}`,`P ${order.join(" ")}`,rng("R",Phaser.Math.RND.state()),rng("B",s.currentBattle.battleSeedState),...frame("I"),`ITAGS ${s.arena.tags.length}`,...s.arena.tags.map((t:any)=>`ITAG ${Object.values(ArenaTagType).indexOf(t.tagType)} ${t.side} ${t.turnCount} ${t.maxDuration} ${t.sourceMove??0} ${t.sourceId??-1} ${t.layers??1}`));
  if(mode)expect(attr.apply(field[0],field[0],move,[])).toBe(true);else s.arena.addTag(ArenaTagType.TAILWIND,4,MoveId.TAILWIND,sourceId,[ArenaTagSide.PLAYER,ArenaTagSide.ENEMY][tagSide],true);
  rows.push(...frame("O"),`TAGS ${s.arena.tags.length}`,...s.arena.tags.map((t:any)=>`TAG ${Object.values(ArenaTagType).indexOf(t.tagType)} ${t.side} ${t.turnCount} ${t.maxDuration} ${t.sourceMove??0} ${t.sourceId??-1} ${t.layers??1}`),rng("RO",Phaser.Math.RND.state()),rng("BO",s.currentBattle.battleSeedState),...queue(),"E");i++;
 }
 expect(i).toBe(2048);
 fs.writeFileSync(process.env.TAILWIND_OWNER_LOCALES_OUT!,gzipSync(JSON.stringify(Object.fromEntries(["battle","pokemon-info","battler-tags","arena-tag","ability-trigger","move-trigger","pokemon","ability","move"].map(name=>[name,fs.readFileSync(`locales/en/${name}.json`,"utf8")])))));
 rows.push(`TAILWIND_OWNER_COMPLETE ${i}`);fs.writeFileSync(process.env.TAILWIND_OWNER_OUT!,gzipSync(rows.join("\n")+"\n"));
},240000);
