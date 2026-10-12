/* Mystery encounter action oracle (card 82's harness; the older gen/dump-mystery-actions.test.ts and
 * tests/mystery_diff_test.c stay as they are). The real encounter is started through the game manager, then for each
 * case the run seed, wave seed and wave are changed and the chosen option's real onOptionPhase is run exactly as
 * MysteryEncounterOptionSelectedPhase runs it (executeWithSeedOffset(.., getSeedOffset() * 500)). Records the
 * RNG before and after the option, money, the party's levels, the items owned, whether the encounter left without
 * a battle, the shop it set up (setEncounterRewards' customModifierSettings) and the options that shop shows
 * (SelectModifierPhase.start's own calls). Family: Department Store Sale.
 * GC_MYS_N=400 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { ModifierPoolType } from "#enums/modifier-pool-type";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { SpeciesId } from "#enums/species-id";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { CIVILIZATION_ENCOUNTER_BIOMES } from "#mystery-encounters/mystery-encounter-biomes";
import { regenerateModifierPoolThresholds } from "#modifiers/modifier-type";
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_MYS_N ?? 400), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const biomeMap = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
CIVILIZATION_ENCOUNTER_BIOMES.forEach(b => biomeMap.set(b, [MysteryEncounterType.DEPARTMENT_STORE_SALE]));
const configs: SpeciesId[][] = [
  [SpeciesId.BULBASAUR], [SpeciesId.LAPRAS, SpeciesId.GENGAR, SpeciesId.ABRA], [SpeciesId.MEW],
  [SpeciesId.RATTATA, SpeciesId.MAGIKARP, SpeciesId.PIKACHU, SpeciesId.EEVEE],
];
const lines = [`V 1 ${N}`];
const cover = { options: [0, 0, 0, 0], keys: new Set<string>(), tiers: new Set<number>() };
const ssbKeys = ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_SCALE", "DEEP_SEA_TOOTH"];
const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "SPECIES_STAT_BOOSTER": case "RARE_SPECIES_STAT_BOOSTER": return ssbKeys.indexOf(t.key);
    case "TM_COMMON": case "TM_GREAT": case "TM_ULTRA": return t.moveId;
  }
  return -1;
};
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 40).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {
  it(`department store sale, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomeMap);
    await game.runToMysteryEncounter(MysteryEncounterType.DEPARTMENT_STORE_SALE, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party = scene.getPlayerParty();
    expect(encounter.options.length).toBe(4);
    const funcKey = new Map<any, string>(Object.entries(modifierTypes as any).map(([k, f]) => [f, k]));
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards");
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    lines.push(`P ${party.length} ${party.map((p: any) => `${p.species.speciesId} ${p.formIndex} ${p.level} ${p.moveset.length} ${p.moveset.map((m: any) => m.moveId).join(" ")}`).join(" ")}`);
    const share = Math.floor(N / configs.length);
    for (let i = 0; i < share; i++) {
      const k = i % 4, wave = 10 + rnd(91), runSeed = word(), waveSeed = word();
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.money = 12345 + rnd(1000);
      scene.modifiers.length = 0;
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      const option = encounter.options[k];
      encounter.selectedOption = option;
      rewards.mockClear(); leave.mockClear();
      Phaser.Math.RND.sow([`mys-${SEED}-${ci}-${i}`]);
      const before = rng(), money = scene.money, itemsBefore = scene.modifiers.length;
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      const afterOption = rng();
      expect(leave).toHaveBeenCalledTimes(1);
      expect(rewards).toHaveBeenCalledTimes(1);
      const settings: any = rewards.mock.calls[0][0];
      const funcs: string[] = (settings.guaranteedModifierTypeFuncs ?? []).map((f: any) => funcKey.get(f) ?? "?");
      expect(funcs.includes("?")).toBe(false);
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${money}`, `R ${before}`,
        `Q ${afterOption}`,
        `F ${funcs.length} ${funcs.join(" ")} ${settings.fillRemaining ? 1 : 0} ${settings.rerollMultiplier === undefined ? "nan" : settings.rerollMultiplier} ${settings.allowLuckUpgrades === undefined ? -1 : +settings.allowLuckUpgrades} ${settings.guaranteedModifierTiers?.length ?? 0} ${settings.guaranteedModifierTypeOptions?.length ?? 0}`,
        `M ${scene.money} ${scene.modifiers.length - itemsBefore} ${leave.mock.calls.length}`);
      // SelectModifierPhase.start() for that shop
      const phase: any = new SelectModifierPhase(0, undefined, settings);
      phase.updateSeed();
      regenerateModifierPoolThresholds(scene.getPlayerParty(), ModifierPoolType.PLAYER, 0);
      const count = phase.getModifierCount();
      const opts = phase.getModifierTypeOptions(count);
      lines.push(`O ${count} ${opts.length} ${opts.map((o: any) => `${o.type.id} ${argOf(o.type)} ${o.type.tier ?? -1} ${o.upgradeCount}`).join(" ")}`,
        `S ${rng()}`, "E");
      cover.options[k]++; funcs.forEach(f => cover.keys.add(f)); opts.forEach((o: any) => cover.tiers.add(o.type.tier));
    }
  }, 600000);
}

it("writes the trace", () => {
  for (const n of cover.options) expect(n).toBeGreaterThan(0);
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync(process.env.GC_MYS_OUT ?? "/tmp/mystery.txt", lines.join("\n") + "\n");
  console.log(`MYSTERY ${SEED}: ${Math.floor(N / configs.length) * configs.length} cases options=${cover.options} keys=${[...cover.keys].sort()} tiers=${[...cover.tiers].sort()}`);
});
