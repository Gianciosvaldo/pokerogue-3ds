/*
 * Upstream half of game-core's boss voucher replay (tools/trainervictorydiff.sh copies it into the pinned upstream
 * checkout). Each case runs upstream's own TrainerVictoryPhase.start() for one trainerConfigs entry, with the voucher
 * of that trainer unlocked or not in gameData.voucherUnlocks and timedEventManager.getUpgradeUnlockedVouchers() false
 * (timed events are not ported). Then it runs every ModifierRewardPhase the phase queued, through
 * ModifierRewardPhase.doReward() (addModifier: AddVoucherModifier.apply for a voucher, PersistentModifier.add for a
 * trainer's own modifierRewardFuncs), so the four voucherCounts changes and the stacks are the ones the player ends up
 * with, not only the ones validateVoucher made. The scene keeps its modifiers from case to case, so the stacks of the
 * reward items grow to their getMaxStackCount() and past it, where addModifier adds nothing.
 * Cases: every trainerConfigs entry once with the voucher unlocked and once without, then random trainers (voucher
 * trainers over-represented) and random states up to N.
 * Output, one line per case:
 *   T <TrainerType> <unlocked before> <unlocked after> <delta of voucherCounts 0..3> <pokeballCounts 0..4 before>
 *   <pokeballCounts 0..4 after> <n> then n phases in queue order, each "M <moneyMultiplier>" or
 *   "R <modifierTypes key> <stack before> <stack after>" (the stack of the player's modifiers with that type id around
 *   the phase; a voucher is no modifier: 0 0), then the text each reward phase showed, one after a tab each, in the
 *   same order (a line break in a text is a |); when addModifier found the stack full it queued battle:itemStackFull
 *   and gave the tier's default item (a Poke Ball): that message follows the reward's text after a \x01
 * and a last line "# cases N", so a cut file can't pass. The Phaser RNG state is checked here: TrainerVictoryPhase
 * must not roll, because game-core's trainer_victory does not. An exception anywhere fails the producer.
 * build/trainer_victory_diff_test replays the cases.
 */
import { audioManager } from "#app/global-audio-manager";
import { timedEventManager } from "#app/global-event-manager";
import { modifierTypes } from "#data/data-lists";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerType } from "#enums/trainer-type";
import { ModifierRewardPhase } from "#phases/modifier-reward-phase";
import { TrainerVictoryPhase } from "#phases/trainer-victory-phase";
import { vouchers } from "#system/voucher";
import { GameManager } from "#test/framework/game-manager";
import { trainerConfigs } from "#trainers/trainer-config";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("trainer victory producer", async () => {
  const N = Number(process.env.GC_VOUCHER_N ?? 2000);
  const OUT = process.env.GC_VOUCHER_OUT ?? "/tmp/gc-trainer-victory.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;

  // what the stubs leave out: the UI, music, timed events (none, as the port assumes)
  vi.spyOn(timedEventManager, "getUpgradeUnlockedVouchers").mockReturnValue(false);
  let runRewards = false;
  let shown: string[] = [];
  let queuedMessages: string[] = [];
  vi.spyOn(scene.ui, "showText").mockImplementation((text: string, _d: any, callback: any) => {
    if (runRewards) {
      shown.push(text);
      callback?.();
    }
  });
  // addModifier's itemStackFull: queueMessage builds a MessagePhase; the text is what the player reads
  vi.spyOn(scene.phaseManager, "queueMessage").mockImplementation((text: string) => {
    if (runRewards) {
      queuedMessages.push(text);
    }
  });
  vi.spyOn(scene.ui.achvBar, "showAchv").mockImplementation(() => {});
  vi.spyOn(audioManager, "playBgm").mockImplementation(() => {});
  vi.spyOn(audioManager, "playSound").mockImplementation(() => {});
  vi.spyOn(scene.phaseManager, "hasPhaseOfType").mockReturnValue(false);
  let queued: any[][] = [];
  vi.spyOn(scene.phaseManager, "pushNew").mockImplementation((...args: any[]) => {
    queued.push(["push", ...args]);
  });
  vi.spyOn(scene.phaseManager, "unshiftNew").mockImplementation((...args: any[]) => {
    queued.push(["unshift", ...args]);
  });

  let seed = Number(process.env.GC_VOUCHER_SEED ?? 777);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const pick = <T>(v: readonly T[]): T => v[rnd(v.length)];
  const state = () => Phaser.Math.RND.state();
  // a ModifierTypeFunc's key: a modifierTypes entry, or a setModifierRewardFuncs wrapper (its type carries the id)
  const funcKey = (fn: any): string => {
    const k = Object.keys(modifierTypes).find(key => (modifierTypes as any)[key] === fn);
    return k ?? String(fn().id);
  };

  // the player's modifiers of one type id: the stack count a PersistentModifier ended with
  const stackOf = (id: string): number =>
    scene.modifiers.filter((m: any) => m.type?.id === id).reduce((n: number, m: any) => n + m.stackCount, 0);

  const trainerTypes = Object.keys(trainerConfigs).map(Number).sort((a, b) => a - b);
  const voucherTrainers = trainerTypes.filter(t => Object.hasOwn(vouchers, TrainerType[t]));
  const plan: [number, boolean][] = [];
  for (const t of trainerTypes) {
    plan.push([t, false], [t, true]);
  }
  while (plan.length < N) {
    plan.push([rnd(2) ? pick(voucherTrainers) : pick(trainerTypes), rnd(2) === 0]);
  }
  plan.length = N;

  const lines: string[] = [];
  for (const [tt, unlockedBefore] of plan) {
    const config = (trainerConfigs as any)[tt];
    const key = TrainerType[tt];
    if (unlockedBefore) {
      scene.gameData.voucherUnlocks[key] = 1;
    } else {
      delete scene.gameData.voucherUnlocks[key];
    }
    const counts = [0, 1, 2, 3].map(k => scene.gameData.voucherCounts[k]);
    const ballsBefore = [0, 1, 2, 3, 4].map(k => scene.pokeballCounts[k]);
    battle.trainer = { config, getName: () => key, getVictoryMessages: () => [], getKey: () => key };
    queued = [];
    runRewards = false;
    const before = state();
    const phase: any = new TrainerVictoryPhase();
    phase.end = () => {};
    phase.showEnemyTrainer = () => {};
    phase.start();
    const out: string[] = [];
    const texts: string[] = [];
    for (const q of queued) {
      if (q[0] !== "unshift") {
        throw new Error(`trainer ${key}: ${q[0]} ${q[1]}`);
      } else if (q[1] === "MoneyRewardPhase") {
        out.push(`M ${q[2]}`);
      } else if (q[1] === "ModifierRewardPhase") {
        const k = funcKey(q[2]);
        const stackBefore = stackOf(k);
        // the same phase upstream would have run; the text it shows is recorded with the case
        shown = [];
        queuedMessages = [];
        runRewards = true;
        await new ModifierRewardPhase(q[2]).doReward();
        runRewards = false;
        out.push(`R ${k} ${stackBefore} ${stackOf(k)}`);
        if (shown.length !== 1) {
          throw new Error(`trainer ${key}: ${k} showed ${shown.length} texts`);
        }
        if (queuedMessages.length > 1) {
          throw new Error(`trainer ${key}: ${k} queued ${queuedMessages.length} messages`);
        }
        // a text can have a line break: it is a | here
        texts.push([shown[0], ...queuedMessages].map(x => x.replace(/\n/g, "|")).join("\x01"));
      } else {
        throw new Error(`trainer ${key}: unexpected phase ${q[1]}`);
      }
    }
    if (state() !== before) {
      throw new Error(`trainer ${key}: the phase rolled on Phaser.Math.RND`);
    }
    const delta = [0, 1, 2, 3].map(k => scene.gameData.voucherCounts[k] - counts[k]);
    lines.push(
      `T ${tt} ${unlockedBefore ? 1 : 0} ${Object.hasOwn(scene.gameData.voucherUnlocks, key) ? 1 : 0} ${delta.join(" ")} `
        + `${ballsBefore.join(" ")} ${[0, 1, 2, 3, 4].map(k => scene.pokeballCounts[k]).join(" ")} ${out.length} ${out.join(" ")}${texts.map(x => `\t${x}`).join("")}`,
    );
    battle.trainer = undefined;
  }
  lines.push(`# cases ${N}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
