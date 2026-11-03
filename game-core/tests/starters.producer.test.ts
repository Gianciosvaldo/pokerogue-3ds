/* The starter rules of the Endless modes, against upstream at 8555c08: the real SelectStarterPhase.initBattle(starters) runs in the
 * case's game mode (Classic, Endless, Spliced Endless) on a party of 1-6 random starters, with the Phaser RNG sown per case, and
 * getRunValueLimit() (the starter select's cost limit) is read in each mode.
 *
 * Per case: the RNG before and after, and per party member the fields game-core compares: id, species, level, nature, ability,
 * gender, shiny, variant, luck, the fusion half (species, form, ability index, gender, shiny, variant, luck: a Spliced Endless
 * starter is fused by generateFusionSpecies(true) as it is made), and the stats and HP after the first updatePartyForModifiers
 * (upstream recalculates them when the first battle starts: newBattle itself is held here).
 * Each starter's dex entry says it was caught with its own shininess so that the luck set from the dex (getDexAttrLuck) is the
 * constructor's own (game-core takes it from the constructor).
 * What the harness changes: the scene has no current battle, as at a new run's start (the game manager's own is a trainer battle); the overrides that would force a starter's shininess, IVs or gender are off; newBattle, the arena's init, asset loading and the music are stubbed (initBattle's tail).
 * GC_STARTERS_N=600 GC_STARTERS_SEED=777 GC_STARTERS_OUT=/abs/starters.txt, Node 22. */
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { audioManager } from "#app/global-audio-manager";
import { activeOverrides } from "#app/overrides";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { Gender } from "#data/gender";
import { GameModes } from "#enums/game-modes";
import { SpeciesId } from "#enums/species-id";
import { Pokemon } from "#field/pokemon";
import { SelectStarterPhase } from "#phases/select-starter-phase";
import { GameManager } from "#test/framework/game-manager";
import { getRunValueLimit } from "#ui/utils/starter-select-ui-utils";
import * as fs from "node:fs";
import Phaser from "phaser";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_STARTERS_N ?? 600);
const SEED = Number(process.env.GC_STARTERS_SEED ?? 777);
const OUT = process.env.GC_STARTERS_OUT ?? "/tmp/starters.txt";
let seed = SEED;
const rnd = (n: number) => {
  seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
  return (seed >>> 8) % n;
};
const state = (r: any) => r.state().replace(/^!rnd,/, "").split(",").join(" ");
const MODES = [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.ENDLESS];

it("native starter rules in the Endless modes", async () => {
  expect(Number(process.versions.node.split(".")[0])).toBe(22);
  const native = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5);
  await game.classicMode.startBattle(SpeciesId.MEW);
  BattleScene.prototype.randBattleSeedInt = native;
  const scene: any = game.scene;
  const ids = speciesDataRegistry.getAllStarters().sort((a: number, b: number) => a - b);
  const lines: string[] = [`V 1 ${SEED} ${N}`];
  for (const mode of [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS]) {
    scene.gameMode = getGameMode(mode);
    lines.push(`L ${mode} ${getRunValueLimit()}`);
  }
  // the game manager forces player Pokemon non-shiny; the starters here keep the shininess they are given
  for (const key of ["SHINY_OVERRIDE", "VARIANT_OVERRIDE", "NATURE_OVERRIDE", "IVS_OVERRIDE", "GENDER_OVERRIDE", "STARTER_SPECIES_OVERRIDE", "STARTER_FUSION_OVERRIDE", "STARTER_FUSION_SPECIES_OVERRIDE"])
    vi.spyOn(activeOverrides as any, key as any, "get").mockReturnValue(key === "STARTER_FUSION_OVERRIDE" ? false : (null as any));
  vi.spyOn(scene, "newBattle").mockImplementation((() => {}) as any);
  vi.spyOn(scene.arena, "init").mockImplementation((() => {}) as any);
  vi.spyOn(audioManager, "playBgm").mockImplementation((() => {}) as any);
  vi.spyOn(Pokemon.prototype, "loadAssets").mockResolvedValue(undefined as any);
  vi.spyOn(Pokemon.prototype, "updateInfo").mockResolvedValue(undefined as any);
  const cover = { fused: 0, plain: 0, shiny: 0, hidden: 0, charms: 0 };
  for (let i = 0; i < N; i++) {
    const mode = MODES[i % MODES.length];
    const n = 1 + rnd(6);
    const starters: any[] = [];
    const used = new Set<number>();
    while (starters.length < n) {
      const id = ids[rnd(ids.length)];
      if (used.has(id)) continue;
      used.add(id);
      const species = speciesDataRegistry.getSpecies(id);
      const shiny = rnd(4) === 0;
      const variant = rnd(3);
      starters.push({
        speciesId: id,
        shiny,
        variant,
        formIndex: 0,
        female: species.malePercent !== null && rnd(2) === 1,
        abilityIndex: species.abilityHidden && rnd(3) === 0 ? 2 : species.ability2 === species.ability1 ? 0 : rnd(2),
        passive: false,
        nature: rnd(25),
        pokerus: false,
        ivs: [0, 0, 0, 0, 0, 0].map(() => rnd(32)),
      });
    }
    // the dex says each starter was caught as it is (its luck is then the constructor's)
    for (const s of starters) {
      const attr = 1n | (s.shiny ? 2n | (s.variant >= 1 ? 32n : 0n) | (s.variant >= 2 ? 64n : 0n) | 16n : 16n);
      scene.gameData.dexData[s.speciesId].caughtAttr = attr;
    }
    // the Ability Charm (hidden-ability rate) in some cases
    const charms = i % 5 === 3 ? 1 + rnd(7) : 0;
    scene.modifiers = [];
    if (charms) {
      const m: any = (await import("#data/data-lists")).modifierTypes.ABILITY_CHARM().newModifier();
      m.stackCount = charms;
      scene.modifiers.push(m);
      cover.charms++;
    }
    scene.gameMode = getGameMode(mode);
    scene.party = [];
    scene.currentBattle = null;   // a new run has no battle yet (the harness's own trainer battle would limit Eevee's and Pikachu's forms)
    Phaser.Math.RND.sow([`starters-${SEED}-${i}`]);
    const before = state(Phaser.Math.RND);
    const phase = new SelectStarterPhase();
    phase.initBattle(starters);
    await new Promise(r => setTimeout(r, 0));
    const after = state(Phaser.Math.RND);
    await scene.updatePartyForModifiers(scene.getPlayerParty(), true);
    const party = scene.getPlayerParty();
    expect(party).toHaveLength(n);
    lines.push(`C ${i} ${mode} ${n} ${charms} ${getRunValueLimit()} ${before} | ${after}`);
    for (let k = 0; k < n; k++) {
      const s = starters[k];
      const p = party[k];
      lines.push(
        `S ${s.speciesId} ${s.shiny ? 1 : 0} ${s.variant} ${s.female ? 1 : 0} ${s.abilityIndex} ${s.nature} ${s.ivs.join(" ")}`,
      );
      const fused = p.fusionSpecies ? 1 : 0;
      if (fused) cover.fused++;
      else cover.plain++;
      if (p.shiny) cover.shiny++;
      if (fused && p.fusionAbilityIndex === 2) cover.hidden++;
      lines.push(
        `M ${p.id} ${p.species.speciesId} ${p.level} ${p.nature} ${p.abilityIndex} ${p.gender} ${p.luck} ${fused ? p.fusionSpecies.speciesId : 0} ${
          fused ? p.fusionFormIndex : 0
        } ${fused ? p.fusionAbilityIndex : 0} ${fused ? p.fusionGender : 0} ${fused ? +p.fusionShiny : 0} ${fused ? p.fusionVariant : 0} ${
          fused ? p.fusionLuck : 0
        } ${p.hp} ${p.stats.join(" ")}`,
      );
    }
    for (const p of party) p.destroy?.();
    scene.party = [];
  }
  lines.push("Z");
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
  console.log(`STARTERS seed=${SEED} cases=${N} fused=${cover.fused} plain=${cover.plain} shiny=${cover.shiny} hidden=${cover.hidden} charms=${cover.charms}`);
  expect(cover.fused).toBeGreaterThan(0);
}, 600000);
