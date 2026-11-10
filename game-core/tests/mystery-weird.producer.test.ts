/* Weird Dream oracle, options 1 (transform) and 3 (lose levels) (the harness family of mystery-clown.producer.test.ts). The real encounter is started through the game manager; per case
 * the run and wave seeds, the wave (30..140), the party (3..6 members: species, levels, natures, IVs, moves with PP used, passives, pokerus, luck, status, HP, held items of every kind)
 * and the dex entries of the species the party will become are changed; onInit runs as EncounterPhase runs it (executeWithSeedOffset(.., waveIndex)): the shuffled party, the species
 * by Base Stat Total, the new Pokemon (the PlayerPokemon constructor's rolls), their Tera types; the option's onPreOptionPhase in (.., getSeedOffset()) and its onOptionPhase in
 * (.., getSeedOffset() * 500) both REALLY run, then are awaited (so everything after the first await is on the run's stream, as in the game). The transformation animations, the
 * dialogue and the dream background are stubbed; setEncounterRewards, leaveEncounterWithoutBattle and the queued PartyHealPhase are recorded.
 * Per case: the party and items before, the dex entries of the new species' lines before and after, the RNG around every step, the species list in registry order (once), onInit's
 * transformations, then the party, the items, the rewards and the dialogue keys after.
 * GC_WD_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MoveId } from "#enums/move-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { PokemonMove } from "#moves/pokemon-move";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { EnemyPokemon, PlayerPokemon } from "#field/pokemon";
import { Trainer } from "#field/trainer";
import * as AnimUtils from "#utils/anim-utils";
import * as DialogueUtils from "#mystery-encounters/encounter-dialogue-utils";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as TransformationSequence from "#mystery-encounters/encounter-transformation-sequence";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { timedEventManager } from "#app/global-event-manager";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_WD_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
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

/** a Pokemon as one record: species form level nature ivs(6) abilityIndex passive shiny variant gender friendship tera luck pokerus hp max stats(6) exp ability status toxicTurns sleepTurns freezeTurns
 * customTypes types n then (move ppUsed ppUp)* */
const dump = (e: any) => `${e.species.speciesId} ${e.formIndex} ${e.level} ${e.nature} ${e.ivs.join(" ")} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.teraType} ${e.luck} ${e.pokerus ? 1 : 0} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.exp} ${e.getAbility().id} ${e.status?.effect ?? 0} ${e.status?.toxicTurnCount ?? 0} ${e.status?.sleepTurnsRemaining ?? 0} ${e.status?.freezeTurnsRemaining ?? 0} ${e.customPokemonData.types && e.customPokemonData.types.length ? e.customPokemonData.types.map((t: any) => (t == null ? -1 : t)).join(",") : "-"} ${e.getTypes({ includeTeraType: false, bypassSummonData: true, ignoreThirdType: true }).join(",")} ${e.moveset.length} ${e.moveset.map((m: any) => `${m.moveId} ${m.ppUsed} ${m.ppUp}`).join(" ")}`;

const dexLine = (scene: any, id: number) => {
  const d = scene.gameData.dexData[id], s = scene.gameData.starterData[id];
  return `${id}:${d.caughtAttr}:${d.natureAttr}:${d.caughtCount}:${d.hatchedCount}:${d.seenAttr}:${d.ivs.join(",")}:${s ? s.candyCount : -1}:${s ? s.abilityAttr : -1}:${s ? s.eggMoves : -1}`;
};

const lines: string[] = [`V 1 ${N}`];
const stage = (t: string) => { if (process.env.GC_WD_DEBUG) fs.appendFileSync('/tmp/wd-stage.log', t + '\n'); };
const cover = { options: [0, 0, 0], kept: 0, eggMoves: 0, starters: 0, shiny: 0, gateau: 0, species: new Set<number>(), over600: 0, retries: 0, small: 0, passives: 0, fainted: 0, status: 0, wide: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 4).toBe(true);
});

const heldPool: Array<[string, boolean, () => number | undefined]> = [
  ["LEFTOVERS", false, () => undefined], ["SHELL_BELL", false, () => undefined], ["SOUL_DEW", false, () => undefined], ["FOCUS_BAND", false, () => undefined],
  ["QUICK_CLAW", false, () => undefined], ["KINGS_ROCK", false, () => undefined], ["WIDE_LENS", false, () => undefined], ["MULTI_LENS", false, () => undefined],
  ["SCOPE_LENS", false, () => undefined], ["WHITE_HERB", false, () => undefined], ["LUCKY_EGG", false, () => undefined], ["GOLDEN_EGG", false, () => undefined],
  ["SOOTHE_BELL", false, () => undefined], ["GOLDEN_PUNCH", false, () => undefined], ["GRIP_CLAW", false, () => undefined], ["TOXIC_ORB", false, () => undefined],
  ["FLAME_ORB", false, () => undefined], ["MINI_BLACK_HOLE", false, () => undefined], ["REVIVER_SEED", false, () => undefined], ["EVIOLITE", false, () => undefined],
  ["BATON", false, () => undefined], ["IV_SCANNER", false, () => undefined], ["MYSTICAL_ROCK", false, () => undefined],
  ["BERRY", true, () => rnd(11)], ["BERRY", true, () => rnd(11)], ["BASE_STAT_BOOSTER", true, () => rnd(6)], ["ATTACK_TYPE_BOOSTER", true, () => rnd(18)],
  ["SPECIES_STAT_BOOSTER", true, () => rnd(6)], ["RARE_SPECIES_STAT_BOOSTER", true, () => rnd(6)],
  ["FORM_CHANGE_ITEM", true, () => 1 + rnd(40)], ["RARE_FORM_CHANGE_ITEM", true, () => 1 + rnd(40)],
  ["EVOLUTION_TRACKER_GIMMIGHOUL", false, () => undefined], ["MYSTERY_ENCOUNTER_SHUCKLE_JUICE", true, () => (rnd(2) ? 10 : -15)],
  ["MYSTERY_ENCOUNTER_OLD_GATEAU", false, () => undefined], ["MYSTERY_ENCOUNTER_MACHO_BRACE", false, () => undefined],
];

for (let ci = 0; ci < configs.length; ci++) {

  it(`weird dream, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(Number(process.env.GC_W ?? 40)).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.PLAINS, [MysteryEncounterType.WEIRD_DREAM]]]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.WEIRD_DREAM, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    /* the dream background (1000 ms) is skipped, the side-by-side transformations (0, 4000, 8000) run at once */
    scene.time.delayedCall = ((delay: number, cb: () => void) => { if (delay !== 1000) cb(); return {}; }) as any;
    vi.spyOn(AnimUtils, "playTween").mockImplementation((async () => {}) as any);
    vi.spyOn(EnemyPokemon.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(Trainer.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(Trainer.prototype, "initSprite").mockImplementation((() => {}) as any);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    vi.spyOn(TransformationSequence, "doPokemonTransformationSequence").mockImplementation((async () => {}) as any);
    vi.spyOn(PlayerPokemon.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(PlayerPokemon.prototype, "updateInfo").mockImplementation((async () => {}) as any);
    const dialogue: string[] = [];
    vi.spyOn(DialogueUtils, "showEncounterText").mockImplementation((async (key: string, ..._rest: any[]) => { const k = String(key); dialogue.push(/added as a starter/.test(k) ? "addedAsAStarter" : k.split(":").pop()!); }) as any);
    vi.spyOn(DialogueUtils, "showEncounterDialogue").mockImplementation((async (key: string) => { dialogue.push(String(key).split(":").pop()!); }) as any);
    const queued: string[] = [];
    vi.spyOn(scene.phaseManager, "unshiftNew").mockImplementation(((name: string, ...args: any[]) => { queued.push(`${name}${args.length ? `(${args.join(",")})` : ""}`); }) as any);
    vi.spyOn(scene.ui, "showText").mockImplementation(((_t: string, _d: any, cb?: () => void) => { if (cb) cb(); }) as any);
    for (const fn of ["setPokemonCaught", "setEggMoveUnlocked"]) {
      const orig = (scene.gameData as any)[fn].bind(scene.gameData);
      vi.spyOn(scene.gameData as any, fn).mockImplementation((async (...a: any[]) => { stage(`${fn} in`); const r = await orig(...a); stage(`${fn} out ${r}`); return r; }) as any);
    }
    const original = [...party];
    const all = speciesDataRegistry.getAllSpecies().filter((s: any) => s.speciesId < 2000);
    lines.push(`P ${party.length}`);
    if (ci === 0) {
      const list = speciesDataRegistry.getAllSpecies();
      lines.push(`S ${list.length} ${list.map((s: any) => `${s.speciesId}:${s.getBaseStatTotal()}`).join(" ")}`);
    }
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
    let current: any[] = original;
    const holder = (m: any) => {
      if (m.pokemonId == null) return -1;
      const k = current.findIndex((p: any) => p.id === m.pokemonId);
      if (k >= 0) return k;
      return ids.indexOf(m.pokemonId) >= 0 ? -3 - ids.indexOf(m.pokemonId) : -2;
    };
    const list = () => scene.modifiers.map((m: any) => `${m.type.id} ${argOf(m.type)} ${holder(m)} ${m.stackCount} ${m.virtualStackCount ?? 0}`);
    for (let i = 0; i < share; i++) {
      const k = i % 3, wave = 30 + rnd(111), runSeed = word(), waveSeed = word(), biome = BIOMES[rnd(BIOMES.length)];
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
      queued.length = 0; dialogue.length = 0;
      party.length = 0; party.push(...original);
      current = party;
      Phaser.Math.RND.sow([`wd-build-${SEED}-${ci}-${i}`]);
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        mon.species = rnd(3) === 0 ? mon.species : all[rnd(all.length)];
        mon.formIndex = 0;
        mon.fusionSpecies = null;
        mon.fusionFormIndex = 0;
        mon.fusionAbilityIndex = 0;
        mon.fusionGender = 0;
        mon.fusionLuck = 0;
        mon.fusionCustomPokemonData = null;
        mon.customPokemonData.types = [];
        mon.level = 5 + rnd(96);
        mon.nature = rnd(25);
        mon.ivs = [0, 1, 2, 3, 4, 5].map(() => rnd(32));
        mon.shiny = false; mon.variant = 0; mon.luck = rnd(4) === 0 ? rnd(4) : 0;
        mon.pokerus = rnd(4) === 0;
        mon.passive = rnd(2) === 0;
        mon.teraType = rnd(19);
        mon.friendship = rnd(256);
        mon.abilityIndex = rnd(2);
        mon.customPokemonData.ability = rnd(6) === 0 ? AbilityId.COMATOSE : AbilityId.BALL_FETCH;
        mon.status = null;
        const n = 1 + rnd(4), moves: number[] = [];
        while (moves.length < n) { const id = 1 + rnd(MoveId.PSYCHIC_NOISE); if (!moves.includes(id)) moves.push(id); }
        mon.moveset = moves.map(id => new PokemonMove(id));
        mon.calculateStats();
        mon.moveset.forEach((mv: any) => { mv.ppUsed = rnd(3) === 0 ? rnd(5) : 0; mv.ppUp = rnd(4) === 0 ? rnd(4) : 0; });
        mon.exp = rnd(100000);
      }
      const adds: Array<() => void> = [];
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        const count = rnd(4) === 0 ? 0 : rnd(6);
        for (let a = 0; a < count; a++) {
          const [id, takesArg, argf] = heldPool[rnd(heldPool.length)];
          const stack = 1 + rnd(rnd(3) === 0 ? 8 : 3);
          adds.push(() => { mk(id, mon, takesArg ? argf() : undefined, stack); });
        }
      }
      if (rnd(2) === 0) adds.push(() => { mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)); });
      if (rnd(3) === 0) adds.push(() => { mk("SHINY_CHARM", undefined, undefined, 1 + rnd(3)); });
      if (rnd(3) === 0) adds.push(() => { mk("ABILITY_CHARM", undefined, undefined, 1 + rnd(3)); });
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      for (const mon of party) {
        const r = rnd(8);
        mon.hp = r === 0 ? 0 : r === 1 ? mon.getMaxHp() : 1 + rnd(mon.getMaxHp());
      }
      const statusIds = [1, 2, 3, 4, 5, 6];   // POISON, TOXIC, PARALYSIS, SLEEP, FREEZE, BURN (FAINT is added by the fainted ones)
      for (const mon of party) {
        if (mon.hp > 0 && rnd(4) === 0) {
          const effect = statusIds[rnd(statusIds.length)];
          mon.status = { effect, toxicTurnCount: effect === 2 ? 1 + rnd(4) : 0, sleepTurnsRemaining: effect === 4 ? 2 + rnd(3) : undefined, freezeTurnsRemaining: effect === 5 ? 1 + rnd(3) : undefined };
        }
      }
      if (!party.some((mon: any) => mon.hp > 0)) party[0].hp = 1 + rnd(party[0].getMaxHp());
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      scene.money = rnd(5000);
      const mons = party.map((mon: any) => dump(mon));
      const before0 = list();
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId} ${scene.money}`, `U ${mons.join(" ; ")}`,
        `L ${before0.length} ${before0.join(" ")}`);

      stage(`case ${i} init`);
      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`wd-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      const rngInit = rng();
      const transformations = encounter.misc.teamTransformations;
      lines.push(`R ${rngBefore}`, `I ${rngInit}`, `TF ${transformations.length} ${transformations.map((t: any) => `${party.indexOf(t.previousPokemon)} ${t.newSpecies.speciesId} ${t.heldItems.length} | ${dump(t.newPokemon)}`).join(" ; ")}`);
      transformations.forEach((t: any) => cover.species.add(t.newSpecies.speciesId));
      cover.over600 += transformations.some((t: any) => t.newSpecies.getBaseStatTotal() > 600) ? 1 : 0;

      /* the dex entries of the new species' lines, part-filled */
      const chain = new Set<number>();
      for (const t of transformations) {
        for (let id: number | null = t.newSpecies.speciesId; id != null; id = speciesDataRegistry.getPrevolution(id)) chain.add(id);
        chain.add(t.newSpecies.getRootSpeciesId(true));
        chain.add(t.newSpecies.getRootSpeciesId());
      }
      const gameData = scene.gameData;
      for (const id of chain) {
        const d = gameData.dexData[id];
        const r = rnd(5);
        d.caughtAttr = r === 0 ? 0n : r === 1 ? 157n : r === 2 ? 3n : r === 3 ? 9n : d.caughtAttr;
        d.natureAttr = rnd(3) === 0 ? 0 : (1 << (1 + rnd(24))) | (rnd(2) ? 1 << (1 + rnd(24)) : 0);
        d.caughtCount = rnd(5);
        d.ivs = Array.from({ length: 6 }, () => (rnd(4) === 0 ? 31 : rnd(32)));
        const sd = gameData.starterData[id];
        if (sd) {
          sd.candyCount = rnd(5) === 0 ? 9998 + rnd(3) : rnd(60);
          sd.abilityAttr = rnd(8);
          sd.eggMoves = rnd(3) === 0 ? 0 : rnd(16);
        }
      }
      const ids2 = [...chain];
      lines.push(`DB ${ids2.length} ${ids2.map(id => dexLine(scene, id)).join(" ")}`);
      const achvBefore = Object.keys(gameData.achvUnlocks).sort();

      stage(`case ${i} option ${k}`);
      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); rewards.mockClear();
      const rngPre = rng();
      let pre: Promise<boolean> | undefined;
      if (option.onPreOptionPhase) scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
      await pre; stage('pre done');
      const afterPre = rng();
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done; stage('option done');
      const afterOption = rng();
      current = scene.getPlayerParty();
      const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
      const funcs = (given?.guaranteedModifierTypeFuncs ?? []).map((f: any) => { const e = Object.entries(modifierTypes as any).find(([, v]) => v === f); return e ? e[0] : "?"; });
      lines.push(`Q ${rngPre} ${afterPre} ${afterOption}`,
        `T ${rewards.mock.calls.length} ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] === undefined ? -1 : leave.mock.calls[0][0] ? 1 : 0) : -2} ${given?.fillRemaining ? 1 : 0} ${funcs.length} ${funcs.join(" ")}`,
        `QU ${queued.length} ${queued.join(" ")}`,
        `DL ${dialogue.length} ${dialogue.join(" ")}`,
        `F ${current.length} ${current.map((mon: any) => dump(mon)).join(" ; ")}`,
        `V ${list().length} ${list().join(" ")}`,
        `DA ${ids2.length} ${ids2.map(id => dexLine(scene, id)).join(" ")}`,
        `AC ${Object.keys(gameData.achvUnlocks).sort().filter(a => !achvBefore.includes(a)).join(" ") || "-"}`);
      cover.options[k]++;
      if (k === 1) {
        const battle = scene.currentBattle, enemies = scene.getEnemyParty(), trainer = battle.trainer;
        const call = rewards.mock.calls.length ? rewards.mock.calls[0] : undefined;
        const given = call ? (call[0] as any) : undefined;
        const enemyItems = scene.enemyModifiers.map((m: any) => `${enemies.findIndex((p: any) => p.id === m.pokemonId)} ${m.type.id} ${argOf(m.type)} ${m.stackCount} ${m.isTransferable ? 1 : 0}`);
        const edump = (e: any) => `${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.id} ${e.trainerSlot} ${e.aiType} ${e.isPopulatedFromDataSource ? 1 : 0} ${e.pokerus ? 1 : 0} ${e.luck} ${e.customPokemonData.ability} | ${dump(e)}`;
        lines.push(`RW ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${(given?.guaranteedModifierTiers ?? []).join(",") || "-"} ${(given?.guaranteedModifierTypeFuncs ?? []).length} ${call && call[2] ? 1 : 0}`,
          `D ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${trainer.config.trainerType} ${trainer.variant} ${trainer.nameKey ?? "-"}`,
          `J ${enemyItems.length} ${enemyItems.join(" ")}`,
          `X ${enemies.length} ${enemies.map((e: any) => edump(e)).join(" ; ")}`,
          `X3 ${enemies.length} ${enemies.map((e: any) => `${e.pokeball} ${e.teraType} ${e.getAbility().id} ${e.name.replace(/\s+/g, "_")}`).join(" ; ")}`,
          `TG ${transformations.length} ${transformations.map((t: any) => dump(t.newPokemon)).join(" ; ")}`);
        /* the win: the callback setEncounterRewards was given runs in the rewards phase's seed offset (wave * 1000) */
        const rBefore = rng();
        if (call && call[2]) scene.executeWithSeedOffset(() => (call[2] as any)(), wave * 1000);
        lines.push(`BR ${rBefore} ${rng()} ${party.map((mon: any) => (mon.passive ? 1 : 0)).join(" ")}`);
      }
      if (k === 0) {
        cover.passives += current.filter((mon: any) => mon.passive).length;
        cover.starters += dialogue.filter(d => d === "addedAsAStarter").length;
        cover.eggMoves += ids2.filter(id => scene.gameData.starterData[id] && 0).length;
        cover.gateau += list().filter((l: string) => l.startsWith("MYSTERY_ENCOUNTER_OLD_GATEAU")).length;
        cover.shiny += current.filter((mon: any) => mon.shiny).length;
        cover.fainted += current.filter((mon: any) => mon.hp === 0).length;
        cover.status += current.filter((mon: any) => mon.status).length;
      }
      lines.push("E");
      party.length = 0; party.push(...original);
      current = party;
    }
  }, 3600000);
}

it("writes the weird dream trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-weird.txt", lines.join("\n") + "\n");
  console.log(`WEIRD DREAM ${SEED}: options=${cover.options} species=${cover.species.size} over600=${cover.over600} starters=${cover.starters} gateau=${cover.gateau} shiny=${cover.shiny} passives=${cover.passives} fainted=${cover.fainted} status=${cover.status}`);
  expect(cover.options[0] > 0 && cover.options[2] > 0).toBe(true);
});
