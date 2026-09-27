/*
 * Upstream half of run-core's victory-experience test (tools/difftest.sh copies it into the upstream
 * checkout). Each case builds a real party of 1 to 6 Pokemon (random levels, some fainted, some with
 * Pokerus, a random set of participants, now and then one that left the party), gives the player Exp.
 * Share, Exp. Balance, Oval Charm and Lucky/Golden Eggs with random stacks, sets a wild or trainer battle
 * on a random wave, and records the ExpPhase / ShowPartyExpBarPhase that applyPartyExp queues (party
 * index and amount). build/partyexptest replays the cases.
 */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { BattleType } from "#enums/battle-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("partyexp producer", async () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-partyexp-cases.txt";
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
  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  const pool = Array.from({ length: 6 }, () => scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5));
  const item = (key: string) => (generateModifierType((modifierTypes as any)[key]) as any);
  const party = scene.getPlayerParty();
  const realCreate = scene.phaseManager.create;
  const realUnshift = scene.phaseManager.unshiftPhase;

  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    const wave = 1 + rnd(250);
    battle.waveIndex = wave;
    battle.battleType = rnd(3) === 0 ? BattleType.TRAINER : BattleType.WILD;
    const n = 1 + rnd(6);
    party.length = 0;
    for (const p of pool.slice(0, n)) {
      p.species = speciesDataRegistry.getSpecies(speciesIds[rnd(speciesIds.length)]);
      p.level = 1 + rnd(rnd(4) === 0 ? 200 : 100);
      p.hp = rnd(6) === 0 ? 0 : 1 + rnd(50);
      p.pokerus = rnd(5) === 0;
      party.push(p);
    }
    const participants = new Set<number>();
    for (const p of party) {
      if (rnd(2) === 0) {
        participants.add(p.id);
      }
    }
    if (rnd(8) === 0) {
      participants.add(987654); // a participant that has left the party
    }
    battle.playerParticipantIds = participants;

    scene.modifiers.length = 0;
    const stacks: Record<string, number> = { EXP_SHARE: 0, EXP_BALANCE: 0, OVAL_CHARM: 0 };
    for (const key of Object.keys(stacks)) {
      if (rnd(2) === 0) {
        const m = item(key).newModifier();
        m.stackCount = 1 + rnd(key === "EXP_BALANCE" ? 4 : 5);
        scene.modifiers.push(m);
        stacks[key] = m.getStackCount();
      }
    }
    const eggs: string[] = [];
    for (let k = rnd(3); k > 0; k--) {
      const holder = party[rnd(party.length)];
      const m = item(rnd(2) === 0 ? "LUCKY_EGG" : "GOLDEN_EGG").newModifier(holder);
      m.stackCount = 1 + rnd(4);
      scene.modifiers.splice(rnd(scene.modifiers.length + 1), 0, m);
    }
    for (const m of scene.modifiers) {
      if (m.constructor.name === "PokemonExpBoosterModifier") {
        eggs.push(`${m.pokemonId} ${m.getStackCount()} ${m.boostMultiplier * 100}`);
      }
    }

    const expValue = rnd(3) === 0 ? 1 + rnd(40) : party[0].getExpValue();
    const useWave = rnd(3) === 0;
    const queued: string[] = [];
    scene.phaseManager.create = (_name: string, index: number, exp: number) => ({ index, exp });
    scene.phaseManager.unshiftPhase = (p: any) => queued.push(`${p.index} ${p.exp}`);
    try {
      scene.applyPartyExp(expValue, false, useWave);
    } finally {
      scene.phaseManager.create = realCreate;
      scene.phaseManager.unshiftPhase = realUnshift;
    }

    lines.push(
      [
        "P", wave, battle.battleType === BattleType.TRAINER ? 1 : 0, scene.getMaxExpLevel(), expValue, useWave ? 1 : 0,
        participants.size, stacks.EXP_SHARE, stacks.EXP_BALANCE, stacks.OVAL_CHARM, party.length,
        ...party.flatMap((p: any) => [p.id, p.level, p.hp, p.pokerus ? 1 : 0, participants.has(p.id) ? 1 : 0]),
        eggs.length, ...eggs, queued.length, ...queued,
      ].join(" "),
    );
  }
  scene.modifiers.length = 0;
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
