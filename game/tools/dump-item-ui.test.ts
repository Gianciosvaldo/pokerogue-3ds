/*
 * Dumps what the reward screens show of each item, for game/ (the shop and reward options on the bottom screen), as C:
 * per modifierTypeInitObj key and, for a generator, per type it can make (the same arg values reward-core's dump uses):
 * the icon frame of the items atlas (type.iconImage), the English name (type.name) and description (type.getDescription()).
 * A description that reads the run (the money items read the wave's money) is dumped in two run states; when the two differ the
 * row is flagged DYNAMIC, only the money items may be, and their amount is written "{money}" for the game to fill in.
 * Run with game/tools/redump-item-ui.sh <upstream dir>; writes game/gen/gm_item_ui.c. The output is generated: don't edit it.
 */
import { globalScene } from "#app/global-scene";
import { tmPoolTiers } from "#balance/tm-pool-tiers";
import { EvolutionItem } from "#balance/pokemon-evolutions";
import { modifierTypes } from "#data/data-lists";
import { BerryType } from "#enums/berry-type";
import { FormChangeItem } from "#enums/form-change-item";
import { MoveId } from "#enums/move-id";
import { Nature } from "#enums/nature";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { ModifierTypeGenerator } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { expect, it } from "vitest";

// generator kinds, as reward-core's dump names them
const GEN_KEYS: Record<string, string> = {
  BERRY: "BERRY", BASE_STAT_BOOSTER: "BASE_STAT", TEMP_STAT_STAGE_BOOSTER: "TEMP_STAT", ATTACK_TYPE_BOOSTER: "ATTACK_TYPE",
  SPECIES_STAT_BOOSTER: "SPECIES_STAT", RARE_SPECIES_STAT_BOOSTER: "RARE_SPECIES_STAT", TM_COMMON: "TM_COMMON",
  TM_GREAT: "TM_GREAT", TM_ULTRA: "TM_ULTRA", EVOLUTION_ITEM: "EVOLUTION_ITEM", RARE_EVOLUTION_ITEM: "RARE_EVOLUTION_ITEM",
  FORM_CHANGE_ITEM: "FORM_CHANGE_ITEM", RARE_FORM_CHANGE_ITEM: "RARE_FORM_CHANGE_ITEM", MINT: "MINT",
  TERA_SHARD: "TERA_SHARD", MYSTERY_ENCOUNTER_SHUCKLE_JUICE: "SHUCKLE_JUICE", MYSTERY_ENCOUNTER_BLACK_SLUDGE: "BLACK_SLUDGE",
};
const SPECIES_STAT_KEYS = ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_SCALE", "DEEP_SEA_TOOTH"];

const cstr = (s: string) =>
  `"${s.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\n/g, "\\n").replace(/\?\?/g, "?\\?")}"`;

it("dump item ui", async () => {
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const outC = process.env.DUMP_OUT ?? "/tmp/gm_item_ui.c";
  expect(Object.keys(modifierTypes).length).toBeGreaterThan(100);

  const rows: { key: string; arg: number; icon: string; name: string; desc: string; dynamic: boolean }[] = [];
  // What the game shows next to the description by itself is cut: a ball's "(Inventory: N)" (the screen has its own count of
  // balls) and "(Hold C or Shift for more info)" (a keyboard hint). What is left is flagged dynamic when it differs between two
  // runs states (another wave, other ball counts and money).
  const clean = (d: string) =>
    d.replace(/ *\(Inventory: \d+\) */g, "").replace(/\s*\(Hold C or Shift for more info\)\.?/g, ".").replace(/\.\.$/, ".");
  const describe = (t: any) => {
    const wave = scene.currentBattle.waveIndex;
    const balls = { ...scene.pokeballCounts };
    const money = scene.money;
    const a = clean(t.getDescription());
    scene.currentBattle.waveIndex = 95;
    scene.money = 123456;
    for (const b of Object.keys(scene.pokeballCounts)) {
      scene.pokeballCounts[b] = 7;
    }
    const b = clean(t.getDescription());
    scene.currentBattle.waveIndex = wave;
    scene.money = money;
    Object.assign(scene.pokeballCounts, balls);
    if (a === b) {
      return { desc: a, dynamic: false };
    }
    // the only run-dependent description is the money items': the amount (formatMoney) is the one ₽ number in the text
    const amounts = a.match(/₽[\d,]+/g) ?? [];
    if (amounts.length !== 1 || a.replace(/₽[\d,]+/, "") !== b.replace(/₽[\d,]+/, "")) {
      throw new Error(`a description that depends on the run in a way this dump doesn't know: ${a} / ${b}`);
    }
    return { desc: a.replace(/₽[\d,]+/, "{money}"), dynamic: true };
  };
  for (const k of Object.keys(modifierTypes)) {
    const t: any = (modifierTypes as any)[k]();
    t.id = k;
    if (!(t instanceof ModifierTypeGenerator)) {
      const d = describe(t);
      rows.push({ key: k, arg: -1, icon: t.iconImage ?? "", name: t.name, desc: d.desc, dynamic: d.dynamic });
      continue;
    }
    const gk = GEN_KEYS[k];
    if (!gk) {
      throw new Error(`${k}: a generator this dump doesn't know`);
    }
    let args: number[] = [];
    switch (gk) {
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
        const tier = { TM_COMMON: 0, TM_GREAT: 1, TM_ULTRA: 2 }[gk as "TM_COMMON"];
        args = Object.keys(tmPoolTiers).map(Number).filter(m => tmPoolTiers[m] === tier).sort((a, b) => a - b);
        break;
      }
      case "EVOLUTION_ITEM":
      case "RARE_EVOLUTION_ITEM":
        args = (Object.values(EvolutionItem).filter(v => typeof v === "number") as number[])
          .filter(i => !!i && i > 50 === (gk === "RARE_EVOLUTION_ITEM"));
        break;
      case "FORM_CHANGE_ITEM":
      case "RARE_FORM_CHANGE_ITEM":
        args = (Object.values(FormChangeItem).filter(v => typeof v === "number") as number[])
          .filter(i => (i && i < 150) === (gk === "RARE_FORM_CHANGE_ITEM"));
        break;
      case "MINT":
        args = Object.values(Nature).filter(v => typeof v === "number") as number[];
        break;
      case "SHUCKLE_JUICE":
        args = [-15, 10];
        break;
      case "BLACK_SLUDGE":
        args = [-1];
        break;
    }
    for (const a of args) {
      const pregen = gk.endsWith("SPECIES_STAT") ? SPECIES_STAT_KEYS[a] : a;
      const g: any = gk === "BLACK_SLUDGE" ? t.generateType([]) : t.generateType([], [pregen]);
      if (!g) {
        throw new Error(`${k}: no type for ${a}`);
      }
      const d = describe(g);
      rows.push({ key: k, arg: a, icon: g.iconImage ?? "", name: g.name, desc: d.desc, dynamic: d.dynamic });
    }
  }
  rows.sort((x, y) => (x.key < y.key ? -1 : x.key > y.key ? 1 : x.arg - y.arg));
  const lines = rows.map(
    r => `  {${cstr(r.key)}, ${r.arg}, ${cstr(r.icon)}, ${cstr(r.name)}, ${cstr(r.desc)}, ${r.dynamic ? 1 : 0}},`,
  );
  fs.writeFileSync(
    outC,
    `/* Generated by game/tools/dump-item-ui.test.ts from upstream's modifier types: don't edit. */\n#include "gm_item_ui.h"\n\nconst gm_item_ui gm_item_ui_rows[] = {\n${lines.join("\n")}\n};\nconst int gm_item_ui_count = ${rows.length};\n`,
  );
  void globalScene;
});
