/*
 * Upstream half of reward-core's trainer reward test (card 97; tools/trainerdiff.sh copies it into the upstream
 * checkout). Three kinds of case, each on upstream's own code:
 *  F  the fixed battles' reward screens. A random game mode, Single Generation value (challenge runs), wave
 *     (mostly the fixed battle waves), offsetGym and path. Victory path: VictoryPhase.start() with the phase queue
 *     stubbed, recording every ModifierRewardPhase item and SelectModifierPhase's customModifierRewardSettings in
 *     queue order. Biome path: SelectBiomePhase's expression `gameMode.isFixedBattle(w) ?
 *     gameMode.getFixedBattle(w)?.customModifierRewardSettings : undefined`, called on upstream's GameMode.
 *  V  TrainerVictoryPhase.start() for a random trainerConfigs entry, a random voucherUnlocks state for its voucher
 *     and a random timedEventManager.getUpgradeUnlockedVouchers(): every phase it queues in order (with its
 *     argument), whether the voucher is unlocked afterwards and the four voucherCounts changes.
 *  G  Trainer.genModifiers(party) (config.genModifiersFunc) on a party of real EnemyPokemon: every modifier's type
 *     id, species stat booster key, holder's party index and stack; or that it threw.
 * Inputs are recorded as upstream had them before the call (the RNG state, the voucher unlock, each enemy's
 * species and getAbility(true) / getPassiveAbility() / hasPassive()); outputs are what upstream did, and the RNG
 * state after every case. An exception anywhere but the call a G case tests fails the producer. The last line
 * counts the cases, so a cut file can't pass. build/trainerrewardstest replays the cases.
 */
import { audioManager } from "#app/global-audio-manager";
import { timedEventManager } from "#app/global-event-manager";
import { getGameMode } from "#app/game-mode";
import { activeOverrides } from "#app/overrides";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { BattleType } from "#enums/battle-type";
import { BattlerIndex } from "#enums/battler-index";
import { Challenges } from "#enums/challenges";
import { ClassicFixedBossWaves } from "#enums/fixed-boss-waves";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerSlot } from "#enums/trainer-slot";
import { TrainerType } from "#enums/trainer-type";
import { Trainer } from "#field/trainer";
import { TrainerVictoryPhase } from "#phases/trainer-victory-phase";
import { VictoryPhase } from "#phases/victory-phase";
import { vouchers } from "#system/voucher";
import { GameManager } from "#test/framework/game-manager";
import { trainerConfigs } from "#trainers/trainer-config";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("trainer rewards producer", async () => {
  const N = Number(process.env.DIFF_N ?? 2000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-trainer-rewards.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  // the G cases build their own enemies: no species override, and no ability override may stand in for theirs
  game.override.enemySpecies(null);
  for (const k of ["ENEMY_ABILITY_OVERRIDE", "ENEMY_PASSIVE_ABILITY_OVERRIDE"] as const) {
    if (activeOverrides[k] !== AbilityId.NONE) {
      throw new Error(`${k} is set`);
    }
  }
  if (activeOverrides.ENEMY_HAS_PASSIVE_ABILITY_OVERRIDE != null) {
    throw new Error("ENEMY_HAS_PASSIVE_ABILITY_OVERRIDE is set");
  }
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;

  // what the stubs leave out: timed event rewards (none, as the port assumes), the UI, music, party EXP
  vi.spyOn(timedEventManager, "getFixedBattleEventRewards").mockReturnValue([]);
  const upgradeSpy = vi.spyOn(timedEventManager, "getUpgradeUnlockedVouchers");
  vi.spyOn(scene, "applyPartyExp").mockImplementation(() => {});
  vi.spyOn(scene.ui, "showText").mockImplementation(() => {});
  vi.spyOn(scene.ui.achvBar, "showAchv").mockImplementation(() => {});
  vi.spyOn(audioManager, "playBgm").mockImplementation(() => {});
  vi.spyOn(scene.phaseManager, "hasPhaseOfType").mockReturnValue(false);
  let queued: any[][] = [];
  vi.spyOn(scene.phaseManager, "pushNew").mockImplementation((...args: any[]) => {
    queued.push(["push", ...args]);
  });
  vi.spyOn(scene.phaseManager, "unshiftNew").mockImplementation((...args: any[]) => {
    queued.push(["unshift", ...args]);
  });
  const realEnemyParty = scene.getEnemyParty.bind(scene);
  let enemyParty: any[] | null = null;
  vi.spyOn(scene, "getEnemyParty").mockImplementation(() => enemyParty ?? realEnemyParty());

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const pick = <T>(v: readonly T[]): T => v[rnd(v.length)];
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const state = () => Phaser.Math.RND.state().split(",").slice(1).join(" ");
  const sow = () => Phaser.Math.RND.sow([seedStr()]);
  // a ModifierTypeFunc's key: a modifierTypes entry, or a setModifierRewardFuncs wrapper (its type carries the id)
  const funcKey = (fn: any): string => {
    const k = Object.keys(modifierTypes).find(key => (modifierTypes as any)[key] === fn);
    return k ?? String(fn().id);
  };
  // customModifierRewardSettings, every key it has, sorted
  const settings = (cs: any): string => {
    if (cs === undefined) {
      return "none";
    }
    const keys = Object.keys(cs).sort();
    return `${keys.length} ${keys.map(k => `${k}=${Array.isArray(cs[k]) ? `[${cs[k].join(",")}]` : String(cs[k])}`).join(" ")}`;
  };

  const modes = [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.DAILY, GameModes.CHALLENGE];
  const fixedWaves = Object.values(ClassicFixedBossWaves).filter(v => typeof v === "number") as number[];
  const trainerTypes = Object.keys(trainerConfigs).map(Number).sort((a, b) => a - b);
  const rewardTrainers = trainerTypes.filter(t => (trainerConfigs as any)[t].modifierRewardFuncs.length > 0);
  const voucherTrainers = trainerTypes.filter(t => Object.hasOwn(vouchers, TrainerType[t]));
  const genTrainers = trainerTypes.filter(t => (trainerConfigs as any)[t].genModifiersFunc);
  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  const favourites = [
    SpeciesId.PIKACHU, SpeciesId.GIGALITH, SpeciesId.PELIPPER, SpeciesId.MILOTIC, SpeciesId.GLISCOR, SpeciesId.SWELLOW,
    SpeciesId.TAILLOW,
  ];
  // the slots the genModifiersFunc closures read, with the species upstream's party funcs put there
  const signature: Record<number, Record<number, SpeciesId>> = {
    [TrainerType.RED]: { 0: SpeciesId.PIKACHU },
    [TrainerType.STEVEN]: { 0: SpeciesId.GIGALITH },
    [TrainerType.WALLACE]: { 0: SpeciesId.PELIPPER, 5: SpeciesId.MILOTIC },
    [TrainerType.KIERAN]: { 2: SpeciesId.GLISCOR },
    [TrainerType.GUZMA_2]: { 4: SpeciesId.GLISCOR },
    [TrainerType.RIVAL_5]: { 1: SpeciesId.SWELLOW },
    [TrainerType.RIVAL_6]: { 1: SpeciesId.SWELLOW },
  };
  const passives = [AbilityId.GUTS, AbilityId.POISON_HEAL, AbilityId.MARVEL_SCALE, AbilityId.INTIMIDATE];

  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    const kind = rnd(10);
    queued = [];
    if (kind < 4) {
      // ---- F: a fixed battle's reward screen ----
      const mode = rnd(3) === 0 ? GameModes.CHALLENGE : pick(modes);
      const gm = getGameMode(mode);
      const g = mode === GameModes.CHALLENGE && rnd(3) ? 1 + rnd(9) : 0;
      if (g) {
        gm.setChallengeValue(Challenges.SINGLE_GENERATION, g);
      }
      scene.gameMode = gm;
      battle.gameMode = gm;
      const wave = rnd(3) ? pick(fixedWaves) : rnd(4) ? 1 + rnd(210) : 1 + rnd(1000);
      battle.waveIndex = wave;
      battle.battleType = rnd(2) ? BattleType.TRAINER : BattleType.WILD;
      scene.offsetGym = rnd(2) === 0;
      const biome = rnd(3) === 0;
      sow();
      const before = state();
      const out: string[] = [];
      if (biome) {
        // select-biome-phase.ts: the settings of the SelectModifierPhase it queues when the party heal is off
        out.push(`S ${settings(gm.isFixedBattle(wave) ? gm.getFixedBattle(wave)?.customModifierRewardSettings : undefined)}`);
      } else {
        enemyParty = [];
        const phase: any = new VictoryPhase(BattlerIndex.ENEMY);
        phase.end = () => {};
        phase.getPokemon = () => ({ getExpValue: () => 0 });
        phase.start();
        enemyParty = null;
        for (const q of queued) {
          if (q[1] === "ModifierRewardPhase") {
            out.push(`R ${funcKey(q[2])}`);
          } else if (q[1] === "SelectModifierPhase") {
            if (q[2] !== undefined || q[3] !== undefined) {
              throw new Error("SelectModifierPhase with a reroll count or tiers");
            }
            out.push(`S ${settings(q[4])}`);
          }
        }
      }
      lines.push(`F ${mode} ${g} ${wave} ${scene.offsetGym ? 1 : 0} ${biome ? 1 : 0} ${before} ${state()} ${out.length} ${out.join(" ; ")}`);
    } else if (kind < 7) {
      // ---- V: TrainerVictoryPhase ----
      const r = rnd(4);
      const tt = r === 0 ? pick(rewardTrainers) : r === 1 ? pick(trainerTypes) : pick(voucherTrainers);
      const config = (trainerConfigs as any)[tt];
      const key = TrainerType[tt];
      if (rnd(2)) {
        scene.gameData.voucherUnlocks[key] = 1;
      } else {
        delete scene.gameData.voucherUnlocks[key];
      }
      const unlocked = Object.hasOwn(scene.gameData.voucherUnlocks, key);
      const upgrade = rnd(2) === 0;
      upgradeSpy.mockReturnValue(upgrade);
      battle.trainer = { config, getName: () => key, getVictoryMessages: () => [], getKey: () => key };
      const counts = [0, 1, 2, 3].map(k => scene.gameData.voucherCounts[k]);
      sow();
      const before = state();
      const phase: any = new TrainerVictoryPhase();
      phase.end = () => {};
      phase.showEnemyTrainer = () => {};
      phase.start();
      const out = queued.map(q => {
        if (q[0] !== "unshift") {
          return `push:${q[1]}`;
        }
        if (q[1] === "MoneyRewardPhase") {
          return `M ${q[2]}`;
        }
        if (q[1] === "ModifierRewardPhase") {
          return `R ${funcKey(q[2])}`;
        }
        return `other:${q[1]}`;
      });
      const delta = [0, 1, 2, 3].map(k => scene.gameData.voucherCounts[k] - counts[k]);
      lines.push(
        `V ${tt} ${unlocked ? 1 : 0} ${upgrade ? 1 : 0} ${before} ${state()} `
          + `${Object.hasOwn(scene.gameData.voucherUnlocks, key) ? 1 : 0} ${delta.join(" ")} ${out.length} ${out.join(" ; ")}`,
      );
      battle.trainer = undefined;
    } else {
      // ---- G: Trainer.genModifiers(party) ----
      const tt = rnd(5) ? pick(genTrainers) : pick(trainerTypes);
      const n = rnd(5) ? 6 : rnd(6);
      const party: any[] = [];
      for (let s = 0; s < n; s++) {
        const sig = signature[tt]?.[s];
        const sid = sig !== undefined && rnd(2) ? sig : rnd(2) ? pick(favourites) : pick(speciesIds);
        const sp: any = speciesDataRegistry.getSpecies(sid);
        const p: any = scene.addEnemyPokemon(sp, 1 + rnd(100), TrainerSlot.TRAINER);
        p.abilityIndex = rnd(3);
        if (rnd(6) === 0) {
          p.fusionSpecies = speciesDataRegistry.getSpecies(pick(speciesIds));
          p.fusionFormIndex = 0;
          p.fusionAbilityIndex = rnd(3);
        }
        if (rnd(4) === 0) {
          p.customPokemonData.passive = pick(passives);
        }
        p.passive = rnd(2) === 0;
        party.push(p);
      }
      const mons = party.map(
        p => `${p.species.speciesId} ${p.getAbility(true).id} ${p.hasPassive() ? p.getPassiveAbility().id : 0}`,
      );
      sow();
      const before = state();
      let result: string;
      try {
        const mods = Trainer.prototype.genModifiers.call({ config: (trainerConfigs as any)[tt] }, party);
        result = `${mods.length} ${mods
          .map((m: any) => `${m.type.id} ${m.type.key ?? "-"} ${party.findIndex(p => p.id === m.pokemonId)} ${m.stackCount}`)
          .join(" ")}`;
      } catch (e) {
        result = `T ${(e as Error).constructor.name}`;
      }
      lines.push(`G ${tt} ${n} ${mons.join(" ")} ${before} ${state()} ${result}`);
      for (const p of party) {
        p.destroy();
      }
    }
  }
  lines.push(`# cases ${N}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
