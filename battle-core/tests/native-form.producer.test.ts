/* Actual pinned native ability dispatch: state and queued phase eligibility. Owning turns and native gain/loss execution are separate. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { applyAbAttrs, applyOnGainAbAttrs } from "#abilities/apply-ab-attrs";
import { Weather } from "#data/weather";
import { WeatherType } from "#enums/weather-type";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual native ability form callbacks and queues",async()=>{
 const N=Number(process.env.NATIVEFORM_N??2000),SEED=Number(process.env.NATIVEFORM_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<512)throw Error("Node22 and N>=512 required");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 const scene:any=game.scene,actors:any[]=[game.field.getPlayerPokemon(),game.field.getEnemyPokemon()];
 vi.spyOn(scene,"updateFieldScale").mockImplementation(async()=>{});
 for(const p of actors){vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"updateSpritePipelineData").mockImplementation(()=>{});}
 let texts:string[]=[];
 vi.spyOn(scene.ui,"showText").mockImplementation((text:any,_delay:any,callback:any)=>{texts.push(text);callback?.();});
 vi.spyOn(scene.phaseManager,"shiftPhase").mockImplementation(()=>{});
 const species=[[SpeciesId.DARMANITAN,AbilityId.ZEN_MODE],[SpeciesId.GALAR_DARMANITAN,AbilityId.ZEN_MODE],[SpeciesId.MINIOR,AbilityId.SHIELDS_DOWN],[SpeciesId.WISHIWASHI,AbilityId.SCHOOLING],[SpeciesId.MORPEKO,AbilityId.HUNGER_SWITCH],[SpeciesId.EISCUE,AbilityId.ICE_FACE],[SpeciesId.MIMIKYU,AbilityId.DISGUISE],[SpeciesId.ZYGARDE,AbilityId.POWER_CONSTRUCT]];
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const runPhase=process.env.NATIVEFORM_PHASE==="1";
 const label=runPhase?"NATIVE_FORM_PHASE":"NATIVE_FORM";
 const rows=[`${label} 1 ${N}`],states=Array(32).fill(0);
 for(let i=0;i<N;i++){
  const state=Math.floor(i/2)%32,p=actors[i%2],entry=species[Math.floor(i/64)%species.length],sp=speciesDataRegistry.getSpecies(entry[0]),from=rnd(sp.forms.length),mode=state%4;states[state]++;
  if(process.env.NATIVEFORM_AURA==="1"){
   const other=actors[1-i%2];other.resetSummonData();other.customPokemonData.ability=[AbilityId.BALL_FETCH,AbilityId.DARK_AURA,AbilityId.FAIRY_AURA][Math.floor(i/64)%3];
   other.customPokemonData.passive=AbilityId.NONE;other.passive=false;other.hp=Math.floor(i/512)%2?other.getMaxHp():0;
  }
  p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=sp;p.formIndex=from;p.abilityIndex=0;
  p.fusionSpecies=null;p.customPokemonData.ability=entry[1];
  if(process.env.NATIVEFORM_NATURAL==="1"){
   const f:any=sp.forms[from],index=[f.ability1,f.ability2,f.abilityHidden].indexOf(entry[1]);
   p.abilityIndex=Math.max(0,index);p.customPokemonData.ability=-1;
  }
  p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.nature=rnd(25);p.level=18+state%5;p.ivs=Array.from({length:6},()=>rnd(32));
  p.isTerastallized=false;p.status=null;p.stats=Array(6).fill(0);p.hp=0;
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.weather=[WeatherType.NONE,WeatherType.HAIL,WeatherType.SNOW][rnd(3)]===WeatherType.NONE?null:new Weather([WeatherType.HAIL,WeatherType.SNOW][rnd(2)]);scene.arena.terrain=null;scene.arena.tags=[];
  p.calculateStats();p.hp=state%4===0?p.getMaxHp():state%4===1?Math.floor(p.getMaxHp()/2):state%4===2?Math.floor(p.getMaxHp()/4):0;if(process.env.NATIVEFORM_HP_MATRIX==="1"){
   const max=p.getMaxHp(),values=[0,1,Math.max(0,Math.floor(max/4)-1),Math.floor(max/4),Math.floor(max/4)+1,Math.floor(max/2),Math.floor(max/2)+1,max];
   p.hp=values[Math.floor(i/512)%values.length];
  }
  p.summonData.ability=state>=24?AbilityId.BALL_FETCH:undefined;
  p.isTerastallized=state>=16;p.teraType=rnd(19);p.stellarTypesBoosted=state>=16?[1,2]:[];p.generateName();
  const snapshot=()=>[p.formIndex,p.abilityIndex,p.getAbility().id,p.getAbility(true).id,p.hp,+p.isTerastallized,p.stellarTypesBoosted.length,...p.stats];
  rows.push(`C ${i} ${p.species.speciesId} ${mode} ${p.level} ${p.nature} ${p.customPokemonData.ability} ${p.summonData.ability??-1} ${p.teraType} ${+(state>=8&&state<16)} ${+(state>=16&&state<24&&mode===0)}`,
    `IV ${p.ivs.join(" ")}`,`W ${scene.arena.weatherType} ${scene.currentBattle.lastUsedPokeball??-1} ${actors[1-i%2].getAbility().id} ${actors[1-i%2].passive?actors[1-i%2].getPassiveAbility().id:0} ${actors[1-i%2].hp} ${+actors[1-i%2].isOnField()} ${+actors[1-i%2].summonData.abilitySuppressed}`,`I ${snapshot().join(" ")}`);
  const before=Phaser.Math.RND.state(),battleBefore=scene.currentBattle.battleSeedState;
  scene.phaseManager.clearAllPhases();texts=[];p.visible=false;
  if(state>=16&&state<24&&mode===0)applyOnGainAbAttrs({pokemon:p,simulated:false});
  else applyAbAttrs(["PostSummonAbAttr","PostTurnAbAttr","PostBattleInitAbAttr","PostFaintAbAttr"][mode] as any,{pokemon:p,simulated:state>=8&&state<16});
  const queue=scene.phaseManager.phaseQueue.levels.flat();
  rows.push(`Q ${queue.length}`, ...queue.map((ph:any)=>`P ${ph.phaseName} ${ph.phaseName==="QuietFormChangePhase"?sp.forms.findIndex(f=>f.formKey===ph.formChange.formKey):-1} ${ph.phaseName==="ShowAbilityPhase"?+ph.passive:-1} ${ph.phaseName==="MessagePhase"?Buffer.from(ph.text,"utf8").toString("hex"):"-"}`));
  if(runPhase)for(const ph of queue.filter((ph:any)=>ph.phaseName==="QuietFormChangePhase")){scene.phaseManager.currentPhase=ph;await ph.start();}
  if(before!==Phaser.Math.RND.state()||battleBefore!==scene.currentBattle.battleSeedState)throw Error("unexpected form API RNG");
  const applied=[...p.turnData.formChangeAbilitiesApplied];
  if(applied.length>16||texts.length>1)throw Error("phase bounds");
  rows.push(`O ${snapshot().join(" ")}`,`F ${applied.length} ${Array.from({length:16},(_,i)=>applied[i]??0).join(" ")}`,
    `T ${texts.length?Buffer.from(texts[0],"utf8").toString("hex"):"-"}`);
 }
 rows.push(`${label}_COMPLETE ${N}`);fs.writeFileSync(process.env.NATIVEFORM_OUT??"/tmp/native-forms.txt",rows.join("\n")+"\n");console.log(`NATIVE_FORM cases=${N} states=${states.join("/")}`);
},600000);
