/*
 * Upstream half of run-core's experience test (tools/difftest.sh copies it into the upstream checkout):
 * getLevelTotalExp and getLevelRelExp for every GrowthRate at every level from 0 to 3,000, plus N random
 * levels up to 100,000. build/exptest replays them.
 */
import { GrowthRate, getLevelRelExp, getLevelTotalExp } from "#data/exp";
import * as fs from "node:fs";
import { it } from "vitest";

it("exp producer", () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-exp-cases.txt";
  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const rates = Object.values(GrowthRate).filter(v => typeof v === "number") as number[];
  const levels = [...Array.from({ length: 3001 }, (_, i) => i), ...Array.from({ length: N }, () => 1 + rnd(100000))];
  const lines: string[] = [];
  for (const level of levels) {
    for (const r of rates) {
      lines.push(`X ${r} ${level} ${getLevelTotalExp(level, r) ?? "NaN"} ${getLevelRelExp(level, r) ?? "NaN"}`);
    }
  }
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
});
