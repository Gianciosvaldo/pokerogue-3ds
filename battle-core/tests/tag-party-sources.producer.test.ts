import { PokemonType } from "#enums/pokemon-type";
import { BattleScene } from "#app/battle-scene";
import { allAbilities } from "#data/data-lists";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
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

it("native battler tags with retained party sources",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,pm:any=s.phaseManager,players:any[]=[...s.getPlayerField()],enemies:any[]=[...s.getEnemyField()];
 players.push(s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30));enemies.push(s.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA),30,TrainerSlot.NONE));
 const actors=[...players,...enemies];expect(vi.isMockFunction(actors[0].addTag)).toBe(false);
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of actors){vi.spyOn(p,"updateInfo").mockResolvedValue(undefined);vi.spyOn(p,"loadAssets").mockResolvedValue(undefined);vi.spyOn(p,"setFrameRate").mockImplementation(()=>{});}
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 const rows:string[]=["TAG_PARTY_SOURCES 1 768",`W ${s.waveSeed}`],rng=(k:string,v:string)=>`${k} ${v.replace(/^!rnd,/,"").replaceAll(","," ")}`,hex=(v:string)=>v?Buffer.from(v).toString("hex"):"-";
 const frame=(prefix:string)=>actors.flatMap((p:any,k:number)=>{const t=p.getTag(BattlerTagType.CHARGED);return [`${prefix} ${k} ${snapshot(p).join(" ")}`,`${prefix}X ${k} ${+!!t} ${t?.turnCount??0} ${t?.sourceMove??0} ${t?.sourceId??-1}`];});
 const queue=()=>{const out:string[]=[];const emit=(scope:string,l:number,j:number,p:any)=>{
  const name=p.phaseName==="DynamicPhaseMarker"?`Marker:${p.phaseType}`:p.phaseName;
  const changes=p.phaseName==="StatStageChangePhase"?p.options.changes.map((v:any)=>`${v.stat}:${v.stages}`).join(","):"-";
  out.push(`Q ${scope} ${l} ${j} ${name} ${p.phaseName==="PokemonAnimPhase"?p.pokemon.getBattlerIndex():p.battlerIndex??-1} ${p.phaseName==="ShowAbilityPhase"?+p.passive:-1} ${p.phaseName==="ShowAbilityPhase"?(allAbilities.find((a:any)=>a?.name===p.abilityName)?.id??-1):-1} ${p.phaseName==="MessagePhase"?hex(p.text):"-"} ${changes} ${p.phaseName==="StatStageChangePhase"?actors.indexOf(p.options.sourcePokemon):-1}`);
 };
 pm.phaseQueue.levels.forEach((q:any[],l:number)=>q.forEach((p:any,j:number)=>emit("L",l,j,p)));
 [...pm.dynamicQueueManager.dynamicPhaseMap.entries()].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a.localeCompare(b)).forEach(([,q]:any,l:number)=>q.queue.forEach((p:any,j:number)=>emit("D",l,j,p)));
 return [`QCOUNT ${out.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...out];};
 const types=[BattlerTagType.TRAPPED,BattlerTagType.OCTOLOCK,BattlerTagType.SEEDED,BattlerTagType.SUBSTITUTE],moves=[MoveId.MEAN_LOOK,MoveId.OCTOLOCK,MoveId.LEECH_SEED,MoveId.SUBSTITUTE];
 const tags=(prefix:string)=>actors.flatMap((p:any,k:number)=>[`${prefix}TAGS ${k} ${p.summonData.tags.length}`,...p.summonData.tags.map((t:any)=>`${prefix}TAG ${Object.values(BattlerTagType).indexOf(t.tagType)} ${t.turnCount} ${t.sourceMove??0} ${t.sourceId??-1} ${t.sourceIndex===undefined?'u':t.sourceIndex} ${t.hp===undefined?'u':t.hp} ${t.sourceInFocus===undefined?'u':+t.sourceInFocus}`)]);
 let i=0;
 for(let mode=0;mode<4;mode++)for(let side=0;side<2;side++)for(let shape=0;shape<4;shape++)for(let idMode=0;idMode<3;idMode++)for(let condition=0;condition<4;condition++)for(const casterLevel of [30,50]){
  const order=[0,1,2,3,4,5];if(shape===1||shape===2){const a=side*3;[order[a],order[a+1],order[a+2]]=[a+1,a+2,a];}
  s.party=order.slice(0,3).map(k=>actors[k]);s.currentBattle.enemyParty=order.slice(3).map(k=>actors[k]);s.modifiers=[];s.enemyModifiers=[];s.arena.tags=[];s.arena.weather=null;s.arena.terrain=null;
  for(let k=0;k<6;k++){
   const p=actors[k];p.id=101+k;p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);p.level=k===side*3?casterLevel:30;p.nature=0;p.customPokemonData.nature=-1;p.ivs=[31,31,31,31,31,31];p.gender=0;p.shiny=false;p.variant=0;p.teraType=0;p.fusionSpecies=null;p.formIndex=0;p.status=null;p.friendship=70;p.passive=false;p.customPokemonData.ability=AbilityId.NONE;p.customPokemonData.passive=AbilityId.NONE;p.switchOutStatus=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=p.getMaxHp();p.stats[Stat.SPD]=10+k*10;p.bossSegments=0;p.bossSegmentIndex=0;p.generateName();if(s.field.getIndex(p)>=0)s.field.remove(p);
  }
  for(const p of [s.party[0],s.party[1],s.currentBattle.enemyParty[0],s.currentBattle.enemyParty[1]])s.field.add(p);
  const caster=actors[side*3],v=(1-side)*3,victim=actors[v];if(shape===2)caster.hp=0;if(idMode===1&&shape!==3)caster.id=0;
  const sid=idMode===2?undefined:shape===3?(idMode===1?0:999999):caster.id;
  if(condition===1)victim.summonData.types=[PokemonType.GHOST];if(condition===2)victim.summonData.types=[PokemonType.GRASS];
  if(condition===3)expect(victim.addTag(BattlerTagType.SUBSTITUTE,0,MoveId.SUBSTITUTE,victim.id)).toBe(true);
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;Phaser.Math.RND.sow([`tag-party-sources-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  const found=s.getPokemonById(sid);expect(found).toBe(shape===3||idMode===2?undefined:caster);
  rows.push(`C ${i} ${mode} ${side} ${shape} ${idMode} ${condition} ${casterLevel}`,`P ${order.join(' ')}`,`SOURCE ${sid??-1} ${actors.indexOf(found)}`,rng('R',Phaser.Math.RND.state()),rng('B',s.currentBattle.battleSeedState),...frame('I'),...tags('I'));
  const result=victim.addTag(types[mode],3,moves[mode],sid);
  rows.push(`RET ${+result}`,...frame('O'),...tags('O'),rng('RO',Phaser.Math.RND.state()),rng('BO',s.currentBattle.battleSeedState),...queue());
  const animations=pm.phaseQueue.levels.flat().filter((p:any)=>p.phaseName==='PokemonAnimPhase');rows.push(`ANIMS ${animations.length}`,...animations.map((p:any)=>`ANIM ${actors.indexOf(p.pokemon)} ${p.key}`));
  rows.push('E');i++;
 }
 expect(i).toBe(768);
 fs.writeFileSync(process.env.TAG_PARTY_SOURCES_LOCALES_OUT!,gzipSync(JSON.stringify(Object.fromEntries(['battle','pokemon-info','battler-tags','arena-tag','ability-trigger','move-trigger','pokemon','ability','move'].map(name=>[name,fs.readFileSync(`locales/en/${name}.json`,'utf8')])))));
 rows.push(`TAG_PARTY_SOURCES_COMPLETE ${i}`);fs.writeFileSync(process.env.TAG_PARTY_SOURCES_OUT!,gzipSync(rows.join('\n')+'\n'));
},240000);
