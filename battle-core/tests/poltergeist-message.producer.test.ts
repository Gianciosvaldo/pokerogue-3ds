/* Actual PreMoveMessageAttr.apply; inputs include list order and exact ownership. */
import { allMoves, modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { BerryType } from "#enums/berry-type";
import { BerryModifierType } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";
it("Poltergeist source messages", async () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any, pm = scene.phaseManager as any;
  const player = scene.getPlayerParty()[0], enemy = scene.getEnemyParty()[0];
  const move = allMoves[MoveId.POLTERGEIST], attr = move.getAttrs("PreMoveMessageAttr")[0];
  const named = ["LEFTOVERS", "SHELL_BELL", "FOCUS_BAND", "QUICK_CLAW", "KINGS_ROCK", "WHITE_HERB"];
  const factories = [...named.map(k => () => modifierTypes[k]()), ...Array.from({length: 11}, (_, i) => () => new BerryModifierType(i as BerryType))];
  const lines = ["POLTERGEIST_MESSAGES 1 25"];
  let messages: string[] = [];
  const original = pm.queueMessage.bind(pm);
  vi.spyOn(pm, "queueMessage").mockImplementation((text: string, ...args: any[]) => { messages.push(text); return original(text, ...args); });
  for (let ci = 0; ci < 25; ci++) {
    pm.clearAllPhases(); scene.modifiers.length = scene.enemyModifiers.length = 0; messages = [];
    const target = ci === 23 ? enemy : player, side = +(target === enemy);
    const add = (factory: number, owner = target.id, transferable = true, itemSide = side) => {
      const m = factories[factory]().newModifier(target) as any;
      m.pokemonId = owner; m.isTransferable = transferable; m.stackCount = 2;
      (itemSide ? scene.enemyModifiers : scene.modifiers).push(m);
    };
    if (ci < 17) add(ci);
    else if (ci === 17) { add(5); add(0); }
    else if (ci === 18) { add(0, target.id, false); add(7); }
    else if (ci === 19) { add(0, enemy.id); add(11); }
    else if (ci === 20) add(0, -1);
    else if (ci === 22 || ci === 23 || ci === 24) add(1);
    if (ci === 24) { scene.modifiers.length = 0; add(0, player.id, true, 1); }
    const nullTarget = ci === 22;
    lines.push(`CASE ${ci} ${side} ${+nullTarget} ${player.id} ${enemy.id}`);
    for (let s = 0; s < 2; s++) for (const m of s ? scene.enemyModifiers : scene.modifiers) lines.push(`MOD ${s} ${m.constructor.name} ${m.berryType ?? -1} ${m.pokemonId} ${+m.isTransferable} ${m.stackCount}`);
    const hp = [player.hp, enemy.hp], stats = [player.stats.slice(), enemy.stats.slice()], mods = [...scene.modifiers, ...scene.enemyModifiers], rng = Phaser.Math.RND.state(), br = scene.currentBattle.rngState;
    const result = attr.apply(enemy, nullTarget ? null : target, move);
    expect(messages.length).toBe(+result);
    expect([player.hp, enemy.hp]).toEqual(hp); expect([player.stats, enemy.stats]).toEqual(stats);
    expect([...scene.modifiers, ...scene.enemyModifiers]).toEqual(mods);
    expect(Phaser.Math.RND.state()).toBe(rng); expect(scene.currentBattle.rngState).toBe(br);
    lines.push(`RESULT ${+result}`, `TEXT ${JSON.stringify(messages[0] ?? "")}`, `END ${ci}`);
  }
  lines.push("COMPLETE 25"); fs.writeFileSync(process.env.POLTERGEIST_OUT!, lines.join("\n") + "\n");
},120000);
