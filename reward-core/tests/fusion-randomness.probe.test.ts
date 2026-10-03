/* Diagnostic only: establishes the missing unseeded draw in the old fusion trace.
 * No C differential claim. The original producer/check remain unchanged. */
import { Status, getRandomStatus } from "#data/status-effect";
import { StatusEffect } from "#enums/status-effect";
import Phaser from "phaser";
import { expect, it } from "vitest";

it("same seeded state allows different fusion statuses through Math.random", () => {
  const a = new Status(StatusEffect.SLEEP), b = new Status(StatusEffect.PARALYSIS);
  const rng = new Phaser.Math.RandomDataGenerator(["fusion-unrecorded-draw"]);
  const originalRng = Phaser.Math.RND;
  Phaser.Math.RND = rng;
  const before = rng.state(), random = Math.random;
  let first: Status | null, second: Status | null;
  try {
    Math.random = () => 0;
    first = getRandomStatus(a, b);
    Math.random = () => 0.999999;
    second = getRandomStatus(a, b);
  } finally { Math.random = random; Phaser.Math.RND = originalRng; }
  expect(first).toBe(b); expect(second).toBe(a);
  expect(rng.state()).toBe(before);
  console.log(`FUSION_RNG_PROBE same inputs/seed: Math.random=0 -> ${first!.effect}; Math.random=0.999999 -> ${second!.effect}; seeded state unchanged`);
});
