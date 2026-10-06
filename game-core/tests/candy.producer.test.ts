/*
 * Upstream half of game-core's Rare Candy / Rarer Candy differential (tools/applydiff.sh copies it into upstream).
 * Runs upstream's own PokemonLevelIncrementModifier through BattleScene.addModifier, the way SelectModifierPhase
 * applies a reward, on random parties (species, level at, below and above the wave's level cap, exp, friendship at and
 * around the candy cap of 200), with 0-5 Candy Jars, on a random wave 1-199 in Classic or Endless, and records for the
 * party: the party menu's filter (modifierType.selectFilter), whether the modifier applied, each member's level, exp and
 * friendship after, the LevelUpPhases it unshifted (party index, old level, new level, in call order) and the
 * starter data (friendship progress and candy count) of every member's root species before and after.
 * Not copied from reward-core's apply producer: that one doesn't record levels, exp, friendship or starter data.
 */
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { getLevelTotalExp } from "#data/exp";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("candy apply producer", async () => {
  const N = Number(process.env.DIFF_N ?? 4000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/gc-candy-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  const starterData = scene.gameData.starterData as Record<number, any>;

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const pick = <T>(v: readonly T[]): T => v[rnd(v.length)];

  // only species whose root has starter data: every member of a real party has it
  const speciesIds = Object.keys(speciesDataRegistry.data)
    .map(Number)
    .filter(id => starterData[speciesDataRegistry.getSpecies(id).getRootSpeciesId()] !== undefined);

  const pool = Array.from({ length: 6 }, () => scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5));
  for (const pokemon of pool) vi.spyOn(pokemon, "playSprite").mockImplementation(() => {});
  const party = scene.getPlayerParty();

  // LevelUpPhase is only recorded, never run: the level-up screens are not what is compared here
  const levelUps: number[][] = [];
  const realUnshift = scene.phaseManager.unshiftNew.bind(scene.phaseManager);
  vi.spyOn(scene.phaseManager, "unshiftNew").mockImplementation((...args: any[]) => {
    if (args[0] === "LevelUpPhase") levelUps.push([args[1], args[2], args[3]]);
    else realUnshift(...args);
  });
  const candyBar = scene.candyBar;
  if (candyBar) vi.spyOn(candyBar, "showStarterSpeciesCandy").mockImplementation(() => {});

  const lines: string[] = [];
  let thrown = 0;
  const keys = ["RARE_CANDY", "RARER_CANDY"];

  for (let i = 0; i < N; i++) {
    try {
      const mode = rnd(3) === 0 ? GameModes.ENDLESS : GameModes.CLASSIC;
      const gm = getGameMode(mode);
      scene.gameMode = gm;
      battle.gameMode = gm;
      // battle-core's level is a byte: Endless waves past ~330 have level caps above 255, which game-core can't hold
      const wave = 1 + rnd(199);
      battle.waveIndex = wave;
      const cap = scene.getMaxExpLevel();

      const size = 1 + rnd(6);
      party.length = 0;
      for (let k = 0; k < size; k++) {
        const p = pool[k];
        p.resetSummonData();
        p.resetBattleAndWaveData();
        const sp = speciesDataRegistry.getSpecies(pick(speciesIds));
        p.species = sp;
        p.formIndex = 0;
        p.fusionSpecies = null;
        p.fusionFormIndex = 0;
        const r = rnd(5);
        p.level = r < 2 ? Math.max(1, cap - rnd(4) + rnd(4)) : r === 2 ? 1 + rnd(100) : r === 3 ? cap + rnd(30) : 1 + rnd(cap);
        p.exp = rnd(3) === 0 ? 0 : rnd(2000000);
        const f = rnd(6);
        p.friendship = f === 0 ? 0 : f === 1 ? 190 + rnd(12) : f === 2 ? 195 + rnd(10) : f === 3 ? 250 + rnd(6) : rnd(256);
        p.calculateStats();
        p.hp = p.getMaxHp();
        party.push(p);
      }

      scene.modifiers.length = 0;
      const stacks = rnd(2) === 0 ? 0 : 1 + rnd(5);
      if (stacks > 0) {
        const jar = (modifierTypes as any)["CANDY_JAR"];
        const t = jar().withIdFromFunc(jar);
        const m = t.newModifier();
        m.stackCount = stacks;
        scene.modifiers.push(m);
      }

      // starter data of each member's root species (several members can share one root: the first one's is used)
      const roots = party.map((p: any) => p.species.getRootSpeciesId());
      for (const root of new Set<number>(roots)) {
        starterData[root].friendship = rnd(3) === 0 ? 0 : rnd(40);
        starterData[root].candyCount = rnd(4) === 0 ? 0 : rnd(100);
      }

      const itemKey = keys[i % keys.length];
      const targetIdx = rnd(party.length);
      const target = party[targetIdx];
      const modifierType = (modifierTypes as any)[itemKey]();
      const filterMsg: string | null = modifierType.selectFilter ? modifierType.selectFilter(target) : null;

      lines.push(["C", itemKey, targetIdx, wave, mode, stacks, party.length].join(" "));
      for (const p of party) {
        lines.push(["P", p.species.speciesId, p.level, p.exp, p.friendship, speciesDataRegistry.getSpecies(p.species.speciesId).growthRate].join(" "));
      }
      for (const p of party) {
        const sd = starterData[p.species.getRootSpeciesId()];
        lines.push(["S", p.species.getRootSpeciesId(), sd.friendship, sd.candyCount].join(" "));
      }

      levelUps.length = 0;
      let applied = false;
      if (filterMsg === null) {
        const mod = itemKey === "RARE_CANDY" ? modifierType.newModifier(target) : modifierType.newModifier();
        applied = !!scene.addModifier(mod);
      }

      const expected = party.map((p: any) => getLevelTotalExp(p.level, p.species.growthRate));
      lines.push(["R", applied ? 1 : 0, filterMsg ? filterMsg.replace(/\s+/g, "_") : "OK", cap, ...party.flatMap((p: any, k: number) => [p.level, p.exp, p.friendship, expected[k]])].join(" "));
      lines.push(["T", ...party.flatMap((p: any) => {
        const sd = starterData[p.species.getRootSpeciesId()];
        return [sd.friendship, sd.candyCount];
      })].join(" "));
      lines.push(["L", levelUps.length, ...levelUps.flat()].join(" "));
      lines.push("E");
    } catch (e) {
      thrown++;
      lines.push(`X ${String(e).slice(0, 120).replace(/\s+/g, "_")}`);
    }
  }

  party.length = 0;
  scene.modifiers.length = 0;
  if (thrown) {
    fs.writeFileSync(OUT, lines.join("\n") + "\n");
    throw new Error(`${thrown} upstream candy cases threw`);
  }
  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
