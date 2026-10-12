/* Numeric API proof of actual BattleScene.addFaintedEnemyScore, not full turns.
 * Item objects exercise actual exported classes/getters; acquisition is not tested. */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { GameManager } from "#test/framework/game-manager";
import { AttackTypeBoosterModifier, BaseStatModifier, PokemonBaseStatTotalModifier,
  PokemonBaseStatFlatModifier, PokemonIncrementingStatModifier, TurnHealModifier } from "#modifiers/modifier";
import { AttackTypeBoosterModifierType } from "#modifiers/modifier-type";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("actual enemy score API", async () => {
  if (!process.versions.node.startsWith("22.")) throw new Error("Node22 required");
  const N=Number(process.env.BOSSSCORE_N??1000), SEED=Number(process.env.BOSSSCORE_SEED??777);
  if(!Number.isSafeInteger(N)||N<100)throw new Error("N>=100 required");
  const LEVEL_MAX=Number(process.env.BOSSSCORE_LEVEL_MAX??1000);
  if(!Number.isSafeInteger(LEVEL_MAX)||LEVEL_MAX<1||LEVEL_MAX>1000)throw new Error("invalid level range");
  let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
  const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
  game.override.battleStyle("single").enemySpecies(SpeciesId.RATTATA).enemyMoveset(MoveId.SPLASH)
    .moveset(MoveId.SPLASH).ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
    .disableTrainerWaves().mysteryEncounterChance(0);
  await game.classicMode.startBattle(SpeciesId.RATTATA);
  const scene:any=game.scene,enemy:any=scene.getEnemyField()[0],lead:any=scene.getPlayerField()[0],battle=scene.currentBattle;
  const rows=[`BOSS_SCORE 1 ${N}`], classes=Array(6).fill(0),segments=Array(9).fill(0);
  const species=[SpeciesId.RATTATA,SpeciesId.MAGIKARP,SpeciesId.SNORLAX,SpeciesId.MEWTWO,SpeciesId.CHARIZARD,SpeciesId.KYOGRE];
  const mt:any=new AttackTypeBoosterModifierType(0,20);
  for(let i=0;i<N;i++){
    const sp=species[rnd(species.length)],form=(sp===SpeciesId.MEWTWO||sp===SpeciesId.CHARIZARD||sp===SpeciesId.KYOGRE)?rnd(2):0;
    enemy.species=speciesDataRegistry.getSpecies(sp);enemy.formIndex=form;enemy.level=1+rnd(LEVEL_MAX);
    enemy.ivs=Array.from({length:6},()=>rnd(32));enemy.bossSegments=[0,1,2,3,4,5,8][i%7];segments[enemy.bossSegments]++;
    battle.waveIndex=[1,9,10,49,99,199,299][rnd(7)];battle.battleScore=rnd(1000)/4;
    scene.enemyModifiers.length=0;scene.modifiers.length=0;
    const mods:any[]=[];const n=i%9;
    for(let j=0;j<n;j++){
      const cls=(i+j)%6,stack=1+rnd(cls===0?99:2),side=j%4===3?0:1,owner=j%5===4?lead.id:enemy.id;
      const constructors=[()=>new AttackTypeBoosterModifier(mt,owner,0,20,stack),
        ()=>new BaseStatModifier(mt,owner,Stat.ATK,stack),()=>new PokemonBaseStatTotalModifier(mt,owner,10,stack),
        ()=>new PokemonBaseStatFlatModifier(mt,owner,stack),()=>new PokemonIncrementingStatModifier(mt,owner,stack),
        ()=>new TurnHealModifier(mt,owner,6.25,stack)];
      const mod:any=constructors[cls]();(side?scene.enemyModifiers:scene.modifiers).push(mod);
      mods.push([cls,side,+(owner===enemy.id),stack]);classes[cls]++;
    }
    const cap=scene.getMaxExpLevel(),initial=battle.battleScore;
    const before=[Phaser.Math.RND.state(),battle.battleSeedState].join("/");
    BattleScene.prototype.addFaintedEnemyScore.call(scene,enemy);
    if(before!==[Phaser.Math.RND.state(),battle.battleSeedState].join("/"))throw new Error("score consumed RNG");
    rows.push(`C ${i} ${sp} ${form} ${enemy.level} ${cap} ${battle.waveIndex} ${enemy.bossSegments} ${initial} ${battle.battleScore} ${mods.length}`);
    rows.push(`I ${enemy.ivs.join(" ")}`);for(const m of mods)rows.push(`M ${m.join(" ")}`);rows.push("E");
  }
  if(classes.some(x=>!x)||[0,1,2,3,4,5,8].some(x=>!segments[x]))throw new Error("missing numeric boundary");
  rows.push(`BOSS_SCORE_COMPLETE ${N}`);fs.writeFileSync(process.env.BOSSSCORE_OUT??"/tmp/boss-score.txt",rows.join("\n")+"\n");
  console.log(`BOSSSCORE cases=${N} classes=${classes.join("/")} segments=${segments.join("/")}`);
},600000);
