import { getArenaTag } from "#data/arena-tag";
import { ArenaTagType } from "#enums/arena-tag-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { Status } from "#data/status-effect";
import { StatusEffect } from "#enums/status-effect";
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves } from "#data/data-lists";
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

import { PokemonType } from "#enums/pokemon-type";

it("actual Reflect Type and Conversion callbacks",async()=>{
 const N=12672,seed=Number(process.env.TYPEMOVES_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];if(actors.length!==4)throw Error("natural doubles required");vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors)vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});
 const rollSpies=actors.map(p=>vi.spyOn(p,"randBattleSeedInt"));
 const moves=[MoveId.REFLECT_TYPE,MoveId.CONVERSION,MoveId.CONVERSION_2],classes=["CopyTypeAttr","FirstMoveTypeAttr","ResistLastMoveTypeAttr"],first=Array.from({length:18},(_,t)=>allMoves.find(m=>m&&m.id>0&&m.type===t)!.id);
 const lists=[[],[PokemonType.UNKNOWN],[PokemonType.NORMAL],[PokemonType.FIRE,PokemonType.WATER],[PokemonType.GHOST,PokemonType.GRASS],[PokemonType.FIRE,PokemonType.WATER,PokemonType.GHOST,PokemonType.GRASS]];
 const fd=fs.openSync(process.env.TYPEMOVES_OUT??"/tmp/type-moves.txt","w"),write=(r:string[])=>fs.writeSync(fd,r.join("\n")+"\n"),modes=[0,0,0];let denied=0,conditionDenied=0,changed=0,messages=0;write([`TYPE_MOVES 3 ${N} ${seed}`]);
 const hist=(prefix:string,p:any,k:number)=>[`${prefix} ${k} ${p.getMoveHistory().length}`,...p.getMoveHistory().map((m:any)=>`HENTRY ${m.move} ${m.result} ${m.useMode} ${m.targets.length} ${m.targets.join(" ")}`)];
 const rng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(","," ");
 for(let i=0;i<N;i++){
  const mode=i>=12288?2:i%3,u=Math.floor(i/3)%4,t=i>=12288?(u+2)%4:Math.floor(i/12)%4,move=allMoves[moves[mode]],ops=1+Math.floor(i/144)%3; modes[mode]++;
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;battle.waveIndex=201;battle.turn=1;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.CHARMANDER,SpeciesId.SQUIRTLE,SpeciesId.MAGIKARP][k]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.summonData.ability=AbilityId.BALL_FETCH;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,256,1000000][(Math.floor(i/48)+k)%4];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();p.hp=(Math.floor(i/96)+k)%4===0?0:p.getMaxHp();p.status=null;p.switchOutStatus=false;p.generateName();
   p.moveset=[new PokemonMove(first[(Math.floor(i/16)+k)%18]),new PokemonMove(move.id)];p.summonData.types=[...lists[(Math.floor(i/32)+k)%lists.length]];p.summonData.addedType=[null,PokemonType.GHOST,PokemonType.GRASS][(Math.floor(i/64)+k)%3];p.isTerastallized=(Math.floor(i/192)+k)%3===0;p.teraType=(Math.floor(i/24)+k)%19;
   const n=(Math.floor(i/8)+k)%4;for(let j=0;j<n;j++)p.pushMoveHistory({move:(i+j+k)%11===0?MoveId.NONE:first[(Math.floor(i/4)+k+j)%18],targets:[(k+j)%4],result:(i+j)%5,useMode:[MoveUseMode.NORMAL,MoveUseMode.FOLLOW_UP,MoveUseMode.INDIRECT][(i+j)%3]});
  }
  if(i>=12288){for(const p of actors){p.isTerastallized=false;p.hp=p.getMaxHp();}const p=actors[u];p.summonData.types=i<12416?[PokemonType.ROCK,PokemonType.STEEL,PokemonType.GHOST]:[PokemonType.ROCK,PokemonType.STEEL];p.summonData.addedType=null;actors[t].summonData.moveHistory=[];actors[t].pushMoveHistory({move:MoveId.TACKLE,targets:[u],result:1,useMode:MoveUseMode.NORMAL});}
  if(i>=12544){actors[u].summonData.types=[];actors[u].summonData.addedType=null;actors[t].isTerastallized=true;actors[t].teraType=PokemonType.FIRE;actors[t].summonData.moveHistory=[];actors[t].pushMoveHistory({move:MoveId.TERA_BLAST,targets:[u],result:1,useMode:MoveUseMode.NORMAL});}
  pm.clearAllPhases();battle.battleSeed=`type-moves-${seed}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`type-moves-${seed}-${i}`]);battle.randSeedInt(2);
  const rows=[`C ${i} ${mode} ${u} ${t} ${ops}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,`QIN ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive} ${pm.phaseQueue.currentLevel}`,...actors.flatMap((p,k)=>[`I ${k} ${snapshot(p).join(" ")}`,...hist("HI",p,k)])];
  for(let op=0;op<ops;op++){
   const cond=[2,3,4].map(seq=>+move.applyConditions(actors[u],actors[t],seq as any));rows.push(`COND ${cond.join(" ")}`);conditionDenied+=+cond.some(n=>!n);
   rollSpies[u].mockClear();
   const before=actors[u].summonData.types.join(","),result=move.getAttrs(classes[mode] as any)[0].apply(actors[u],actors[t],move,[]);denied+=+!result;changed+=+(before!==actors[u].summonData.types.join(","));rows.push(`RET ${+result}`,`CALLS ${rollSpies[u].mock.calls.length}`,...rollSpies[u].mock.calls.map((a:any[])=>`CALL ${a[0]} ${a[1]??0}`),...actors.flatMap((p,k)=>[`O ${k} ${snapshot(p).join(" ")}`,...hist("HO",p,k)]));
   const flat=pm.phaseQueue.levels.flat();rows.push(`QCOUNT ${flat.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`);pm.phaseQueue.levels.forEach((q:any[],l:number)=>q.forEach((p:any,k:number)=>{if(p.phaseName!=="MessagePhase")throw Error(`unexpected phase ${p.phaseName}`);rows.push(`Q ${l} ${k} ${hex(p.text)}`);}));messages=messages+flat.length;
   if([...pm.dynamicQueueManager.dynamicPhaseMap.values()].some((q:any)=>q.queue.length))throw Error("unexpected dynamic payload");
  }
  rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(denied<100||conditionDenied<100||changed<100||messages<100||modes.some(n=>n<100))throw Error(`coverage missing ${denied}/${conditionDenied}/${changed}/${messages}/${modes}`);write([`TYPE_MOVES_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`TYPE_MOVES denied=${denied} conditionDenied=${conditionDenied} changed=${changed} messages=${messages} modes=${modes}`);
},600000);
