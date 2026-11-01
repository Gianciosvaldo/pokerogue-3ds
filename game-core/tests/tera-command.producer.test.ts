/*
 * Upstream half of game-core's player Terastallize command test (tools/teracmddiff.sh copies it into the upstream checkout).
 * A real singles and a real doubles battle; each case sets what the command depends on (the Tera Orb, the arena's Tera counter,
 * the Tera chosen for slot 0 this turn, the field Pokemon's species, form, fusion half, Tera type and moves with used PP),
 * then asks upstream's own code:
 *   - canTerastallize(pokemon) (pokemon-utils.ts) and CommandUiHandler.canTera() for the field slot (the handler the battle menu uses),
 *   - getTeraType() (the button's type),
 *   - CommandPhase.handleCommand(Command.TERA, cursor, NORMAL), whether it is accepted, and the pre-turn command and the turn command it
 *     writes (command, skip, targets / cursor, move), recorded before any phase runs.
 * Written for tera-command-diff: game-core must offer the same button, refuse what upstream's menu would not offer and write the same
 * commands when it is offered.
 */
import { MAX_TERAS_PER_ARENA } from "#app/constants";
import { globalScene } from "#app/global-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { Command } from "#enums/command";
import { MoveId } from "#enums/move-id";
import { MoveUseMode } from "#enums/move-use-mode";
import { SpeciesId } from "#enums/species-id";
import { UiMode } from "#enums/ui-mode";
import { PokemonMove } from "#moves/pokemon-move";
import { CommandPhase } from "#phases/command-phase";
import { GameManager } from "#test/framework/game-manager";
import { canTerastallize } from "#utils/pokemon-utils";
import * as fs from "node:fs";
import Phaser from "phaser";
import { afterAll, describe, expect, it, vi } from "vitest";

const N = Number(process.env.GC_TERA_N ?? 600);
const OUT = process.env.GC_TERA_OUT ?? "/tmp/gc-tera-command.txt";
let seed = Number(process.env.GC_TERA_SEED ?? 777);
const rnd = (n: number) => {
  seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
  return (seed >>> 8) % n;
};
const pick = <T>(v: readonly T[]): T => v[rnd(v.length)];
const lines: string[] = [];
let caseNo = 0;

// Species whose forms or identity change the answer, drawn half the time
const hot = [
  SpeciesId.CHARIZARD, SpeciesId.VENUSAUR, SpeciesId.BLASTOISE, SpeciesId.GENGAR, SpeciesId.LAPRAS, SpeciesId.MEOWTH,
  SpeciesId.NECROZMA, SpeciesId.TERAPAGOS, SpeciesId.OGERPON, SpeciesId.SHEDINJA, SpeciesId.RAYQUAZA, SpeciesId.KYOGRE,
  SpeciesId.GROUDON, SpeciesId.ETERNATUS, SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.URSHIFU, SpeciesId.ZACIAN,
  SpeciesId.MEWTWO, SpeciesId.LUCARIO, SpeciesId.GARCHOMP, SpeciesId.DITTO, SpeciesId.ARCEUS, SpeciesId.MAGIKARP,
];
const moves = [
  MoveId.SPLASH, MoveId.TACKLE, MoveId.EMBER, MoveId.SURF, MoveId.EARTHQUAKE, MoveId.HOWL, MoveId.HELPING_HAND,
  MoveId.QUICK_ATTACK, MoveId.GROWL, MoveId.WATER_GUN, MoveId.TERA_BLAST, MoveId.TERA_STARSTORM,
];
const speciesIds = (): number[] => Object.keys(speciesDataRegistry.data).map(Number);

function setMon(p: any, double: boolean) {
  const ids = speciesIds();
  const sid = rnd(2) === 0 ? pick(hot) : pick(ids);
  const sp = speciesDataRegistry.getSpecies(sid);
  p.species = sp;
  p.formIndex = sp.forms.length ? (rnd(3) === 0 ? 0 : rnd(sp.forms.length)) : 0;
  if (rnd(4) === 0) {
    const fid = rnd(2) === 0 ? pick(hot) : pick(ids);
    const fs2 = speciesDataRegistry.getSpecies(fid);
    p.fusionSpecies = fs2;
    p.fusionFormIndex = fs2.forms.length ? rnd(fs2.forms.length) : 0;
  } else {
    p.fusionSpecies = null;
    p.fusionFormIndex = 0;
  }
  if (rnd(10) === 0) {
    // Ultra Necrozma, as the species or as the fusion half (canSpeciesTera's hasSpecies(NECROZMA, "ultra"))
    const necrozma = speciesDataRegistry.getSpecies(SpeciesId.NECROZMA);
    const ultra = necrozma.forms.findIndex((f: any) => f.formKey === "ultra");
    expect(ultra).toBeGreaterThan(0);
    if (rnd(2) === 0) {
      p.species = necrozma;
      p.formIndex = ultra;
    } else {
      p.fusionSpecies = necrozma;
      p.fusionFormIndex = ultra;
    }
  }
  p.teraType = rnd(19); // 0..17 and Stellar (18)
  const n = 1 + rnd(4);
  const allSpent = rnd(8) === 0;
  p.moveset = [];
  for (let m = 0; m < n; m++) {
    const mv = new PokemonMove(pick(moves));
    mv.ppUsed = allSpent || rnd(5) === 0 ? mv.getMovePp() : 0;
    p.moveset.push(mv);
  }
  p.summonData.moveset = null;
  void double;
}

async function run(double: boolean, cases: number) {
  const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
  const game = new GameManager(phaserGame);
  game.override
    .battleStyle(double ? "double" : "single")
    .enemySpecies(SpeciesId.MAGIKARP)
    .enemyMoveset(MoveId.SPLASH)
    .enemyLevel(50);
  if (double) {
    await game.classicMode.startBattle(SpeciesId.PIKACHU, SpeciesId.BULBASAUR);
  } else {
    await game.classicMode.startBattle(SpeciesId.PIKACHU);
  }
  const scene = game.scene as any;
  const queued: string[] = [];
  vi.spyOn(scene.phaseManager, "unshiftNew").mockImplementation((name: any) => {
    queued.push(String(name));
  });
  vi.spyOn(scene.ui, "setMode").mockImplementation(() => Promise.resolve());
  vi.spyOn(scene.ui, "showText").mockImplementation(() => {});
  const handler = scene.ui.handlers[UiMode.COMMAND] as any;
  expect(handler).toBeTruthy();

  for (let i = 0; i < cases; i++) {
    const field = double ? rnd(2) : 0;
    const orb = rnd(5) !== 0;
    const used = rnd(6) === 0 ? 2 : rnd(4) === 0 ? 1 : 0;
    const slot0Tera = double && rnd(3) === 0;
    scene.modifiers = scene.modifiers.filter((m: any) => !m.is("TerastallizeAccessModifier"));
    if (orb) {
      scene.modifiers.push(modifierTypes.TERA_ORB().newModifier());
    }
    scene.arena.playerTerasUsed = used;
    const party = scene.getPlayerField();
    for (const p of party) {
      setMon(p, double);
    }
    scene.currentBattle.preTurnCommands = {};
    scene.currentBattle.turnCommands = {};
    if (slot0Tera) {
      scene.currentBattle.preTurnCommands[0] = { command: Command.TERA, targets: [0], skip: false };
    }
    const mon = party[field];
    const cursor = rnd(mon.getMoveset().length);

    handler.fieldIndex = field;
    const canTera = handler.canTera() ? 1 : 0;
    const canTeraPokemon = canTerastallize(mon) ? 1 : 0;
    const teraType = mon.getTeraType();
    const mons = party.map((p: any) => [
      p.species.speciesId, p.formIndex, p.fusionSpecies ? p.fusionSpecies.speciesId : -1, p.fusionFormIndex, p.teraType,
      p.getMoveset().length, ...p.getMoveset().flatMap((m: any) => [m.moveId, m.ppUsed]),
    ].join(" "));

    // the command itself, as the FIGHT menu calls it. (The menu would not offer it when canTera is 0, upstream's CommandPhase has no gate of
    // its own: the result is recorded either way and compared only where the menu offers it.)
    queued.length = 0;
    const phase = new CommandPhase(field);
    const ok = (phase as any).handleFightCommand(Command.TERA, cursor, MoveUseMode.NORMAL, undefined) ? 1 : 0;
    const pre = scene.currentBattle.preTurnCommands[field];
    const turn = scene.currentBattle.turnCommands[field];
    const cmd = (c: any) => (c ? [c.command, c.skip ? 1 : 0, (c.targets ?? []).length, ...(c.targets ?? [])] : [-1, 0, 0]);
    lines.push(
      [
        "C", caseNo++, double ? 1 : 0, field, orb ? 1 : 0, used, slot0Tera ? 1 : 0, cursor, MAX_TERAS_PER_ARENA, mons.length, ...mons.map(m => `[ ${m} ]`),
      ].join(" "),
    );
    lines.push(
      [
        "R", canTera, canTeraPokemon, teraType, ok, ...cmd(pre), ...(turn ? [turn.command, turn.cursor ?? -1, turn.move?.move ?? -1] : [-1, -1, -1]),
      ].join(" "),
    );
  }
  void globalScene;
}

describe("tera command producer", () => {
  it("singles", async () => {
    await run(false, Math.ceil(N / 2));
  }, 600000);
  it("doubles", async () => {
    await run(true, Math.floor(N / 2));
  }, 600000);
  afterAll(() => {
    fs.writeFileSync(OUT, `TERACMD 1 ${lines.length / 2}\n${lines.join("\n")}\nEND\n`);
  });
});
