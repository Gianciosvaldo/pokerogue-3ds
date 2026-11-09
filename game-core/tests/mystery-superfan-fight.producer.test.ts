/* The Bug-Type Superfan's fight oracle (the harness family of mystery-breeder.producer.test.ts). The real encounter is started through the game manager; per case the
 * run and wave seeds, the biome, the wave (30..180, never a boss wave or one ending in 1: every band of getTrainerConfigForWave) and the party (levels, held items, shiny
 * luck) are changed, onInit runs as EncounterPhase runs it (in executeWithSeedOffset(.., waveIndex)), option 1's onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500).
 * initBattleWithEnemyConfig is REALLY run for the fight (the trainer, its team, the levels and the held items are the real ones; the assets and the intro visuals complete at
 * once). setEncounterRewards is recorded. The four Bug-type tutor moves and the rewards callback (doBugTypeMoveTutor) are recorded too.
 * GC_SF_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import * as AnimUtils from "#utils/anim-utils";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { EnemyPokemon } from "#field/pokemon";
import { Trainer } from "#field/trainer";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { timedEventManager } from "#app/global-event-manager";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_SF_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.PIKACHU, SpeciesId.BEEDRILL, SpeciesId.EEVEE, SpeciesId.MAGIKARP],
  [SpeciesId.MEWTWO, SpeciesId.CATERPIE, SpeciesId.SNORLAX, SpeciesId.RATTATA, SpeciesId.ARCEUS],
  [SpeciesId.SCYTHER, SpeciesId.LEAFEON, SpeciesId.CHERUBI, SpeciesId.SNORLAX, SpeciesId.BLISSEY, SpeciesId.DITTO],
  [SpeciesId.PINSIR, SpeciesId.TROPIUS, SpeciesId.SNORLAX, SpeciesId.PICHU],
];
const BIOMES = [BiomeId.TEMPLE, BiomeId.TALL_GRASS, BiomeId.BEACH, BiomeId.GRASS, BiomeId.ISLAND, BiomeId.RUINS, BiomeId.SPACE, BiomeId.GRAVEYARD, BiomeId.BADLANDS, BiomeId.MEADOW, BiomeId.PLAINS, BiomeId.FOREST, BiomeId.SEA, BiomeId.VOLCANO, BiomeId.CAVE];

const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "MINT": return t.nature;
    case "SPECIES_STAT_BOOSTER": case "RARE_SPECIES_STAT_BOOSTER":
      return ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_TOOTH", "DEEP_SEA_SCALE"].indexOf(t.key);
  }
  return -1;
};

const lines: string[] = [`V 1 ${N}`];
const cover = { bands: [0, 0, 0, 0, 0, 0, 0, 0], forms: 0, bosses: 0, items: 0, errors: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

/** a Pokemon as one record: species form level boss segments ivs nature abilityIndex passive shiny variant gender friendship id hp max stats n moves */
const dump = (e: any) => `${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`;
/** what the encounter's config adds: the ball, the tera type, the ability, the luck, the slot, the name */
const extra = (e: any) => `${e.pokeball} ${e.teraType} ${e.getAbility().id} ${e.luck} ${e.trainerSlot} ${e.fusionSpecies ? e.fusionSpecies.speciesId : 0} ${e.name.replace(/\s+/g, "_")}`;
const tokenOf = (s: string | undefined) => (s ?? "").replace(/\s+/g, "_").replace(/\n/g, "|");


for (let ci = 0; ci < configs.length; ci++) {

  it(`the bug type superfan fight, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(42).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.PLAINS, [MysteryEncounterType.BUG_TYPE_SUPERFAN]]]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.BUG_TYPE_SUPERFAN, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    scene.time.delayedCall = () => ({}) as any;
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((async () => {}) as any);
    vi.spyOn(AnimUtils, "playTween").mockImplementation((async () => {}) as any);
    vi.spyOn(EnemyPokemon.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(Trainer.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(Trainer.prototype, "initSprite").mockImplementation((() => {}) as any);
    vi.spyOn(Trainer.prototype, "getSprites").mockImplementation((() => []) as any);
    vi.spyOn(Trainer.prototype, "getTintSprites").mockImplementation((() => []) as any);
    scene.tweens.add = (cfg: any) => { cfg.onComplete?.(); return { stop: () => {}, remove: () => {}, isPlaying: () => false } as any; };
    const original = [...party];
    lines.push(`P ${party.length} ${party.map((p: any) => `${p.species.speciesId} ${p.formIndex}`).join(" ")}`);
    const share = Math.floor(N / configs.length);
    const mk = (id: string, mon?: any, arg?: number, stack = 1) => () => {
      const type: any = arg === undefined ? (modifierTypes as any)[id]() : (EncounterPhaseUtils.generateModifierType((modifierTypes as any)[id], [arg]) as any);
      if (!type.id) type.id = id;
      const modifier = type.newModifier(mon);
      modifier.stackCount = stack;
      scene.addModifier(modifier, true);
    };
    const pm = scene.phaseManager;
    for (let i = 0; i < share; i++) {
      let wave = 30 + rnd(150);
      while (wave % 10 === 0 || wave % 10 === 1) wave++;
      const runSeed = word(), waveSeed = word(), biome = BIOMES[rnd(BIOMES.length)];
      scene.setSeed(runSeed); scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.enemyParty = [];
      scene.newArena(biome);
      scene.party = [...original];
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = null;
      encounter.dialogueTokens = {};
      encounter.doEncounterRewards = undefined;
      encounter.enemyPartyConfigs.length = 0;
      encounter.startOfBattleEffects.length = 0;
      scene.modifiers.length = 0;
      scene.enemyModifiers.length = 0;
      party.length = 0; party.push(...original);
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        mon.level = 5 + rnd(96);
        mon.nature = rnd(25);
        mon.ivs = [0, 1, 2, 3, 4, 5].map(() => rnd(32));
        mon.shiny = rnd(5) === 0;
        mon.variant = mon.shiny ? rnd(3) : 0;
        mon.luck = mon.shiny ? mon.variant + 1 : 0;
        mon.abilityIndex = rnd(2);
        mon.friendship = rnd(3) === 0 ? 70 : rnd(256);
        mon.customPokemonData.ability = AbilityId.BALL_FETCH;
        mon.status = null;
        mon.calculateStats();
      }
      const adds: Array<() => void> = [];
      for (const mon of party) {
        if (rnd(2) === 0) adds.push(mk("BERRY", mon, rnd(11), 1 + rnd(3)));
        if (rnd(4) === 0) adds.push(mk("LEFTOVERS", mon));
        if (rnd(5) === 0) adds.push(mk("SOOTHE_BELL", mon));
      }
      if (rnd(2) === 0) adds.push(mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      /* about one case in five has a fainted member or two (fewer than three able ones is not tested: the encounter cannot be offered then) */
      for (const mon of party) mon.hp = 1 + rnd(mon.getMaxHp());
      if (rnd(5) === 0) party[rnd(party.length)].hp = 0;
      if (rnd(7) === 0) party[rnd(party.length)].hp = 0;
      while (party.filter((mon: any) => mon.hp > 0).length < 3) party.find((mon: any) => mon.hp <= 0).hp = 1;
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      scene.money = rnd(5000);

      const mons = party.map((mon: any) => `${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.luck}`);
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId} ${scene.money}`, `U ${mons.join(" ; ")}`);

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`sf-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      const rngInit = rng();
      const cfg = encounter.enemyPartyConfigs[0];
      const tc = cfg.trainerConfig;
      lines.push(`R ${rngBefore}`, `I ${rngInit}`, `Y ${cfg.trainerConfig.trainerType} ${cfg.female === undefined ? -1 : cfg.female ? 1 : 0} ${cfg.doubleBattle ? 1 : 0} ${tc.partyTemplates.length} ${tc.partyTemplates[0].size} ${tc.partyTemplates[0].getStrength(tc.partyTemplates[0].size - 1)} ${tokenOf(tc.name)}`);

      const option = encounter.options[0];
      encounter.selectedOption = option;
      rewards.mockClear();
      const rngPre = rng();
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      const afterOption = rng();
      const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
      const funcs = (given?.guaranteedModifierTypeFuncs ?? []).map((f: any) => Object.entries(modifierTypes as any).filter(([, v]) => v === f).map(([key]) => key)[0]);
      const tutor = (encounter.misc?.moveTutorOptions ?? []).map((m: any) => m.moveId);
      lines.push(`Q ${rngPre} ${afterOption}`,
        `T ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${funcs.length} ${funcs.join(" ")} ${(given?.guaranteedModifierTiers ?? []).length} ${(given?.guaranteedModifierTypeOptions ?? []).length} ${given?.rerollMultiplier ?? 0} ${encounter.onRewards ? 1 : 0} ${encounter.doEncounterRewards ? 1 : 0}`,
        `M ${tutor.length} ${tutor.join(" ")}`);

      /* the fight: the real trainer and its team */
      const battle = scene.currentBattle, enemies = scene.getEnemyParty(), trainer = battle.trainer;
      const enemyItems = scene.enemyModifiers.map((mm: any) => `${enemies.findIndex((p: any) => p.id === mm.pokemonId)} ${mm.type.id} ${argOf(mm.type)} ${mm.stackCount}`);
      lines.push(`D ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${trainer.config.trainerType} ${trainer.variant} ${trainer.partyTemplateIndex} ${trainer.nameKey ?? "-"} ${tokenOf(trainer.name) || "-"} ${trainer.partnerNameKey ?? "-"}`,
        `F ${encounter.startOfBattleEffects.length}`, `J ${enemyItems.length} ${enemyItems.join(" ")}`);
      lines.push(`X ${enemies.length} ${enemies.map((e: any) => dump(e)).join(" ; ")}`);
      lines.push(`X3 ${enemies.length} ${enemies.map((e: any) => extra(e)).join(" ; ")}`);
      lines.push(`N ${enemies.map((e: any) => `${e.status?.effect ?? 0} ${e.aiType}`).join(" ; ")} ${[...new Set(trainer.config.trainerAI.instantTeras as number[])].sort().join(",") || "-"} ${trainer.config.trainerAI.teraMode}`);
      const band = wave < 30 ? 0 : wave < 50 ? 1 : wave < 70 ? 2 : wave < 100 ? 3 : wave < 120 ? 4 : wave < 140 ? 5 : wave < 160 ? 6 : 7;
      cover.bands[band]++;
      cover.forms += enemies.filter((e: any) => e.formIndex > 0).length;
      cover.bosses += enemies.filter((e: any) => e.isBoss()).length;
      cover.items += enemyItems.length > 0 ? 1 : 0;
      lines.push("E");
      scene.enemyModifiers.length = 0;
      scene.currentBattle.enemyParty = [];
      scene.party = [...original];
      party.length = 0; party.push(...original);
    }
  }, 1800000);
}

it("writes the bug type superfan fight trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-superfanfight.txt", lines.join("\n") + "\n");
  console.log(`BUG-TYPE SUPERFAN FIGHT ${SEED}: bands=${cover.bands} forms=${cover.forms} bosses=${cover.bosses} fights with items=${cover.items} errors=${cover.errors}`);
  expect(cover.bands.slice(1).every(n => n > 0)).toBe(true);
});
