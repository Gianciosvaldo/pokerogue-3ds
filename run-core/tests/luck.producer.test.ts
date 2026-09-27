/*
 * Upstream half of run-core's party luck test (tools/difftest.sh copies it into the upstream checkout).
 * Each case builds a party of 0 to 6 members (random luck and fusion luck, fused or not, allowed in battle
 * or not, and a species that a timed event may boost), sets a random game mode and run seed and a timed
 * event's luck boost and boosted species, and asks upstream's own getPartyLuckValue(party). The members
 * are stand-ins that borrow Pokemon.prototype.getLuck and isFusion, so upstream's own sum is what runs.
 * build/lucktest replays the cases.
 */
import { timedEventManager } from "#app/global-event-manager";
import { getGameMode } from "#app/game-mode";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Pokemon } from "#field/pokemon";
import { getPartyLuckValue } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("luck producer", async () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-luck-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  let ev = { boost: 0, species: [] as number[] };
  vi.spyOn(timedEventManager, "getEventLuckBoost").mockImplementation(() => ev.boost);
  vi.spyOn(timedEventManager, "getEventLuckBoostedSpecies").mockImplementation(() => [...ev.species]);

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const state = () => Phaser.Math.RND.state().split(",").slice(1).join(" ");
  const modes = [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.DAILY, GameModes.CHALLENGE];
  const getLuck = (Pokemon.prototype as any).getLuck;
  const isFusion = (Pokemon.prototype as any).isFusion;

  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    const gm = getGameMode(modes[rnd(modes.length)]);
    scene.gameMode = gm;
    scene.currentBattle.gameMode = gm;
    scene.seed = seedStr();
    scene.waveSeed = seedStr();   // the daily roll is on the run seed: a wave seed that differs shows it
    ev = { boost: rnd(3) === 0 ? rnd(5) : 0, species: rnd(3) === 0 ? [1 + rnd(6), 1 + rnd(6)] : [] };
    const party: any[] = [];
    const fields: number[] = [];
    for (let k = rnd(7); k > 0; k--) {
      const m: any = {
        species: { speciesId: 1 + rnd(6) },
        luck: rnd(4),
        fusionLuck: rnd(4),
        fusionSpecies: rnd(3) === 0 ? {} : null,
        summonData: {},
        allowed: rnd(5) !== 0,
      };
      m.getLuck = getLuck;
      m.isFusion = isFusion;
      m.isAllowedInBattle = () => m.allowed;
      party.push(m);
      fields.push(m.species.speciesId, m.luck, m.fusionLuck, m.fusionSpecies ? 1 : 0, m.allowed ? 1 : 0);
    }
    Phaser.Math.RND.sow([seedStr()]);
    const before = state();
    const luck = getPartyLuckValue(party);
    lines.push(
      [
        "L", gm.modeId, scene.seed, scene.waveSeed, ev.boost, ev.species.length, ...ev.species, party.length, ...fields, before, luck,
        state(),
      ].join(" "),
    );
  }
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
