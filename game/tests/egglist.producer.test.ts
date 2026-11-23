/*
 * Upstream half of the Egg List differential (game/tests/egglist_model_test.c): the real EggListUiHandler, headless, over random egg lists.
 * Per case: the eggs (id, tier, species, source type, hatch waves, timestamp, as the Egg objects hold them) and, for every egg, what the handler's
 * details panel shows for it (setEggDetails: the egg's icon key, the name "Egg (Common)", the date, the hatch message and the gacha type text);
 * then a run of 300 UP / DOWN / LEFT / RIGHT presses (a third of them leaning DOWN, a third UP) on the grid, after each the result of processInput, the cursor and the scroll position
 * (the ScrollableGridHelper that the Pokedex list shares). Also the English text of every key the panel uses and the name of every species it names.
 * Run with TZ=UTC (the date is local time).
 *
 * Copy into an upstream checkout as test/tests/zz-dump/gc-egglist.producer.test.ts. GC_EL_N cases (default 14), GC_EL_SEED, GC_EL_OUT.
 */
import { globalScene } from "#app/global-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { Egg, getLegendaryGachaSpeciesForTimestamp } from "#data/egg";
import { Button } from "#enums/buttons";
import { UiMode } from "#enums/ui-mode";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import i18next from "i18next";
import Phaser from "phaser";
import { it } from "vitest";

const KEYS = [
  "egg", "greatTier", "ultraTier", "masterTier", "defaultTier", "manaphyTier", "hatchWavesMessageSoon", "hatchWavesMessageClose",
  "hatchWavesMessageNotClose", "hatchWavesMessageLongTime", "gachaTypeLegendary", "gachaTypeMove", "gachaTypeShiny", "eventType",
];

it("dump egg list", async () => {
  new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  const outFile = process.env.GC_EL_OUT ?? "/tmp/gc_egglist.txt";
  const cases = Number(process.env.GC_EL_N ?? 14);
  const seed = process.env.GC_EL_SEED ?? "777";
  const rnd = new Phaser.Math.RandomDataGenerator(["egglist", seed]);
  const gameData = globalScene.gameData;
  const handler: any = globalScene.ui.handlers[UiMode.EGG_LIST];
  const allSpecies = speciesDataRegistry.getAllSpecies().filter(s => s.speciesId <= 1025); // an egg holds a base species (regional forms are 2000+)
  // the headless scene has no sprites: plain stand-ins for the icons and the cursor, the texts and the grid logic are the real ones
  const icon = (): any => {
    const o: any = { x: 0, y: 0, frame: "" };
    o.setFrame = (f: string) => ((o.frame = f), o);
    o.visible = false;
    o.setVisible = (v: boolean) => ((o.visible = v), o);
    return o;
  };
  handler.initEggIcons = function () {
    this.eggIcons = [];
    for (let i = 0; i < Math.min(this.ROWS * this.COLUMNS, globalScene.gameData.eggs.length); i++) this.eggIcons.push(icon());
  };
  handler.iconAnimHandler = { addOrUpdate: () => {}, removeAll: () => {} };
  handler.cursorObj = { setPositionRelative: () => {}, setVisible: () => {}, setPosition: () => {} };
  const out: string[] = [];
  // the panel's name and date and the icon grid as they stand: "m" is the Manaphy egg, "-" an icon that is hidden
  const state = () =>
    `${handler.eggNameText.text}|${handler.eggDateText.text}|${handler.eggIcons.map((i: any) => (i.visible ? (i.frame === "manaphy" ? "m" : i.frame) : "-")).join("")}`;
  // a screen is opened fresh: the handler's cursor is -1 (what clear() leaves) and the grid helper starts at 0, 0
  const fresh = () => {
    handler.cursor = -1;
    handler.scrollGridHandler.cursor = 0;
    handler.scrollGridHandler.scrollCursor = 0;
  };

  for (const k of KEYS) out.push(`LOC ${k} ${i18next.t(`egg:${k}`)}`);
  out.push(`LOC sameSpeciesEgg ${i18next.t("egg:sameSpeciesEgg", { species: "{{species}}", interpolation: { escapeValue: false } })}`);
  const names = new Map<number, string>();
  const nameOf = (id: number) => {
    if (!names.has(id)) names.set(id, speciesDataRegistry.getSpecies(id).getName());
    return names.get(id)!;
  };

  const waves = [0, 1, 5, 6, 15, 16, 50, 51, 100, 150];
  const totals = [1, 2, 10, 11, 12, 22, 98, 99, 100, 101, 150];
  const eggLines: string[] = [];
  for (let c = 0; c < cases; c++) {
    const total = c === 0 ? 1 : rnd.pick(totals);
    const eggs: Egg[] = [];
    for (let i = 0; i < total; i++) {
      const source = rnd.integerInRange(0, 5); // 5: no source (a legacy egg)
      // the constructor rolls a species (and reads the dex) when it is not given one: always give it, then set the fields it overwrites
      const species = allSpecies[rnd.integerInRange(0, allSpecies.length - 1)].speciesId;
      const egg = new Egg({
        id: rnd.integerInRange(0, 6) === 0 ? 204 * rnd.integerInRange(0, 5000) : rnd.integerInRange(1, 4000000000),
        sourceType: source === 5 ? undefined : source,
        hatchWaves: rnd.pick(waves),
        species,
        timestamp: 1640995200000 + rnd.integerInRange(0, 365 * 5 * 86400) * 1000 + rnd.integerInRange(0, 86399999),
      });
      (egg as any)._tier = rnd.integerInRange(0, 3);
      // a legacy egg has no species yet; a Manaphy egg is a Common one with an id divisible by 204 and none
      if (source !== 3 && rnd.integerInRange(0, 4) === 0) (egg as any)._species = 0;
      eggs.push(egg);
    }
    gameData.eggs = eggs;
    out.push(`CASE ${c} ${total}`);
    fresh();
    handler.show([]);
    for (let i = 0; i < total; i++) {
      const e: any = eggs[i];
      if (e.species) nameOf(e.species);
      if (e.sourceType === 1) nameOf(getLegendaryGachaSpeciesForTimestamp(e.timestamp));
      out.push(`EGG ${i} ${e.id} ${e.tier} ${e.species} ${e.sourceType === undefined ? -1 : e.sourceType} ${e.hatchWaves} ${e.timestamp}`);
      handler.setEggDetails(i);
      out.push(
        `TXT ${i} ${e.getKey()}|${handler.eggNameText.text}|${handler.eggDateText.text}|${handler.eggHatchWavesText.text}|${handler.eggGachaInfoText.text}`,
      );
    }
    // navigation from a fresh show
    fresh();
    handler.show([]);
    const buttons = [Button.UP, Button.DOWN, Button.LEFT, Button.RIGHT];
    out.push(`NAV0 ${handler.cursor} ${handler.scrollGridHandler.scrollCursor} ${state()}`);
    for (let s = 0; s < 300; s++) {
      // 100 presses of any button, 100 leaning DOWN (to reach the scrolled rows and the end of the list), 100 leaning UP
      const b = s < 100 ? rnd.pick(buttons) : rnd.pick(s < 200 ? [...buttons, Button.DOWN, Button.DOWN] : [...buttons, Button.UP, Button.UP]);
      const ok = handler.scrollGridHandler.processInput(b);
      out.push(`NAV ${b} ${ok ? 1 : 0} ${handler.cursor} ${handler.scrollGridHandler.scrollCursor} ${state()}`);
    }
  }
  for (const [id, n] of names) eggLines.push(`SPN ${id} ${n}`);
  fs.writeFileSync(outFile, `${eggLines.join("\n")}\n${out.join("\n")}\n`);
});
