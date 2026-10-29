import { MoveUseMode } from "#enums/move-use-mode";
import { MoveResult } from "#enums/move-result";
import { PokemonType } from "#enums/pokemon-type";
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

it("native permanent maximum HP move owners",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,pm:any=s.phaseManager,players:any[]=[...s.getPlayerField()],enemies:any[]=[...s.getEnemyField()];
 players.push(s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30));enemies.push(s.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30,TrainerSlot.NONE));
 const actors=[...players,...enemies],move=allMoves[MoveId.COURT_CHANGE],attr=move.getAttrs("SwapArenaTagsAttr")[0];expect(attr).toBeTruthy();expect(vi.isMockFunction(attr.apply)).toBe(false);
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockResolvedValue(undefined);vi.spyOn(p,"loadAssets").mockResolvedValue(undefined);vi.spyOn(p,"setFrameRate").mockImplementation(()=>{});}
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 const rows:string[]=["MAX_HP_MOVES 1 1344",`W ${s.waveSeed}`],rng=(k:string,v:string)=>`${k} ${v.replace(/^!rnd,/,"").replaceAll(","," ")}`,hex=(v:string)=>v?Buffer.from(v).toString("hex"):"-";
 const frame=(prefix:string)=>actors.flatMap((p:any,k:number)=>{const t=p.getTag(BattlerTagType.CHARGED);return [`${prefix} ${k} ${snapshot(p).join(" ")}`,`${prefix}X ${k} ${+!!t} ${t?.turnCount??0} ${t?.sourceMove??0} ${t?.sourceId??-1}`];});
 const queue=()=>{const out:string[]=[];const emit=(scope:string,l:number,j:number,p:any)=>{
  const name=p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName;
  const changes=p.phaseName==="StatStageChangePhase"?p.options.changes.map((v:any)=>`${v.stat}:${v.stages}`).join(","):"-";
  out.push(`Q ${scope} ${l} ${j} ${name} ${(p.phaseName==="PokemonAnimPhase"||p.phaseName==="ResetStatusPhase")?p.pokemon.getBattlerIndex():p.battlerIndex??-1} ${p.phaseName==="ShowAbilityPhase"?+p.passive:-1} ${p.phaseName==="ShowAbilityPhase"?(allAbilities.find((a:any)=>a?.name===p.abilityName)?.id??-1):-1} ${p.phaseName==="MessagePhase"?hex(p.text):"-"} ${changes} ${p.phaseName==="StatStageChangePhase"?actors.indexOf(p.options.sourcePokemon):-1}`);
 };
 pm.phaseQueue.levels.forEach((q:any[],l:number)=>q.forEach((p:any,j:number)=>emit("L",l,j,p)));
 [...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a.localeCompare(b)).forEach(([,q]:any,l:number)=>q.queue.forEach((p:any,j:number)=>emit("D",l,j,p)));
 return [`QCOUNT ${out.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...out];};
 let i=0;
 const species=[SpeciesId.RATTATA,SpeciesId.BLISSEY,SpeciesId.SHEDINJA],levels=[1,50,255,1000000];
 const defs:[[MoveId,string],...Array<[MoveId,string]>]=[[MoveId.STRUGGLE,"RecoilAttr"],[MoveId.FLAME_BURST,"FlameBurstAttr"],[MoveId.STEEL_BEAM,"HalfSacrificialAttr"],[MoveId.REST,"RestAttr"],[MoveId.CURSE,"CurseAttr"],[MoveId.RECOVER,"HealAttr"],[MoveId.MOONLIGHT,"VariableHealAttr"]];
 const subs=()=>actors.map((p:any,k:number)=>{const t=p.getTag(BattlerTagType.SUBSTITUTE);return `SUB ${k} ${+!!t} ${t?.hp??0} ${+!!t?.sourceInFocus} ${t?.sourceMove??0} ${t?.sourceId??-1}`;});
 for(const side of [0,1])for(const slot of [0,1])for(const sp of species)for(const level of levels)for(const override of [0,1,2,3])for(const mode of [0,1,2,3,4,5,6]){
  const u=side*3+slot;s.party=players;s.currentBattle.enemyParty=enemies;s.modifiers=[];s.enemyModifiers=[];s.arena.tags=[];s.arena.weather=null;s.arena.terrain=null;
  for(let k=0;k<6;k++){
   const p=actors[k];p.id=101+k;p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(k===u?sp:SpeciesId.RATTATA);p.formIndex=0;p.level=k===u?level:30;p.nature=0;p.customPokemonData.nature=-1;p.ivs=[31,31,31,31,31,31];p.gender=0;p.shiny=false;p.variant=0;p.teraType=0;p.fusionSpecies=null;p.status=null;p.friendship=70;p.passive=false;p.customPokemonData.ability=AbilityId.NONE;p.customPokemonData.passive=AbilityId.NONE;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=p.getMaxHp();p.stats[Stat.SPD]=10+k*10;p.bossSegments=0;p.bossSegmentIndex=0;p.generateName();if(s.field.getIndex(p)>=0)s.field.remove(p);
  }
  for(const p of [players[0],players[1],enemies[0],enemies[1]])s.field.add(p);
  const user=actors[u],target=actors[(side?0:3)],chosen=allMoves[defs[mode][0]];
  if(mode===4){user.summonData.types=[PokemonType.GHOST];expect(user.getTypes().includes(PokemonType.GHOST)).toBe(true);}
  for(const p of actors)if(override)p.setStat(Stat.HP,[0,1,2*p.getMaxHp()+1,34359738371][override],false);
  if(mode===0)user.pushMoveHistory({move:chosen.id,targets:[target.getBattlerIndex()],useMode:MoveUseMode.NORMAL,result:MoveResult.SUCCESS});
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;Phaser.Math.RND.sow([`substitute-max-hp-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  rows.push(`C ${i} ${side} ${slot} ${sp} ${level} ${override} ${mode}`,rng('R',Phaser.Math.RND.state()),rng('B',s.currentBattle.battleSeedState),...frame('I'));
  const actual=chosen.getAttrs(defs[mode][1] as any)[0];expect(actual).toBeTruthy();expect(vi.isMockFunction(actual.apply)).toBe(false);expect(actual.apply(user,target,chosen,[])).toBe(true);
  rows.push(...frame('O'),rng('RO',Phaser.Math.RND.state()),rng('BO',s.currentBattle.battleSeedState),...queue());
  for(let k=0;k<6;k++){const p=actors[k];rows.push(`TD ${k} ${p.turnData.damageTaken} ${p.turnData.singleHitDamageDealt} ${p.turnData.totalDamageDealt} ${p.turnData.pendingStatus}`);rows.push(`TAGS ${k} ${p.summonData.tags.length}`,...p.summonData.tags.map((t:any)=>`TAG ${Object.values(BattlerTagType).indexOf(t.tagType)} ${t.turnCount} ${t.sourceMove??0} ${t.sourceId??-1}`));}
  const flat=pm.phaseQueue.levels.flatMap((xs:any[],l:number)=>xs.map((p:any,j:number)=>({p,l,j})));rows.push(`PAYLOADS ${flat.length}`);
  for(const {p,l,j} of flat){let payload:any[]=[];
   if(p.phaseName==='DamageAnimPhase')payload=[p.amount,p.damageResult,+p.critical];
   else if(p.phaseName==='PokemonHealPhase')payload=[p.hpHealed,+p.showFullHpMessage,+p.skipAnim,+p.revive,+p.healStatus,+p.preventFullHeal,+p.fullRestorePP,p.message===undefined?'-':hex(p.message)];
   else if(p.phaseName==='ObtainStatusEffectPhase')payload=[p.statusEffect,actors.indexOf(p.sourcePokemon),p.sleepTurnsRemaining??-1,hex(p.statusMessage)];
   else if(p.phaseName==='DynamicPhaseMarker')payload=[p.phaseType];
   else if(p.phaseName==='ResetStatusPhase')payload=[actors.indexOf(p.pokemon),+p.affectConfusion];
   else if(p.phaseName==='MessagePhase')payload=[hex(p.text)];
   else throw Error('Unexpected native phase payload '+p.phaseName);
   rows.push(`PAY ${l} ${j} ${p.phaseName} ${payload.join(' ')}`);
  }
  const statuses=pm.dynamicQueueManager.dynamicPhaseMap.get('ObtainStatusEffectPhase')?.queue??[];rows.push(`STATUSES ${statuses.length}`,...statuses.map((p:any,j:number)=>`S ${j} ${p.battlerIndex} ${p.statusEffect} ${actors.indexOf(p.sourcePokemon)} ${p.sleepTurnsRemaining??-1} ${hex(p.statusMessage)}`));
  const faints=pm.dynamicQueueManager.dynamicPhaseMap.get('FaintPhase')?.queue??[];rows.push(`FAINTS ${faints.length}`,...faints.map((p:any,j:number)=>`F ${j} ${p.battlerIndex} ${+p.preventInstantRevive} ${actors.indexOf(p.source)}`));
  rows.push('E');i++;
 }
 expect(i).toBe(1344);
 fs.writeFileSync(process.env.MAX_HP_MOVES_LOCALES_OUT!,gzipSync(JSON.stringify(Object.fromEntries(['battle','pokemon-info','battler-tags','arena-tag','ability-trigger','move-trigger','pokemon','ability','move','status-effect'].map(name=>[name,fs.readFileSync(`locales/en/${name}.json`,'utf8')])))));
 rows.push(`MAX_HP_MOVES_COMPLETE ${i}`);fs.writeFileSync(process.env.MAX_HP_MOVES_OUT!,gzipSync(rows.join('\n')+'\n'));
},240000);
