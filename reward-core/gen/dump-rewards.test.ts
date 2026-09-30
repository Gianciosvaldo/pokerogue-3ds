/*
 * Dumps what the player's reward options are rolled from, for reward-core, as C:
 * - the items (modifierTypeInitObj keys, in order) with what the rolls read of each: its class kind, whether it is
 *   a held item (PokemonHeldItemModifierType) or a form change item, its English name and group (the duplicate
 *   check compares both), its stack limits, and for a generator every type it can make (arg, name, group);
 * - the PLAYER pool by tier: each entry's item, its weight (a number, or -1 for a weight function, which
 *   src/pools.c ports by item and tier: the dump fails on a function it doesn't list), its maxWeight;
 * - the TMs: tmPoolTiers, isUnimplemented, and every species form's getTms(formKey) list (deduplicated);
 * - the item evolutions (getEvolutions, item ones, in order) with their conditions (EvoCondKey data);
 * - the form changes with an item trigger (getFormChanges, in order) with their trigger, keys and conditions
 *   (closures, classified by their source and probed for the species a dex condition reads);
 * - hasEvolutions per species, the G-Max forms (isMax), the form keys of the species the tables above name.
 * Run with tools/redump.sh <upstream dir>; writes gen/out/reward_data.c and gen/out/reward_items.h.
 * The output is generated: don't edit it.
 */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { globalScene } from "#app/global-scene";
import { EvoCondKey } from "#balance/pokemon-evolutions";
import { tmPoolTiers } from "#balance/tm-pool-tiers";
import { allMoves, modifierTypes } from "#data/data-lists";
import { SpeciesFormChangeCondition } from "#data/pokemon-forms";
import { SpeciesFormChangeItemTrigger } from "#data/pokemon-forms/form-change-triggers";
import { AbilityId } from "#enums/ability-id";
import { BerryType } from "#enums/berry-type";
import { EvolutionItem } from "#balance/pokemon-evolutions";
import { FormChangeItem } from "#enums/form-change-item";
import { ModifierPoolType } from "#enums/modifier-pool-type";
import { ModifierTier } from "#enums/modifier-tier";
import { MoveId } from "#enums/move-id";
import { Nature } from "#enums/nature";
import { PokemonType } from "#enums/pokemon-type";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { PersistentModifier, PokemonHeldItemModifier } from "#modifiers/modifier";
import {
  FormChangeItemModifierType,
  ModifierTypeGenerator,
  PokemonHeldItemModifierType,
  PokemonModifierType,
} from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import { getModifierPoolForType } from "#utils/modifier-utils";
import * as fs from "node:fs";
import Phaser from "phaser";
import { expect, it } from "vitest";

// The weight functions src/pools.c ports, by "<ITEM>@<tier>"
const PORTED_WEIGHT_FUNCS = new Set([
  "POKEBALL@0", "POTION@0", "SUPER_POTION@0", "ETHER@0", "MAX_ETHER@0", "LURE@0",
  "GREAT_BALL@1", "FULL_HEAL@1", "REVIVE@1", "MAX_REVIVE@1", "SACRED_ASH@1", "HYPER_POTION@1", "MAX_POTION@1",
  "FULL_RESTORE@1", "ELIXIR@1", "MAX_ELIXIR@1", "SUPER_LURE@1", "NUGGET@1", "EVOLUTION_ITEM@1", "MAP@1",
  "MEMORY_MUSHROOM@1", "TERA_SHARD@1", "DNA_SPLICERS@1", "VOUCHER@1",
  "ULTRA_BALL@2", "MAX_LURE@2", "BIG_NUGGET@2", "RARE_EVOLUTION_ITEM@2", "FORM_CHANGE_ITEM@2", "AMULET_COIN@2",
  "EVIOLITE@2", "LEEK@2", "TOXIC_ORB@2", "FLAME_ORB@2", "MYSTICAL_ROCK@2", "CANDY_JAR@2", "GOLDEN_PUNCH@2",
  "IV_SCANNER@2", "EXP_CHARM@2", "EXP_SHARE@2", "TERA_ORB@2",
  "ROGUE_BALL@3", "RELIC_GOLD@3", "CATCHING_CHARM@3", "ABILITY_CHARM@3", "LOCK_CAPSULE@3", "SUPER_EXP_CHARM@3",
  "RARE_FORM_CHANGE_ITEM@3", "MEGA_BRACELET@3", "DYNAMAX_BAND@3", "VOUCHER_PLUS@3",
  "MASTER_BALL@4", "VOUCHER_PREMIUM@4", "DNA_SPLICERS@4", "MINI_BLACK_HOLE@4",
]);

// generator kinds (src/reward.c): 0 = not a generator
const GEN = {
  BERRY: 1, BASE_STAT: 2, TEMP_STAT: 3, ATTACK_TYPE: 4, SPECIES_STAT: 5, RARE_SPECIES_STAT: 6, TM_COMMON: 7,
  TM_GREAT: 8, TM_ULTRA: 9, EVOLUTION_ITEM: 10, RARE_EVOLUTION_ITEM: 11, FORM_CHANGE_ITEM: 12,
  RARE_FORM_CHANGE_ITEM: 13, MINT: 14, TERA_SHARD: 15, SHUCKLE_JUICE: 16, BLACK_SLUDGE: 17,
} as Record<string, number>;
const GEN_KEYS: Record<string, string> = {
  BERRY: "BERRY", BASE_STAT_BOOSTER: "BASE_STAT", TEMP_STAT_STAGE_BOOSTER: "TEMP_STAT", ATTACK_TYPE_BOOSTER: "ATTACK_TYPE",
  SPECIES_STAT_BOOSTER: "SPECIES_STAT", RARE_SPECIES_STAT_BOOSTER: "RARE_SPECIES_STAT", TM_COMMON: "TM_COMMON",
  TM_GREAT: "TM_GREAT", TM_ULTRA: "TM_ULTRA", EVOLUTION_ITEM: "EVOLUTION_ITEM", RARE_EVOLUTION_ITEM: "RARE_EVOLUTION_ITEM",
  FORM_CHANGE_ITEM: "FORM_CHANGE_ITEM", RARE_FORM_CHANGE_ITEM: "RARE_FORM_CHANGE_ITEM", MINT: "MINT",
  TERA_SHARD: "TERA_SHARD", MYSTERY_ENCOUNTER_SHUCKLE_JUICE: "SHUCKLE_JUICE", MYSTERY_ENCOUNTER_BLACK_SLUDGE: "BLACK_SLUDGE",
};
// species stat booster keys, in the generator's items order (the arg of a generated one)
const SPECIES_STAT_KEYS = ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_SCALE", "DEEP_SEA_TOOTH"];

it("dump rewards", async () => {
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const player = scene.getPlayerParty()[0];
  const outC = process.env.DUMP_OUT ?? "/tmp/reward_data.c";
  const outH = process.env.DUMP_OUT_H ?? outC.replace(/reward_data\.c$/, "reward_items.h");
  expect([ModifierTier.COMMON, ModifierTier.MASTER, ModifierTier.LUXURY]).toEqual([0, 4, 5]);
  expect(PokemonType.STELLAR).toBe(18);

  // interned strings: index 0 is "undefined" (a group that isn't set), 1 is ""
  const strings: string[] = ["\u0000undefined", ""];
  const str = (s: string | undefined | null) => {
    const k = s == null ? "\u0000undefined" : s;
    let i = strings.indexOf(k);
    if (i < 0) {
      i = strings.push(k) - 1;
    }
    return i;
  };
  const cstr = (s: string) => JSON.stringify(s).replace(/\\u([0-9a-f]{4})/g, (_m, h) => `\\x${h.slice(2)}`);

  // ---- items ----
  const keys = Object.keys(modifierTypes);
  const itemIndex = (k: string) => {
    const i = keys.indexOf(k);
    if (i < 0) {
      throw new Error(`unknown item ${k}`);
    }
    return i;
  };
  const items: string[] = [];
  const genTypes: string[] = [];   // {item, arg, name, group}, sorted by item then arg
  const genTypeRows: [number, number, number, number][] = [];
  const tmCount = Object.keys(tmPoolTiers).length;
  for (const k of keys) {
    const t: any = (modifierTypes as any)[k]();
    t.id = k;
    const gen = t instanceof ModifierTypeGenerator;
    const genKind = gen ? GEN[GEN_KEYS[k]] : 0;
    if (gen && !genKind) {
      throw new Error(`${k}: a generator the port doesn't know`);
    }
    let flags = 0;
    let name = -1;
    let group = str(t.group);
    let maxStack = -1;
    let maxStackThreshold = -1;
    let maxHeld = -1;
    if (!gen) {
      name = str(t.name);
      if (t instanceof PokemonHeldItemModifierType) {
        flags |= 1;
      }
      if (t instanceof FormChangeItemModifierType) {
        flags |= 2;
      }
      if (t instanceof PokemonModifierType) {
        flags |= 4;
      }
      // a persistent modifier made without a Pokemon: its stack limits
      let m: any = null;
      try {
        m = t instanceof PokemonModifierType ? t.newModifier(player) : t.newModifier();
      } catch {
        m = null;
      }
      if (m instanceof PersistentModifier) {
        flags |= 8;
        if (m instanceof PokemonHeldItemModifier) {
          maxHeld = (m as any).getMaxHeldItemCount(player);
        } else {
          maxStack = m.getMaxStackCount();
          maxStackThreshold = m.getMaxStackCount(true);
        }
      }
    } else {
      // every type the generator can make, through its pregenArgs path, with the name and group each has
      let args: number[] = [];
      switch (GEN_KEYS[k]) {
        case "BERRY":
          args = Object.values(BerryType).filter(v => typeof v === "number") as number[];
          break;
        case "BASE_STAT":
          args = [Stat.HP, Stat.ATK, Stat.DEF, Stat.SPATK, Stat.SPDEF, Stat.SPD];
          break;
        case "TEMP_STAT":
          args = [Stat.ATK, Stat.DEF, Stat.SPATK, Stat.SPDEF, Stat.SPD, Stat.ACC];
          break;
        case "ATTACK_TYPE":
          args = Array.from({ length: 18 }, (_v, i) => i);
          break;
        case "TERA_SHARD":
          args = Array.from({ length: 19 }, (_v, i) => i);
          break;
        case "SPECIES_STAT":
        case "RARE_SPECIES_STAT":
          args = SPECIES_STAT_KEYS.map((_v, i) => i);
          break;
        case "TM_COMMON":
        case "TM_GREAT":
        case "TM_ULTRA": {
          const tier = { TM_COMMON: 0, TM_GREAT: 1, TM_ULTRA: 2 }[GEN_KEYS[k] as "TM_COMMON"];
          args = Object.keys(tmPoolTiers).map(Number).filter(m => tmPoolTiers[m] === tier).sort((a, b) => a - b);
          break;
        }
        case "EVOLUTION_ITEM":
        case "RARE_EVOLUTION_ITEM":
          args = (Object.values(EvolutionItem).filter(v => typeof v === "number") as number[])
            .filter(i => !!i && i > 50 === (GEN_KEYS[k] === "RARE_EVOLUTION_ITEM"));
          break;
        case "FORM_CHANGE_ITEM":
        case "RARE_FORM_CHANGE_ITEM":
          args = (Object.values(FormChangeItem).filter(v => typeof v === "number") as number[])
            .filter(i => (i && i < 150) === (GEN_KEYS[k] === "RARE_FORM_CHANGE_ITEM"));
          break;
        case "MINT":
          args = Object.values(Nature).filter(v => typeof v === "number") as number[];
          break;
        case "SHUCKLE_JUICE":
          args = [-15, 10];
          break;
        case "BLACK_SLUDGE":
          args = [-1];   // no pregenArgs: the one type it makes
          break;
        default:
          args = [];
      }
      for (const a of args) {
        const pregen = GEN_KEYS[k].endsWith("SPECIES_STAT") ? SPECIES_STAT_KEYS[a] : a;
        const g: any = GEN_KEYS[k] === "BLACK_SLUDGE" ? t.generateType([]) : t.generateType([], [pregen]);
        if (!g) {
          throw new Error(`${k}: no type for ${a}`);
        }
        if (a === args[0]) {
          if (g instanceof PokemonHeldItemModifierType) {
            flags |= 1;
          }
          if (g instanceof FormChangeItemModifierType) {
            flags |= 2;
          }
          if (g instanceof PokemonModifierType) {
            flags |= 4;
          }
          // a generated persistent modifier that isn't held (the X items): its stack limits
          let m: any = null;
          try {
            m = g instanceof PokemonModifierType ? null : g.newModifier();
          } catch {
            m = null;
          }
          if (m instanceof PersistentModifier && !(m instanceof PokemonHeldItemModifier)) {
            flags |= 8;
            maxStack = m.getMaxStackCount();
            maxStackThreshold = m.getMaxStackCount(true);
          }
        }
        // a generated held item: getMaxHeldItemCount() per type (a vitamin's is the holder's IV for its stat: -2);
        // the item's own is that one when every type shares it, else -3 (per type)
        let held = -1;
        let hm: any = null;
        try {
          hm = g instanceof PokemonModifierType ? g.newModifier(player) : null;
        } catch {
          hm = null;
        }
        if (hm instanceof PokemonHeldItemModifier) {   // form change items too
          held = GEN_KEYS[k] === "BASE_STAT" ? -2 : hm.getMaxHeldItemCount(player);
          maxHeld = a === args[0] || held === maxHeld ? held : -3;
        }
        genTypeRows.push([itemIndex(k), a, str(g.name), str(g.group), held]);
      }
      group = -1;
    }
    items.push(`{${genKind}, ${flags}, ${name}, ${group}, ${maxStack}, ${maxStackThreshold}, ${maxHeld}}`);
  }
  for (const r of genTypeRows) {
    genTypes.push(`{${r.join(", ")}}`);
  }

  // ---- the PLAYER pool ----
  const pool: any = getModifierPoolForType(ModifierPoolType.PLAYER);
  const entries: string[] = [];
  const tiers: string[] = [];
  const funcs: string[] = [];
  for (let t = 0; t <= ModifierTier.LUXURY; t++) {
    if (!Object.hasOwn(pool, t)) {
      tiers.push("{0, -1}");
      continue;
    }
    tiers.push(`{${entries.length}, ${pool[t].length}}`);
    for (const w of pool[t]) {
      const id = w.modifierType.id;
      expect(w.modifierType.tier).toBe(t);
      const fn = typeof w.weight === "function";
      if (fn) {
        funcs.push(`${id}@${t}`);
        if (!PORTED_WEIGHT_FUNCS.has(`${id}@${t}`)) {
          throw new Error(`${id}@${t}: a weight function src/pools.c doesn't port`);
        }
      }
      const maxW = typeof w.maxWeight === "function" ? -1 : w.maxWeight;
      entries.push(`{${itemIndex(id)}, ${fn ? -1 : w.weight}, ${maxW}}`);
    }
  }
  for (const f of PORTED_WEIGHT_FUNCS) {
    if (!funcs.includes(f)) {
      throw new Error(`${f}: listed as a weight function but upstream has none`);
    }
  }

  // ---- TMs ----
  const moveIds = Object.values(MoveId).filter(v => typeof v === "number") as number[];
  const maxMove = Math.max(...moveIds);
  const tmTier: number[] = new Array(maxMove + 1).fill(-1);
  for (const m of Object.keys(tmPoolTiers).map(Number)) {
    tmTier[m] = tmPoolTiers[m] | (allMoves[m].isUnimplemented ? 8 : 0);
  }
  const ids = Object.keys(speciesDataRegistry.data).map(Number).sort((a, b) => a - b);
  const tmLists: string[] = [];
  const tmListIndex = new Map<string, number>();
  const tmPool: number[] = [];
  const speciesRows: string[] = [];
  const formRows: string[] = [];
  const formKeyRows: number[] = [];
  const maxForms = ["gigantamax", "gigantamax-rapid", "gigantamax-single", "eternamax"];
  for (const id of ids) {
    const sp: any = speciesDataRegistry.getSpecies(id);
    const nForms = Math.max(sp.forms.length, 1);
    const formStart = formRows.length;
    for (let f = 0; f < nForms; f++) {
      const formKey = sp.forms.length ? sp.forms[f].formKey : "";
      const tms: number[] = speciesDataRegistry.getTms(id, formKey);
      const key = tms.join(",");
      let li = tmListIndex.get(key);
      if (li === undefined) {
        li = tmLists.length;
        tmListIndex.set(key, li);
        tmLists.push(`{${tmPool.length}, ${tms.length}}`);
        tmPool.push(...tms);
      }
      formRows.push(`{${li}, ${str(formKey)}, ${maxForms.includes(formKey) ? 1 : 0}}`);
      formKeyRows.push(str(formKey));
    }
    const flags = (speciesDataRegistry.hasEvolutions(id) ? 1 : 0) | (speciesDataRegistry.hasFormChanges(id) ? 2 : 0);
    speciesRows.push(`{${id}, ${flags}, ${sp.forms.length}, ${formStart}}`);
  }

  // ---- item evolutions ----
  const evoRows: string[] = [];
  const condRows: string[] = [];
  const listPool: number[] = [];
  const evoSpecies: string[] = [];
  const condKeys = Object.values(EvoCondKey) as number[];
  for (const id of ids) {
    if (!speciesDataRegistry.hasEvolutions(id)) {
      continue;
    }
    const start = evoRows.length;
    for (const e of speciesDataRegistry.getEvolutions(id) as any[]) {
      if (!e.item) {
        continue;
      }
      const cs: any[] = e.condition == null ? [] : e.condition.data;
      const cStart = condRows.length;
      for (const c of cs) {
        if (!condKeys.includes(c.key)) {
          throw new Error(`evolution condition ${c.key}`);
        }
        let a = 0;
        let list: number[] = [];
        switch (c.key) {
          case EvoCondKey.FRIENDSHIP:
          case EvoCondKey.RANDOM_FORM:
          case EvoCondKey.EVO_TREASURE_TRACKER:
            a = c.value;
            break;
          case EvoCondKey.MOVE:
          case EvoCondKey.TYROGUE:
            a = c.move;
            break;
          case EvoCondKey.TIME:
            list = c.time;
            break;
          case EvoCondKey.BIOME:
            list = c.biome;
            break;
          case EvoCondKey.NATURE:
            list = c.nature;
            break;
          case EvoCondKey.WEATHER:
            list = c.weather;
            break;
          case EvoCondKey.GENDER:
            a = c.gender;
            break;
          case EvoCondKey.MOVE_TYPE:
          case EvoCondKey.PARTY_TYPE:
            a = c.pkmnType;
            break;
          case EvoCondKey.SPECIES_CAUGHT:
            a = c.speciesCaught;
            break;
          case EvoCondKey.HELD_ITEM:
            a = SPECIES_STAT_KEYS.indexOf(c.itemKey);
            if (a < 0) {
              throw new Error(`held item condition ${c.itemKey}`);
            }
            break;
          case EvoCondKey.SHEDINJA:
            break;
        }
        condRows.push(`{${c.key}, ${a}, ${listPool.length}, ${list.length}}`);
        listPool.push(...list);
      }
      const pre = e.preFormKey == null ? -1 : str(e.preFormKey);
      evoRows.push(`{${e.speciesId}, ${e.item}, ${e.level}, ${pre}, ${cStart}, ${condRows.length - cStart}}`);
    }
    if (evoRows.length > start) {
      evoSpecies.push(`{${id}, ${start}, ${evoRows.length - start}}`);
    }
  }

  // ---- form changes with an item trigger ----
  // condition kinds: 1 hasAbility(ability) (canApply), 2 isDaily !== true, 3 hpRatio >= 0.5, 4 hpRatio < 0.5,
  // 5 the dex has caught `arg`
  const classify = (cond: any): [number, number] => {
    // the transpiled source: parenthesised parameters, enum members inlined as numbers
    const src = String(cond.predicate)
      .replace(/\s+/g, " ")
      .replace(/^\((\w*)\) =>/, "$1 =>")
      .replace(/__vite_ssr_import_\d+__\./g, "");
    const m = /^p => p\.hasAbility\((?:AbilityId\.([A-Z_]+)|(\d+))\)$/.exec(src);
    if (m) {
      return [1, m[1] ? (AbilityId as any)[m[1]] : Number(m[2])];
    }
    if (src === "() => globalScene.gameMode.isDaily !== true") {
      return [2, 0];
    }
    if (src === "p => p.getHpRatio() >= 0.5") {
      return [3, 0];
    }
    if (src === "p => p.getHpRatio() < 0.5") {
      return [4, 0];
    }
    if (/^_p => !!globalScene\.gameData\.dexData\[species\]\.caughtAttr$/.test(src)) {
      // probe which species it reads
      const real = globalScene.gameData.dexData;
      let read = -1;
      (globalScene.gameData as any).dexData = new Proxy({}, {
        get: (_t, k) => {
          read = Number(k);
          return { caughtAttr: 1n };
        },
      });
      try {
        cond.predicate(player);
      } finally {
        (globalScene.gameData as any).dexData = real;
      }
      return [5, read];
    }
    throw new Error(`form change condition the port doesn't know: ${src}`);
  };
  const fcRows: string[] = [];
  const fcCondRows: string[] = [];
  const fcSpecies: string[] = [];
  for (const id of ids) {
    if (!speciesDataRegistry.hasFormChanges(id)) {
      continue;
    }
    const start = fcRows.length;
    for (const fc of speciesDataRegistry.getFormChanges(id) as any[]) {
      const t: any = fc.findTrigger(SpeciesFormChangeItemTrigger);
      if (!t) {
        continue;
      }
      const cStart = fcCondRows.length;
      for (const c of fc.conditions) {
        if (!(c instanceof SpeciesFormChangeCondition)) {
          fcCondRows.push("{0, 0}");   // not a SpeciesFormChangeCondition: never counts
          continue;
        }
        const [kind, arg] = classify(c);
        fcCondRows.push(`{${kind}, ${arg}}`);
      }
      const fk: string = fc.formKey;
      const flags =
        (fk.indexOf("mega") !== -1 || fk.indexOf("primal") !== -1 ? 1 : 0)
        | (fk.indexOf("gigantamax") !== -1 || fk.indexOf("eternamax") !== -1 ? 2 : 0)
        | (t.active ? 4 : 0);
      fcRows.push(`{${t.item}, ${str(fc.preFormKey)}, ${flags}, ${cStart}, ${fcCondRows.length - cStart}}`);
    }
    if (fcRows.length > start) {
      fcSpecies.push(`{${id}, ${start}, ${fcRows.length - start}}`);
    }
  }

  // ---- output ----
  const enumName = (k: string) => `PR_ITEM_${k}`;
  const h = [
    "/* Generated by gen/dump-rewards.test.ts from upstream's modifierTypeInitObj. Do not edit. */",
    "#ifndef PR_REWARD_ITEMS_H",
    "#define PR_REWARD_ITEMS_H",
    "/* modifierTypes keys, in upstream's order */",
    "enum pr_item {",
    ...keys.map((k, i) => `    ${enumName(k)} = ${i},`),
    `    PR_ITEM_COUNT_ = ${keys.length}`,
    "};",
    "#endif",
    "",
  ];
  const c = [
    "/* Generated by gen/dump-rewards.test.ts from upstream's modifier types, the PLAYER pool, TM, evolution and",
    " * form change data. Do not edit. */",
    '#include "pr/reward.h"',
    "",
    `const char *const pr_reward_strings[${strings.length}] = {${strings.map(s => (s === "\u0000undefined" ? "NULL" : cstr(s))).join(", ")}};`,
    `const int pr_reward_string_count = ${strings.length};`,
    `const pr_item_info pr_items[${keys.length}] = {${items.join(", ")}};`,
    `const char *const pr_item_keys[${keys.length}] = {${keys.map(k => `"${k}"`).join(", ")}};`,
    `const pr_gen_type pr_gen_types[${genTypes.length}] = {${genTypes.join(", ")}};`,
    `const int pr_gen_type_count = ${genTypes.length};`,
    `const pr_pool_entry_p pr_player_pool_entries[${entries.length}] = {${entries.join(", ")}};`,
    `const pr_pool_tier pr_player_pool[6] = {${tiers.join(", ")}};`,
    `const int8_t pr_tm_tier[${maxMove + 1}] = {${tmTier.join(", ")}};`,
    `const int pr_tm_tier_count = ${maxMove + 1};`,
    `const pr_tm_list pr_tm_lists[${tmLists.length}] = {${tmLists.join(", ")}};`,
    `const uint16_t pr_tm_pool[${tmPool.length}] = {${tmPool.join(", ")}};`,
    `const pr_reward_species pr_reward_species_data[${speciesRows.length}] = {${speciesRows.join(", ")}};`,
    `const int pr_reward_species_count = ${speciesRows.length};`,
    `const pr_reward_form pr_reward_forms[${formRows.length}] = {${formRows.join(", ")}};`,
    `const pr_item_evo pr_item_evos[${evoRows.length}] = {${evoRows.join(", ")}};`,
    `const pr_evo_cond pr_evo_conds[${Math.max(condRows.length, 1)}] = {${condRows.join(", ") || "{0}"}};`,
    `const int16_t pr_evo_cond_lists[${Math.max(listPool.length, 1)}] = {${listPool.join(", ") || "0"}};`,
    `const pr_species_slice pr_item_evo_species[${evoSpecies.length}] = {${evoSpecies.join(", ")}};`,
    `const int pr_item_evo_species_count = ${evoSpecies.length};`,
    `const pr_item_fc pr_item_fcs[${fcRows.length}] = {${fcRows.join(", ")}};`,
    `const pr_fc_cond pr_fc_conds[${Math.max(fcCondRows.length, 1)}] = {${fcCondRows.join(", ") || "{0, 0}"}};`,
    `const pr_species_slice pr_item_fc_species[${fcSpecies.length}] = {${fcSpecies.join(", ")}};`,
    `const int pr_item_fc_species_count = ${fcSpecies.length};`,
    "",
  ];
  fs.writeFileSync(outH, h.join("\n"));
  fs.writeFileSync(outC, c.join("\n"));
  console.log(
    "items", keys.length, "gen types", genTypes.length, "pool", entries.length, "tm lists", tmLists.length,
    "tm pool", tmPool.length, "evos", evoRows.length, "fcs", fcRows.length, "strings", strings.length,
  );
});
