/* Actual Transform phase followed by native ability eligibility/turn dispatch. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { applyAbAttrs } from "#abilities/apply-ab-attrs";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { Stat } from "#enums/stat";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
function snapshot(p: any): number[] {
  const sd = p.summonData, form = p.getSpeciesForm(), transformed = p.isTransformed();
  const formIndex = Math.max(0, speciesDataRegistry.getSpecies(form.speciesId).forms.indexOf(form));
  const moves = (ms: any[]) => [ms.length, ...Array.from({ length: 4 }, (_, i) => ms[i]?.moveId ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUsed ?? 0), ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUp ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.maxPpOverride ?? -1)];
  const types = p.getTypes();
  return [p.species.speciesId, p.formIndex, p.gender, p.id, p.level, p.nature, p.hp, p.status?.effect ?? 0,
    +p.shiny, p.getAbility(true).id, p.getAbility().id, p.passive ? p.getPassiveAbility().id : 0, +p.passive,
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

it("actual Transform/native ability interactions",async()=>{
 const N=Number(process.env.FORMINTER_N??8192),seed=Number(process.env.FORMINTER_SEED??777);if(!Number.isSafeInteger(N)||N<512||!process.versions.node.startsWith("22."))throw Error("Node22,N>=512");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);await game.classicMode.startBattle(SpeciesId.DITTO);
 game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const s:any=game.scene,pm:any=s.phaseManager,ps:any[]=[game.field.getPlayerPokemon(),game.field.getEnemyPokemon()];vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 for(const p of ps){vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"playAnim").mockImplementation(()=>{});}
 const types=[[SpeciesId.MORPEKO,AbilityId.HUNGER_SWITCH],[SpeciesId.MIMIKYU,AbilityId.DISGUISE],[SpeciesId.MINIOR,AbilityId.SHIELDS_DOWN],[SpeciesId.EISCUE,AbilityId.ICE_FACE],[SpeciesId.DARMANITAN,AbilityId.ZEN_MODE],[SpeciesId.WISHIWASHI,AbilityId.SCHOOLING],[SpeciesId.ZYGARDE,AbilityId.POWER_CONSTRUCT],[SpeciesId.AEGISLASH,AbilityId.STANCE_CHANGE]];
 const rng=(r:string)=>r.replace(/^!rnd,/,"").replaceAll(","," ");const rows=[`FORMINTER 1 ${N} ${seed}`];
 for(let i=0;i<N;i++){
  const b=i%2,user=ps[b],target=ps[1-b],entry=types[Math.floor(i/64)%8],state=Math.floor(i/2)%32;
  for(const p of ps){p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.fusionSpecies=null;p.fusionCustomPokemonData=null;p.formIndex=0;p.abilityIndex=0;p.gender=-1;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.switchOutStatus=false;p.status=null;p.isTerastallized=false;p.teraType=-1;p.stellarTypesBoosted=[];p.moveset=[new PokemonMove(MoveId.SPLASH),new PokemonMove(MoveId.TACKLE)];}
  user.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);target.species=speciesDataRegistry.getSpecies(entry[0]);target.formIndex=state%target.species.forms.length;target.customPokemonData.ability=entry[1];
  for(const p of ps){p.level=30+i%21;p.calculateStats();p.hp=p.getMaxHp();p.generateName();}
  if(state%4===1)user.hp=Math.floor(user.getMaxHp()/2);if(state%4===2)user.hp=1;
  if(state%4===3){user.passive=true;user.customPokemonData.passive=entry[1];}
  if(state>=16)user.summonData.abilitySuppressed=true;
  s.modifiers=[];s.enemyModifiers=[];s.arena.tags=[];s.arena.weather=null;s.arena.terrain=null;pm.clearAllPhases();pm.phaseQueue.deferredActive=false;
  Phaser.Math.RND.sow([`forminter-${seed}-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  rows.push(`C ${i} ${b?2:0} ${state>=8&&state<16?1:0}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(s.currentBattle.battleSeedState)}`,...ps.map((p,j)=>`I ${j?2:0} ${snapshot(p).join(" ")}`));
  const phase:any=pm.create("PokemonTransformPhase",user.getBattlerIndex(),target.getBattlerIndex(),false);pm.currentPhase=phase;phase.start();await Promise.resolve();await Promise.resolve();await Promise.resolve();
  rows.push(...ps.map((p,j)=>`A ${j?2:0} ${snapshot(p).join(" ")}`),`AC ${ps.flatMap(p=>[+p.canApplyAbility(false),+p.canApplyAbility(true)]).join(" ")}`);
  applyAbAttrs("PostTurnAbAttr",{pokemon:user,simulated:state>=8&&state<16});
  rows.push(...ps.map((p,j)=>`O ${j?2:0} ${snapshot(p).join(" ")}`),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(s.currentBattle.battleSeedState)}`);
  const queue:string[]=[];pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((ph:any,k:number)=>queue.push(`P ${l} ${k} ${ph.phaseName} ${ph.phaseName==="MessagePhase"?Buffer.from(ph.text).toString("hex"):"-"}`)));
  if([...pm.dynamicQueueManager.dynamicPhaseMap.values()].some((x:any)=>x.queue.length))throw Error("unexpected dynamic callback");
  rows.push(`Q ${queue.length} ${pm.phaseQueue.levels.length} ${pm.phaseQueue.currentLevel} ${+pm.phaseQueue.deferredActive}`,...queue,"E");
 }
 rows.push(`FORMINTER_COMPLETE ${N}`);fs.writeFileSync(process.env.FORMINTER_OUT??"/tmp/forminter.txt",rows.join("\n")+"\n");
},600000);
