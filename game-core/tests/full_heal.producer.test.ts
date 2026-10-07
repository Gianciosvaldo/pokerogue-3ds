/*
 * Upstream half of game-core's Full Heal differential (tools/applydiff.sh copies it into upstream).
 * Runs upstream's own PokemonStatusHealModifierType through BattleScene.addModifier, on random parties with
 * varying HP, statuses (NONE, POISON, TOXIC, PARALYSIS, SLEEP, FREEZE, BURN, FAINT), confusion and nightmare tags,
 * and Healing Charm stacks.
 */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { Status } from "#data/status-effect";
import { BattlerTagType } from "#enums/battler-tag-type";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("full heal apply producer", async () => {
  const N = Number(process.env.DIFF_N ?? 4000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/gc-fullheal-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const pick = <T>(v: readonly T[]): T => v[rnd(v.length)];

  const pool = Array.from({ length: 6 }, () => scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 50));
  for (const pokemon of pool) vi.spyOn(pokemon, "playSprite").mockImplementation(() => {});
  const party = scene.getPlayerParty();

  const statuses = [
    StatusEffect.NONE,
    StatusEffect.POISON,
    StatusEffect.TOXIC,
    StatusEffect.PARALYSIS,
    StatusEffect.SLEEP,
    StatusEffect.FREEZE,
    StatusEffect.BURN,
  ];

  const lines: string[] = [];
  let thrown = 0;

  for (let i = 0; i < N; i++) {
    try {
      const size = 1 + rnd(6);
      party.length = 0;
      for (let k = 0; k < size; k++) {
        const p = pool[k];
        p.resetSummonData();
        p.resetBattleAndWaveData();
        p.species = speciesDataRegistry.getSpecies(SpeciesId.PIDGEY);
        p.level = 50;
        p.calculateStats();

        const hpChoice = rnd(5);
        if (hpChoice === 0) {
          p.hp = 0;
          p.status = new Status(StatusEffect.FAINT);
        } else {
          p.hp = hpChoice === 1 ? p.getMaxHp() : 1 + rnd(p.getMaxHp());
          const st = pick(statuses);
          p.status = st ? new Status(st) : null;
        }

        // Random confused tag
        if (p.hp > 0 && rnd(3) === 0) {
          p.addTag(BattlerTagType.CONFUSED, 1 + rnd(4));
        }

        // Random nightmare tag (especially if sleep)
        if (p.hp > 0 && p.status?.effect === StatusEffect.SLEEP && rnd(2) === 0) {
          p.addTag(BattlerTagType.NIGHTMARE);
        }

        party.push(p);
      }

      scene.modifiers.length = 0;
      const stacks = rnd(3) === 0 ? 0 : 1 + rnd(5);
      if (stacks > 0) {
        const charm = (modifierTypes as any)["HEALING_CHARM"];
        if (charm) {
          const t = charm().withIdFromFunc(charm);
          const m = t.newModifier();
          m.stackCount = stacks;
          scene.modifiers.push(m);
        }
      }

      const targetIdx = rnd(party.length);
      const target = party[targetIdx];
      const modifierType = (modifierTypes as any)["FULL_HEAL"]();
      const filterMsg: string | null = modifierType.selectFilter ? modifierType.selectFilter(target) : null;

      lines.push(["F", "FULL_HEAL", targetIdx, stacks, party.length].join(" "));
      for (const p of party) {
        const st = p.status?.effect ?? 0;
        const confused = p.findTag((t: any) => t.tagType === BattlerTagType.CONFUSED) ? 1 : 0;
        const nightmare = p.findTag((t: any) => t.tagType === BattlerTagType.NIGHTMARE) ? 1 : 0;
        lines.push(["P", p.hp, p.getMaxHp(), st, confused, nightmare].join(" "));
      }

      let applied = false;
      if (filterMsg === null) {
        const mod = modifierType.newModifier(target);
        applied = !!scene.addModifier(mod);
      }

      lines.push(["R", applied ? 1 : 0, filterMsg ? filterMsg.replace(/\s+/g, "_") : "OK"].join(" "));
      for (const p of party) {
        const st = p.status?.effect ?? 0;
        const confused = p.findTag((t: any) => t.tagType === BattlerTagType.CONFUSED) ? 1 : 0;
        const nightmare = p.findTag((t: any) => t.tagType === BattlerTagType.NIGHTMARE) ? 1 : 0;
        lines.push(["A", p.hp, st, confused, nightmare].join(" "));
      }
      lines.push("E");
    } catch (e) {
      thrown++;
      lines.push(`X ${String(e).slice(0, 120).replace(/\s+/g, "_")}`);
    }
  }

  party.length = 0;
  scene.modifiers.length = 0;
  if (thrown) {
    fs.writeFileSync(OUT, lines.join("\n") + "\n");
    throw new Error(`${thrown} upstream full heal cases threw`);
  }
  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
