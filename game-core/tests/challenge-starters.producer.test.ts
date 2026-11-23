/* Challenge Mode's starter rules, against upstream at 8555c08: Fresh Start, through the real
 * functions the starter select and SelectStarterPhase call, in a real Challenge Mode game mode:
 *   getRunValueLimit()                      STARTER_POINTS     (no ported challenge changes it: 10)
 *   gameData.getSpeciesStarterValue(id)     STARTER_COST       (Fresh Start: the base cost)
 *   checkStarterValidForChallenge(id, props, soft)   STARTER_CHOICE (Fresh Start 1), and its evolution walk
 *   getStarterData(id)                      STARTER_SELECT_MODIFY (Fresh Start's view of the profile)
 *   SelectStarterPhase.initBattle(starters) STARTER_MODIFY     (Fresh Start's changes to the starter Pokemon)
 * Each case sets a random profile for the species it asks about (dex and starter entries), sets the challenges (each 0 = off), and records
 * what upstream answers. The harness stubs what initBattle does after the party (newBattle, the arena, assets, the music).
 *
 * Trace lines (whitespace separated):
 *   V 1 SEED N
 *   C i fresh budget                                   one case's Fresh Start value and getRunValueLimit()
 *   D species caught_hex nature ability passive reduction egg iv0..iv5     the profile entry the next lines read
 *   Q species form shiny variant female cost hard soft_valid   cost = getSpeciesStarterValue, hard = checkStarterValidForChallenge(.., soft = false),
 *                                                      soft_valid = the same with soft = true
 *   W species caught_hex nature ability passive reduction egg iv0..iv5     getStarterData(species) under the challenges
 *   B n before(c s0 s1 s2) | after                     a party of n, and the Phaser RNG around SelectStarterPhase.initBattle
 *   S species form shiny variant female ability nature passive tera iv0..iv5 m0 m1 m2 m3     the starter handed over (moves 0-padded)
 *   M id species form level nature abilityIndex ability passive gender luck tera hp stat0..stat5 iv0..iv5 m0..m3   the Pokemon after the first
 *                                                      updatePartyForModifiers (the stats upstream recalculates when the first battle starts)
 * GC_CHALLENGE_N=400 GC_CHALLENGE_SEED=777 GC_CHALLENGE_OUT=/abs/trace.txt, Node 22. */
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { defaultStarterSpecies } from "#app/constants";
import { audioManager } from "#app/global-audio-manager";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { activeOverrides } from "#app/overrides";
import { speciesEggMoves } from "#balance/egg-moves";
import { Challenges } from "#enums/challenges";
import { GameModes } from "#enums/game-modes";
import { SpeciesId } from "#enums/species-id";
import { Pokemon } from "#field/pokemon";
import { SelectStarterPhase } from "#phases/select-starter-phase";
import { GameManager } from "#test/framework/game-manager";
import { getRunValueLimit, getStarterData } from "#ui/utils/starter-select-ui-utils";
import { checkStarterValidForChallenge } from "#utils/challenge-utils";
import * as fs from "node:fs";
import Phaser from "phaser";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_CHALLENGE_N ?? 400);
const SEED = Number(process.env.GC_CHALLENGE_SEED ?? 777);
const OUT = process.env.GC_CHALLENGE_OUT ?? "/tmp/challenge-starters.txt";
let seed = SEED;
const rnd = (n: number) => {
  seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
  return (seed >>> 8) % n;
};
const state = (r: any) => r.state().replace(/^!rnd,/, "").split(",").join(" ");

it("native Challenge Mode starter rules", async () => {
  expect(Number(process.versions.node.split(".")[0])).toBe(22);
  const native = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5);
  await game.classicMode.startBattle(SpeciesId.MEW);
  BattleScene.prototype.randBattleSeedInt = native;
  const scene: any = game.scene;
  const ids: number[] = speciesDataRegistry.getAllStarters().sort((a: number, b: number) => a - b);
  for (const key of ["SHINY_OVERRIDE", "VARIANT_OVERRIDE", "NATURE_OVERRIDE", "IVS_OVERRIDE", "GENDER_OVERRIDE", "STARTER_SPECIES_OVERRIDE", "STARTER_FUSION_OVERRIDE", "STARTER_FUSION_SPECIES_OVERRIDE"])
    vi.spyOn(activeOverrides as any, key as any, "get").mockReturnValue(key === "STARTER_FUSION_OVERRIDE" ? false : (null as any));
  vi.spyOn(scene, "newBattle").mockImplementation((() => {}) as any);
  vi.spyOn(scene.arena, "init").mockImplementation((() => {}) as any);
  vi.spyOn(audioManager, "playBgm").mockImplementation((() => {}) as any);
  vi.spyOn(Pokemon.prototype, "loadAssets").mockResolvedValue(undefined as any);
  vi.spyOn(Pokemon.prototype, "updateInfo").mockResolvedValue(undefined as any);

  const lines: string[] = [`V 1 ${SEED} ${N}`];
  const cover = { freshCases: 0, validHard: 0, invalidHard: 0, softOnly: 0, moved: 0, shinyFresh: 0, formReset: 0, eggRemoved: 0 };
  const FORM_SPECIES = [SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.PICHU, SpeciesId.ROTOM, SpeciesId.MELOETTA];
  const hex = (b: bigint) => b.toString(16);

  for (let i = 0; i < N; i++) {
    // the challenge: Fresh Start 1 or 2 in most cases, none in some (a Challenge Mode game with every challenge at 0 is Classic's starters).
    // Lower Max Starter Cost and Lower Starter Points are not in upstream's allChallenges: setChallengeValue on them changes nothing.
    const fresh = i % 7 === 0 ? 0 : 1 + ((i + rnd(2)) % 2);
    scene.gameMode = getGameMode(GameModes.CHALLENGE);
    scene.gameMode.setChallengeValue(Challenges.FRESH_START, fresh);
    if (fresh) cover.freshCases++;
    lines.push(`C ${i} ${fresh} ${getRunValueLimit()}`);

    // profile queries: a random profile for a handful of starters
    for (let q = 0; q < 12; q++) {
      const id = rnd(3) === 0 ? defaultStarterSpecies[rnd(defaultStarterSpecies.length)] : ids[rnd(ids.length)];   // a third from the species Fresh Start 1 allows
      const species = speciesDataRegistry.getSpecies(id);
      const dex = scene.gameData.dexData[id];
      const sd = scene.gameData.starterData[id];
      const forms = Math.max(1, species.forms.length);
      const form = rnd(forms);
      // caughtAttr: shiny / non-shiny, the variants, genders, and the form bits (bit 7 + form)
      let caught = 0n;
      if (rnd(4) !== 0) caught |= 1n;
      if (rnd(3) === 0) caught |= 2n;
      if (rnd(2)) caught |= 4n;
      if (rnd(2)) caught |= 8n;
      caught |= 16n;
      if (rnd(3) === 0) caught |= 32n;
      if (rnd(4) === 0) caught |= 64n;
      for (let f = 0; f < forms; f++) if (f === 0 || rnd(2)) caught |= 128n << BigInt(f);
      dex.caughtAttr = caught;
      let nat = 0;
      for (let n = 0; n < 25; n++) if (rnd(3) === 0) nat |= 1 << (n + 1);
      dex.natureAttr = nat;
      dex.ivs = [0, 0, 0, 0, 0, 0].map(() => rnd(32));
      sd.abilityAttr = rnd(8);
      sd.passiveAttr = rnd(4);
      sd.valueReduction = rnd(3);
      sd.eggMoves = rnd(16);
      lines.push(`D ${id} ${hex(caught)} ${nat} ${sd.abilityAttr} ${sd.passiveAttr} ${sd.valueReduction} ${sd.eggMoves} ${dex.ivs.join(" ")}`);
      const props = { formIndex: form, shiny: rnd(2) === 0, variant: rnd(3), female: rnd(2) === 0 };
      const cost = scene.gameData.getSpeciesStarterValue(id);
      const hard = checkStarterValidForChallenge(id, props, false);
      const soft = checkStarterValidForChallenge(id, props, true);
      if (hard) cover.validHard++;
      else cover.invalidHard++;
      if (!hard && soft) cover.softOnly++;
      lines.push(`Q ${id} ${form} ${+props.shiny} ${props.variant} ${+props.female} ${cost} ${+hard} ${+soft}`);
      const view = getStarterData(id);
      lines.push(
        `W ${id} ${hex(view.dexEntry.caughtAttr)} ${view.dexEntry.natureAttr} ${view.starterDataEntry.abilityAttr} ${view.starterDataEntry.passiveAttr} ${view.starterDataEntry.valueReduction} ${view.starterDataEntry.eggMoves} ${view.dexEntry.ivs.join(" ")}`,
      );
      if (fresh && FORM_SPECIES.includes(id) && (view.dexEntry.caughtAttr & ~255n) === 0n && (caught & ~255n) !== 0n) cover.formReset++;
      if (fresh && sd.eggMoves && view.starterDataEntry.eggMoves === 0) cover.eggRemoved++;
    }

    // a party through SelectStarterPhase.initBattle
    const n = 1 + rnd(6);
    const starters: any[] = [];
    const used = new Set<number>();
    const eggByRoot = new Map<number, number>();   // the species of one line share their root's egg moves, set once for the party
    while (starters.length < n) {
      // lean toward the species Fresh Start has special rules for
      const id = rnd(4) === 0 ? FORM_SPECIES[rnd(FORM_SPECIES.length)] : ids[rnd(ids.length)];
      if (used.has(id)) continue;
      used.add(id);
      const species = speciesDataRegistry.getSpecies(id);
      const shiny = rnd(3) === 0;
      const variant = rnd(3);
      const formIndex = species.forms.length ? (rnd(2) ? rnd(species.forms.length) : 0) : 0;
      const root = species.getRootSpeciesId();
      if (!eggByRoot.has(root)) eggByRoot.set(root, Object.hasOwn(speciesEggMoves, root) ? rnd(16) : 0);
      const eggBits = eggByRoot.get(root)!;
      // a legal moveset (validateStarterMoveset reads the form's own learnset): level 1-5 moves of the form and the egg moves the profile has unlocked
      const movePool: number[] = species.getLevelMoves(formIndex).filter(([l]: [number, number]) => l >= 1 && l <= 5).map(([, m]: [number, number]) => m);
      if (eggBits) for (let e = 0; e < 4; e++) if (eggBits & (1 << e)) movePool.push((speciesEggMoves as any)[root][e]);
      const moveset: number[] = [];
      const want = 1 + rnd(4);
      for (let t = 0; t < 20 && moveset.length < want && movePool.length; t++) {
        const m = movePool[rnd(movePool.length)];
        if (!moveset.includes(m)) moveset.push(m);
      }
      scene.gameData.starterData[root].eggMoves = eggBits;
      const attr = 1n | (shiny ? 2n | (variant >= 1 ? 32n : 0n) | (variant >= 2 ? 64n : 0n) | 16n : 16n);
      scene.gameData.dexData[id].caughtAttr = attr | (128n << BigInt(formIndex));
      const s = {
        speciesId: id,
        shiny,
        variant,
        formIndex,
        female: species.malePercent !== null && rnd(2) === 1,
        abilityIndex: species.abilityHidden && rnd(2) === 0 ? 2 : species.ability2 === species.ability1 ? 0 : rnd(2),
        passive: rnd(2) === 0,
        nature: rnd(25),
        pokerus: false,
        ivs: [0, 0, 0, 0, 0, 0].map(() => rnd(32)),
        moveset,
        teraType: rnd(3) === 0 ? null : rnd(2) === 0 ? species.type1 : (species.type2 ?? species.type1),
      };
      starters.push(s);
    }
    scene.modifiers = [];
    scene.party = [];
    scene.currentBattle = null;
    Phaser.Math.RND.sow([`challenge-starters-${SEED}-${i}`]);
    const before = state(Phaser.Math.RND);
    // what is handed over, taken before initBattle (the Pokemon keep the starter's ivs array and the challenge caps it in place)
    const given = starters.map(s => ({ ivs: [...s.ivs], abilityIndex: s.abilityIndex, passive: s.passive }));
    new SelectStarterPhase().initBattle(starters);
    await new Promise(r => setTimeout(r, 0));
    const after = state(Phaser.Math.RND);
    await scene.updatePartyForModifiers(scene.getPlayerParty(), true);
    const party = scene.getPlayerParty();
    expect(party).toHaveLength(n);
    lines.push(`B ${n} ${before} | ${after}`);
    for (let k = 0; k < n; k++) {
      const s = starters[k];
      const p = party[k];
      const mv = [0, 1, 2, 3].map(j => s.moveset[j] ?? 0);
      lines.push(
        `S ${s.speciesId} ${s.formIndex} ${+s.shiny} ${s.variant} ${+s.female} ${s.abilityIndex} ${s.nature} ${+s.passive} ${s.teraType ?? -1} ${given[k].ivs.join(" ")} ${mv.join(" ")}`,
      );
      const pm = [0, 1, 2, 3].map(j => p.getMoveset()[j]?.moveId ?? 0);
      lines.push(
        `M ${p.id} ${p.species.speciesId} ${p.formIndex} ${p.level} ${p.nature} ${p.abilityIndex} ${p.getAbility(true).id} ${+!!p.passive} ${p.gender} ${p.luck} ${p.teraType} ${p.hp} ${p.stats.join(" ")} ${p.ivs.join(" ")} ${pm.join(" ")}`,
      );
      if (fresh) {
        if (s.shiny) cover.shinyFresh++;
        if (p.ivs.join() !== given[k].ivs.join() || p.abilityIndex !== given[k].abilityIndex || p.passive !== given[k].passive) cover.moved++;
      }
    }
    for (const p of party) p.destroy?.();
    scene.party = [];
  }
  lines.push("Z");
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
  console.log(
    `CHALLENGE-STARTERS seed=${SEED} cases=${N} fresh=${cover.freshCases} hardValid=${cover.validHard} hardInvalid=${cover.invalidHard} softOnly=${cover.softOnly} changedByFresh=${cover.moved} shinyFresh=${cover.shinyFresh} formsReset=${cover.formReset} eggRemoved=${cover.eggRemoved}`,
  );
  expect(cover.freshCases && cover.validHard && cover.invalidHard && cover.moved).toBeGreaterThan(0);
}, 900000);
