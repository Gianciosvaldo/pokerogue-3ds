/* Additive canonical temporary booster primitive oracle, pinned8555c08 Node22.
 * Real factories/classes only. No Scene.addModifier tier fallback or phase claim. */
import { audioManager } from "#app/global-audio-manager";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TempCritBoosterModifier, TempStatStageBoosterModifier } from "#modifiers/modifier";
import { GameManager } from "#test/framework/game-manager";
import { PokemonData } from "#system/pokemon-data";
import { TempStatStageBoosterModifierType } from "#modifiers/modifier-type";
import { NumberHolder } from "#utils/common";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

it("canonical temporary booster primitive producer", async () => {
  const N = Number(process.env.TEMP_N ?? 2000), SEED = Number(process.env.TEMP_SEED ?? 777);
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 120).toBe(true);
  let seed = SEED;
  const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE)
    .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any, names = Array(6).fill("TEMP_STAT_STAGE_BOOSTER").concat("DIRE_HIT"), durations = Array(7).fill(5);
  const lines = [`B 1 ${N}`], adds = Array(7).fill(0), refreshes = Array(7).fill(0), full = Array(7).fill(0), expired = Array(7).fill(0);
  let duplicateCases = 0, stackTwo = 0, totalTicks = 0;
  const items = () => scene.modifiers.map((m: any) => `${m.type.id} ${m.stackCount} ${m.pokemonId == null ? -1 : 0} ${m instanceof TempStatStageBoosterModifier ? m.getArgs()[0] : 0} ${m.getBattleCount?.() ?? 0} 0`);
  const emitItems = () => { const a = items(); lines.push(`I ${a.length}`, ...a.map((s: string) => `H ${s}`)); };
  const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").map(Number);
  const effects = (base: number) => {
    const values = Array.from({length: 8}, (_, stat) => {
      const v = new NumberHolder(base); scene.applyModifiers(TempStatStageBoosterModifier, true, stat, v); return v.value;
    });
    const crit = new NumberHolder(base); scene.applyModifiers(TempCritBoosterModifier, true, crit);
    return values.concat(crit.value).join(" ");
  };
  const family = (m: any) => m instanceof TempStatStageBoosterModifier ? m.getArgs()[0] - 1 : m instanceof TempCritBoosterModifier ? 6 : -1;
  for (let i = 0; i < N; i++) {
    scene.modifiers.length = 0;
    const which = i % 7, mode = Math.floor(i / 7) % 4, ticks = 1 + rnd(7);
    const base = rnd(13) - 6;
    const create = (name: string, arg = 0) => {
      if (name === "TEMP_STAT_STAGE_BOOSTER") { const t = new TempStatStageBoosterModifierType(arg); t.id = name; return t; }
      const f = (modifierTypes as any)[name]; return f().withIdFromFunc(f);
    };
    const append = (name: string, count?: number, arg = 0) => {
      const type = create(name, arg), m = name === "LEFTOVERS" ? type.newModifier(scene.getPlayerParty()[0]) : type.newModifier();
      if ((m instanceof TempStatStageBoosterModifier || m instanceof TempCritBoosterModifier)) {
        // getArgs/current duration constructor preserves factory's canonical maximum.
        const copy = m instanceof TempCritBoosterModifier
          ? new TempCritBoosterModifier(type, 5, count, 1 + rnd(2))
          : new TempStatStageBoosterModifier(type, arg, 5, count, 1 + rnd(2));
        stackTwo += +(copy.stackCount === 2); scene.modifiers.push(copy);
      } else scene.modifiers.push(m);
    };
    append("HEALING_CHARM"); append("LURE");
    // First matching duration controls add even when another copy can refresh.
    if (mode) append(names[which], mode === 2 || (mode === 3 && i % 2 === 0) ? durations[which] : 1 + rnd(durations[which] - 1), which === 6 ? 0 : which + 1);
    for (let j = 0; j < 7; j++) if (j !== which && rnd(2)) append(names[j], 1 + rnd(durations[j]), j === 6 ? 0 : j + 1);
    append("LEFTOVERS");
    if (mode === 3) { append(names[which], 1 + rnd(durations[which] - 1), which === 6 ? 0 : which + 1); duplicateCases++; }
    scene.money = 9999 + i; Phaser.Math.RND.sow([`lure-${SEED}-${i}`]);
    const beforeRng = rng(), battleRng = scene.currentBattle.rngState;
    const partyBefore = JSON.stringify(scene.getPlayerParty().map((p: any) => new PokemonData(p)));
    const countBefore = scene.modifiers.length;
    lines.push(`C ${i} ${names[which]} ${which === 6 ? 0 : which + 1} ${ticks} ${base}`, `R ${beforeRng.join(" ")}`); emitItems();
    let sounds = 0;
    const originalSound = audioManager.playSound;
    audioManager.playSound = (key: string) => { expect(key).toBe("se/restore"); sounds++; };
    const added = create(names[which], which === 6 ? 0 : which + 1).newModifier().add(scene.modifiers, false);
    audioManager.playSound = originalSound;
    const status = !added ? 1 : scene.modifiers.length > countBefore ? 2 : 3;
    if (status === 1) full[which]++; else if (status === 2) adds[which]++; else refreshes[which]++;
    lines.push(`A ${status} ${sounds} ${effects(base)}`); emitItems();
    for (let tick = 0; tick < ticks; tick++) {
      let removed = 0;
      // Exact BattleEnd primitive loop, deliberately filtered to the temporary booster families.
      const lapsing = scene.findModifiers((m: any) => (m instanceof TempStatStageBoosterModifier || m instanceof TempCritBoosterModifier));
      for (const m of lapsing) if (!m.lapse()) { scene.removeModifier(m); removed++; expired[family(m)]++; }
      lines.push(`T ${tick} ${removed} ${effects(base)}`); emitItems(); totalTicks++;
    }
    expect(rng()).toEqual(beforeRng); expect(scene.currentBattle.rngState).toBe(battleRng);
    expect(scene.money).toBe(9999 + i);
    expect(JSON.stringify(scene.getPlayerParty().map((p: any) => new PokemonData(p)))).toBe(partyBefore);
    lines.push(`E ${i} ${rng().join(" ")}`);
  }
  for (let j = 0; j < 7; j++) { expect(adds[j]).toBeGreaterThan(0); expect(refreshes[j]).toBeGreaterThan(0);
    expect(full[j]).toBeGreaterThan(0); expect(expired[j]).toBeGreaterThan(0); }
  expect(duplicateCases).toBeGreaterThan(0); expect(stackTwo).toBeGreaterThan(0);
  lines.push(`Z ${N} 0 0 ${adds.join(" ")} ${refreshes.join(" ")} ${full.join(" ")} ${expired.join(" ")} ${duplicateCases} ${stackTwo} ${totalTicks}`);
  fs.writeFileSync(process.env.TEMP_OUT ?? "/tmp/temp-boosters.txt", lines.join("\n") + "\n");
}, 240000);
