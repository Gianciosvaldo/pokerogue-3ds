/*
 * Upstream half of Task 43 Memory Mushroom differential test.
 * Adds a separate mode; existing reward producers are intentionally untouched.
 */
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { speciesEggMoves } from "#balance/egg-moves";
import { ChallengeType } from "#enums/challenge-type";
import { Challenges } from "#enums/challenges";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { LearnMoveType } from "#enums/learn-move-type";
import { LearnableMoveSource } from "#enums/learnable-move-source";
import { allMoves, modifierTypes } from "#data/data-lists";
import { PokemonMove } from "#moves/pokemon-move";
import { applyChallenges } from "#utils/challenge-utils";
import { ValueHolder } from "#utils/value-holder";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("memory mushroom producer", async () => {
  const N = Number(process.env.DIFF_N ?? 4000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-memory-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;

  let seed = Number(process.env.DIFF_SEED ?? 12345) >>> 0;
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const pick = <T>(v: readonly T[]): T => v[rnd(v.length)];
  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  const moveIds = allMoves.map((m: any) => m.id).filter((id: number) => id > 0);
  const modes = [GameModes.CLASSIC, GameModes.DAILY, GameModes.CHALLENGE];
  const lines: string[] = [];

  for (let c = 0; c < N; c++) {
    const gm = getGameMode(pick(modes));
    if (gm.modeId === GameModes.CHALLENGE) {
      gm.setChallengeValue(Challenges.FRESH_START, rnd(3) === 0 ? 1 : 0);
    }
    scene.gameMode = gm;
    scene.currentBattle.gameMode = gm;

    const sid = pick(speciesIds);
    const sp: any = speciesDataRegistry.getSpecies(sid);
    const p: any = scene.addPlayerPokemon(sp, 1 + rnd(100));
    p.formIndex = sp.forms.length ? rnd(sp.forms.length) : 0;
    p.metSpecies = sid;
    p.metBiome = rnd(2) ? -1 : 0;

    const root = sp.getRootSpeciesId();
    if (scene.gameData.starterData[root] && speciesEggMoves[root]) {
      scene.gameData.starterData[root].eggMoves = rnd(2) ? 15 : rnd(16);
    }

    const levelRows: any[] = p.getLevelMoves({
      startingLevel: 1,
      includeEvolutionMoves: true,
      includePrevolutionMoves: true,
      includeRelearnerMoves: true,
    });
    const unlockedEgg: number[] = (p as any).getUnlockedEggMoves();

    const nm = 1 + rnd(4);
    p.moveset = [];
    for (let i = 0; i < nm; i++) {
      const candidate = rnd(3) === 0 && levelRows.length ? pick(levelRows)[1] : pick(moveIds);
      if (!p.moveset.some((m: any) => m.moveId === candidate)) p.moveset.push(new PokemonMove(candidate));
    }
    if (!p.moveset.length) p.moveset.push(new PokemonMove(MoveId.TACKLE));
    p.usedTMs = Array.from({ length: rnd(5) }, () => pick(moveIds));

    const eggAllowed = new ValueHolder(true);
    applyChallenges(ChallengeType.EGG_MOVE_RELEARN_AVAILABILITY, p, eggAllowed);
    const expected: any[] = p.getLearnableLevelMoves();

    let chosen = expected.length ? rnd(expected.length) : -1;
    const cost = 100 + rnd(5000);
    let queuedParty = expected.length > 0 ? 1 : 0;
    let queuedMove = -1, queuedType = -1, queuedCost = -1;

    if (chosen >= 0) {
      const typ: any = (modifierTypes as any).MEMORY_MUSHROOM();
      const mod: any = typ.newModifier(p, chosen);
      const old = scene.phaseManager.unshiftNew.bind(scene.phaseManager);
      const spy = vi.spyOn(scene.phaseManager, "unshiftNew").mockImplementation((name: any, ...args: any[]) => {
        if (name === "LearnMovePhase") {
          queuedMove = args[1];
          queuedType = args[2];
          queuedCost = args[3] ?? -1;
        }
        return undefined as any;
      });
      mod.apply(p, cost);
      spy.mockRestore();
      // keep TS happy and make the expected enum explicit in the case
      if (queuedType !== LearnMoveType.MEMORY) throw new Error("Memory Mushroom did not queue MEMORY learn phase");
      void old;
    }

    lines.push([
      "C", gm.modeId, p.metBiome === -1 ? 1 : 0, eggAllowed.value ? 1 : 0,
      p.moveset.length, ...p.moveset.map((m: any) => m.moveId),
      levelRows.length, ...levelRows.flatMap((x: any) => [x[0] ?? -1, x[1], x[2] ?? LearnableMoveSource.LEVEL]),
      unlockedEgg.length, ...unlockedEgg,
      p.usedTMs.length, ...p.usedTMs,
      expected.length, ...expected.flatMap((x: any) => [x[0] ?? -1, x[1], x[2]]),
      chosen, cost, queuedParty, queuedMove, queuedType, queuedCost,
    ].join(" "));

    scene.getPlayerParty().pop();
    p.destroy();
  }

  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
