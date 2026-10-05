/* Additive Task 61 fixtures. LC_OUT, LC_N (default 240), LC_SEED.
 * Real selection, addModifier, evolve/changeForm, stats and dex methods at
 * 8555c08. Only asset loading/info/tweens are omitted. Animation completion and
 * player choices are explicit boundaries; no existing producer is changed. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { EvolutionItem } from "#balance/pokemon-evolutions";
import { FormChangeItem } from "#enums/form-change-item";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import { EvolutionItemModifierType, FormChangeItemModifierType } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

it("bounded reward lifecycle", async () => {
  const N = Number(process.env.LC_N ?? 280), SEED = Number(process.env.LC_SEED ?? 777);
  let seed = SEED;
  const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.GROWLITHE);
  const scene = game.scene as any, p = scene.getPlayerParty()[0];
  vi.spyOn(p, "loadAssets").mockResolvedValue(undefined);
  vi.spyOn(p, "updateInfo").mockResolvedValue(undefined);
  // changeForm's UI refresh; stat calculation itself still uses real modifiers.
  vi.spyOn(scene, "updateModifiers").mockImplementation(() => {});
  const queued: any[][] = [];
  let currentAction = -1;
  vi.spyOn(scene.phaseManager, "unshiftNew").mockImplementation((name: any, ...args: any[]) => { queued.push([name, ...args]); });
  vi.spyOn(scene.phaseManager, "unshiftPhase").mockImplementation((phase: any) => {
    if (phase.phaseName === "MessagePhase") {
      expect(currentAction).toBe(6);
      expect(phase.text).toContain("The stack for Griseous Core is full.");
      return;
    }
    expect(phase.phaseName).toBe("FormChangePhase"); queued.push([phase.phaseName, phase]);
  });
  const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");
  const state = () => [p.species.speciesId, p.formIndex, p.level, p.getNature(), p.abilityIndex,
    p.getAbility(true).id, +p.pauseEvolutions, p.hp, ...p.stats, p.moveset.length,
    ...Array.from({ length: 4 }, (_, j) => { const m = p.moveset[j]; return m ? [m.moveId, m.ppUsed, m.ppUp] : [0,0,0]; }).flat()];
  const dexSpecies = [SpeciesId.GROWLITHE, SpeciesId.ARCANINE, SpeciesId.GIRATINA];
  const counts = Array(7).fill(0), lines = [`V 1 ${N}`];
  for (let i = 0; i < N; i++) {
    const action = i % 7, form = action >= 3;
    currentAction = action;
    scene.modifiers.length = 0; queued.length = 0;
    p.resetSummonData(); p.resetBattleAndWaveData();
    p.species = speciesDataRegistry.getSpecies(form ? SpeciesId.GIRATINA : SpeciesId.GROWLITHE);
    p.formIndex = action === 4 || action === 6 ? 1 : 0;
    p.fusionSpecies = null; p.fusionFormIndex = 0;
    p.level = 5 + rnd(96); p.nature = rnd(25); p.abilityIndex = rnd(3);
    p.gender = rnd(2) ? 1 : -1; p.shiny = rnd(2) === 0; p.variant = 0;
    p.passive = false; p.teraType = -1; p.pauseEvolutions = rnd(2) === 0;
    p.ivs = Array.from({ length: 6 }, () => rnd(32));
    p.moveset = [new PokemonMove(MoveId.TACKLE, rnd(4), rnd(4)), new PokemonMove(MoveId.GROWL, rnd(4), rnd(4))];
    p.calculateStats(); p.hp = rnd(p.getMaxHp() + 1);
    for (const id of dexSpecies) {
      const d = scene.gameData.dexData[id];
      d.seenAttr = d.caughtAttr = 0n; d.natureAttr = d.seenCount = d.caughtCount = d.hatchedCount = 0; d.ivs = [0,0,0,0,0,0];
    }
    // A party member's root species is already owned; exercise caught/nature
    // updates rather than the early return for an entirely locked starter.
    scene.gameData.dexData[p.species.speciesId].caughtAttr = 1n;
    for (let ball = 0; ball < 5; ball++) scene.pokeballCounts[ball] = 0;
    Phaser.Math.RND.sow([`lc-${SEED}-${i}`]);
    const battleRng = scene.currentBattle.rngState;
    lines.push(`C ${i} ${action} ${p.gender} ${+p.shiny} ${p.ivs.join(" ")}`, `R ${rng()}`, `I ${state().join(" ")}`);
    const t = form ? new FormChangeItemModifierType(FormChangeItem.GRISEOUS_CORE) : new EvolutionItemModifierType(EvolutionItem.FIRE_STONE);
    t.id = form ? "FORM_CHANGE_ITEM" : "EVOLUTION_ITEM";
    t.tier = 2;
    if (action === 6) scene.modifiers.push(t.newModifier(p));
    let accepted = false;
    // action 2: reject an invalid target; action 5: cancel reward selection.
    if (action !== 2 && action !== 5 && t.selectFilter(p) == null) {
      scene.addModifier(t.newModifier(p), false, false);
      // A free SelectModifierPhase ends regardless of addModifier's result.
      accepted = true;
    }
    if (accepted && action === 0) {
      expect(queued[0][0]).toBe("EvolutionPhase");
      const e = queued[0][2], before = p.species;
      await p.evolve(e, before);
      const moves = p.getLevelMoves({ startingLevel: p.level, includeEvolutionMoves: true }).filter((m: any[]) => m[0] === 0);
      for (const [,move] of moves) if (!p.moveset.some((m: any) => m.moveId === move) && p.moveset.length < 4) p.setMove(p.moveset.length, move);
    } else if (accepted && action === 1) {
      // EvolutionScene's cancelled animation followed by 'pause evolutions: yes'.
      p.pauseEvolutions = true;
    } else if (accepted && action === 3) {
      expect(queued[0][0]).toBe("FormChangePhase");
      await p.changeForm(queued[0][1].formChange);
    }
    lines.push(`A ${+accepted} ${queued.length}`, `T ${state().join(" ")}`);
    for (const id of dexSpecies) {
      const d = scene.gameData.dexData[id];
      lines.push(`D ${id} ${d.seenAttr} ${d.caughtAttr} ${d.natureAttr} ${d.ivs.join(" ")}`);
    }
    lines.push(`H ${scene.modifiers.length} ${scene.modifiers[0]?.stackCount ?? 0} ${+(scene.modifiers[0]?.pokemonId === p.id)}`,
      `B ${Array.from({length:5},(_,j) => scene.pokeballCounts[j]).join(" ")}`, `S ${rng()}`, "E");
    counts[action]++;
    expect(scene.currentBattle.rngState).toBe(battleRng);
  }
  expect(counts.every(n => n > 0)).toBe(true);
  lines.push(`Z ${N}`);
  fs.writeFileSync(process.env.LC_OUT ?? "/tmp/gc-lifecycle.txt", lines.join("\n") + "\n");
  console.log(`LIFECYCLE seed=${SEED} cases=${N} accept-evolution/cancel-evolution/reject-target/accept-form/no-activation-form/skip/full-stack=${counts.join("/")} thrown=0 skipped=0`);
}, 120000);
