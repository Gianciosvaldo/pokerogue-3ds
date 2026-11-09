/* A Trainer's Test oracle (the harness family of mystery-dance.producer.test.ts). The real encounter is started through the game manager; per case the run and
 * wave seeds, the biome, the wave (10 * n + 2..4, n = 10..18) and the party (levels, fainted members, held items, shiny luck) are changed, onInit runs as
 * EncounterPhase runs it (in executeWithSeedOffset(.., waveIndex)), the option's onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500).
 * initBattleWithEnemyConfig is REALLY run for the fight: the trainer, its party (genPartyMember through the stat trainer's closures), the levels and the items are
 * the real ones; the trainer's assets and the intro visuals complete at once. setEncounterRewards and leaveEncounterWithoutBattle are recorded.
 * GC_TRN_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
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

const N = Number(process.env.GC_TRN_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.PIKACHU, SpeciesId.BULBASAUR, SpeciesId.EEVEE],
  [SpeciesId.BULBASAUR, SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE, SpeciesId.MAGNEMITE],
  [SpeciesId.SLUGMA, SpeciesId.RATTATA, SpeciesId.LEAFEON, SpeciesId.CHERUBI, SpeciesId.SNORLAX],
  [SpeciesId.EEVEE],
  [SpeciesId.LEAFEON, SpeciesId.TROPIUS, SpeciesId.SNORLAX],
];
const BIOMES = [BiomeId.TEMPLE, BiomeId.TALL_GRASS, BiomeId.BEACH, BiomeId.GRASS, BiomeId.ISLAND, BiomeId.RUINS, BiomeId.GRAVEYARD, BiomeId.BADLANDS, BiomeId.MEADOW, BiomeId.PLAINS, BiomeId.FOREST, BiomeId.SEA, BiomeId.VOLCANO, BiomeId.CAVE];

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
const cover = { options: [0, 0], types: [0, 0, 0, 0, 0], skipped: 0, items: 0, female: 0, boss: 0, forms: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

/** a Pokemon as one record: species form level boss segments ivs nature abilityIndex passive shiny variant gender friendship id hp max stats n moves */
const dump = (e: any) => `${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`;
/** what the trainer's closures add: the ball, the tera type, the ability, the luck, the slot, the name */
const extra = (e: any) => `${e.pokeball} ${e.teraType} ${e.getAbility().id} ${e.luck} ${e.trainerSlot} ${e.fusionSpecies ? e.fusionSpecies.speciesId : 0} ${e.name.replace(/\s+/g, "_")}`;
const tokenOf = (s: string | undefined) => (s ?? "").replace(/\s+/g, "_").replace(/\n/g, "|");

for (let ci = 0; ci < configs.length; ci++) {

  it(`a trainer's test, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(Number(process.env.GC_W ?? 102)).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.PLAINS, [MysteryEncounterType.A_TRAINERS_TEST]]]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.A_TRAINERS_TEST, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(2);
    scene.time.delayedCall = () => ({}) as any;
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((async () => {}) as any);
    vi.spyOn(AnimUtils, "playTween").mockImplementation((async () => {}) as any);
    vi.spyOn(EnemyPokemon.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(Trainer.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(Trainer.prototype, "initSprite").mockImplementation((() => {}) as any);
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
    const list = () => scene.modifiers.map((m: any) => {
      const idx = m.pokemonId == null ? -1 : original.findIndex((p: any) => p.id === m.pokemonId);
      return `${m.type.id} ${argOf(m.type)} ${m.pokemonId == null ? -1 : idx} ${m.stackCount}`;
    });
    const pm = scene.phaseManager;
    for (let i = 0; i < share; i++) {
      const k = i % 2, wave = 10 * (10 + rnd(9)) + 2 + rnd(3), runSeed = word(), waveSeed = word(), biome = BIOMES[rnd(BIOMES.length)];
      scene.setSeed(runSeed); scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.enemyParty = [];
      scene.newArena(biome);
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = null;
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
        mon.customPokemonData.ability = AbilityId.BALL_FETCH;
        mon.status = null;
        mon.calculateStats();
      }
      const adds: Array<() => void> = [];
      for (const mon of party) {
        if (rnd(2) === 0) adds.push(mk("BERRY", mon, rnd(11), 1 + rnd(3)));
        if (rnd(4) === 0) adds.push(mk("LEFTOVERS", mon));
      }
      if (rnd(2) === 0) adds.push(mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      for (const mon of party) mon.hp = rnd(5) === 0 ? 0 : 1 + rnd(mon.getMaxHp());
      if (!party.some((mon: any) => mon.hp > 0)) party[0].hp = 1 + rnd(party[0].getMaxHp());
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      scene.money = rnd(5000);
      const mons = party.map((mon: any) => `${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.hp} ${mon.getMaxHp()} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((mv: any) => mv.moveId).join(" ")} ${mon.luck} ${mon.getAbility().id}`);
      const before0 = list();
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId} ${scene.money}`, `U ${mons.join(" ; ")}`,
        `L ${before0.length} ${before0.join(" ")}`, `A ${meets.join(" ")}`);

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`trn-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      let initMsgs = 0;
      vi.spyOn(pm, "queueMessage").mockImplementation((() => { initMsgs++; }) as any);
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      vi.mocked(pm.queueMessage).mockRestore();
      const rngInit = rng();
      const cfg = encounter.enemyPartyConfigs[0];
      lines.push(`R ${rngBefore}`, `I ${rngInit}`, `W ${initMsgs} ${encounter.misc.trainerType} ${encounter.misc.trainerNameKey} ${tokenOf(encounter.misc.trainerEggDescription)} ${tokenOf(encounter.dialogueTokens.statTrainerName)}`,
        `Y ${encounter.enemyPartyConfigs.length} ${cfg.levelAdditiveModifier ?? 0} ${cfg.trainerConfig.trainerType} ${cfg.female === undefined ? -1 : cfg.female ? 1 : 0} ${cfg.doubleBattle ? 1 : 0} ${(cfg.pokemonConfigs ?? []).length} ${encounter.spriteConfigs.length} ${encounter.spriteConfigs.map((s: any) => s.spriteKey).join(" ")}`);
      cover.types[[0, 121, 122, 123, 124].indexOf(encounter.misc.trainerType) < 0 ? 0 : [0, 121, 122, 123, 124].indexOf(encounter.misc.trainerType)]++;

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); rewards.mockClear();
      const queued: string[] = [];
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...args: any[]) => { queued.push(args.length && args.every(a => typeof a === "number" || typeof a === "boolean") ? `${name}_${args.join("_")}` : name); }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      const pre = option.onPreOptionPhase ? 1 : 0;
      const rngPre = rng();
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      vi.mocked(pm.unshiftNew).mockRestore();
      vi.mocked(pm.pushNew).mockRestore();
      const afterOption = rng();
      const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
      const eggs: any[] = rewards.mock.calls.length ? ((rewards.mock.calls[0][1] as any[]) ?? []) : [];
      const funcs = (given?.guaranteedModifierTypeFuncs ?? []).map((f: any) => Object.entries(modifierTypes as any).filter(([, v]) => v === f).map(([key]) => key)[0]);
      lines.push(`Q ${pre} ${rngPre} ${afterOption}`,
        `T ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${funcs.length} ${funcs.join(" ")} ${(given?.guaranteedModifierTiers ?? []).length} ${(given?.guaranteedModifierTiers ?? []).join(" ")} ${(given?.guaranteedModifierTypeOptions ?? []).length} ${given?.rerollMultiplier ?? 0} ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] === undefined ? -1 : leave.mock.calls[0][0] ? 1 : 0) : -2}`,
        `Z2 ${eggs.length} ${eggs.map(e => `${e.tier} ${e.sourceType} ${e.pulled ? 1 : 0} ${tokenOf(e.eggDescriptor)}`).join(" ; ")}`,
        `O ${scene.money} ${tokenOf(encounter.dialogueTokens.eggType)}`,
        `V ${list().length} ${list().join(" ")}`,
        `H ${queued.length} ${queued.join(" ")}`);
      cover.options[k]++;

      if (k === 0) {
        const battle = scene.currentBattle, enemies = scene.getEnemyParty(), trainer = battle.trainer;
        const enemyItems = scene.enemyModifiers.map((m: any) => `${enemies.findIndex((p: any) => p.id === m.pokemonId)} ${m.type.id} ${argOf(m.type)} ${m.stackCount}`);
        lines.push(`D ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${trainer.config.trainerType} ${trainer.variant} ${trainer.partyTemplateIndex} ${trainer.nameKey ?? "-"} ${tokenOf(trainer.name)} ${trainer.partnerNameKey ?? "-"}`,
          `F ${encounter.startOfBattleEffects.length}`, `J ${enemyItems.length} ${enemyItems.join(" ")}`);
        lines.push(`X ${enemies.length} ${enemies.map((e: any) => dump(e)).join(" ; ")}`);
        lines.push(`X3 ${enemies.length} ${enemies.map((e: any) => extra(e)).join(" ; ")}`);
        lines.push(`N ${enemies.map((e: any) => `${e.status?.effect ?? 0} ${e.aiType}`).join(" ; ")}`);
        cover.items += enemyItems.length > 0 ? 1 : 0;
        cover.female += trainer.variant === 1 ? 1 : 0;
        cover.boss += enemies.some((e: any) => e.isBoss()) ? 1 : 0;
        cover.forms += enemies.some((e: any) => e.formIndex > 0) ? 1 : 0;
      }
      lines.push("E");
      party.length = 0; party.push(...original);
    }
  }, 1800000);
}

it("writes the a trainer's test trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-trainer.txt", lines.join("\n") + "\n");
  console.log(`A TRAINER'S TEST ${SEED}: options=${cover.options} types=${cover.types} skipped=${cover.skipped} female=${cover.female} boss=${cover.boss} forms=${cover.forms} fights with items=${cover.items}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
});
