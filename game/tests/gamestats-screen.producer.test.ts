/*
 * Upstream half of the Game Stats screen differential (game/tests/gamestats_model_test.c): the real GameStatsUiHandler, headless, shown over random
 * profiles. For each case the 35 gameStats counters and some dexData entries (seenCount, seenAttr, caughtAttr) are set at random, the handler is
 * shown and the 18 label/value slots of every scroll position (cursor 0..maxCursorPos) are read. The 40 stats are rebuilt from the pages (a stat
 * must read the same wherever it appears), so the fixture holds one row per stat: the label as shown ("???" for a hidden stat that is 0) and the value.
 * Also the label text of every key (i18next, English), the stats per page and the last cursor position.
 *
 * Copy into an upstream checkout as test/tests/zz-dump/gc-gsscreen.producer.test.ts. GC_GSS_N cases (default 60), GC_GSS_SEED, GC_GSS_OUT.
 */
import { globalScene } from "#app/global-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { UiMode } from "#enums/ui-mode";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import i18next from "i18next";
import Phaser from "phaser";
import { it } from "vitest";

const GS_FIELDS = [
  "playTime", "battles", "ribbonsOwned", "classicSessionsPlayed", "sessionsWon", "dailyRunSessionsPlayed", "dailyRunSessionsWon",
  "endlessSessionsPlayed", "highestEndlessWave", "highestMoney", "highestDamage", "highestHeal", "pokemonSeen", "pokemonDefeated",
  "pokemonCaught", "pokemonHatched", "subLegendaryPokemonSeen", "subLegendaryPokemonCaught", "subLegendaryPokemonHatched",
  "legendaryPokemonSeen", "legendaryPokemonCaught", "legendaryPokemonHatched", "mythicalPokemonSeen", "mythicalPokemonCaught",
  "mythicalPokemonHatched", "shinyPokemonSeen", "shinyPokemonCaught", "shinyPokemonHatched", "pokemonFused", "trainersDefeated",
  "eggsPulled", "rareEggsPulled", "epicEggsPulled", "legendaryEggsPulled", "manaphyEggsPulled",
];
const LABEL_KEYS = [
  "playTime", "totalBattles", "starters", "shinyStarters", "speciesEncountered", "speciesSeen", "speciesCaught", "ribbonsOwned", "classicRuns",
  "classicWins", "dailyRunAttempts", "dailyRunWins", "endlessRuns", "highestWaveEndless", "highestMoney", "highestDamage", "highestHpHealed",
  "pokemonEncountered", "pokemonDefeated", "pokemonCaught", "eggsHatched", "subLegendsSeen", "subLegendsCaught", "subLegendsHatched",
  "legendsSeen", "legendsCaught", "legendsHatched", "mythicalsSeen", "mythicalsCaught", "mythicalsHatched", "shiniesSeen", "shiniesCaught",
  "shiniesHatched", "pokemonFused", "trainersDefeated", "eggsPulled", "rareEggsPulled", "epicEggsPulled", "legendaryEggsPulled",
  "manaphyEggsPulled",
];
const COUNT = 40;

it("dump game stats screen", async () => {
  new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  const outFile = process.env.GC_GSS_OUT ?? "/tmp/gc_gsscreen.txt";
  const cases = Number(process.env.GC_GSS_N ?? 60);
  const seed = process.env.GC_GSS_SEED ?? "777";
  const rnd = new Phaser.Math.RandomDataGenerator(["gsscreen", seed]);
  const gameData = globalScene.gameData;
  const handler: any = globalScene.ui.handlers[UiMode.GAME_STATS];
  const allSpecies = speciesDataRegistry.getAllSpecies();
  const starters = allSpecies.filter(s => speciesDataRegistry.isStarter(s.speciesId));
  const out: string[] = [];

  for (const k of LABEL_KEYS) out.push(`LABEL ${k} ${i18next.t(`gameStatsUiHandler:${k}`)}`);
  out.push(`ABBR ${["abrThousand", "abrMillion", "abrBillion"].map(k => i18next.t(`common:${k}`)).join(" ")}`);
  out.push(`SPECIES ${allSpecies.length} STARTERS ${speciesDataRegistry.getAllStarters().length}`);

  // the values that matter: 0 (hidden stats), 1, powers of ten and their neighbours (the abbreviation edges), the largest 32-bit value
  const edges = [0, 1, 2, 9, 99, 999, 1000, 1001, 1500, 12345, 99999, 100000, 999999, 1000000, 1234567, 2000000, 999999999, 1000000000, 4294967295];
  const num = (): number => {
    const r = rnd.integerInRange(0, 9);
    if (r < 3) return 0;
    if (r < 5) return edges[rnd.integerInRange(0, edges.length - 1)];
    if (r < 8) return rnd.integerInRange(0, 5000);
    return rnd.integerInRange(0, 4294967295);
  };
  const attr = (): bigint => {
    const r = rnd.integerInRange(0, 5);
    if (r === 0) return 0n;
    if (r === 1) return 2n;                // shiny only
    if (r === 2) return 1n | 4n | 128n;    // default starter attrs
    if (r === 3) return 1n | 2n | 4n | 8n | 128n;
    return BigInt(rnd.integerInRange(1, 1023));
  };

  const dexIds = allSpecies.map(s => s.speciesId);
  // the new-game dex (GameData.defaultDexData shares its entries with dexData, so it cannot be used to go back)
  const defaults = new Map<number, [number, bigint, bigint]>();
  for (const id of dexIds) {
    const e: any = gameData.dexData[id];
    defaults.set(id, [e.seenCount, e.seenAttr, e.caughtAttr]);
  }
  for (let c = 0; c < cases; c++) {
    // a case starts from the profile a new game has (the default dex) and changes some of it
    const touched: number[] = [];
    for (const k of GS_FIELDS) (gameData.gameStats as any)[k] = c === 0 ? 0 : num();
    const nDex = c === 0 ? 0 : rnd.pick([0, 3, 12, 60, 300]);
    for (let i = 0; i < nDex; i++) {
      const pool = rnd.integerInRange(0, 1) ? starters.map(s => s.speciesId) : dexIds;
      const id = pool[rnd.integerInRange(0, pool.length - 1)];
      const e: any = gameData.dexData[id];
      e.seenCount = rnd.pick([0, 0, 1, 4, 30]);
      e.seenAttr = attr();
      e.caughtAttr = rnd.integerInRange(0, 2) ? attr() : 0n;
      touched.push(id);
    }
    out.push(`CASE ${c}`);
    out.push(`GS ${GS_FIELDS.map(k => `${k}=${(gameData.gameStats as any)[k]}`).join(" ")}`);
    for (const id of new Set(touched)) {
      const e: any = gameData.dexData[id];
      out.push(`DEX ${id} ${e.seenCount} ${e.seenAttr} ${e.caughtAttr}`);
    }
    handler.show([]);
    const perPage: number = handler.statsPerPage;
    const cols: number = handler.columnCount;
    const maxCursor: number = handler.maxCursorPos;
    const rows: string[] = new Array(COUNT).fill("");
    for (let cur = 0; cur <= maxCursor; cur++) {
      handler.setCursor(cur);
      for (let s = 0; s < perPage; s++) {
        const idx = cur * cols + s;
        const text = `${handler.statLabels[s].text}|${handler.statValues[s].text}`;
        if (idx >= COUNT) {
          if (text !== "|") throw new Error(`slot past the end is not empty: ${text}`);
          continue;
        }
        if (rows[idx] && rows[idx] !== text) throw new Error(`stat ${idx} reads ${rows[idx]} and ${text}`);
        rows[idx] = text;
      }
    }
    if (rows.some(r => !r)) throw new Error("a stat was never on a page");
    out.push(`PAGE perPage=${perPage} columns=${cols} maxCursor=${maxCursor}`);
    rows.forEach((r, i) => out.push(`ROW ${i} ${r}`));
    out.push("END");
    // back to the new-game profile for the next case
    for (const id of new Set(touched)) {
      const e: any = gameData.dexData[id];
      [e.seenCount, e.seenAttr, e.caughtAttr] = defaults.get(id)!;
    }
  }
  fs.writeFileSync(outFile, `${out.join("\n")}\n`);
});
