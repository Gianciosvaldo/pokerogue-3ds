/* Additive canonical Lure primitive oracle, pinned8555c08 Node22.
 * Real factories/classes only. No Scene.addModifier tier fallback or phase claim. */
import { audioManager } from "#app/global-audio-manager";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { DoubleBattleChanceBoosterModifier } from "#modifiers/modifier";
import { GameManager } from "#test/framework/game-manager";
import { PokemonData } from "#system/pokemon-data";
import { NumberHolder } from "#utils/common";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

it("canonical Lure primitive producer", async () => {
  const N = Number(process.env.LURE_N ?? 2000), SEED = Number(process.env.LURE_SEED ?? 777);
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 120).toBe(true);
  let seed = SEED;
  const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE)
    .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any, names = ["LURE", "SUPER_LURE", "MAX_LURE"], durations = [10, 15, 30];
  const lines = [`L 1 ${N}`], adds = [0, 0, 0], refreshes = [0, 0, 0], full = [0, 0, 0], expired = [0, 0, 0];
  let duplicateCases = 0, stackTwo = 0, totalTicks = 0;
  const items = () => scene.modifiers.map((m: any) => `${m.type.id} ${m.stackCount} ${m.pokemonId == null ? -1 : 0} 0 ${m.getBattleCount?.() ?? 0} 0`);
  const emitItems = () => { const a = items(); lines.push(`I ${a.length}`, ...a.map((s: string) => `H ${s}`)); };
  const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").map(Number);
  const chance = (base: number) => {
    const holder = new NumberHolder(base);
    const applied = scene.applyModifiers(DoubleBattleChanceBoosterModifier, true, holder);
    expect(applied.every((m: any) => m instanceof DoubleBattleChanceBoosterModifier)).toBe(true);
    return holder.value;
  };
  for (let i = 0; i < N; i++) {
    scene.modifiers.length = 0;
    const which = i % 3, mode = Math.floor(i / 3) % 4, ticks = 1 + rnd(35);
    const base = [0, 1, 8, 32, 0.125, 65535][rnd(6)];
    const create = (name: string) => { const f = (modifierTypes as any)[name]; return f().withIdFromFunc(f); };
    const append = (name: string, count?: number) => {
      const type = create(name), m = name === "LEFTOVERS" ? type.newModifier(scene.getPlayerParty()[0]) : type.newModifier();
      if (m instanceof DoubleBattleChanceBoosterModifier) {
        // getArgs/current duration constructor preserves factory's canonical maximum.
        const copy = new DoubleBattleChanceBoosterModifier(type, m.getMaxBattles(), count, 1 + rnd(2));
        stackTwo += +(copy.stackCount === 2); scene.modifiers.push(copy);
      } else scene.modifiers.push(m);
    };
    append("HEALING_CHARM");
    // First matching duration controls add even when another copy can refresh.
    if (mode) append(names[which], mode === 2 || (mode === 3 && i % 2 === 0) ? durations[which] : 1 + rnd(durations[which] - 1));
    for (let j = 0; j < 3; j++) if (j !== which && rnd(2)) append(names[j], 1 + rnd(durations[j]));
    append("LEFTOVERS");
    if (mode === 3) { append(names[which], 1 + rnd(durations[which] - 1)); duplicateCases++; }
    scene.money = 9999 + i; Phaser.Math.RND.sow([`lure-${SEED}-${i}`]);
    const beforeRng = rng(), battleRng = scene.currentBattle.rngState;
    const partyBefore = JSON.stringify(scene.getPlayerParty().map((p: any) => new PokemonData(p)));
    const countBefore = scene.modifiers.length;
    lines.push(`C ${i} ${names[which]} ${ticks} ${base}`, `R ${beforeRng.join(" ")}`); emitItems();
    let sounds = 0;
    const originalSound = audioManager.playSound;
    audioManager.playSound = (key: string) => { expect(key).toBe("se/restore"); sounds++; };
    const added = create(names[which]).newModifier().add(scene.modifiers, false);
    audioManager.playSound = originalSound;
    const status = !added ? 1 : scene.modifiers.length > countBefore ? 2 : 3;
    if (status === 1) full[which]++; else if (status === 2) adds[which]++; else refreshes[which]++;
    lines.push(`A ${status} ${sounds} ${chance(base)}`); emitItems();
    for (let tick = 0; tick < ticks; tick++) {
      let removed = 0;
      // Exact BattleEnd primitive loop, deliberately filtered to the Lure family.
      const lapsing = scene.findModifiers((m: any) => m instanceof DoubleBattleChanceBoosterModifier);
      for (const m of lapsing) if (!m.lapse()) { scene.removeModifier(m); removed++; expired[names.indexOf(m.type.id)]++; }
      lines.push(`T ${tick} ${removed} ${chance(base)}`); emitItems(); totalTicks++;
    }
    expect(rng()).toEqual(beforeRng); expect(scene.currentBattle.rngState).toBe(battleRng);
    expect(scene.money).toBe(9999 + i);
    expect(JSON.stringify(scene.getPlayerParty().map((p: any) => new PokemonData(p)))).toBe(partyBefore);
    lines.push(`E ${i} ${rng().join(" ")}`);
  }
  for (let j = 0; j < 3; j++) { expect(adds[j]).toBeGreaterThan(0); expect(refreshes[j]).toBeGreaterThan(0);
    expect(full[j]).toBeGreaterThan(0); expect(expired[j]).toBeGreaterThan(0); }
  expect(duplicateCases).toBeGreaterThan(0); expect(stackTwo).toBeGreaterThan(0);
  lines.push(`Z ${N} 0 0 ${adds.join(" ")} ${refreshes.join(" ")} ${full.join(" ")} ${expired.join(" ")} ${duplicateCases} ${stackTwo} ${totalTicks}`);
  fs.writeFileSync(process.env.LURE_OUT ?? "/tmp/lures.txt", lines.join("\n") + "\n");
}, 240000);
