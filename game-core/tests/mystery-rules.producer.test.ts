/* The mystery encounters' selection rules at the pinned upstream (battle-scene.ts getMysteryEncounter reads them): per encounter its tier, the effective per-run cap
 * (maxAllowedEncounters after the constructor's `(max ?? tier === ROGUE) ? 1 : 2` rule), the wave range, the least party size, the money multiplier, the wave-modulus
 * gate and the disallowed challenges; per biome the encounters that can spawn there (mysteryEncountersByBiome after initMysteryEncounters).
 * GC_RULES_OUT=/abs/rules.txt, Node22. */
import { BiomeId } from "#enums/biome-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import * as ME from "#mystery-encounters/mystery-encounter-biomes";
import * as fs from "node:fs";
import { expect, it } from "vitest";

it("dump the mystery encounter rules", () => {
  ME.initMysteryEncounters();
  const out: string[] = [];
  for (const name of Object.keys(MysteryEncounterType).filter(k => Number.isNaN(Number(k)))) {
    const type = (MysteryEncounterType as any)[name];
    const e: any = (ME.allMysteryEncounters as any)[type];
    if (!e) { out.push(`T ${type} ${name} none`); continue; }
    let wmin = 0, wmax = 0, party = 0, money = 0, mod = 0;
    for (const r of e.requirements) {
      const c = r.constructor.name;
      if (c === "WaveRangeRequirement") { wmin = r.waveRange[0]; wmax = r.waveRange[1]; }
      else if (c === "PartySizeRequirement") party = r.partySizeRange[0];
      else if (c === "WaveModulusRequirement") for (const m of r.waveModuli) mod |= 1 << m;
      else if (c === "MoneyRequirement") money = r.scalingMultiplier;
    }
    let chal = 0;
    for (const c of e.disallowedChallenges ?? []) chal |= 1 << c;
    out.push(`T ${type} ${name} tier=${e.encounterTier} max=${e.maxAllowedEncounters} wave=${wmin}-${wmax} party=${party} money=${money} mod=${mod} chal=${chal}`);
  }
  for (const [biome, list] of ME.mysteryEncountersByBiome) {
    out.push(`B ${biome} ${list.map(t => t).sort((a, b) => a - b).join(",")}`);
  }
  fs.writeFileSync(process.env.GC_RULES_OUT ?? "/tmp/mystery-rules.txt", out.join("\n") + "\n");
  expect(out.length).toBeGreaterThan(10);
});
