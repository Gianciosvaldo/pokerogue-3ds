/*
 * Upstream half of run-core's enemy item test (tools/difftest.sh copies it into the upstream checkout).
 * Each case sets a random game mode, run seed and wave, a wild or trainer battle (a real Trainer of a
 * random type without a genModifiersFunc), single or double, and makes an enemy party of real Pokemon (with their real movesets)
 * through upstream's own addEnemyPokemon. It records what the item step reads from them: whether each
 * is a boss, and the attack type booster's input, each attack move's types in moveset order, asked with
 * the booster generator's own calls (its six lines are transcribed below). Then, on a freshly sown
 * Phaser.Math.RND, it runs what the EncounterPhase runs: regenerateModifierPoolThresholds(enemy field,
 * WILD or TRAINER) and scene.generateEnemyModifiers(), recording every item as it is added
 * (PersistentModifier.prototype.add) and the RNG state afterwards. build/enemyitemstest replays the cases.
 */
import { getGameMode } from "#app/game-mode";
import { activeOverrides } from "#app/overrides";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { BattleType } from "#enums/battle-type";
import { GameModes } from "#enums/game-modes";
import { ModifierPoolType } from "#enums/modifier-pool-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerType } from "#enums/trainer-type";
import { TrainerVariant } from "#enums/trainer-variant";
import { Trainer } from "#field/trainer";
import { PersistentModifier } from "#modifiers/modifier";
import { regenerateModifierPoolThresholds } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import { trainerConfigs } from "#trainers/trainer-config";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("enemyitems producer", async () => {
  const N = Number(process.env.DIFF_N ?? 3000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-enemyitems-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  const ov: [string, any][] = [
    ["ENEMY_LEVEL_OVERRIDE", 0], ["ENEMY_SPECIES_OVERRIDE", null], ["ENEMY_FUSION_OVERRIDE", false],
    ["ENEMY_IVS_OVERRIDE", null], ["ENEMY_NATURE_OVERRIDE", null], ["ENEMY_STATUS_OVERRIDE", 0],
    ["ENEMY_GENDER_OVERRIDE", null], ["ENEMY_FORM_OVERRIDES", {}], ["ENEMY_SHINY_OVERRIDE", null],
    ["ENEMY_VARIANT_OVERRIDE", null], ["ENEMY_MOVESET_OVERRIDE", []], ["ENEMY_MODIFIER_OVERRIDE", []],
    ["ENEMY_HELD_ITEMS_OVERRIDE", []],
  ];
  for (const [k, v] of ov) {
    vi.spyOn(activeOverrides as any, k, "get").mockReturnValue(v);
  }

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const state = () => Phaser.Math.RND.state().split(",").slice(1).join(" ");
  const modes = [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.DAILY, GameModes.CHALLENGE];
  const types = Object.values(TrainerType).filter(v => typeof v === "number" && (trainerConfigs as any)[v]) as number[];
  const plain = types.filter(t => !(trainerConfigs as any)[t].genModifiersFunc);
  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  // The attack type booster generator's input, asked as it asks it
  const attackTypes = (p: any): number[] => {
    const out: number[] = [];
    for (const pm of p.getMoveset()) {
      const move = pm.getMove();
      if (!move.is("AttackMove")) {
        continue;
      }
      const attr = move.getAttrs("VariableMoveTypeAttr")[0];
      out.push(...(attr ? attr.getTypesForItemSpawn(p, move) : p.getMoveTypeForItemSpawn(move)));
    }
    return out;
  };
  let added: any[] = [];
  const realAdd = PersistentModifier.prototype.add;
  PersistentModifier.prototype.add = function (this: any, modifiers: any, virtual: boolean) {
    added.push(this);
    return realAdd.call(this, modifiers, virtual);
  };

  const lines: string[] = [];
  let thrown = 0;
  for (let i = 0; i < N; i++) {
    const gm = getGameMode(modes[rnd(modes.length)]);
    scene.gameMode = gm;
    battle.gameMode = gm;
    scene.seed = seedStr();
    const wave = rnd(12) === 0 ? 1000 * (1 + rnd(3)) : rnd(4) === 0 ? 1 + rnd(1000) : 1 + rnd(210);
    battle.waveIndex = wave;
    const trainerBattle = rnd(2) === 0;
    battle.battleType = trainerBattle ? BattleType.TRAINER : BattleType.WILD;
    battle.double = rnd(3) === 0;
    let trainerType = -1;
    battle.trainer = null;
    try {
      if (trainerBattle) {
        // trainers with a genModifiersFunc (a closure the port reports as unported) are left out
        trainerType = plain[rnd(plain.length)];
        battle.trainer = new Trainer(trainerType, TrainerVariant.DEFAULT);
      }
      const party: any[] = [];
      battle.enemyParty = party;
      const size = trainerBattle ? 1 + rnd(6) : battle.double ? 2 : 1;
      for (let k = 0; k < size; k++) {
        const sp = speciesDataRegistry.getSpecies(speciesIds[rnd(speciesIds.length)]);
        party.push(scene.addEnemyPokemon(sp, 1 + rnd(100), trainerBattle ? 1 : 0, rnd(4) === 0));
      }
      scene.enemyModifiers.length = 0;
      added = [];
      Phaser.Math.RND.sow([seedStr()]);
      const before = state();
      regenerateModifierPoolThresholds(
        scene.getEnemyField(),
        trainerBattle ? ModifierPoolType.TRAINER : ModifierPoolType.WILD,
      );
      await scene.generateEnemyModifiers();
      const after = state();
      const head = [
        "I", gm.modeId, scene.seed, wave, trainerBattle ? 1 : 0, battle.double ? 1 : 0, trainerType, party.length,
      ];
      for (const p of party) {
        const t = attackTypes(p);
        head.push(p.isBoss() ? 1 : 0, t.length, ...t);
      }
      lines.push([...head, before, after].join(" "));
      for (const p of party) {
        const items = added.filter(m => m.pokemonId === p.id);
        const arg = (m: any) => m.berryType ?? m.stat ?? m.moveType ?? -1;
        lines.push(["M", items.length, ...items.flatMap(m => [m.type.id, arg(m)])].join(" "));
      }
      for (const p of party) {
        p.destroy();
      }
      battle.trainer?.destroy();
    } catch (e) {
      thrown++;
      lines.push(`X ${String(e).slice(0, 60).replace(/\s+/g, "_")}`);
    }
  }
  PersistentModifier.prototype.add = realAdd;
  battle.enemyParty = [];
  scene.enemyModifiers.length = 0;
  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
