import { HitResult } from "#enums/hit-result";
import { CommonBattleAnim } from "#data/battle-anims";
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

import { Challenges } from "#enums/challenges";
import { TrainerSlot } from "#enums/trainer-slot";
import { Pokemon } from "#field/pokemon";
import { EvolutionPhase } from "#phases/evolution-phase";
import { FormChangePhase } from "#phases/form-change-phase";
import { audioManager } from "#app/global-audio-manager";
import { SpeciesFormChangeMoveLearnedTrigger } from "#data/form-change-triggers";
// Only scene assets, tweens, UI and audio are replaced. Native phase start,
// clone construction, form mutation, conditions and end callbacks all run.
it("native Keldeo learned-move form queues and phase execution",async()=>{
 const N=33792,seed=Number(process.env.KELDEO_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager;
 const actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField(),...Array.from({length:2},()=>scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),50)),...Array.from({length:2},()=>scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),50,TrainerSlot.NONE))];
 if(actors.length!==8)throw Error("eight natural actors required");
 const sprite:any=new Proxy({}, {get:()=>()=>sprite});
 vi.spyOn(EvolutionPhase.prototype as any,"setupEvolutionAssets").mockImplementation(function(this:any){this.evolutionContainer=sprite;this.evolutionBaseBg=sprite;this.evolutionBg=sprite;this.evolutionBgOverlay=sprite;this.evolutionOverlay=sprite;});
 vi.spyOn(EvolutionPhase.prototype as any,"setupPokemonSprites").mockImplementation(function(this:any){this.pokemonSprite=sprite;this.pokemonTintSprite=sprite;this.pokemonEvoSprite=sprite;this.pokemonEvoTintSprite=sprite;});
 vi.spyOn(FormChangePhase.prototype as any,"configureSprite").mockImplementation(()=>sprite);
 vi.spyOn(FormChangePhase.prototype as any,"beginTweens").mockImplementation(function(this:any,pre:any,clone:any){this.afterCycle(pre,clone);});
 vi.spyOn(Pokemon.prototype,"loadAssets").mockImplementation(()=>Promise.resolve());
 vi.spyOn(Pokemon.prototype,"updateInfo").mockImplementation(()=>Promise.resolve());
 vi.spyOn(Pokemon.prototype,"cry").mockImplementation(()=>null);
 const shifted=vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 vi.spyOn(scene.ui,"setModeForceTransition").mockImplementation(()=>Promise.resolve());
 vi.spyOn(scene.ui,"setOverlayMode").mockImplementation(()=>Promise.resolve());
 vi.spyOn(scene.ui,"revertMode").mockImplementation(()=>Promise.resolve());
 vi.spyOn(scene.animations,"doCircleInward").mockImplementation(()=>{});vi.spyOn(scene.animations,"doSpray").mockImplementation(()=>{});
 vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});vi.spyOn(audioManager,"replaceBgmUntilEnd").mockImplementation(()=>null);
 const timers:(()=>void)[]=[],texts:string[]=[];vi.spyOn(scene.time,"delayedCall").mockImplementation((_d:any,cb:any)=>{timers.push(cb);return{} as any;});
 vi.spyOn(scene.ui,"showText").mockImplementation((text:any,_d:any,cb:any)=>{texts.push(text);cb?.();});
 vi.spyOn(scene.tweens,"chain").mockImplementation((cfg:any)=>{for(const t of cfg.tweens)t.onComplete?.();cfg.onComplete?.();return{} as any;});
 const fd=fs.openSync(process.env.KELDEO_OUT??"/tmp/keldeo.txt","w"),write=(xs:string[])=>fs.writeSync(fd,xs.join("\n")+"\n"),actor=(p:any)=>actors.indexOf(p);
 write([`KELDEO 5 ${N} ${seed}`]);let queued=0,executed=0,nonquiet=0,modalExecutions=0;
 const queues=()=>{const rows:string[]=[];pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,j:number)=>{
  const isForm=["QuietFormChangePhase","FormChangePhase"].includes(p.phaseName);
  if(!isForm&&p.phaseName!=="EndEvolutionPhase"&&p.phaseName!=="MessagePhase"&&p.phaseName!=="ShowAbilityPhase"&&p.phaseName!=="HideAbilityPhase")throw Error(`unexpected queue ${p.phaseName}`);
  rows.push(`P ${l} ${j} ${p.phaseName} ${isForm?actor(p.pokemon):p.phaseName==="ShowAbilityPhase"?p.battlerIndex:-1} ${isForm?p.pokemon.species.forms.findIndex((f:any)=>f.formKey===p.formChange.formKey):p.phaseName==="ShowAbilityPhase"?+p.passive:-1} ${p.phaseName==="MessagePhase"?hex(p.text):p.phaseName==="ShowAbilityPhase"?hex(p.pokemonName??"")+"."+hex(p.abilityName??""):"-"} ${p.phaseName==="MessagePhase"?+!!p.prompt:-1}`);
 }));return [`Q ${rows.length} ${pm.phaseQueue.levels.length} ${pm.phaseQueue.currentLevel} ${+pm.phaseQueue.deferredActive}`,...rows];};
 for(let i=0;i<N;i++){
  const u=i>=32768?[0,1,4,5][i%4]:i%8,mode=Math.floor(i/8)%2,shape=Math.floor(i/16)%32,delayed=!!(Math.floor(i/512)%2),gm=[GameModes.CLASSIC,GameModes.DAILY,GameModes.ENDLESS,GameModes.CHALLENGE][Math.floor(i/1024)%4];scene.gameMode=getGameMode(gm);
  for(let k=0;k<8;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(k===u?SpeciesId.KELDEO:SpeciesId.BULBASAUR);p.formIndex=k===u?shape%2:0;p.fusionSpecies=null;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.passive=false;p.customPokemonData.passive=AbilityId.NONE;p.level=[1,50,256,1000000][Math.floor(i/4096)%4];p.gender=-1;p.abilityIndex=0;p.calculateStats();p.hp=shape>=16?0:p.getMaxHp();p.status=null;p.moveset=[new PokemonMove(shape%4>=2?MoveId.SECRET_SWORD:MoveId.SPLASH)];p.visible=false;p.switchOutStatus=false;p.isTerastallized=!!(shape%8>=4);p.teraType=18;p.stellarTypesBoosted=p.isTerastallized?[1,2]:[];p.summonData.addedType=11;p.generateName();
  }
  if(i>=16384){
   const p=actors[u],context=Math.floor((i-16384)/512)%32;
   if(context%2===0){p.summonData.speciesForm=speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR);p.summonData.gender=0;p.summonData.moveset=[new PokemonMove(p.moveset[0].moveId===MoveId.SECRET_SWORD?MoveId.SPLASH:MoveId.SECRET_SWORD)];}
   if(context%4===1)p.customPokemonData.ability=AbilityId.DISGUISE;
   if(context%4===3){p.customPokemonData.ability=AbilityId.MIMICRY;p.summonData.types=[7,11];}
   const tag:any=getBattlerTag(BattlerTagType.AUTOTOMIZED);tag.autotomizeCount=1+(context%3);p.summonData.tags.push(tag);
   p.generateName();p.calculateStats();p.hp=shape>=16?0:p.getMaxHp();
  }
  pm.clearAllPhases();timers.length=0;texts.length=0;pm.pushNew("MessagePhase","anchor");pm.currentPhase=pm.create("MovePhase",actors[0],[2],new PokemonMove(MoveId.SPLASH),MoveUseMode.NORMAL);
  battle.battleSeed=`keldeo-${seed}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`keldeo-${seed}-${i}`]);battle.randSeedInt(2);
  const direct=i>=32768,modal=direct&&!!(Math.floor((i-32768)/32)%2),target=direct?Math.floor((i-32768)/64)%2:-1;
  const rows=[`C ${i} ${u} ${mode} ${gm} ${+delayed} ${Object.values(Challenges).filter((c:any)=>typeof c==="number"&&scene.gameMode.hasChallenge(c)).reduce((bits:number,c:any)=>bits|(1<<c),0)} ${+direct} ${+modal} ${target}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`),`IT ${actors.map(p=>p.getTag(BattlerTagType.AUTOTOMIZED)?.autotomizeCount??0).join(" ")}`,`IW ${actors.map(p=>p.getWeight()).join(" ")}`];
  let result:boolean;
  if(direct){
   const p=actors[u],fc=speciesDataRegistry.getFormChanges(p.species.speciesId).find((f:any)=>f.formKey===p.species.forms[target].formKey);
   if(!fc)throw Error("missing registered constructor target");
   const ph=pm.create("FormChangePhase",p,fc,modal);if(delayed)pm.pushPhase(ph);else pm.unshiftPhase(ph);result=true;
  }else result=scene.triggerPokemonFormChange(actors[u],SpeciesFormChangeMoveLearnedTrigger,delayed);
  rows.push(`RESULT ${+result}`,...queues());const forms=pm.phaseQueue.levels.flat().filter((p:any)=>["FormChangePhase","QuietFormChangePhase"].includes(p.phaseName));queued+=forms.length;nonquiet+=forms.filter((p:any)=>p.phaseName==="FormChangePhase").length;
  if(mode===1)for(const ph of forms){shifted.mockClear();pm.currentPhase=ph;await ph.start();for(let turn=0;turn<64&&!shifted.mock.calls.length;turn++){while(timers.length)timers.shift()!();await Promise.resolve();}if(!shifted.mock.calls.length)throw Error(`phase not ended ${i} ${ph.phaseName}`);executed++;modalExecutions+=+modal;}
  rows.push(...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),`OT ${actors.map(p=>p.getTag(BattlerTagType.AUTOTOMIZED)?.autotomizeCount??0).join(" ")}`,`OW ${actors.map(p=>p.getWeight()).join(" ")}`,...queues(),`T ${texts.length}`,...texts.map(t=>`TEXT ${hex(t)}`),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E");write(rows);
 }
 if(queued<100||executed<100||nonquiet<100||modalExecutions<100)throw Error(`coverage ${queued}/${executed}/${nonquiet}`);write([`KELDEO_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`KELDEO queued=${queued} executed=${executed} nonquiet=${nonquiet} modal=${modalExecutions}`);
},600000);
