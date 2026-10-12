/* Additive Pay Day pickup oracle: the real Battle.pickUpScatteredMoney with real Amulet Coin modifiers and the real
 * Happy Hour arena tag. Records the scattered money, starting money, Amulet Coin stacks, Happy Hour, and the money
 * credited. GC_PAYDAY_N=2000 GC_PAYDAY_SEED=777 GC_PAYDAY_OUT=/abs/payday.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { ArenaTagType } from "#enums/arena-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

it("pay day pickup producer", async () => {
  const N = Number(process.env.GC_PAYDAY_N ?? 2000), SEED = Number(process.env.GC_PAYDAY_SEED ?? 777);
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 100).toBe(true);
  let seed = SEED;
  const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE)
    .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  scene.updateMoneyText = () => {}; scene.animateMoneyChanged = () => {}; scene.validateAchvs = () => {};
  const lines: string[] = [`V 1 ${N}`];
  let happy = 0, coins = 0, capped = 0, zero = 0;
  for (let i = 0; i < N; i++) {
    scene.modifiers.length = 0;
    for (const t of [...scene.arena.tags]) scene.arena.removeTag(t.tagType);
    const stack = i % 6 === 0 ? 0 : rnd(6), hh = rnd(3) === 0;
    if (stack) {
      const f = (modifierTypes as any).AMULET_COIN, m = f().withIdFromFunc(f).newModifier();
      m.stackCount = stack; scene.modifiers.push(m);
    }
    if (hh) expect(scene.arena.addTag(ArenaTagType.HAPPY_HOUR, 5, MoveId.HAPPY_HOUR, 0, ArenaTagSide.BOTH, true)).toBe(true);
    const scattered = i % 11 === 0 ? 0 : [1 + rnd(5000), 1 + rnd(200), 99999, 12345678][rnd(4)];
    const money = [rnd(100000), 0, Number.MAX_SAFE_INTEGER - rnd(3000), Number.MAX_SAFE_INTEGER][rnd(4)];
    scene.money = money; scene.currentBattle.moneyScattered = scattered;
    scene.currentBattle.pickUpScatteredMoney();
    expect(scene.currentBattle.moneyScattered).toBe(0);
    lines.push(`C ${i} ${scattered} ${money} ${stack} ${+hh} ${scene.money}`);
    happy += +hh; coins += +(stack > 0); capped += +(scene.money === Number.MAX_SAFE_INTEGER); zero += +(scattered === 0);
  }
  for (const n of [happy, coins, capped, zero]) expect(n).toBeGreaterThan(0);
  lines.push(`Z ${N}`);
  fs.writeFileSync(process.env.GC_PAYDAY_OUT ?? "/tmp/game-payday.txt", lines.join("\n") + "\n");
  console.log(`PAYDAY ${SEED}: ${N} cases happy=${happy} coin=${coins} capped=${capped} zero=${zero}`);
}, 240000);
