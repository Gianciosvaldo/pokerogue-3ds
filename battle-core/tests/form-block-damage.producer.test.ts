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
import { applyAbAttrs } from "#abilities/apply-ab-attrs";
import { NumberHolder } from "#utils/common";
import { Pokemon } from "#field/pokemon";
it("native FormBlockDamage dispatch, recoil and form phases",async()=>{
 const N=8704,SEED=Number(process.env.FORMBLOCK_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.RATTATA,SpeciesId.RATTATA);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager,actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField()];if(actors.length!==4)throw Error("natural doubles required");
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});for(const p of actors){vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});if(!p.isPlayer())vi.spyOn(p.battleInfo,"updateBossSegments").mockImplementation(()=>{});}
 const postSpy=vi.spyOn(allAbilities[AbilityId.WIMP_OUT].getAttrs("PostDamageForceSwitchAbAttr")[0],"apply");
 const damageSpy=vi.spyOn(Pokemon.prototype,"damageAndUpdate"),aSpies=[AbilityId.DISGUISE,AbilityId.ICE_FACE].map(a=>vi.spyOn(allAbilities[a].getAttrs("FormBlockDamageAbAttr")[0],"apply"));
 const fd=fs.openSync(process.env.FORMBLOCK_OUT??"/tmp/form-block.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\n")+"\n");write([`FORM_BLOCK 2 ${N} ${SEED}`]);let changed=0,simulations=0,recoil=0,formQueues=0,shields=0,zeroHp=0;
 const boss=(p:any,k:number)=>`${k} ${+p.isBoss()} ${p.bossSegments??0} ${p.bossSegmentIndex??0} ${+p.hasTrainer()}`;
 for(let i=0;i<N;i++){
  let u=i%4,t=Math.floor(i/4)%4,simulated=Math.floor(i/64)%5===0,move=allMoves[[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.HYPER_VOICE,MoveId.SPLASH][Math.floor(i/16)%4]],damage=new NumberHolder([0,1,50,2147483648][Math.floor(i/256)%4]);
  scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;battle.waveIndex=201;battle.turn=1;scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;scene.arena.ignoreAbilities=i%7===0;scene.arena.ignoringEffectSource=i%11===0?t:undefined;
  for(let k=0;k<4;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();const kind=(Math.floor(i/32)+k)%3;p.species=speciesDataRegistry.getSpecies([SpeciesId.MIMIKYU,SpeciesId.EISCUE,SpeciesId.BULBASAUR][kind]);p.formIndex=kind<2?Math.floor(i/128)%2:0;p.fusionSpecies=null;p.customPokemonData.ability=(Math.floor(i/512)+k)%2?AbilityId.DISGUISE:AbilityId.ICE_FACE;p.summonData.ability=p.customPokemonData.ability;p.customPokemonData.passive=i%13===0?p.customPokemonData.ability:i%17===0?AbilityId.DISGUISE:AbilityId.NONE;p.passive=p.customPokemonData.passive!==AbilityId.NONE;p.summonData.abilitySuppressed=i%19===0;p.nature=0;p.ivs=Array(6).fill(15);p.level=[1,50,256,1000000][(Math.floor(i/64)+k)%4];p.stats=Array(6).fill(0);p.hp=undefined;p.calculateStats();const mx=p.getMaxHp();p.hp=[0,1,mx,Math.max(1,Math.floor(mx/2)),Math.max(1,mx-1)][(Math.floor(i/1024)+k)%5];p.status=null;p.switchOutStatus=false;p.moveset=[new PokemonMove(move.id)];p.generateName();if(k>=2){p.bossSegments=0;p.bossSegmentIndex=0;}
   if(i%23===0){const tag:any=getBattlerTag(BattlerTagType.SUBSTITUTE,0,MoveId.SUBSTITUTE,p.id);tag.hp=50;tag.sourceInFocus=false;p.summonData.tags.push(tag);}
  }
  if(i>=8192){u=i%2;t=2+i%2;simulated=false;move=allMoves[MoveId.TACKLE];damage.value=100;scene.arena.ignoreAbilities=false;scene.arena.ignoringEffectSource=undefined;for(const p of actors){p.species=speciesDataRegistry.getSpecies(SpeciesId.MIMIKYU);p.formIndex=0;p.summonData.tags=[];p.summonData.abilitySuppressed=false;p.customPokemonData.ability=AbilityId.DISGUISE;p.summonData.ability=AbilityId.DISGUISE;p.passive=false;p.customPokemonData.passive=AbilityId.NONE;p.level=1000000;p.calculateStats();p.hp=p.getMaxHp();p.generateName();}
   const p=actors[t];if(i<8448){p.bossSegments=[2,3,5,8][Math.floor(i/2)%4];p.hp=Math.max(1,Math.floor(p.getMaxHp()/p.bossSegments)+1);p.bossSegmentIndex=p.getBossSegmentIndex();shields++;}else{p.hp=i%2?1:p.getMaxHp();p.setStat(Stat.HP,i%2?2*p.getMaxHp():1,false);}
  }
  if(i>=8576){u=2+i%2;t=i%2;for(const p of actors){if(!p.isPlayer()){p.bossSegments=0;p.bossSegmentIndex=0;}}const p=actors[t];p.hp=Math.ceil(p.getMaxHp()/2);p.customPokemonData.passive=AbilityId.WIMP_OUT;p.passive=true;}
  pm.clearAllPhases();battle.battleSeed=`form-block-${SEED}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`form-block-${SEED}-${i}`]);battle.randSeedInt(2);damageSpy.mockClear();for(const spy of aSpies)spy.mockClear();
  const rows=[`C ${i} ${u} ${t} ${move.id} ${+simulated} ${damage.value}`,`W ${scene.waveSeed}`,`IGNORE ${+scene.arena.ignoreAbilities} ${scene.arena.ignoringEffectSource??-1}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,`QIN ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive} ${pm.phaseQueue.currentLevel}`,...actors.flatMap((p,k)=>[`I ${k} ${snapshot(p).join(" ")}`,`IB ${boss(p,k)}`])];
  const before=damage.value;applyAbAttrs("ReceivedMoveDamageMultiplierAbAttr",{pokemon:actors[t],opponent:actors[u],move,damage,simulated});changed+=+(before!==damage.value);simulations+=+simulated;recoil+=damageSpy.mock.calls.length;zeroHp+=+(actors[t].hp===0);rows.push(`RESULT ${damage.value}`,...actors.flatMap((p,k)=>[`O ${k} ${snapshot(p).join(" ")}`,`OB ${boss(p,k)}`]));
  rows.push(`QCOUNT ${pm.phaseQueue.levels.flat().length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`);
  pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,k:number)=>{
   if(p.phaseName==="MessagePhase")rows.push(`Q ${l} ${k} M ${hex(p.text)} -1 -1 -1`);
   else if(p.phaseName==="QuietFormChangePhase"){formQueues++;rows.push(`Q ${l} ${k} F - ${actors.indexOf(p.pokemon)} ${p.pokemon.species.forms.findIndex((f:any)=>f.formKey===p.formChange.formKey)} -1`);}
   else if(p.phaseName==="DamageAnimPhase")rows.push(`Q ${l} ${k} A - ${p.battlerIndex} ${p.amount} ${p.damageResult*2+(+p.critical)}`);
   else if(p.phaseName==="DynamicPhaseMarker"&&p.phaseType==="StatStageChangePhase")rows.push(`Q ${l} ${k} S - -1 -1 -1`);
   else if(p.phaseName==="ShowAbilityPhase"){const matches=allAbilities.filter(a=>a.name===p.abilityName);if(matches.length!==1)throw Error("cached ability ambiguous");rows.push(`Q ${l} ${k} B ${hex(p.pokemonName??"")} ${p.battlerIndex} ${matches[0].id} ${(+p.passive)*2+(+p.pokemonOnField)}`);}
   else if(p.phaseName==="HideAbilityPhase")rows.push(`Q ${l} ${k} H - -1 -1 -1`);
   else throw Error(`unexpected phase ${p.phaseName}:${p.phaseType} case${i}`);
  }));
  const dynamic:any[]=[...pm.dynamicQueueManager.dynamicPhaseMap.values()].flatMap((q:any)=>q.queue);rows.push(`DCOUNT ${dynamic.length}`);
  dynamic.forEach((p:any,k:number)=>{const o=p.options;if(p.phaseName!=="StatStageChangePhase")throw Error("unexpected dynamic");rows.push(`D ${k} ${p.battlerIndex} ${actors.indexOf(o.sourcePokemon)} ${+p.selfTarget} ${o.changes.length} ${+!!o.ignoreAbilities} ${+!!o.processed} ${o.source??0} ${+(o.message!==undefined)}`,...o.changes.map((c:any)=>`CHANGE ${c.stat} ${c.stages}`));});
  rows.push(`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(postSpy.mock.calls.length<128||changed<100||simulations<100||recoil<100||formQueues<100||shields<256||zeroHp<100)throw Error(`missing positives ${changed}/${simulations}/${recoil}/${formQueues}/${shields}/${zeroHp}`);write([`FORM_BLOCK_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`FORM_BLOCK postDamage=${postSpy.mock.calls.length} changed=${changed} simulated=${simulations} recoil=${recoil} forms=${formQueues} shields=${shields} zeroHP=${zeroHp}`);
},600000);
