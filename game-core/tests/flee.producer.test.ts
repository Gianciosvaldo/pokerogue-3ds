/* Fleeing (Task 77 / PR #125 left it without an upstream differential), against upstream at 8555c08. Additive.
 * Writes FLEE_OUT (default /tmp/gc-flee.txt); FLEE_N cases (default 600), FLEE_SEED (default 777).
 *
 * The real AttemptRunPhase.start() runs on a live battle: its escapeAttempts++ on entry, the roll
 * (randBattleSeedInt(100) on the battle's own seed), calculateEscapeChance, RunSuccessAbAttr, and then the success
 * branch (the phases it pushes, in order, with SelectBiomePhase only for hasRandomBiomes || isNewBiome) or the failure
 * branch (failedRunAway and the message). Between cases the lead's and the enemy's raw speed stat, stat stages,
 * paralysis, abilities, the boss shield, the attempts so far, the wave, the game mode and the battle seed are changed.
 *
 * Two probes per case. The roll of a case is not left to chance: the battle seed is searched for one whose first
 * randSeedInt(100) is chance-1 (a roll that succeeds) or chance (one that fails), so a chance that differs by one in
 * either direction flips the outcome. A third kind of case leaves the roll to the seed.
 *
 * What the harness changes: GameManager makes randBattleSeedInt return its maximum; it is put back to
 * Battle.randSeedInt here (the original, from battle-scene.ts). Tweens, sounds and Pokemon.destroy are stubbed, and
 * the pushed phases are recorded and not queued. Cases where the effective speeds of the two sides tie are skipped:
 * the turn's order is then drawn from the battle RNG before AttemptRunPhase runs in C, and here it has already run. */
import { getGameMode } from "#app/game-mode";
import { BattleScene } from "#app/battle-scene";
import { audioManager } from "#app/global-audio-manager";
import { Status } from "#data/status-effect";
import { AbilityId } from "#enums/ability-id";
import { Command } from "#enums/command";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { StatusEffect } from "#enums/status-effect";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import i18next from "i18next";
import { afterAll, beforeAll, beforeEach, describe, expect, it, vi } from "vitest";

const N = Number(process.env.FLEE_N ?? 600);
const SEED = Number(process.env.FLEE_SEED ?? 777);
const OUT = process.env.FLEE_OUT ?? "/tmp/gc-flee.txt";
const BATTLES = 6;

let seed = SEED;
const rnd = (n: number) => {
  seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
  return (seed >>> 8) % n;
};
const pick = <T>(a: readonly T[]): T => a[rnd(a.length)];

const lines: string[] = [`V 1 ${SEED}`];
const counts: Record<string, number> = {};
const count = (k: string) => {
  counts[k] = (counts[k] ?? 0) + 1;
};

const MODES = [GameModes.CLASSIC, GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.DAILY, GameModes.CHALLENGE];
// waves where isNewBiome differs between the modes: tens, fives, 49 mod 50, and the rest
const WAVES = [1, 2, 4, 5, 9, 10, 11, 15, 19, 20, 24, 25, 29, 30, 44, 45, 49, 50, 51, 99, 100, 145, 149, 150, 195, 199, 249, 250, 299];
const ENEMIES = [SpeciesId.CATERPIE, SpeciesId.MAGIKARP, SpeciesId.PIKACHU, SpeciesId.GENGAR, SpeciesId.MEWTWO, SpeciesId.SNORLAX];

describe("flee differential", () => {
  let phaserGame: Phaser.Game;
  let game: GameManager;

  beforeAll(() => {
    phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
  });
  beforeEach(() => {
    game = new GameManager(phaserGame);
  });
  afterAll(() => {
    lines.push(`Z ${N}`);
    fs.writeFileSync(OUT, lines.join("\n") + "\n");
    console.log(`FLEE seed=${SEED} ${Object.entries(counts).map(([k, v]) => `${k}=${v}`).join(" ")}`);
  });

  for (let b = 0; b < BATTLES; b++) {
    it(`battle ${b}`, async () => {
      game.override
        .battleStyle("single")
        .enemySpecies(pick(ENEMIES))
        .enemyMoveset(MoveId.SPLASH)
        .moveset(MoveId.SPLASH)
        .ability(AbilityId.INSOMNIA)
        .passiveAbility(AbilityId.NONE)
        .enemyAbility(AbilityId.INSOMNIA)
        .enemyPassiveAbility(AbilityId.NONE)
        .startingLevel(50)
        .enemyLevel(20)
        .disableTrainerWaves()
        .mysteryEncounterChance(0);
      await game.classicMode.startBattle(SpeciesId.RATTATA);
      const scene = game.scene as any;
      const battle = scene.currentBattle;
      const lead = scene.getPlayerField()[0];
      const enemy = scene.getEnemyField()[0];
      // the CommandPhase's RUN, then the phase itself is held before it starts
      scene.phaseManager.getCurrentPhase().handleCommand(Command.RUN, 0);
      await game.phaseInterceptor.to("AttemptRunPhase", false);
      const phase = scene.phaseManager.getCurrentPhase();
      expect(phase.phaseName).toBe("AttemptRunPhase");

      // the real randBattleSeedInt (GameManager replaced it with one that returns the maximum)
      BattleScene.prototype.randBattleSeedInt = function (range: number, min = 0) {
        return (this as any).currentBattle?.randSeedInt(range, min);
      };
      const log = vi.spyOn(console, "log").mockImplementation(() => {});
      vi.spyOn(audioManager, "playSound").mockImplementation(() => {});
      vi.spyOn(scene.tweens, "add").mockImplementation((() => ({})) as any);
      vi.spyOn(enemy, "hideInfo").mockImplementation(() => Promise.resolve());
      vi.spyOn(enemy, "destroy").mockImplementation(() => {});
      vi.spyOn(phase, "end").mockImplementation(() => {});
      const pushed: string[] = [];
      vi.spyOn(scene.phaseManager, "pushNew").mockImplementation(((name: string) => {
        pushed.push(name);
      }) as any);
      vi.spyOn(scene.phaseManager, "queueMessage").mockImplementation(() => {});
      const keys: string[] = [];
      const realT = i18next.t.bind(i18next);
      vi.spyOn(i18next, "t").mockImplementation(((k: any, o?: any) => {
        keys.push(String(k));
        return realT(k, o);
      }) as any);
      void 0;
      const rngTuple = () => (battle.battleSeedState ? battle.battleSeedState.split(",").slice(1).join(",") : "none");

      /** the first roll of randSeedInt(100) on the battle seed `s` at turn `t` */
      const firstRoll = (s: string, t: number) => {
        battle.battleSeed = s;
        battle.battleSeedState = undefined;
        battle.turn = t;
        const r = battle.randSeedInt(100);
        battle.battleSeedState = undefined;
        return r;
      };

      for (let i = 0; i < Math.ceil(N / BATTLES); i++) {
        const idx = b * Math.ceil(N / BATTLES) + i;
        const mode = pick(MODES);
        scene.gameMode = getGameMode(mode);
        const wave = rnd(3) === 0 ? 1 + rnd(300) : pick(WAVES);
        battle.waveIndex = wave;
        // raw speed stats: ratios from far below the minimum to far above the cap
        const spdE = 1 + rnd(rnd(2) ? 700 : 80);
        const ratio = [0.01, 0.1, 0.25, 0.5, 0.8, 1, 1.2, 1.5, 2, 3, 3.8, 4, 4.5, 6, 8, 100][rnd(16)];
        let spdP = rnd(3) === 0 ? 1 + rnd(900) : Math.max(1, Math.round(spdE * ratio) + rnd(3) - 1);
        const segs = pick([0, 0, 0, 2, 3, 4, 5]);
        const attempts = pick([0, 0, 1, 2, 3, 4, 5, 7, 12]);
        const stP = rnd(3) === 0 ? rnd(13) - 6 : 0;
        const stE = rnd(3) === 0 ? rnd(13) - 6 : 0;
        const parP = rnd(4) === 0;
        const parE = rnd(4) === 0;
        const runAway = rnd(5) === 0;
        const enemyRunAway = rnd(8) === 0;

        lead.stats[Stat.SPD] = spdP;
        enemy.stats[Stat.SPD] = spdE;
        lead.setStatStage(Stat.SPD, stP);
        enemy.setStatStage(Stat.SPD, stE);
        lead.status = parP ? new Status(StatusEffect.PARALYSIS) : null;
        enemy.status = parE ? new Status(StatusEffect.PARALYSIS) : null;
        lead.summonData.ability = runAway ? AbilityId.RUN_AWAY : AbilityId.INSOMNIA;
        enemy.summonData.ability = enemyRunAway ? AbilityId.RUN_AWAY : AbilityId.INSOMNIA;
        lead.hp = lead.getMaxHp();
        enemy.hp = enemy.getMaxHp();
        enemy.bossSegments = segs;
        battle.escapeAttempts = attempts;
        battle.successfulRun = false;
        lead.turnData.failedRunAway = false;
        if (lead.getEffectiveStat(Stat.SPD) === enemy.getEffectiveStat(Stat.SPD)) {
          lines.push(`SKIP i=${idx} tie`);
          count("skipped-tie");
          continue;
        }

        const base = phase.calculateEscapeChance(attempts);
        // the probe: a roll just under the chance (it succeeds), at it (it fails), or whatever the seed gives
        const probe = pick(["under", "at", "free"] as const);
        let want = -1;
        if (probe !== "free") {
          const c = runAway ? 256 : base;
          want = probe === "under" ? c - 1 : c;
          if (want < 0 || want > 99) want = -1;
        }
        const turn = 1 + rnd(40);
        let bseed = `flee-${SEED}-${idx}`;
        if (want >= 0) {
          let k = 0;
          while (firstRoll(`${bseed}-${k}`, turn) !== want) k++;
          bseed = `${bseed}-${k}`;
        }
        battle.battleSeed = bseed;
        battle.battleSeedState = undefined;
        battle.turn = turn;
        pushed.length = 0;
        keys.length = 0;
        // EnemyCommandPhase has run before the turn starts and draws from the battle seed, if it draws: the same
        // call on the seed of the case, as the command phase makes it
        enemy.getNextMove();
        const drew = rngTuple() !== "none";
        phase.start();
        const after = rngTuple();
        const flee = keys.filter(k => k === "battle:runAwaySuccess" || k === "battle:runAwayCannotEscape");
        const roll = firstRoll(bseed, turn);
        const ok = battle.successfulRun;
        lines.push(
          `CASE i=${idx} wave=${wave} mode=${mode} att=${attempts} seed=${bseed} turn=${turn} spdp=${spdP} spde=${spdE} stp=${stP} ste=${stE} parp=${+parP} pare=${+parE} abp=${+runAway} abe=${+enemyRunAway} segs=${segs} probe=${probe}`,
          `RES base=${base} roll=${roll} ok=${+ok} att2=${battle.escapeAttempts} msgs=${flee.join(",")} pushed=${pushed.join(",") || "-"} failed=${+!!lead.turnData.failedRunAway} ehp=${enemy.hp} est=${enemy.status?.effect ?? 0} drew=${+drew}`,
          "E",
        );
        count(ok ? "success" : "failure");
        count(`probe-${probe}`);
        if (segs) count("boss");
        if (runAway) count("run-away");
        if (pushed.includes("SelectBiomePhase")) count("select-biome");
        // put the enemy back
        enemy.hp = enemy.getMaxHp();
        enemy.status = null;
        lead.hp = lead.getMaxHp();
      }
      log.mockRestore();
    }, 600000);
  }
});
