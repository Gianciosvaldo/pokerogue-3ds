/* Clowning Around oracle, options 2 and 3 (the harness family of mystery-trtest.producer.test.ts). The real encounter is started through the game manager; per case the
 * run and wave seeds, the biome, the wave (80..180) and the party (species, fusions, moves, levels, fainted members, held items of every tier and stack) are changed,
 * onInit runs as EncounterPhase runs it (in executeWithSeedOffset(.., waveIndex)), the option's onPreOptionPhase in executeWithSeedOffset(.., getSeedOffset()) and its
 * onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500), both REALLY run: the item shuffle (Berries, Ultra and Rogue items, applyModifierTypeToPlayerPokemon)
 * and the type shuffle (customPokemonData.types of every Pokemon and its fusion half). leaveEncounterWithoutBattle and setEncounterRewards are recorded.
 * Per case: the party, the items before, the RNG around every step, onInit's ability and types and tokens, the Pokemon chosen for the item shuffle, the items after in
 * order, every Pokemon's custom types and the types it reports.
 * GC_CLN_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MoveId } from "#enums/move-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { PokemonMove } from "#moves/pokemon-move";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { timedEventManager } from "#app/global-event-manager";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_CLN_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.PIKACHU, SpeciesId.BULBASAUR],
  [SpeciesId.PIKACHU, SpeciesId.BULBASAUR, SpeciesId.EEVEE],
  [SpeciesId.BULBASAUR, SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE, SpeciesId.MAGNEMITE],
  [SpeciesId.SLUGMA, SpeciesId.RATTATA, SpeciesId.LEAFEON, SpeciesId.CHERUBI, SpeciesId.SNORLAX],
  [SpeciesId.EEVEE],
  [SpeciesId.LEAFEON, SpeciesId.TROPIUS, SpeciesId.SNORLAX, SpeciesId.GENGAR, SpeciesId.LAPRAS, SpeciesId.ABRA],
];
const BIOMES = [BiomeId.TEMPLE, BiomeId.TALL_GRASS, BiomeId.BEACH, BiomeId.GRASS, BiomeId.ISLAND, BiomeId.RUINS, BiomeId.GRAVEYARD, BiomeId.BADLANDS, BiomeId.MEADOW, BiomeId.PLAINS, BiomeId.FOREST, BiomeId.SEA, BiomeId.VOLCANO, BiomeId.CAVE];

const ssbKeys = ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_SCALE", "DEEP_SEA_TOOTH"];
const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "MINT": return t.nature;
    case "SPECIES_STAT_BOOSTER": case "RARE_SPECIES_STAT_BOOSTER": return ssbKeys.indexOf(t.key);
    case "FORM_CHANGE_ITEM": case "RARE_FORM_CHANGE_ITEM": return t.formChangeItem;
    case "MYSTERY_ENCOUNTER_SHUCKLE_JUICE": return t.statModifier;
    case "MYSTERY_ENCOUNTER_OLD_GATEAU": return 0;
  }
  return -1;
};

const lines: string[] = [`V 1 ${N}`];
const cover = { options: [0, 0, 0], abilities: new Set<number>(), tiers: 0, berries: 0, ultra: 0, rogue: 0, fused: 0, priority: 0, items: new Set<string>(), maxed: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

/** the held items a party member can carry: [key, takes an argument, argument source, stack source] */
const heldPool: Array<[string, boolean, () => number | undefined]> = [
  ["LEFTOVERS", false, () => undefined], ["SHELL_BELL", false, () => undefined], ["SOUL_DEW", false, () => undefined], ["FOCUS_BAND", false, () => undefined],
  ["QUICK_CLAW", false, () => undefined], ["KINGS_ROCK", false, () => undefined], ["WIDE_LENS", false, () => undefined], ["MULTI_LENS", false, () => undefined],
  ["SCOPE_LENS", false, () => undefined], ["WHITE_HERB", false, () => undefined], ["LUCKY_EGG", false, () => undefined], ["GOLDEN_EGG", false, () => undefined],
  ["SOOTHE_BELL", false, () => undefined], ["GOLDEN_PUNCH", false, () => undefined], ["GRIP_CLAW", false, () => undefined], ["TOXIC_ORB", false, () => undefined],
  ["FLAME_ORB", false, () => undefined], ["MINI_BLACK_HOLE", false, () => undefined], ["REVIVER_SEED", false, () => undefined], ["EVIOLITE", false, () => undefined],
  ["BATON", false, () => undefined], ["IV_SCANNER", false, () => undefined], ["MYSTICAL_ROCK", false, () => undefined],
  ["BERRY", true, () => rnd(11)], ["BERRY", true, () => rnd(11)], ["BASE_STAT_BOOSTER", true, () => rnd(6)], ["ATTACK_TYPE_BOOSTER", true, () => rnd(18)],
  ["ATTACK_TYPE_BOOSTER", true, () => rnd(18)], ["SPECIES_STAT_BOOSTER", true, () => rnd(6)], ["RARE_SPECIES_STAT_BOOSTER", true, () => rnd(6)],
  ["FORM_CHANGE_ITEM", true, () => 1 + rnd(40)], ["RARE_FORM_CHANGE_ITEM", true, () => 1 + rnd(40)],
  ["EVOLUTION_TRACKER_GIMMIGHOUL", false, () => undefined], ["MYSTERY_ENCOUNTER_SHUCKLE_JUICE", true, () => (rnd(2) ? 10 : -15)],
  ["MYSTERY_ENCOUNTER_OLD_GATEAU", false, () => undefined], ["MYSTERY_ENCOUNTER_MACHO_BRACE", false, () => undefined],
];

for (let ci = 0; ci < configs.length; ci++) {

  it(`clowning around, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(Number(process.env.GC_W ?? 82)).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.PLAINS, [MysteryEncounterType.CLOWNING_AROUND]]]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.CLOWNING_AROUND, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    scene.time.delayedCall = () => ({}) as any;
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    vi.spyOn(EncounterPhaseUtils, "loadCustomMovesForEncounter").mockImplementation((() => {}) as any);
    const original = [...party];
    const all = speciesDataRegistry.getAllSpecies().filter((s: any) => s.speciesId < 2000);
    lines.push(`P ${party.length}`);
    const share = Math.floor(N / configs.length);
    const mk = (id: string, mon: any, arg: number | undefined, stack: number): boolean => {
      try {
        const gen = (modifierTypes as any)[id];
        if (!gen) return false;
        let type: any;
        if (arg === undefined) type = gen();
        else if (id === "SPECIES_STAT_BOOSTER" || id === "RARE_SPECIES_STAT_BOOSTER") type = EncounterPhaseUtils.generateModifierType(gen, [ssbKeys[arg]]) as any;
        else type = EncounterPhaseUtils.generateModifierType(gen, [arg]) as any;
        if (!type) return false;
        if (!type.id) type.id = id;
        const modifier = type.newModifier(mon);
        if (!modifier) return false;
        modifier.stackCount = Math.max(1, Math.min(stack, modifier.getMaxStackCount()));
        scene.addModifier(modifier, true);
        return true;
      } catch {
        return false;
      }
    };
    const ids = original.map((p: any) => p.id);
    const holder = (m: any) => (m.pokemonId == null ? -1 : (ids.indexOf(m.pokemonId) >= 0 ? ids.indexOf(m.pokemonId) : -2));
    const list = () => scene.modifiers.map((m: any) => `${m.type.id} ${argOf(m.type)} ${holder(m)} ${m.stackCount}`);
    const pm = scene.phaseManager;
    for (let i = 0; i < share; i++) {
      const k = 1 + (i % 2), wave = 80 + rnd(101), runSeed = word(), waveSeed = word(), biome = BIOMES[rnd(BIOMES.length)];
      scene.setSeed(runSeed); scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.enemyParty = [];
      scene.newArena(biome);
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = null;
      encounter.dialogueTokens = {};
      encounter.enemyPartyConfigs.length = 0;
      encounter.startOfBattleEffects.length = 0;
      scene.modifiers.length = 0;
      scene.enemyModifiers.length = 0;
      party.length = 0; party.push(...original);
      Phaser.Math.RND.sow([`cln-build-${SEED}-${ci}-${i}`]);
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        mon.species = rnd(3) === 0 ? mon.species : all[rnd(all.length)];
        mon.formIndex = 0;
        mon.fusionSpecies = rnd(4) === 0 ? all[rnd(all.length)] : null;
        mon.fusionFormIndex = 0;
        mon.fusionAbilityIndex = 0;
        mon.fusionGender = 0;
        mon.fusionLuck = 0;
        mon.fusionCustomPokemonData = null;
        mon.customPokemonData.types = [];
        mon.level = 5 + rnd(96);
        mon.nature = rnd(25);
        mon.ivs = [0, 1, 2, 3, 4, 5].map(() => rnd(32));
        mon.shiny = false; mon.variant = 0; mon.luck = 0;
        mon.abilityIndex = rnd(2);
        mon.customPokemonData.ability = AbilityId.BALL_FETCH;
        mon.status = null;
        /* a moveset of attacks of every type and the odd status move */
        const n = 1 + rnd(4), moves: number[] = [];
        while (moves.length < n) { const id = 1 + rnd(MoveId.PSYCHIC_NOISE); if (!moves.includes(id)) moves.push(id); }
        mon.moveset = moves.map(id => new PokemonMove(id));
        mon.calculateStats();
      }
      if (!party.some((mon: any) => mon.moveset.some((mv: any) => mv.getMove().category !== 2))) party[0].moveset[0] = new PokemonMove(MoveId.TACKLE);   // an attack in the party: the Attack Type Booster generator needs one
      const adds: Array<() => void> = [];
      const heavy = rnd(3) === 0 ? rnd(party.length) : -1;   // one Pokemon with a pile of items now and then (ties and strict winners both happen)
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        const count = m === heavy ? 6 + rnd(8) : (rnd(4) === 0 ? 0 : rnd(5));
        for (let a = 0; a < count; a++) {
          const [id, takesArg, argf] = heldPool[rnd(heldPool.length)];
          const stack = 1 + rnd(rnd(3) === 0 ? 8 : 3);
          adds.push(() => { mk(id, mon, takesArg ? argf() : undefined, stack); });
        }
      }
      if (heavy >= 0 && rnd(2) === 0) {   // a pile of Berries on the heavy holder: draws past a berry's stack limit, and a long run of the pool
        const mon = party[heavy], extra = 6 + rnd(10);
        for (let a = 0; a < extra; a++) { const berry = rnd(11), stack = 1 + rnd(3); adds.push(() => { mk("BERRY", mon, berry, stack); }); }
      }
      if (heavy >= 0 && rnd(2) === 0) {   // and of Rogue and Ultra items with room to stack
        const mon = party[heavy], extra = 4 + rnd(6), picks = ["LEFTOVERS", "SHELL_BELL", "SOUL_DEW", "FOCUS_BAND", "GRIP_CLAW", "KINGS_ROCK", "QUICK_CLAW", "WIDE_LENS", "GOLDEN_PUNCH", "SCOPE_LENS", "BATON", "REVIVER_SEED"];
        for (let a = 0; a < extra; a++) { const id = picks[rnd(picks.length)], stack = 1 + rnd(5); adds.push(() => { mk(id, mon, undefined, stack); }); }
      }
      if (rnd(2) === 0) adds.push(() => { mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)); });
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      for (const mon of party) mon.hp = rnd(5) === 0 ? 0 : 1 + rnd(mon.getMaxHp());
      if (!party.some((mon: any) => mon.hp > 0)) party[0].hp = 1 + rnd(party[0].getMaxHp());
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      scene.money = rnd(5000);
      const mons = party.map((mon: any) => `${mon.species.speciesId} ${mon.formIndex} ${mon.fusionSpecies ? mon.fusionSpecies.speciesId : 0} ${mon.fusionFormIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.hp} ${mon.getMaxHp()} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((mv: any) => mv.moveId).join(" ")} ${mon.luck} ${mon.getAbility().id}`);
      const before0 = list();
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId} ${scene.money}`, `U ${mons.join(" ; ")}`,
        `L ${before0.length} ${before0.join(" ")}`, `A ${meets.join(" ")}`);

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`cln-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      const rngInit = rng();
      const cfg = encounter.enemyPartyConfigs[0];
      const blace = cfg.pokemonConfigs[1].customPokemonData;
      lines.push(`R ${rngBefore}`, `I ${rngInit}`, `W ${encounter.misc.ability} ${blace.types.join(" ")} ${encounter.dialogueTokens.ability.replace(/\s+/g, "_")} ${encounter.dialogueTokens.blacephalonName}`,
        `Y ${encounter.enemyPartyConfigs.length} ${cfg.trainerConfig.trainerType} ${cfg.doubleBattle ? 1 : 0} ${cfg.pokemonConfigs.length} ${cfg.pokemonConfigs.map((c: any) => `${c.species.speciesId} ${c.isBoss ? 1 : 0} ${c.moveSet.join(",")}`).join(" ; ")}`);
      cover.abilities.add(encounter.misc.ability);

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); rewards.mockClear();
      const rngPre = rng();
      let pre: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
      await pre;
      const afterPre = rng();
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      const afterOption = rng();
      const typesOf = (mon: any) => mon.getTypes({ includeTeraType: false, bypassSummonData: true, ignoreThirdType: true }).join(",");
      const customOf = (d: any) => (d && d.types && d.types.length ? d.types.map((t: any) => (t == null ? -1 : t)).join(",") : "-");
      lines.push(`Q ${rngPre} ${afterPre} ${afterOption}`,
        `T ${rewards.mock.calls.length} ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] === undefined ? -1 : leave.mock.calls[0][0] ? 1 : 0) : -2} ${encounter.dialogueTokens.switchPokemon === undefined ? -1 : party.findIndex((mon: any) => mon.getNameToRender() === encounter.dialogueTokens.switchPokemon)}`,
        `V ${list().length} ${list().join(" ")}`,
        `K ${party.map((mon: any) => `${customOf(mon.customPokemonData)} ${mon.fusionSpecies ? customOf(mon.fusionCustomPokemonData) : "-"} ${typesOf(mon)}`).join(" ; ")}`);
      cover.options[k]++;
      const after = list();
      cover.items.add(after.length ? "x" : "empty");
      lines.push("E");
      party.length = 0; party.push(...original);
    }
  }, 1800000);
}

it("writes the clowning around trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-clown.txt", lines.join("\n") + "\n");
  console.log(`CLOWNING AROUND ${SEED}: options=${cover.options} abilities=${cover.abilities.size}`);
  expect(cover.options[1] > 0 && cover.options[2] > 0).toBe(true);
});
