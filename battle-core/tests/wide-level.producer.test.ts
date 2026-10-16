/* Bounded actual calculateStats API proof, not a complete Endless run. */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getGameMode } from "#app/game-mode";
import { GameModes } from "#enums/game-modes";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";
it("actual wide-level permanent stats",async()=>{
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const nativeBattleRng=BattleScene.prototype.randBattleSeedInt;
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 const scene:any=game.scene;scene.randBattleSeedInt=nativeBattleRng;
 game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const ps:any[]=[game.field.getPlayerPokemon(),game.field.getEnemyPokemon()];
 scene.modifiers=[];scene.enemyModifiers=[];scene.arena.tags=[];scene.arena.weather=null;scene.arena.terrain=null;
 const species=[SpeciesId.BULBASAUR,SpeciesId.SHEDINJA,SpeciesId.CHARIZARD,SpeciesId.MEWTWO,SpeciesId.SHUCKLE,SpeciesId.BLISSEY,SpeciesId.MAGIKARP,SpeciesId.DEOXYS,SpeciesId.KYOGRE,SpeciesId.SNORLAX,SpeciesId.CASTFORM,SpeciesId.CHERRIM];
 const levels=[1,199,200,201,254,255,256,257,511,1000,65535,65536,100000,1000000];
 const N=16800,rows=[`WIDE_LEVEL 1 ${N}`];let seed=777;
 const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 for(let i=0;i<N;i++){
  const p=ps[i%2],level=levels[i%levels.length],mode=i%3===0?GameModes.SPLICED_ENDLESS:GameModes.ENDLESS;
  p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();
  p.species=speciesDataRegistry.getSpecies(species[rnd(species.length)]);p.formIndex=rnd(Math.max(1,p.species.forms.length));
  p.fusionSpecies=i%4===0?speciesDataRegistry.getSpecies(SpeciesId.PIKACHU):null;p.fusionFormIndex=0;p.fusionAbilityIndex=0;p.fusionCustomPokemonData=null;
  p.abilityIndex=0;p.customPokemonData.ability=-1;p.customPokemonData.passive=AbilityId.WONDER_GUARD;p.passive=i%7===0;
  p.nature=Math.floor(i/levels.length)%25;p.level=level;p.ivs=Array.from({length:6},()=>rnd(32));
  scene.gameMode=getGameMode(mode);p.stats=[123,10,10,10,10,10];
  const hpMode=Math.floor(i/2)%4;p.hp=[undefined,0,60,2000000000][hpMode];
  const raw=p.getAbility(true).id,passive=p.passive?p.getPassiveAbility().id:0;
  Phaser.Math.RND.sow([`wide-level-${i}`]);scene.currentBattle.battleSeedState=null;scene.randBattleSeedInt(2);
  const before=JSON.stringify([Phaser.Math.RND.state(),scene.currentBattle.battleSeedState]);
  rows.push(`C ${i} ${mode} ${p.species.speciesId} ${p.formIndex} ${p.fusionSpecies?.speciesId??0} ${level} ${p.getNature()} ${raw} ${passive} ${hpMode} ${+p.isPlayer()}`,`I ${p.ivs.join(" ")}`);
  p.calculateStats();
  if(before!==JSON.stringify([Phaser.Math.RND.state(),scene.currentBattle.battleSeedState]))throw Error("stats consumed RNG");
  if(![...p.stats,p.hp].every(Number.isSafeInteger))throw Error("noninteger stats");
  rows.push(`O ${p.hp} ${p.stats.join(" ")}`,"E");
 }
 rows.push(`WIDE_LEVEL_COMPLETE ${N}`);fs.writeFileSync(process.env.WIDELEVEL_OUT??"/tmp/wide-level.txt",rows.join("\n")+"\n");
},600000);
