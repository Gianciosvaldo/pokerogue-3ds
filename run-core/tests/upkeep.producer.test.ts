/*
 * Upstream half of run-core's upkeep test (tools/difftest.sh copies it into the upstream checkout).
 * Each case builds a real party of 1 to 6 with random Pokerus carriers and runs trySpreadPokerus for a
 * random run seed and wave; then gives the player 0 to 2 Coin Cases (random stacks), sets random money
 * (sometimes near Number.MAX_SAFE_INTEGER) and applies their interest as the biome change does.
 * build/upkeeptest replays the cases.
 */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { MoneyInterestModifier } from "#modifiers/modifier";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("upkeep producer", async () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-upkeep-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const pool = Array.from({ length: 6 }, () => scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5));
  const party = scene.getPlayerParty();
  const realUpdateMoney = scene.updateMoneyText;
  const realAnimate = scene.animateMoneyChanged;
  const realAchv = scene.validateAchvs;
  scene.updateMoneyText = () => {};
  scene.animateMoneyChanged = () => {};
  scene.validateAchvs = () => {};

  const lines: string[] = [];
  try {
    for (let i = 0; i < N; i++) {
      scene.seed = seedStr();
      const wave = 1 + rnd(3000);
      battle.waveIndex = wave;
      const n = 1 + rnd(6);
      party.length = 0;
      for (const p of pool.slice(0, n)) {
        p.pokerus = rnd(3) === 0;
        party.push(p);
      }
      const before = party.map((p: any) => (p.pokerus ? 1 : 0));
      scene.trySpreadPokerus();
      const after = party.map((p: any) => (p.pokerus ? 1 : 0));

      scene.modifiers.length = 0;
      const stacks: number[] = [];
      for (let k = rnd(3) === 0 ? 0 : 1 + rnd(2); k > 0; k--) {
        const m = (generateModifierType((modifierTypes as any).COIN_CASE) as any).newModifier();
        m.stackCount = 1 + rnd(3);
        if (rnd(6) === 0) {
          m.virtualStackCount = 1 + rnd(2);
        }
        scene.modifiers.push(m);
        stacks.push(m.getStackCount());
      }
      const money = rnd(10) === 0 ? Number.MAX_SAFE_INTEGER - rnd(1000) * 1e12 : rnd(10) === 0 ? rnd(1e9) : rnd(200000);
      scene.money = money;
      scene.applyModifiers(MoneyInterestModifier, true);
      lines.push(
        ["U", scene.seed, wave, n, ...before, ...after, money, stacks.length, ...stacks, scene.money].join(" "),
      );
    }
  } finally {
    scene.updateMoneyText = realUpdateMoney;
    scene.animateMoneyChanged = realAnimate;
    scene.validateAchvs = realAchv;
    scene.modifiers.length = 0;
  }
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
