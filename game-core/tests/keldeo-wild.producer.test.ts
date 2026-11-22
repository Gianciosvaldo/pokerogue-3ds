/* The wild Keldeo's form change against upstream at 8555c08 (src/gc_moveset.c: gc_enemy_learned_form_target, gc_enemy_quiet_form_change;
 * tests/keldeo_wild_diff_test.c is the comparator). Per case a Keldeo of a random shape (game mode, boss or not, boss segments, form, level, ability slot,
 * gender, nature, IVs, weather, double battle) has upstream's own EnemyPokemon.generateAndPopulateMoveset run in executeWithSeedOffset(.., offset, seed), which ends
 * with triggerPokemonFormChange(this, SpeciesFormChangeMoveLearnedTrigger): the moveset, the next two draws of the RNG and the phase that call queued (its phase
 * type and target form) are recorded. The queued QuietFormChangePhase then runs for the Pokemon on the field and visible (what EncounterPhase leaves a wild
 * enemy as): its form, ability slot, stats, HP, the dex entry's seen and caught flags before and after, and its text are recorded.
 * GC_KW_N=1500 GC_KW_SEED=777 GC_KW_OUT=/abs/keldeo-wild.txt, Node 22. */
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { getGameMode } from "#app/game-mode";
import { audioManager } from "#app/global-audio-manager";
import { randSeedInt } from "#utils/common";
import { GameModes } from "#enums/game-modes";
import { SpeciesId } from "#enums/species-id";
import { Pokemon } from "#field/pokemon";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_KW_N ?? 1500), SEED = Number(process.env.GC_KW_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const hex = (s: string) => (s ? Buffer.from(s, "utf8").toString("hex") : "-");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
});

it("keldeo wild oracle", async () => {
  const game = new GameManager(phaserGame);
  game.override.startingWave(45).startingBiome(1);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const pm = scene.phaseManager;
  const keldeo = speciesDataRegistry.getSpecies(SpeciesId.KELDEO);
  const e = scene.addEnemyPokemon(keldeo, 20, 0, false, true);
  scene.field.add(e); // EncounterPhase: globalScene.field.add(enemyPokemon)
  e.setVisible(true);

  // only scene assets, tweens, timers, the UI and audio are replaced: the phase's start, changeForm, conditions and end callbacks run
  const queued: any[] = [];
  vi.spyOn(pm, "unshiftPhase").mockImplementation((...ps: any[]) => { queued.push(...ps); });
  vi.spyOn(pm, "pushPhase").mockImplementation((...ps: any[]) => { queued.push(...ps); });
  const ended = vi.spyOn(pm, "shiftPhase").mockImplementation(() => {});
  vi.spyOn(Pokemon.prototype, "loadAssets").mockImplementation(() => Promise.resolve());
  vi.spyOn(Pokemon.prototype, "updateInfo").mockImplementation(() => Promise.resolve());
  vi.spyOn(scene, "updateFieldScale").mockImplementation(() => Promise.resolve());
  vi.spyOn(audioManager, "playSound").mockImplementation(() => {});
  const texts: string[] = [];
  vi.spyOn(scene.ui, "showText").mockImplementation((text: any, _d: any, cb: any) => { texts.push(String(text)); cb?.(); });
  vi.spyOn(scene.tweens, "add").mockImplementation((cfg: any) => { cfg.onComplete?.(); return {} as any; });
  vi.spyOn(scene.time, "delayedCall").mockImplementation((_d: any, cb: any) => { cb?.(); return {} as any; });

  const out = process.env.GC_KW_OUT ?? "/tmp/keldeo-wild.txt";
  const lines: string[] = ["V 1"];
  /* the modes a wild Keldeo is made in: Spliced Endless makes every wild Pokemon a fusion (a fused Keldeo keeps the stand-in moveset) and a Challenge run's constructor
   * is not validated; Daily is here for the registered change's condition (not in Daily mode) */
  const modes = [GameModes.CLASSIC, GameModes.CLASSIC, GameModes.ENDLESS, GameModes.DAILY];
  let changed = 0, kept = 0, unqueued = 0, revert = 0, withSecretSword = 0;
  for (let i = 0; i < N; i++) {
    const mode = modes[rnd(modes.length)];
    const boss = rnd(5) !== 0;   /* a wild Keldeo is a mythical: always a boss; the others cover the shared generator */
    const segments = boss ? 1 + rnd(4) : 0;
    const form0 = rnd(8) === 0 ? 1 : 0;   /* a wild Keldeo is made in its first form; the second covers the change back */
    const level = rnd(3) === 0 ? 1 + rnd(200) : 40 + rnd(90);   /* Secret Sword is a level 50 move */
    const abilityIndex = rnd(3);
    const gender = -1;   /* Keldeo is genderless */
    const nature = rnd(25);
    const ivs = Array.from({ length: 6 }, () => rnd(32));
    const weather = rnd(2) ? 0 : rnd(10);
    const double = boss && rnd(4) === 0;
    const offset = rnd(2000), waveSeed = word();
    const seen0 = rnd(3) === 0 ? 0n : BigInt(rnd(1 << 20)), caught0 = rnd(2) ? 0n : BigInt(rnd(1 << 20));

    scene.gameMode = getGameMode(mode);
    e.species = keldeo;
    e.fusionSpecies = null;
    e.formIndex = form0;
    e.level = level;
    e.abilityIndex = abilityIndex;
    e.customPokemonData.ability = -1;
    e.customPokemonData.passive = -1;
    e.gender = gender;
    e.nature = nature;
    e.ivs = ivs;
    e.moveset = [];
    e.bossSegments = segments;
    e.fainted = false;
    scene.currentBattle.double = double;
    e.calculateStats();
    e.hp = e.stats[0];
    scene.arena.weather = weather ? ({ weatherType: weather, isImmutable: () => false, turnsLeft: 0, maxDuration: 0, isEffectSuppressed: () => false } as any) : null;
    const dex = scene.gameData.dexData[SpeciesId.KELDEO];
    dex.seenAttr = seen0;
    dex.caughtAttr = caught0;
    queued.length = 0;
    texts.length = 0;
    ended.mockClear();

    let moves: number[] = [], d1 = 0, d2 = 0;
    scene.executeWithSeedOffset(() => {
      e.generateAndPopulateMoveset();
      moves = e.moveset.map((m: any) => m.moveId);
      d1 = randSeedInt(1000000);
      d2 = randSeedInt(1000000);
    }, offset, waveSeed);
    const hasSS = moves.includes(548);
    if (hasSS) withSecretSword++;
    const phases = queued.slice();
    const stats0 = [...e.stats];
    let record = `K ${i} ${mode} ${+boss} ${segments} ${form0} ${level} ${abilityIndex} ${nature} ${ivs.join(" ")} ${weather} ${+double} ${offset} ${waveSeed} ${seen0} ${caught0}`;
    record += ` ${e.stats.join(" ")} ${moves.length} ${moves.join(" ")} ${d1} ${d2}`;
    if (phases.length > 1 || (phases.length === 1 && phases[0].phaseName !== "QuietFormChangePhase")) throw Error(`unexpected phases ${phases.map(p => p.phaseName)}`);
    if (!phases.length) {
      unqueued++;
      record += " Q 0 -1";
    } else {
      const ph = phases[0];
      const target = keldeo.forms.findIndex((f: any) => f.formKey === ph.formChange.formKey);
      record += ` Q 1 ${target}`;
      pm.currentPhase = ph;
      await ph.start();
      if (!ended.mock.calls.length) throw Error(`phase not ended, case ${i}`);
      if (e.formIndex !== form0) { changed++; if (target === 0) revert++; } else kept++;
    }
    const after = scene.gameData.dexData[SpeciesId.KELDEO];
    record += ` X ${e.formIndex} ${e.abilityIndex} ${e.getAbility().id} ${e.stats.join(" ")} ${e.hp} ${after.seenAttr} ${after.caughtAttr} ${texts.length} ${texts.map(hex).join(" ")}`;
    void stats0;
    lines.push(record);
  }
  lines.push("Z");
  fs.writeFileSync(out, lines.join("\n") + "\n");
  console.log(`KELDEO WILD cases=${N} secretSword=${withSecretSword} queued=${N - unqueued} changed=${changed} kept=${kept} revert=${revert}`);
  expect(changed).toBeGreaterThan(50);
  expect(revert).toBeGreaterThan(5);
  expect(unqueued).toBeGreaterThan(50);
}, 900000);
