/* Additive adapter test: construct actual upstream held modifiers and record item loss.
 * Copy to test/tests/zz-dump/ and run ITEM_BRIDGE_OUT=/tmp/itembridge.txt npx vitest run ... */
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { BerryType } from "#enums/berry-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { GameManager } from "#test/framework/game-manager";
import fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("held item adapter producer", async () => {
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").ability(AbilityId.BALL_FETCH)
    .enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const pokemon = [game.field.getPlayerPokemon(), game.field.getEnemyPokemon()] as any[];
  const entries: [string, number][] = ["LEFTOVERS", "SHELL_BELL", "FOCUS_BAND", "QUICK_CLAW",
    "KINGS_ROCK", "WHITE_HERB"].map(key => [key, -1]);
  for (const value of Object.values(BerryType)) if (typeof value === "number") entries.push(["BERRY", value]);
  const lines: string[] = [];
  for (let side = 0; side < 2; side++) for (const [key, arg] of entries) {
    const type = generateModifierType((modifierTypes as any)[key], key === "BERRY" ? [arg] : undefined) as any;
    for (const owner of [123, 456, 4294967295]) for (let stack = 1; stack <= 4; stack++) {
      const m = type.newModifier({ id: owner });
      m.stackCount = stack;
      lines.push(`M ${key} ${arg} ${side} ${owner} ${stack} ${m.constructor.name} ${+m.isTransferable} ${m.berryType ?? 0}`);
    }
    if (key !== "BERRY" && key !== "WHITE_HERB") continue;
    for (let stack = 1; stack <= 4; stack++) {
      const p = pokemon[side];
      p.id = side ? 456 : 123;
      const m = type.newModifier(p);
      m.stackCount = stack;
      scene.modifiers = side ? [] : [m];
      scene.enemyModifiers = side ? [m] : [];
      for (let step = 1; step <= stack; step++) {
        if (!p.loseHeldItem(m)) throw new Error("upstream item loss rejected");
        const remaining = (side ? scene.enemyModifiers : scene.modifiers).reduce((n: number, x: any) => n + x.stackCount, 0);
        lines.push(`C ${key} ${arg} ${side} ${p.id} ${stack} ${step} ${remaining}`);
      }
    }
  }
  fs.writeFileSync(process.env.ITEM_BRIDGE_OUT ?? "/tmp/itembridge.txt", lines.join("\n") + "\n");
  console.log(`item bridge producer: ${lines.length} cases`);
}, 300000);
