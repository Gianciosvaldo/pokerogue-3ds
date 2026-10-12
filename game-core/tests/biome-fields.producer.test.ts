/* Additive biome weather/terrain oracle: the real Arena.resetArenaEffects, setBiomeWeather and setBiomeTerrain on
 * arenas holding no, a permanent, a timed or a primal weather and terrain, in every biome and at waves of every time of
 * day. Records the weather and terrain left (type, turns left, max duration), the messages queued and the RNG before and
 * after. GC_BF_N=3000 GC_BF_SEED=777 GC_BF_OUT=/abs/biome-fields.txt, Node22. */
import { allBiomes } from "#data/data-lists";
import { Terrain } from "#data/terrain";
import { Weather } from "#data/weather";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import i18next from "i18next";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

it("biome weather and terrain producer", async () => {
  const N = Number(process.env.GC_BF_N ?? 3000), SEED = Number(process.env.GC_BF_SEED ?? 777);
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 500).toBe(true);
  let seed = SEED;
  const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any, arena = scene.arena;
  const messages: string[] = [];
  vi.spyOn(scene.phaseManager, "queueMessage").mockImplementation((m: any) => { messages.push(String(m)); });
  vi.spyOn(scene.phaseManager, "unshiftNew").mockImplementation(() => {});
  // the messages' texts back to their locale keys, which is what game-core emits
  const keyOf = new Map<string, string>();
  for (const w of ["sunny", "rain", "sandstorm", "hail", "snow", "fog", "heavyRain", "harshSun", "strongWinds"])
    for (const k of ["Start", "Clear", "Continue"]) {
      const key = `weather:${w}${k}Message`, text = i18next.t(key);
      if (text !== key) { expect(keyOf.has(text)).toBe(false); keyOf.set(text, key); }
    }
  for (const w of ["misty", "electric", "grassy", "psychic"])
    for (const k of ["Start", "Clear"]) {
      const key = `terrain:${w}${k}Message`, text = i18next.t(key);
      expect(text).not.toBe(key); expect(keyOf.has(text)).toBe(false); keyOf.set(text, key);
    }
  const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");
  const ids = [...allBiomes.keys()].map(Number).sort((a, b) => a - b);
  const PRIMAL = [7, 8, 9], PLAIN = [1, 2, 3, 4, 5, 6];
  const lines = [`V 1 ${N}`];
  const cover = { rolled: 0, reset: 0, kept: 0, timedCleared: 0, primalBlocked: 0, night: 0, forestFog: 0, tods: [0, 0, 0, 0], weathers: new Set<number>(), terrains: new Set<number>(), biomes: new Set<number>() };
  for (let i = 0; i < N; i++) {
    const forest = i % 10 === 3;   // the forest at night is the one biome with its own weather rule
    const biome = forest ? 5 : i < ids.length ? ids[i] : ids[rnd(ids.length)];
    const wave = 1 + rnd(240), offset = rnd(8) * 5;
    const mode = forest ? (i % 2 ? 0 : 2) : i % 4;   // 0: reset then roll (a new biome), 1: reset only (a trainer battle), 2: roll only (Daily), 3: neither
    const pwk = rnd(4), ptk = rnd(3);
    const pw = pwk === 0 ? 0 : pwk === 3 ? PRIMAL[rnd(3)] : PLAIN[rnd(6)], pt = ptk === 0 ? 0 : 1 + rnd(4);
    const pwTurns = pwk === 2 ? 1 + rnd(5) : 0, ptTurns = ptk === 2 ? 1 + rnd(5) : 0;
    scene.currentBattle.waveIndex = wave; scene.waveCycleOffset = offset;
    (arena as any).biomeId = biome;
    arena.weather = pw ? new Weather(pw, pwTurns, pwk === 2 ? 5 : 0) : null;
    arena.terrain = pt ? new Terrain(pt, ptTurns, ptk === 2 ? 5 : 0) : null;
    Phaser.Math.RND.sow([`bf-${SEED}-${i}`]);
    messages.length = 0;
    const before = rng();
    const tod = arena.getTimeOfDay();
    lines.push(`C ${i} ${biome} ${wave} ${offset} ${mode} ${pw} ${pw ? arena.weather.turnsLeft : 0} ${pw ? arena.weather.maxDuration : 0} ${pt} ${pt ? arena.terrain.turnsLeft : 0} ${pt ? arena.terrain.maxDuration : 0}`, `R ${before}`);
    const reset = mode === 0 || mode === 1, roll = mode === 0 || mode === 2;
    if (reset) arena.resetArenaEffects();
    const mid = arena.weatherType;
    if (roll) { arena.setBiomeWeather(); arena.setBiomeTerrain(); }
    const w = arena.weatherType, t = arena.terrainType;
    lines.push(`A ${w} ${arena.weather ? arena.weather.turnsLeft : 0} ${arena.weather ? arena.weather.maxDuration : 0} ${t} ${arena.terrain ? arena.terrain.turnsLeft : 0} ${arena.terrain ? arena.terrain.maxDuration : 0}`,
      `M ${messages.length} ${messages.map(m => keyOf.get(m) ?? `?${m.replace(/\s+/g, "_")}`).join(" ")}`, `S ${rng()}`, "E");
    cover.rolled += +roll; cover.reset += +reset; cover.kept += +(reset && pw && mid === pw);
    cover.timedCleared += +(reset && pwk === 2 && mid === 0);
    cover.primalBlocked += +(roll && pwk === 3 && messages.some(m => keyOf.get(m)?.endsWith("ContinueMessage")) && w === pw);
    cover.tods[tod]++; cover.night += +(roll && (tod === 2 || tod === 3)); cover.forestFog += +(roll && biome === 5 && w === 6);
    if (roll) { cover.weathers.add(w); cover.terrains.add(t); cover.biomes.add(biome); }
  }
  for (const n of [cover.rolled, cover.reset, cover.kept, cover.timedCleared, cover.night, ...cover.tods]) expect(n).toBeGreaterThan(0);
  expect(cover.biomes.size).toBe(ids.length);
  lines.push(`Z ${N}`);
  fs.writeFileSync(process.env.GC_BF_OUT ?? "/tmp/biome-fields.txt", lines.join("\n") + "\n");
  console.log(`BIOMEFIELDS ${SEED}: ${N} cases rolled=${cover.rolled} reset=${cover.reset} permanentKept=${cover.kept} timedCleared=${cover.timedCleared} primalBlocked=${cover.primalBlocked} night=${cover.night} forestFog=${cover.forestFog} tod=${cover.tods} weathers=${[...cover.weathers].sort()} terrains=${[...cover.terrains].sort()}`);
}, 240000);
