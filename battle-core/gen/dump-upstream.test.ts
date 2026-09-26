/*
 * Dumps upstream PokeRogue's live game data to JSON for the C port.
 * Copy into an upstream checkout as test/tests/zz-dump/dump-upstream.test.ts and run
 *   DUMP_OUT=/abs/out/dir npx vitest run test/tests/zz-dump/dump-upstream.test.ts
 * It runs after upstream's own initializeGame(), so everything is the real objects.
 *
 * Every closure (move conditions, ability predicates...) is written to closures.json
 * with its exact source text. The C port hand-writes each one, and gen/check-closures.py
 * flags any whose upstream text changed, which is how upstream updates get tracked.
 */
import { allAbilities, allMoves } from "#data/data-lists";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getTypeDamageMultiplier } from "#data/type";
import { getNatureStatMultiplier } from "#data/nature";
import * as fs from "node:fs";
import * as path from "node:path";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as invalidMoves from "#data/moves/invalid-moves";
import { Terrain } from "#data/terrain";
import { Weather } from "#data/weather";
import { globalScene } from "#app/global-scene";
import { it } from "vitest";

const OUT = process.env.DUMP_OUT ?? "/tmp/pr-dump";

/** Prototype chain of every class seen, most-derived first (attr dispatch needs "is-a"). */
const classChains: Record<string, string[]> = {};
function noteClass(v: any) {
  const name = v.constructor?.name;
  if (!name || name === "Object" || classChains[name]) return;
  const chain: string[] = [];
  for (let p = Object.getPrototypeOf(v); p && p !== Object.prototype; p = Object.getPrototypeOf(p)) chain.push(p.constructor.name);
  classChains[name] = chain;
}

const closures: { id: number; owner: string; src: string; cap?: number[] }[] = [];
const closureIds = new Map<Function, number>();

function closureRef(fn: Function, owner: string) {
  let id = closureIds.get(fn);
  if (id === undefined) {
    id = closures.length;
    closureIds.set(fn, id);
    closures.push({ id, owner, src: fn.toString() });
  }
  return { $fn: id };
}

/** Plain-data serialisation that keeps class names and turns shared game objects into refs. */
function ser(v: any, owner: string, depth = 0): any {
  if (v === null || v === undefined) return v ?? null;
  const t = typeof v;
  if (t === "number") return Number.isFinite(v) ? v : { $num: String(v) };
  if (t === "string" || t === "boolean") return v;
  if (t === "bigint") return { $bigint: String(v) };
  if (t === "function") return closureRef(v, owner);
  if (depth > 12) return { $deep: v?.constructor?.name };
  if (Array.isArray(v) || ArrayBuffer.isView(v)) return Array.from(v as any, (x: any) => ser(x, owner, depth + 1));
  if (v instanceof Map) return { $map: [...v.entries()].map(([k, x]) => [ser(k, owner, depth + 1), ser(x, owner, depth + 1)]) };
  if (v instanceof Set) return { $set: [...v].map(x => ser(x, owner, depth + 1)) };
  const cls = v.constructor?.name;
  if (depth > 0 && allMoves.includes(v)) return { $move: v.id };
  if (depth > 0 && allAbilities.includes(v)) return { $ability: v.id };
  if (depth > 1 && cls === "PokemonSpecies") return { $species: v.speciesId };
  noteClass(v);
  const o: any = cls && cls !== "Object" ? { $class: cls } : {};
  for (const k of Object.keys(v)) o[k] = ser(v[k], owner, depth + 1);
  return o;
}

/**
 * Closures built inside constructors capture arguments (boostedType, moveType,
 * weatherTypes, terrainTypes, boostedCategories) that aren't visible in their source.
 * Recover them by calling the closure against each candidate value. The result is part
 * of the closure's identity, so a changed constructor argument upstream shows as drift.
 */
function probeCaptures() {
  const truthy = (f: () => any) => {
    try { return !!f(); } catch { return false; }
  };
  for (const cl of closures) {
    const fn = [...closureIds.entries()].find(([, id]) => id === cl.id)![0] as any;
    const src = cl.src;
    const cap: number[] = [];
    if (/getMoveType\(move\) === (boostedType|moveType)\b/.test(src)) {
      for (let t = -1; t <= 18; t++) {
        const p = { getMoveType: () => t };
        if (truthy(() => fn(p, p, { type: t, category: 0 }))) cap.push(t);
      }
    } else if (/boostedCategories/.test(src)) {
      for (let c = 0; c <= 2; c++) if (truthy(() => fn({}, {}, { category: c }))) cap.push(c);
    } else if (/weatherTypes/.test(src)) {
      const saved = globalScene.arena.weather;
      for (let w = 1; w <= 9; w++) {
        globalScene.arena.weather = new Weather(w, 5);
        if (truthy(() => fn({}))) cap.push(w);
      }
      globalScene.arena.weather = saved;
    } else if (/terrainTypes/.test(src)) {
      const saved = globalScene.arena.terrain;
      for (let t = 1; t <= 4; t++) {
        globalScene.arena.terrain = new Terrain(t, 5);
        if (truthy(() => fn({}))) cap.push(t);
      }
      globalScene.arena.terrain = saved;
    } else {
      continue;
    }
    cl.cap = cap;
  }
}

function write(name: string, data: any) {
  fs.writeFileSync(path.join(OUT, name), JSON.stringify(data, null, 1));
}

it("dump upstream data", () => {
  fs.mkdirSync(OUT, { recursive: true });
  // A GameManager gives upstream a globalScene/gameMode (Classic, no challenges).
  new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));

  // Enums: every numeric-valued export in src/enums
  const enumMods = import.meta.glob("/src/enums/*.ts", { eager: true }) as Record<string, any>;
  const enums: Record<string, Record<string, number>> = {};
  for (const mod of Object.values(enumMods)) {
    for (const [name, val] of Object.entries(mod)) {
      if (!val || typeof val !== "object" || Array.isArray(val)) continue;
      const entries = Object.entries(val).filter(([k, x]) => typeof x === "number" && Number.isNaN(Number(k)));
      if (entries.length && entries.length === Object.keys(val).filter(k => Number.isNaN(Number(k))).length) {
        enums[name] = Object.fromEntries(entries) as Record<string, number>;
      }
    }
  }
  write("enums.json", enums);
  // String-valued enums (BattlerTagType, ArenaTagType...): the C side numbers them.
  const strEnums: Record<string, Record<string, string>> = {};
  for (const mod of Object.values(enumMods)) {
    for (const [name, val] of Object.entries(mod)) {
      if (!val || typeof val !== "object" || Array.isArray(val) || enums[name]) continue;
      const vals = Object.entries(val);
      if (vals.length && vals.every(([, x]) => typeof x === "string")) strEnums[name] = Object.fromEntries(vals) as any;
    }
  }
  write("string-enums.json", strEnums);

  const T = enums.PokemonType;
  const types = Object.values(T).filter(x => x >= 0 && x <= 17);
  write("typechart.json", types.map(a => types.map(d => getTypeDamageMultiplier(a, d))));
  const S = enums.Stat;
  write("natures.json", Object.values(enums.Nature).map(n =>
    [S.HP, S.ATK, S.DEF, S.SPATK, S.SPDEF, S.SPD].map(s => getNatureStatMultiplier(n, s))));

  write("moves.json", allMoves.map((m: any) => ser(m, `move:${m.id}`)));
  write("abilities.json", allAbilities.map((a: any) => ser(a, `ability:${a.id}`)));
  const data = speciesDataRegistry.data as any;
  write("species.json", Object.fromEntries(Object.entries(data).map(([id, d]) => [id, ser(d, `species:${id}`)])));
  probeCaptures();
  write("closures.json", closures);
  write("move-sets.json", Object.fromEntries(
    Object.entries(invalidMoves).filter(([, v]) => v instanceof Set).map(([k, v]) => [k, [...(v as Set<number>)]])));
  write("classes.json", classChains);
});
