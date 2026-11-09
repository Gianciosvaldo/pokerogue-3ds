/* ML_OUT lines "R ..." : LearnMovePhase.learnMove. Lines "Q ..." : the real SelectModifierPhase.applyModifier, the phases it queues in order.
 * Memory Mushroom / TM learn: what LearnMovePhase.learnMove does to the queued copy of the reward screen and to the money.
 * Real LearnMovePhase.learnMove at 8555c08 with a SelectModifierPhase in the queue (the copy SelectModifierPhase.applyModifier
 * unshifts for a TM or a Memory Mushroom). Presentation (ui.setMode, text, audio, move animation assets) is bypassed.
 * ML_OUT = output file. One line per case: "R <type> <cost> <money before> <money after> <copy still queued 0|1> <used TMs>". */
import { allMoves } from "#data/data-lists";
import { audioManager } from "#app/global-audio-manager";
import { AbilityId } from "#enums/ability-id";
import { LearnMoveType } from "#enums/learn-move-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { UiMode } from "#enums/ui-mode";
import { PokemonMove } from "#moves/pokemon-move";
import { LearnMovePhase } from "#phases/learn-move-phase";
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { Phase } from "#app/phase";
import { modifierTypes } from "#data/data-lists";
import { TmModifierType } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect, it, vi } from "vitest";

vi.mock("#data/battle-anims", async importOriginal => ({
  ...(await importOriginal<any>()),
  initMoveAnim: async () => {},
  loadMoveAnimAssets: () => {},
}));

it("learnMove removes the queued reward screen for a TM and a free Memory Mushroom, and charges a bought one", async () => {
  expect(Number(process.versions.node.split(".")[0])).toBe(22);
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override
    .ability(AbilityId.NONE)
    .enemyAbility(AbilityId.NONE)
    .moveset(MoveId.SPLASH)
    .enemyMoveset(MoveId.SPLASH)
    .enemySpecies(SpeciesId.MAGIKARP)
    .disableTrainerWaves()
    .mysteryEncounterChance(0);
  await game.classicMode.startBattle(SpeciesId.MEW);
  const s: any = game.scene;
  const p: any = s.getPlayerParty()[0];
  vi.spyOn(s.ui, "setMode").mockResolvedValue(undefined);
  vi.spyOn(s.ui, "showText").mockImplementation(() => {});
  vi.spyOn(audioManager, "playSound").mockImplementation(() => {});
  const cases: [LearnMoveType, number][] = [
    [LearnMoveType.TM, -1],
    [LearnMoveType.MEMORY, -1],
    [LearnMoveType.MEMORY, 800],
    [LearnMoveType.LEARN_MOVE, -1],
  ];
  const out: string[] = ["V 1"];
  for (const [type, cost] of cases) {
    p.usedTMs = [];
    p.moveset = [MoveId.SPLASH, MoveId.TACKLE, MoveId.POUND, MoveId.GROWL].map(m => new PokemonMove(m));
    s.money = 5000;
    s.phaseManager.tryRemovePhase("SelectModifierPhase");
    s.phaseManager.unshiftNew("SelectModifierPhase", 0, undefined, undefined, true);
    expect(s.phaseManager.hasPhaseOfType("SelectModifierPhase")).toBe(true);
    const phase: any = new LearnMovePhase(0, MoveId.FLAMETHROWER, type, cost);
    phase.messageMode = UiMode.MESSAGE;
    await phase.learnMove(0, allMoves[MoveId.FLAMETHROWER], p);
    out.push(
      `R ${LearnMoveType[type]} ${cost} 5000 ${s.money} ${+s.phaseManager.hasPhaseOfType("SelectModifierPhase")} ${p.usedTMs.length}`,
    );
  }
  // SelectModifierPhase.applyModifier itself: which phases it queues, in which order, and what it charges
  const queued: string[] = [];
  vi.spyOn(s.phaseManager, "unshiftNew").mockImplementation((name: any) => {
    queued.push(name);
  });
  vi.spyOn(s.phaseManager, "unshiftPhase").mockImplementation((ph: any) => {
    queued.push(ph.phaseName);
  });
  vi.spyOn(s.phaseManager, "create").mockImplementation(((name: any) => ({ phaseName: name })) as any);
  vi.spyOn(Phase.prototype, "end").mockImplementation(() => {
    queued.push("END");
  });
  vi.spyOn(s.ui, "clearText").mockImplementation(() => {});
  vi.spyOn(s.ui, "getHandler").mockReturnValue({ updateCostText() {} } as any);
  vi.spyOn(s, "updateMoneyText").mockImplementation(() => {});
  vi.spyOn(s, "animateMoneyChanged").mockImplementation(() => {});
  const forms: [string, () => any, number][] = [
    ["MEMORY_MUSHROOM", () => modifierTypes.MEMORY_MUSHROOM().newModifier(p, 0), -1],
    ["MEMORY_MUSHROOM", () => modifierTypes.MEMORY_MUSHROOM().newModifier(p, 0), 800],
    ["TM_COMMON", () => new TmModifierType(MoveId.FLAMETHROWER).newModifier(p), -1],
    ["TM_COMMON", () => new TmModifierType(MoveId.FLAMETHROWER).newModifier(p), 300],
  ];
  for (const [name, make, cost] of forms) {
    queued.length = 0;
    s.money = 5000;
    const smp: any = new SelectModifierPhase();
    smp.typeOptions = [];
    smp.applyModifier(make(), cost, false);
    out.push(`Q ${name} ${cost} ${5000} ${s.money} ${queued.join(",")}`);
  }
  fs.writeFileSync(process.env.ML_OUT!, out.join("\n") + "\n");
}, 240000);
