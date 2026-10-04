/* Additive focused oracle. Actual trigger selection and ability dispatch at 8555c08. */
import { globalScene } from "#app/global-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { applyAbAttrs } from "#abilities/apply-ab-attrs";
import { SpeciesFormChangeWeatherTrigger, SpeciesFormChangeRevertWeatherFormTrigger, SpeciesFormChangeAbilityTrigger, SpeciesFormChangeActiveTrigger } from "#data/form-change-triggers";
import { Weather } from "#data/weather";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import { it, expect, vi } from "vitest";
import * as fs from "node:fs";

it("weather form trigger and callback producer", async () => {
 const n=Number(process.env.WFORMS_N??2000);let seed=Number(process.env.WFORMS_SEED??777)>>>0;
 const rnd=(k:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%k;};
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
  .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH).startingWave(5);
 await game.classicMode.startBattle(SpeciesId.CASTFORM);
 game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 const p:any=game.field.getPlayerPokemon(),enemy:any=game.field.getEnemyPokemon();
 // Intercept only insertion, retaining real phase objects and trigger selection.
 let queue:any[]=[];
 vi.spyOn(globalScene.phaseManager,"unshiftPhase").mockImplementation((ph:any)=>{queue.unshift(ph);});
 p.loadAssets=async()=>{};p.updateInfo=async()=>{};p.visible=false;
 vi.spyOn(globalScene,"updateFieldScale").mockImplementation(async()=>{});
 vi.spyOn(globalScene.phaseManager,"shiftPhase").mockImplementation(()=>{});
 let messages:string[]=[];
 vi.spyOn(globalScene.ui,"showText").mockImplementation((text:any,_a:any,cb:any)=>{messages.push(text);cb?.();});
 const ids=[SpeciesId.CASTFORM,SpeciesId.CHERRIM,SpeciesId.EISCUE];
 const abs=[AbilityId.FORECAST,AbilityId.FLOWER_GIFT,AbilityId.ICE_FACE];
 const counts=[0,0,0],modes=Array(9).fill(0);const lines=[`V 1 ${n}`];
 for(let i=0;i<n;i++) {
  const kind=Math.floor(i/9)%3,sp=ids[kind],base=abs[kind],form=rnd(kind===0?4:2),weather=rnd(10),supp=rnd(4)===0?1:0;
  const passive=rnd(3)===0?base:0,override=rnd(4)===0?AbilityId.BALL_FETCH:0,cloud=rnd(4)===0?1:0,mode=i%9,oldWeather=rnd(10);
  p.resetTurnData();
  p.species=speciesDataRegistry.getSpecies(sp);p.formIndex=form;p.fusionSpecies=null;p.fusionAbilityIndex=-1;
  p.abilityIndex=0;p.customPokemonData.ability=-1;p.customPokemonData.passive=passive;
  const naturalPassive=rnd(4)===0;
  if(naturalPassive)p.customPokemonData.passive=-1;
  p.passive=!!passive||naturalPassive;p.resetSummonData();p.summonData.abilitySuppressed=!!supp;
  p.summonData.ability=override;p.hp=100;
  enemy.summonData.ability=cloud?AbilityId.CLOUD_NINE:AbilityId.BALL_FETCH;enemy.hp=mode===8?0:100;
  p.switchOutStatus=mode===7;
  globalScene.arena.weather=(mode===4?oldWeather:weather)?new Weather(mode===4?oldWeather:weather):null;
  p.generateName();p.calculateStats();
  const initial=[p.level,p.nature,...p.ivs,...p.stats,p.hp];
  queue=[];const rng=Phaser.Math.RND.state();
  if(mode===0)globalScene.triggerPokemonFormChange(p,SpeciesFormChangeWeatherTrigger);
  else if(mode===1)globalScene.triggerPokemonFormChange(p,SpeciesFormChangeRevertWeatherFormTrigger);
  else if(mode===2)applyAbAttrs("PostWeatherChangeAbAttr",{pokemon:p,weather});
  else if(mode===3)applyAbAttrs("PostSummonFormChangeByWeatherAbAttr",{pokemon:p});
  else if(mode===4)globalScene.arena.trySetWeather(weather);
  else if(mode===5)applyAbAttrs("PostSummonWeatherSuppressedFormChangeAbAttr",{pokemon:enemy});
  else if(mode===6)globalScene.arena.triggerWeatherBasedFormChanges(enemy);
  else if(mode===7)globalScene.triggerPokemonFormChange(p,SpeciesFormChangeActiveTrigger);
  else applyAbAttrs("PostFaintAbAttr",{pokemon:enemy});
  const forms=queue.filter(ph=>ph.phaseName==="QuietFormChangePhase");
  const targets=forms.map(ph=>p.species.forms.findIndex((f:any)=>f.formKey===ph.formChange.formKey));
  expect(p.formIndex).toBe(form);expect(Phaser.Math.RND.state()).toBe(rng);
  counts[kind]+=forms.length;modes[mode]+=forms.length;
  lines.push([sp,form,weather,supp,passive,override,cloud,mode,oldWeather,Number(p.hasPassive()),p.customPokemonData.passive,...initial,targets.length,...targets].join(" "));
  for(const ph of forms) {
   messages=[];await ph.start();
   const types=p.getTypes();const bytes=[...Buffer.from(messages.join("\n"),"utf8")];
   lines.push([p.formIndex,types.length,...types,...p.stats,p.hp,p.getAbility().id,globalScene.arena.weather?.weatherType??0,p.hasPassive()?p.getPassiveAbility().id:0,bytes.length,...bytes].join(" "));
  }
  expect(Phaser.Math.RND.state()).toBe(rng);
 }
 lines.push(`Z ${n}`);for(const c of counts)expect(c).toBeGreaterThan(0);for(const c of modes)expect(c).toBeGreaterThan(0);
 fs.writeFileSync(process.env.WFORMS_OUT??"/tmp/weatherforms.txt",lines.join("\n")+"\n");
 console.log(`Weather forms ${n} cases; queued Forecast/FlowerGift/IceFace ${counts}; modes ${modes}; RNG unchanged; thrown/skipped 0`);
},120000);
