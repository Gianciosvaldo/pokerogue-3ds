/*
 * Upstream half of the boss health-segment differential test.
 * Copy into upstream 8555c08 as test/tests/zz-dump/bosssegment.producer.test.ts and run with:
 *   BOSSSEG_OUT=/tmp/bosssegments.txt BOSSSEG_N=10000 BOSSSEG_SEED=777 \
 *     npx vitest run test/tests/zz-dump/bosssegment.producer.test.ts
 */
import { calculateBossSegmentDamage } from "#utils/damage";
import * as fs from "node:fs";
import { it } from "vitest";

it("boss segment damage producer", () => {
  const N = Number(process.env.BOSSSEG_N ?? 10000);
  let seed = Number(process.env.BOSSSEG_SEED ?? 12345) >>> 0;
  const OUT = process.env.BOSSSEG_OUT ?? "/tmp/pr-bosssegments.txt";
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };

  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    const segments = 2 + rnd(7); // 2..8 covers ordinary, daily, legendary and high-segment bosses
    const maxHp = 40 + rnd(1961);
    const segmentHp = maxHp / segments;
    const minSegment = rnd(8) === 0 ? 1 : 0; // classic final-boss base form rule
    const segmentIndex = minSegment + rnd(segments - minSegment);

    const lower = Math.round(segmentHp * segmentIndex);
    const upper = Math.max(lower, Math.min(maxHp, Math.round(segmentHp * (segmentIndex + 1))));
    const currentHp = lower + rnd(upper - lower + 1);

    // Mix ordinary hits, exact boundaries and deliberately huge multi-shield hits.
    let damage: number;
    switch (rnd(5)) {
      case 0:
        damage = rnd(maxHp + 1);
        break;
      case 1:
        damage = Math.max(0, currentHp - Math.round(segmentHp * segmentIndex));
        break;
      case 2:
        damage = Math.max(0, currentHp - Math.round(segmentHp * segmentIndex)) + Math.round(segmentHp);
        break;
      default: {
        const exponent = rnd(segmentIndex + 2);
        damage = Math.max(0, currentHp - Math.round(segmentHp * segmentIndex))
          + Math.round(segmentHp * (2 ** exponent))
          + rnd(Math.max(1, Math.ceil(segmentHp)));
        break;
      }
    }

    const [adjusted, cleared] = calculateBossSegmentDamage(
      damage,
      currentHp,
      segmentHp,
      minSegment,
      segmentIndex,
    );
    lines.push([
      segments,
      maxHp,
      currentHp,
      minSegment,
      segmentIndex,
      damage,
      adjusted,
      cleared,
    ].join(" "));
  }

  fs.writeFileSync(OUT, lines.join("\n") + "\n");
});
