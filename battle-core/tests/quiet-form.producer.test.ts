/* Actual pinned QuietFormChangePhase; deterministic NONE/Wonder Guard/Ball Fetch actors isolate state, index, callback-set and text semantics. Trigger selection is separate. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { QuietFormChangePhase } from "#phases/quiet-form-change-phase";
import { SpeciesFormChange } from "#data/pokemon-forms";
import { SpeciesFormChangeAbilityTrigger } from "#data/form-change-triggers";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual quiet form phase state and text",async()=>{
 const N=Number(process.env.QUIETFORM_N??2000),SEED=Number(process.env.QUIETFORM_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<64)throw Error("Node22 and N>=64 required");
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
 const species=Object.keys(speciesDataRegistry.data).map(Number).map(id=>speciesDataRegistry.getSpecies(id)).filter(p=>p.forms.length>1);
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const rows=[`QUIET_FORM 1 ${N}`],states=Array(32).fill(0);
 for(let i=0;i<N;i++){
  const state=Math.floor(i/2)%32,p=actors[i%2],sp=species[rnd(species.length)],from=rnd(sp.forms.length),to=rnd(sp.forms.length);states[state]++;
  p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=sp;p.formIndex=from;p.abilityIndex=state%3;
  p.fusionSpecies=null;p.customPokemonData.ability=state%3===0?0:state%3===1?AbilityId.WONDER_GUARD:AbilityId.BALL_FETCH;
  p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.nature=rnd(25);p.level=1+rnd(100);p.ivs=Array.from({length:6},()=>rnd(32));
  p.isTerastallized=false;p.status=null;p.stats=Array(6).fill(0);p.hp=0;
  scene.modifiers=[];scene.enemyModifiers=[];scene.arena.weather=null;scene.arena.terrain=null;scene.arena.tags=[];
  p.calculateStats();p.hp=rnd(p.getMaxHp()+1);p.summonData.ability=state%2?0:AbilityId.BALL_FETCH;
  p.isTerastallized=state>=16;p.teraType=rnd(19);p.stellarTypesBoosted=state>=16?[1,2]:[];p.generateName();
  const snapshot=()=>[p.formIndex,p.abilityIndex,p.getAbility().id,p.getAbility(true).id,p.hp,+p.isTerastallized,p.stellarTypesBoosted.length,...p.stats];
  rows.push(`C ${i} ${p.species.speciesId} ${to} ${p.level} ${p.nature} ${p.customPokemonData.ability} ${p.summonData.ability??-1} ${p.teraType}`,
    `IV ${p.ivs.join(" ")}`,`I ${snapshot().join(" ")}`);
  const before=Phaser.Math.RND.state(),battleBefore=scene.currentBattle.battleSeedState;
  scene.phaseManager.clearAllPhases();texts=[];p.visible=false;
  const phase=new QuietFormChangePhase(p,new SpeciesFormChange({speciesId:sp.speciesId,preFormKey:sp.forms[from].formKey,evoFormKey:sp.forms[to].formKey,trigger:new SpeciesFormChangeAbilityTrigger(),quiet:true}));
  scene.phaseManager.currentPhase=phase;await phase.start();
  if(before!==Phaser.Math.RND.state()||battleBefore!==scene.currentBattle.battleSeedState)throw Error("unexpected form API RNG");
  const applied=[...p.turnData.formChangeAbilitiesApplied];
  if(applied.length>16||texts.length>1)throw Error("phase bounds");
  rows.push(`O ${snapshot().join(" ")}`,`F ${applied.length} ${Array.from({length:16},(_,i)=>applied[i]??0).join(" ")}`,
    `T ${texts.length?Buffer.from(texts[0],"utf8").toString("hex"):"-"}`);
 }
 rows.push(`QUIET_FORM_COMPLETE ${N}`);fs.writeFileSync(process.env.QUIETFORM_OUT??"/tmp/form-lifecycle.txt",rows.join("\n")+"\n");console.log(`QUIET_FORM cases=${N} states=${states.join("/")}`);
},600000);
