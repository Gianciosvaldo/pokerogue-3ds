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

import { HitResult } from "#enums/hit-result";
import { Weather } from "#data/weather";
import { WeatherType } from "#enums/weather-type";


it("actual ability-changing move callbacks and conditions",async()=>{
 const N=12544,SEED=Number(process.env.ABILITYMOVES_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];if(actors.length!==4)throw Error("natural doubles required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors)vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});
 const moves=[MoveId.WORRY_SEED,MoveId.SIMPLE_BEAM,MoveId.ROLE_PLAY,MoveId.DOODLE,MoveId.ENTRAINMENT,MoveId.SKILL_SWAP],classes=["AbilityChangeAttr","AbilityChangeAttr","AbilityCopyAttr","AbilityCopyAttr","AbilityGiveAttr","SwitchAbilitiesAttr"] as const;
 const choices=[AbilityId.NONE,AbilityId.BALL_FETCH,AbilityId.SIMPLE,AbilityId.CONTRARY,AbilityId.INSOMNIA,AbilityId.WONDER_GUARD,AbilityId.MULTITYPE,AbilityId.FORECAST,AbilityId.FLOWER_GIFT];
 const loss=vi.spyOn(allAbilities[AbilityId.PRIMORDIAL_SEA].getAttrs("PreLeaveFieldClearWeatherAbAttr")[0],"apply");
 const fd=fs.openSync(process.env.ABILITYMOVES_OUT??"/tmp/ability-moves.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`ABILITY_MOVES 4 ${N} ${SEED}`]);let denied=0,conditionDenied=0,partner=0,formQueues=0;const modes=Array(6).fill(0);
 for(let i=0;i<N;i++){
  const mode=i%6,u=Math.floor(i/6)%4,t=Math.floor(i/24)%4,move=allMoves[moves[mode]],attr=move.getAttrs(classes[mode])[0];if(!attr)throw Error("owning registered attribute required");modes[mode]++;
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;battle.waveIndex=201;battle.turn=1;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=choices[(Math.floor(i/96)+k)%choices.length];p.customPokemonData.passive=i%7===0?AbilityId.INTIMIDATE:AbilityId.NONE;p.passive=p.customPokemonData.passive!==AbilityId.NONE;p.summonData.ability=p.customPokemonData.ability;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,256,1000000][(Math.floor(i/64)+k)%4];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=(Math.floor(i/192)+k)%5===0?0:p.getMaxHp();p.status=null;p.switchOutStatus=false;p.moveset=[new PokemonMove(move.id)];p.generateName();p.summonData.abilitySuppressed=i%11===0;
  }
  if(i>=12288&&i<12416){for(let k=0;k<4;k++){const p=actors[k];p.species=speciesDataRegistry.getSpecies(k%2?SpeciesId.CHERRIM:SpeciesId.CASTFORM);p.formIndex=1;p.customPokemonData.ability=k%2?AbilityId.FLOWER_GIFT:AbilityId.FORECAST;p.summonData.ability=p.customPokemonData.ability;p.passive=false;p.customPokemonData.passive=AbilityId.NONE;p.level=50;p.calculateStats();p.hp=p.getMaxHp();p.generateName();}}
  if(i>=12416){for(const p of actors){p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=AbilityId.BALL_FETCH;p.summonData.abilitySuppressed=false;p.passive=false;p.customPokemonData.passive=AbilityId.NONE;p.hp=p.getMaxHp();}const loser=actors[mode===0||mode===1||mode===4?t:u];loser.customPokemonData.ability=AbilityId.PRIMORDIAL_SEA;loser.summonData.ability=AbilityId.PRIMORDIAL_SEA;scene.arena.weather=new Weather(WeatherType.HEAVY_RAIN,0);}
  pm.clearAllPhases();battle.battleSeed=`ability-moves-${SEED}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`ability-moves-${SEED}-${i}`]);battle.randSeedInt(2);
  const rows=[`C ${i} ${mode} ${u} ${t}`,`W ${scene.waveSeed}`,`WI ${scene.arena.weather?.weatherType??0} ${scene.arena.weather?.turnsLeft??0} ${scene.arena.weather?.maxDuration??0}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)];
  const cond=move.applyConditions(actors[u],actors[t]);conditionDenied+=+!cond;rows.push(`COND ${+cond}`);
  const result=attr.apply(actors[u],actors[t],move,[]);denied+=+!result;partner+=+(mode===3&&result&&actors[u].getAlly()?.hp>0);
  rows.push(`RET ${+result}`,...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`));
  rows.push(`WO ${scene.arena.weather?.weatherType??0} ${scene.arena.weather?.turnsLeft??0} ${scene.arena.weather?.maxDuration??0}`);
  const flat=pm.phaseQueue.levels.flat();rows.push(`QCOUNT ${flat.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`);
  pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,k:number)=>{
   if(p.phaseName==="MessagePhase")rows.push(`Q ${l} ${k} M ${hex(p.text)} -1 -1`);
   else if(p.phaseName==="QuietFormChangePhase"){formQueues++;rows.push(`Q ${l} ${k} F - ${actors.indexOf(p.pokemon)} ${p.pokemon.species.forms.findIndex((f:any)=>f.formKey===p.formChange.formKey)}`);}
   else if(p.phaseName==="DynamicPhaseMarker"&&p.phaseType==="CommonAnimPhase")rows.push(`Q ${l} ${k} K CommonAnimPhase -1 -1`);
   else if(p.phaseName==="ShowAbilityPhase"){const found=allAbilities.filter(a=>a.name===p.abilityName);if(found.length!==1)throw Error("ambiguous cached ability");rows.push(`Q ${l} ${k} A - ${p.battlerIndex} ${found[0].id*2+(+p.passive)}`);}
   else if(p.phaseName==="HideAbilityPhase")rows.push(`Q ${l} ${k} H - -1 -1`);
   else if(p.phaseName==="CommonAnimPhase")rows.push(`Q ${l} ${k} C T${p.targetIndex??-1} ${p.battlerIndex} ${p.anim}`);
   else throw Error(`unexpected native phase ${p.phaseName}:${p.phaseType} at ${i} dynamic ${[...pm.dynamicQueueManager.dynamicPhaseMap.entries()].map(([key,q]:any)=>`${key}=${q.queue.map((x:any)=>x.phaseName)}`).join(";")}`);
  }));
  const dynamic:any[]=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue);rows.push(`DCOUNT ${dynamic.length}`);dynamic.forEach((p:any,k:number)=>{if(p.phaseName!=="CommonAnimPhase")throw Error(`unexpected dynamic phase ${p.phaseName}`);rows.push(`D ${k} C T${p.targetIndex??-1} ${p.battlerIndex} ${p.anim}`);});
  for(const p of actors)p.resetSummonData();rows.push(...actors.map((p,k)=>`Z ${k} ${snapshot(p).join(" ")}`));
  rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(loss.mock.calls.length<100||denied<100||conditionDenied<100||partner<100||formQueues<100||modes.some(n=>n<100))throw Error(`ability callback coverage missing ${denied}/${conditionDenied}/${partner}/${modes}`);write([`ABILITY_MOVES_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`ABILITY_MOVES lossCalls=${loss.mock.calls.length} denied=${denied} conditionDenied=${conditionDenied} partner=${partner} formQueues=${formQueues} modes=${modes}`);
},600000);
