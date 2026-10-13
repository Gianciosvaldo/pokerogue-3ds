/* Actual pinned Pokemon.getAbility getter; only input state is configured. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { GameModes } from "#enums/game-modes";
import { getGameMode } from "#app/game-mode";
import { getDailyEventSeedBoss } from "#data/daily-seed/daily-run";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";
it("actual ability lookup",async()=>{
 const N=Number(process.env.ABLOOKUP_N??3000),SEED=Number(process.env.ABLOOKUP_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<64)throw Error("Node22 and N>=64 required");
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await game.classicMode.startBattle(SpeciesId.DITTO);
 const scene:any=game.scene,actors:any[]=[game.field.getPlayerPokemon(),game.field.getEnemyPokemon()];
 const ids=Object.keys(speciesDataRegistry.data).map(Number);let seed=SEED;
 const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const form=()=>{const sp=speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);return{sp,index:rnd(sp.forms.length||1)};};
 const rows=[`ABILITY_LOOKUP 1 ${N}`];const states=Array(32).fill(0);
 for(let i=0;i<N;i++){
  const state=Math.floor(i/2)%32,p=actors[i%2];states[state]++;
  game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
  p.resetSummonData();p.resetBattleAndWaveData();p.fusionSpecies=null;p.fusionCustomPokemonData=null;
  const raw=form(),temporary=form(),fusion=form(),fusionTemporary=form();
  p.species=raw.sp;p.formIndex=raw.index;p.abilityIndex=rnd(3);p.customPokemonData.ability=-1;p.bossSegments=0;
  p.fusionAbilityIndex=rnd(3);p.customPokemonData.passive=AbilityId.NONE;p.passive=false;
  let custom=-1,fcustom=-1,fsp=0,fform=0,tsp=0,tform=0,ftsp=0,ftform=0,override=-1,temp=-1,daily=-1;
  if(state>=4&&state<12){p.summonData.speciesForm=temporary.sp.forms.length?temporary.sp.forms[temporary.index]:temporary.sp;tsp=temporary.sp.speciesId;tform=temporary.index;}
  if(state>=12&&state<24){p.fusionSpecies=fusion.sp;p.fusionFormIndex=fusion.index;fsp=fusion.sp.speciesId;fform=fusion.index;}
  if(state>=16&&state<20){p.summonData.fusionSpeciesForm=fusionTemporary.sp.forms.length?fusionTemporary.sp.forms[fusionTemporary.index]:fusionTemporary.sp;ftsp=fusionTemporary.sp.speciesId;ftform=fusionTemporary.index;}
  if([2,6,13,17,21,26].includes(state)){custom=0;p.customPokemonData.ability=custom;}
  if([3,7,14,18,22,27].includes(state)){custom=rnd(319)+1;p.customPokemonData.ability=custom;}
  if([15,19,23].includes(state)){fcustom=state===19?0:rnd(319)+1;p.fusionCustomPokemonData={ability:fcustom};}
  if([1,5,9,12,16,20,24,28].includes(state)){temp=0;p.summonData.ability=0;}
  if([8,10,11,25,29].includes(state)){temp=rnd(319)+1;p.summonData.ability=temp;}
  if([10,11,22,23].includes(state)){override=rnd(319)+1;if(p.isPlayer())game.override.ability(override);else game.override.enemyAbility(override);}
  scene.gameMode=getGameMode(state>=28?GameModes.DAILY:GameModes.CLASSIC);scene.currentBattle.waveIndex=state>=28?50:3;
  if(state>=28){p.bossSegments=p.isPlayer()?0:2;scene.gameMode.dailyConfig={boss:{speciesId:SpeciesId.RATTATA,ability:state===28?0:rnd(319)+1}};if(p.isBoss())daily=getDailyEventSeedBoss()?.ability??-1;}
  const before=Phaser.Math.RND.state(),battleBefore=scene.currentBattle.battleSeedState;
  const effective=p.getAbility().id,permanent=p.getAbility(true).id;
  if(before!==Phaser.Math.RND.state()||battleBefore!==scene.currentBattle.battleSeedState)throw Error("getter consumed RNG");
  rows.push(`C ${i} ${p.species.speciesId} ${p.formIndex} ${p.abilityIndex} ${fsp} ${fform} ${p.fusionAbilityIndex} ${custom} ${fcustom} ${override} ${temp} ${tsp} ${tform} ${ftsp} ${ftform} ${daily} ${effective} ${permanent}`);
 }
 rows.push(`ABILITY_LOOKUP_COMPLETE ${N}`);fs.writeFileSync(process.env.ABLOOKUP_OUT??"/tmp/ability-lookup.txt",rows.join("\n")+"\n");console.log(`ABILITY_LOOKUP cases=${N} states=${states.join("/")}`);
},600000);
