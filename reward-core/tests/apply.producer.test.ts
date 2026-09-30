/*
 * Upstream half of reward-core's item application test (tools/difftest.sh copies it into upstream).
 * Tests applying 14 reward items (potions, revives, Sacred Ash, PP items) to random parties with random HP,
 * statuses, PP, PP Ups, held items, and challenge settings.
 */
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves, modifierTypes } from "#data/data-lists";
import { Status } from "#data/status-effect";
import { Challenges } from "#enums/challenges";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { PokemonHeldItemModifier } from "#modifiers/modifier";
import { PokemonMove } from "#moves/pokemon-move";
import { PokemonMoveModifierType } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("reward apply producer", async () => {
  const N = Number(process.env.DIFF_N ?? 4000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-apply-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const pick = <T>(v: readonly T[]): T => v[rnd(v.length)];
  const keyOf = (t: any) => (t.id ?? "UNKNOWN") as string;

  const itemKeys = [
    "POTION", "SUPER_POTION", "HYPER_POTION", "MAX_POTION", "FULL_RESTORE",
    "REVIVE", "MAX_REVIVE", "SACRED_ASH",
    "ETHER", "MAX_ETHER", "ELIXIR", "MAX_ELIXIR",
    "PP_UP", "PP_MAX",
  ];

  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  const moveIds = allMoves.map((m: any) => m.id).filter((id: number) => id > 0);
  const statuses = [
    StatusEffect.POISON, StatusEffect.TOXIC, StatusEffect.PARALYSIS, StatusEffect.SLEEP, StatusEffect.FREEZE, StatusEffect.BURN,
  ];

  const pool = Array.from({ length: 6 }, () => scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5));
  const party = scene.getPlayerParty();

  const lines: string[] = [];
  let thrown = 0;

  for (let i = 0; i < N; i++) {
    try {
      const isChallenge = rnd(2) === 0;
      const gm = getGameMode(isChallenge ? GameModes.CHALLENGE : GameModes.CLASSIC);
      let challenges = 0;
      if (isChallenge) {
        if (rnd(2) === 0) {
          gm.setChallengeValue(Challenges.HARDCORE, 1);
          challenges |= 1 << Challenges.HARDCORE;
        }
      }
      scene.gameMode = gm;
      battle.gameMode = gm;

      const size = 1 + rnd(6);
      party.length = 0;
      for (let k = 0; k < size; k++) {
        const p = pool[k];
        p.resetSummonData();
        p.resetBattleAndWaveData();
        const sid = pick(speciesIds);
        const sp = speciesDataRegistry.getSpecies(sid);
        p.species = sp;
        p.formIndex = sp.forms.length ? rnd(sp.forms.length) : 0;
        p.fusionSpecies = null;
        p.fusionFormIndex = 0;
        p.level = 1 + rnd(100);
        p.gender = rnd(3) - 1;
        p.nature = rnd(25);
        p.teraType = rnd(18);

        const nm = 1 + rnd(4);
        p.moveset = [];
        for (let m = 0; m < nm; m++) {
          const mv = new PokemonMove(pick(moveIds), rnd(4), rnd(4));
          const maxPp = mv.getMovePp();
          mv.ppUsed = rnd(3) === 0 ? 0 : rnd(maxPp + 1);
          p.moveset.push(mv);
        }
        p.calculateStats();
        const max = p.getMaxHp();
        const r = rnd(8);
        p.hp = r < 2 ? max : r === 2 ? 0 : 1 + rnd(max);
        const st = p.hp === 0 ? (rnd(2) === 0 ? StatusEffect.FAINT : 0) : rnd(3) === 0 ? pick(statuses) : 0;
        p.status = st ? new Status(st) : null;
        party.push(p);
      }

      // items (e.g. HEALING_CHARM)
      scene.modifiers.length = 0;
      if (rnd(2) === 0) {
        const hcFunc = (modifierTypes as any)["HEALING_CHARM"];
        if (hcFunc) {
          const t = hcFunc().withIdFromFunc(hcFunc);
          const m = t.newModifier();
          m.stackCount = 1 + rnd(5);
          scene.modifiers.push(m);
        }
      }

      const itemKey = itemKeys[i % itemKeys.length];
      const targetPartyIdx = rnd(party.length);
      const targetMon = party[targetPartyIdx];
      const targetMoveIdx = rnd(targetMon.moveset.length);

      // Record input state for C
      lines.push(["A", itemKey, targetPartyIdx, targetMoveIdx, challenges, party.length].join(" "));
      for (const p of party) {
        const ms = p.getMoveset();
        lines.push(
          [
            "P", p.id, p.hp, p.getMaxHp(), p.status?.effect ?? 0, ms.length,
            ...ms.flatMap((m: any) => [m.moveId, m.ppUsed, m.ppUp, m.getMovePp()]),
          ].join(" "),
        );
      }
      const mods = scene.modifiers;
      lines.push(
        [
          "I", mods.length,
          ...mods.flatMap((m: any) => [
            keyOf(m.type), m.stackCount,
            m instanceof PokemonHeldItemModifier ? party.findIndex((p: any) => p.id === m.pokemonId) : -1,
          ]),
        ].join(" "),
      );

      // Execute item apply in upstream
      const typeFunc = (modifierTypes as any)[itemKey];
      const modifierType = typeFunc();
      let filterMsg: string | null = null;

      if (modifierType.selectFilter) {
        filterMsg = modifierType.selectFilter(targetMon);
      }
      if (filterMsg === null && modifierType instanceof PokemonMoveModifierType) {
        if (modifierType.moveSelectFilter) {
          filterMsg = modifierType.moveSelectFilter(targetMon.moveset[targetMoveIdx]);
        }
      }

      let applied = false;
      if (filterMsg === null) {
        let mod: any;
        if (modifierType instanceof PokemonMoveModifierType) {
          mod = modifierType.newModifier(targetMon, targetMoveIdx);
        } else if (typeof modifierType.newModifier === "function") {
          mod = modifierType.newModifier(targetMon);
        }
        if (mod) {
          applied = scene.addModifier(mod);
        }
      }

      // Record result state after upstream apply
      const outMon = party.flatMap((p: any) => [
        p.hp, p.status?.effect ?? 0, p.moveset.length,
        ...p.moveset.flatMap((m: any) => [m.ppUsed, m.ppUp, m.getMovePp()]),
      ]);
      lines.push(["R", applied ? 1 : 0, filterMsg ? filterMsg.replace(/\s+/g, "_") : "OK", ...outMon].join(" "));
      lines.push("E");
    } catch (e) {
      thrown++;
      lines.push(`X ${String(e).slice(0, 80).replace(/\s+/g, "_")}`);
    }
  }

  party.length = 0;
  scene.modifiers.length = 0;
  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
