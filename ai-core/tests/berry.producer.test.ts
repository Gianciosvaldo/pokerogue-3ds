/* Additive Stuff Cheeks restriction differential. Copy to test/tests/zz-dump
 * in upstream@8555c08 and run BERRY_OUT=/tmp/ai-berry.txt npx vitest run
 * test/tests/zz-dump/berry.producer.test.ts. No existing producer is changed. */
import { allMoves } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { BattleType } from "#enums/battle-type";
import { BerryType } from "#enums/berry-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { BerryModifier, PokemonHeldItemModifier } from "#modifiers/modifier";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("Stuff Cheeks side-wide berry restriction producer", async () => {
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").ability(AbilityId.BALL_FETCH)
    .enemyAbility(AbilityId.BALL_FETCH).battleType(BattleType.WILD)
    .startingWave(3).enemySpecies(SpeciesId.MAGIKARP);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const mons = [game.field.getEnemyPokemon(), game.field.getPlayerPokemon()];
  const lines: string[] = [];
  for (let player = 0; player <= 1; player++) {
    const user = mons[player];
    const side = player ? 0 : 1;
    const record = (modSide: number, type: number, owner: number, stack: number) => {
      scene.modifiers = [];
      scene.enemyModifiers = [];
      if (modSide >= 0) {
        const realOwner = owner === 10 ? user.id : owner === 20 ? user.id + 1 : -1;
        const modifier = type >= 0
          ? new BerryModifier(null as any, realOwner, type, stack)
          : Object.assign(Object.create(PokemonHeldItemModifier.prototype), { pokemonId: realOwner, stackCount: stack });
        (modSide === 0 ? scene.modifiers : scene.enemyModifiers).push(modifier);
      }
      const [selectable] = allMoves[MoveId.STUFF_CHEEKS].checkRestrictions(user);
      lines.push(`${player} ${modSide} ${type} ${owner} ${stack} ${Number(selectable)}`);
    };
    record(-1, -1, 10, 1);
    record(side, -1, 10, 1);
    for (let type = BerryType.SITRUS; type <= BerryType.LEPPA; type++)
      for (const owner of [10, 20, -1])
        for (const stack of [0, 1]) {
          record(side, type, owner, stack);
          record(1 - side, type, owner, stack);
        }
  }
  fs.writeFileSync(process.env.BERRY_OUT ?? "/tmp/ai-berry.txt", `${lines.join("\n")}\n`);
  console.log(`Stuff Cheeks upstream: ${lines.length} cases`);
});
