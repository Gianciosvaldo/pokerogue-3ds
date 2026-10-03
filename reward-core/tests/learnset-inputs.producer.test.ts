/* Additive Task 91 probe: simultaneous real TM and relearner caller inputs. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves, modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { GameModes } from "#enums/game-modes";
import { getGameMode } from "#app/game-mode";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";

it("separate TM exclusions and relearner inputs", async () => {
  const n = Number(process.env.DIFF_N ?? 2000);
  const originalSeed = Number(process.env.DIFF_SEED ?? 777);
  let seed = originalSeed >>> 0;
  const rnd = (k: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % k;
  };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).ability(AbilityId.BALL_FETCH)
    .enemyAbility(AbilityId.BALL_FETCH).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene: any = game.scene;
  const ids = Object.keys(speciesDataRegistry.data).map(Number);
  const moves = allMoves.filter((m: any) => m.id > 0).map((m: any) => m.id);
  const state = () => Phaser.Math.RND.state().split(",").slice(1).join(" ");
  const lines = [`V 1 ${n}`];
  let different = 0, fusion = 0, overrides = 0, empty = 0;
  for (let c = 0; c < n; c++) {
    scene.gameMode = getGameMode(c % 3 === 0 ? GameModes.DAILY : GameModes.CLASSIC);
    scene.currentBattle.gameMode = scene.gameMode;
    const species: any = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
    const p: any = scene.addPlayerPokemon(species, 1 + rnd(100));
    p.formIndex = species.forms.length ? rnd(species.forms.length) : 0;
    if (rnd(2)) {
      p.fusionSpecies = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
      p.fusionFormIndex = p.fusionSpecies.forms.length ? rnd(p.fusionSpecies.forms.length) : 0;
      fusion++;
    }
    p.metSpecies = species.speciesId;
    p.metBiome = rnd(2) ? -1 : 0;
    const tmRows: any[] = p.getLevelMoves({ includeEvolutionMoves: true, includeRelearnerMoves: true });
    const memoryRows: any[] = p.getLevelMoves({ startingLevel: 1, includeEvolutionMoves: true,
      includePrevolutionMoves: true, includeRelearnerMoves: true });
    if (JSON.stringify(tmRows) !== JSON.stringify(memoryRows)) different++;
    if (!memoryRows.length) empty++;
    p.moveset = [];
    for (let i = 0; i < 1 + rnd(4); i++) {
      const candidate = memoryRows.length && rnd(2) ? memoryRows[rnd(memoryRows.length)][1] : moves[rnd(moves.length)];
      if (!p.moveset.some((m: any) => m.moveId === candidate)) p.moveset.push(new PokemonMove(candidate));
    }
    p.usedTMs = Array.from({ length: rnd(5) }, () => moves[rnd(moves.length)]);
    p.resetSummonData();
    if (rnd(2)) {
      p.summonData.moveset = Array.from({ length: 1 + rnd(4) }, () => new PokemonMove(moves[rnd(moves.length)]));
      overrides++;
    }
    const eggs = p.getUnlockedEggMoves();
    const expected = p.getLearnableLevelMoves();
    const row = (tag: string, list: any[]) => lines.push(tag + " " + [list.length, ...list.flatMap(x => [x[0] ?? -1, x[1], x[2]])].join(" "));
    lines.push("C " + [scene.gameMode.modeId, p.species.speciesId, p.formIndex,
      p.fusionSpecies?.speciesId ?? -1, p.fusionFormIndex ?? 0, Number(p.metBiome === -1)].join(" "));
    lines.push("B " + [p.moveset.length, ...p.moveset.map((m: any) => m.moveId)].join(" "));
    lines.push("S " + [p.getMoveset().length, ...p.getMoveset().map((m: any) => m.moveId)].join(" "));
    row("L", tmRows); row("R", memoryRows);
    lines.push("E " + [eggs.length, ...eggs].join(" "));
    lines.push("U " + [p.usedTMs.length, ...p.usedTMs].join(" "));
    row("M", expected);
    for (const key of ["TM_COMMON", "TM_GREAT", "TM_ULTRA"]) {
      Phaser.Math.RND.sow([`dual${originalSeed}_${c}_${key}`]);
      const before = state();
      const type: any = (modifierTypes as any)[key]().generateType([p]);
      lines.push("T " + [before, type ? type.moveId : -2, state()].join(" "));
    }
    p.destroy();
  }
  lines.push(`Z ${n}`);
  fs.writeFileSync(process.env.DIFF_OUT ?? "/tmp/reward-learnset-inputs.txt", lines.join("\n") + "\n");
  console.log(`dual learnset inputs: ${n} cases, distinct lists ${different}, fusion ${fusion}, summon moveset overrides ${overrides}, empty relearn lists ${empty}; thrown/skipped 0`);
}, 120000);
