/* Task 63 NEW mode: the real EnemyPokemon moveset method on real Pokemon.
 * Constructor draws are outside the measured generation boundary. */
import { activeOverrides } from "#app/overrides";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { Challenges } from "#enums/challenges";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { TrainerType } from "#enums/trainer-type";
import { TrainerVariant } from "#enums/trainer-variant";
import { Trainer } from "#field/trainer";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("bounded enemy moveset producer", async () => {
  if (process.env.DIFF_MOVESET !== "1") throw new Error("requires DIFF_MOVESET=1");
  const n=Number(process.env.DIFF_N ?? 2000), initialSeed=Number(process.env.DIFF_SEED ?? 777) >>> 0;
  const out=process.env.DIFF_OUT;
  if (!out || !Number.isInteger(n) || n<1) throw new Error("invalid producer arguments");
  const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene=game.scene as any, battle=scene.currentBattle as any;
  for (const [key,value] of [
    ["ENEMY_LEVEL_OVERRIDE",0],["ENEMY_SPECIES_OVERRIDE",null],["ENEMY_FUSION_OVERRIDE",false],
    ["ENEMY_IVS_OVERRIDE",null],["ENEMY_NATURE_OVERRIDE",null],["ENEMY_GENDER_OVERRIDE",null],
    ["ENEMY_SHINY_OVERRIDE",false],["ENEMY_FORM_OVERRIDES",{}],["ENEMY_MOVESET_OVERRIDE",[]],
  ] as [string,any][]) vi.spyOn(activeOverrides as any,key,"get").mockReturnValue(value);
  const trainer=new Trainer(TrainerType.YOUNGSTER,TrainerVariant.DEFAULT);
  let seed=initialSeed;
  const rnd=(m:number)=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed%m;};
  const state=()=>Phaser.Math.RND.state().split(",").slice(1).join(" ");
  const species=[SpeciesId.CATERPIE,SpeciesId.WEEDLE,SpeciesId.MAGIKARP,SpeciesId.DITTO,SpeciesId.SMEARGLE,SpeciesId.ETERNATUS];
  const lines=[`# MOVESET 1 ${n} ${initialSeed} 8555c08c823b856cbec4eb99ca84ea52a955836d`];
  for (let i=0;i<n;i++) {
    const id=species[i%6], hasTrainer=Math.floor(i/6)%2!==0, boss=Math.floor(i/12)%2!==0;
    const form=id===SpeciesId.ETERNATUS ? Math.floor(i/24)%2 : 0;
    const inverse=id===SpeciesId.ETERNATUS && Math.floor(i/48)%2!==0;
    scene.gameMode.setChallengeValue(Challenges.INVERSE_BATTLE,inverse ? 1 : 0);
    const level=id===SpeciesId.MAGIKARP || (hasTrainer && [SpeciesId.CATERPIE,SpeciesId.WEEDLE].includes(id))
      ? 1+rnd(24) : id===SpeciesId.DITTO && hasTrainer ? 1+rnd(79) : 1+rnd(220);
    const wave=1+rnd(300);
    battle.waveIndex=wave; battle.battleType=hasTrainer ? 1 : 0;
    battle.trainer=hasTrainer ? trainer : null; battle.enemyParty=[];
    scene.modifiers.length=0; scene.enemyModifiers.length=0;
    const p:any=scene.addEnemyPokemon(speciesDataRegistry.getSpecies(id),level,hasTrainer ? 1 : 0,boss,true);
    p.formIndex=form; p.calculateStats(); battle.enemyParty=[p];
    const atk=p.getStat(Stat.ATK), spa=p.getStat(Stat.SPATK);
    Phaser.Math.RND.sow([`moveset-${initialSeed}-${i}-${rnd(0x10000000)}`]);
    const before=state();
    p.generateAndPopulateMoveset();
    const moves=p.moveset.map((m:any)=>`${m.moveId} ${m.getMovePp()} ${m.ppUp} ${m.ppUsed}`).join(" ");
    lines.push(`M ${id} ${form} ${level} ${p.getAbility().id} ${atk} ${spa} ${+hasTrainer} ${+boss} ${+inverse} ${wave} ${before} ${p.moveset.length} ${moves} ${state()}`);
    p.destroy(); battle.enemyParty=[];
    vi.clearAllMocks(); // release destroyed mock receivers; implementations stay intact
  }
  trainer.destroy(); lines.push(`END ${n} ${initialSeed}`);
  fs.writeFileSync(out,lines.join("\n")+"\n");
},1_800_000);
