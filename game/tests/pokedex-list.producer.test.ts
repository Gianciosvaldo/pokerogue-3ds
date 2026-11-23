/*
 * Upstream half of the Pokedex list differential (game/tests/pokedex_model_test.c): the real PokedexUiHandler, headless, over random dex data.
 * Per case: every species' dex entry (seen and caught attributes), then the list as the handler opens it (default filters, sorted by number) and a
 * run of UP / DOWN / LEFT / RIGHT presses; after each press: the result, the cursor, the scroll position, whether the press entered the filter bar or the
 * filter text (the handler is put back in the grid afterwards: those modes are not ported yet), the info panel (number, name, the two type icons) and
 * the grid (the species of every visible cell and the tint of its icon: c clear, g grey, b black).
 * Run from the checkout's root (it reads assets/images/pokemon/variant/_masterlist.json, which the harness does not load: the species with shiny
 * variants decide each species' full unlock mask) with GC_PX_OUT=... GC_PX_N=... GC_PX_SEED=...
 *
 * Copy into an upstream checkout as test/tests/zz-dump/gc-pokedex-list.producer.test.ts.
 */
import { globalScene } from "#app/global-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { Button } from "#enums/buttons";
import { UiMode } from "#enums/ui-mode";
import { GameManager } from "#test/framework/game-manager";
import { variantData } from "#sprites/variant";
import * as fs from "node:fs";
import i18next from "i18next";
import Phaser from "phaser";
import { it } from "vitest";

it("dump pokedex list", async () => {
  new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  const outFile = process.env.GC_PX_OUT ?? "/tmp/gc_pokedex.txt";
  const cases = Number(process.env.GC_PX_N ?? 6);
  // the variant data the game loads at boot (which species have shiny variants decides their full unlock mask); the harness boots without it
  const master = JSON.parse(fs.readFileSync("assets/images/pokemon/variant/_masterlist.json", "utf8"));
  for (const k of Object.keys(master)) variantData[k] = master[k];
  const seed = process.env.GC_PX_SEED ?? "777";
  const steps = Number(process.env.GC_PX_STEPS ?? 450);
  const rnd = new Phaser.Math.RandomDataGenerator(["pokedex", seed]);
  const gameData = globalScene.gameData;
  const handler: any = globalScene.ui.handlers[UiMode.POKEDEX];
  const all = speciesDataRegistry.getAllSpecies();
  const out: string[] = [];

  out.push(`LOC pokemonNumber ${i18next.t("pokedexUiHandler:pokemonNumber", { dexNo: "{{dexNo}}", interpolation: { escapeValue: false } })}`);
  for (const s of all) out.push(`SPN ${s.speciesId} ${s.name}`);
  // getStarter(id): the species whose caught attributes mask a species' own (a starter is its own)
  for (const s of all) out.push(`STARTER ${s.speciesId} ${speciesDataRegistry.getStarter(s.speciesId)} ${speciesDataRegistry.isStarter(s.speciesId) ? 1 : 0}`);

  // the icons keep the tint they were given (the headless sprites do not); setSpecies makes a new icon each time
  const patch = (c: any) => {
    const icon = c.icon;
    icon.__tint = -1;
    icon.setTint = (v: number) => ((icon.__tint = v), icon);
    icon.clearTint = () => ((icon.__tint = -1), icon);
  };
  for (const c of handler.pokemonContainers) {
    const orig = c.setSpecies.bind(c);
    c.setSpecies = (sp: any, o: any) => {
      orig(sp, o);
      patch(c);
    };
    patch(c);
  }
  const tintChar = (c: any) => {
    if (!c.visible) return "-";
    const t = c.icon.__tint;
    return t === -1 ? "c" : t === 0 ? "b" : t === 0x808080 ? "g" : "?";
  };
  const state = () => {
    const t1 = handler.type1Icon;
    const t2 = handler.type2Icon;
    const ty = (i: any) => (i.__vis ? String(i.__frame) : "-");
    const grid = handler.pokemonContainers.map(tintChar).join("");
    const ids = handler.pokemonContainers[0].species.speciesId;
    return `${handler.pokemonNumberText.text}|${handler.pokemonNameText.text}|${ty(t1)}|${ty(t2)}|${ids}|${grid}`;
  };
  for (const t of [handler.type1Icon, handler.type2Icon]) {
    t.__frame = "";
    t.__vis = false;
    t.setFrame = (f: string) => ((t.__frame = f), t);
    t.setVisible = (v: boolean) => ((t.__vis = v), t);
  }

  const buttons = [Button.UP, Button.DOWN, Button.LEFT, Button.RIGHT];
  for (let c = 0; c < cases; c++) {
    // the roots of the evolution lines first: their catches mask the others'
    const caughtOf = (full: bigint) => {
      let v = 0n;
      for (let b = 0n; b < 20n; b++) if ((full >> b) & 1n && rnd.integerInRange(0, 2) > 0) v |= 1n << b;
      return v;
    };
    const mode = rnd.integerInRange(0, 3); // 0: little caught, 1: some, 2: most, 3: all
    for (const s of all) {
      const e = gameData.dexData[s.speciesId];
      const full = s.getFullUnlocksData();
      const roll = rnd.integerInRange(0, 9);
      const caught = roll < [1, 4, 8, 10][mode];
      // mostly what the species can unlock, sometimes bits it cannot (a prevolution's catch brings the evolved form's bit): the effective catch masks them
      const outside = rnd.integerInRange(0, 5) === 0; // a catch made only of bits the species cannot unlock: it counts as nothing
      e.caughtAttr = caught ? (outside ? (caughtOf(0xfffffn) & ~full) | 1n << 19n : caughtOf(full) | (rnd.integerInRange(0, 4) === 0 ? caughtOf(0xfffffn) : 0n)) : 0n;
      e.seenAttr = rnd.integerInRange(0, 3) === 0 ? caughtOf(full) || 1n : 0n;
    }
    out.push(`CASE ${c}`);
    for (const s of all) {
      const e = gameData.dexData[s.speciesId];
      out.push(`DEX ${s.speciesId} ${e.seenAttr} ${e.caughtAttr}`);
    }
    // stored form preferences: only valid ones (a selectable form the starter has caught), as the starter select stores them
    handler.starterPreferences = {};
    for (const s of all) {
      if (!speciesDataRegistry.isStarter(s.speciesId) || rnd.integerInRange(0, 2) > 0) continue;
      const own = gameData.dexData[s.speciesId].caughtAttr & s.getFullUnlocksData();
      const ok = s.forms.map((f: any, i: number) => i).filter((i: number) => i > 0 && s.forms[i].isStarterSelectable && (own & (1n << BigInt(7 + i))) !== 0n);
      if (ok.length === 0) continue;
      const f = rnd.pick(ok);
      handler.starterPreferences[s.speciesId] = { formIndex: f };
      out.push(`PREF ${s.speciesId} ${f}`);
    }
    handler.show([]);
    handler.setFilterMode(false);
    handler.setFilterTextMode(false);
    out.push(`OPEN ${handler.filteredPokemonData.length} ${handler.cursor} ${handler.scrollCursor} ${state()}`);
    for (let n = 0; n < steps; n++) {
      // 100 presses of any button, 200 almost all DOWN (the list is 121 rows long: this reaches the end, wraps and goes on), then leaning UP
      const lean = n < 100 ? buttons : n < 300 ? [Button.DOWN, Button.DOWN, Button.DOWN, Button.DOWN, Button.DOWN, Button.DOWN, Button.DOWN, Button.DOWN, Button.DOWN, ...buttons] : [...buttons, Button.UP, Button.UP, Button.UP];
      const b = rnd.pick(lean);
      const ok = handler.processInput(b);
      const mode = handler.filterMode ? "bar" : handler.filterTextMode ? "text" : "grid";
      if (handler.filterMode) handler.setFilterMode(false);
      if (handler.filterTextMode) handler.setFilterTextMode(false);
      out.push(`NAV ${b} ${ok ? 1 : 0} ${handler.cursor} ${handler.scrollCursor} ${mode} ${state()}`);
    }
    // a scripted walk to the short last row: opened again, DOWN to the last row, along it (it has fewer than 9 species), back, DOWN wraps, UP
    handler.show([]);
    handler.setFilterMode(false);
    handler.setFilterTextMode(false);
    out.push(`OPEN ${handler.filteredPokemonData.length} ${handler.cursor} ${handler.scrollCursor} ${state()}`);
    const script = [...new Array(120).fill(Button.DOWN), ...new Array(5).fill(Button.RIGHT), ...new Array(4).fill(Button.LEFT), Button.DOWN, Button.DOWN, Button.UP, Button.UP, Button.UP];
    for (const b of script) {
      const ok = handler.processInput(b);
      const mode = handler.filterMode ? "bar" : handler.filterTextMode ? "text" : "grid";
      if (handler.filterMode) handler.setFilterMode(false);
      if (handler.filterTextMode) handler.setFilterTextMode(false);
      out.push(`NAV ${b} ${ok ? 1 : 0} ${handler.cursor} ${handler.scrollCursor} ${mode} ${state()}`);
    }
  }
  fs.writeFileSync(outFile, `${out.join("\n")}\n`);
}, 600_000);
