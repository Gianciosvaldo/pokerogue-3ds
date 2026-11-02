/*
 * Upstream half of game-core's trainer Terastallization test (tools/trainerteradiff.sh copies it into the upstream checkout).
 * For random trainer types (every config, the ones with tera slots and random Tera modifiers drawn more often), waves, party sizes and
 * parties (Shedinja drawn often, random Tera types), it sows the wave RNG, runs the real Trainer.genAI on the config and asks the real
 * Trainer.shouldTera for every slot, recording:
 *   - the config's trainerAI.instantTeras afterwards (the slots that Terastallize), which party members got which Tera type, and the RNG
 *     state before and after (the pick draws from it).
 * The config is shared between battles upstream and genAI keeps pushing into its list: each case starts from the list the config was made
 * with and puts it back, which is what a freshly started game sees (game-core keeps the pick with the battle's party).
 */
import { globalScene } from "#app/global-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerType } from "#enums/trainer-type";
import { Trainer } from "#field/trainer";
import { trainerConfigs } from "#trainers/trainer-config";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("trainer tera producer", async () => {
  const N = Number(process.env.GC_TTERA_N ?? 600);
  const OUT = process.env.GC_TTERA_OUT ?? "/tmp/gc-trainer-tera.txt";
  let seed = Number(process.env.GC_TTERA_SEED ?? 777);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const pick = <T>(v: readonly T[]): T => v[rnd(v.length)];
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 16 }, () => chars[rnd(chars.length)]).join("");
  const state = () => Phaser.Math.RND.state().split(",").slice(1, 5).join(" ");

  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const battle = (globalScene as any).currentBattle;

  const types = Object.keys(trainerConfigs).map(Number).sort((a, b) => a - b);
  const withTera = types.filter(t => {
    const c: any = trainerConfigs[t];
    return c.genAIFuncs.length > 0 || c.trainerAI.teraLogic.length > 0;
  });
  const ids = Object.keys(speciesDataRegistry.data).map(Number);
  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    const t = rnd(5) === 0 ? pick(types) : pick(withTera);
    const c: any = trainerConfigs[t];
    const wave = rnd(3) === 0 ? pick([1, 99, 100, 101, 150, 200]) : 1 + rnd(250);
    const n = 1 + rnd(6);
    const allShedinja = rnd(12) === 0;
    const party = Array.from({ length: n }, (_, k) => ({
      species: { speciesId: allShedinja || rnd(4) === 0 ? SpeciesId.SHEDINJA : pick(ids) },
      teraType: rnd(18),
      initialTeamIndex: k,
      id: k + 1,
      isTerastallized: false,
    }));
    battle.waveIndex = wave;
    battle.enemyFaintsHistory = [];
    const saved = { mode: c.trainerAI.teraMode, logic: c.trainerAI.teraLogic.slice() };
    const before = party.map(p => p.teraType);
    const s = seedStr();
    Phaser.Math.RND.sow([s]);
    const stateBefore = state();
    // a party of nothing but Shedinja under a specialty type other than Bug leaves no member to pick: upstream indexes the party with
    // undefined and throws (recorded as X; game-core does nothing)
    let threw = false;
    try {
      (Trainer.prototype.genAI as (p: any[]) => void).call({ config: c }, party);
    } catch {
      threw = true;
    }
    const instants: number[] = c.trainerAI.instantTeras;
    const should = party.map(p => ((Trainer.prototype.shouldTera as (p: any) => boolean).call({ config: c }, p) ? 1 : 0));
    lines.push(["C", i, t, wave, n, s, stateBefore, ...party.map((p, k) => `${p.species.speciesId} ${before[k]}`)].join(" "));
    lines.push(
      threw
        ? ["R", "X", ...party.map(p => p.teraType), ...should].join(" ")
        : ["R", instants.length, ...instants, ...party.map(p => p.teraType), ...should, state()].join(" "),
    );
    c.trainerAI.teraMode = saved.mode;
    c.trainerAI.teraLogic.length = 0;
    c.trainerAI.teraLogic.push(...saved.logic);
  }
  void TrainerType;
  fs.writeFileSync(OUT, `TRAINERTERA 1 ${lines.length / 2}\n${lines.join("\n")}\nEND\n`);
}, 600000);
