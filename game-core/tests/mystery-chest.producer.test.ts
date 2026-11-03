/* The Mysterious Chest oracle (the harness family of mystery-snorlax.producer.test.ts). The real encounter is started through the game
 * manager; per case the run and wave seeds, the wave, the biome and the party (levels with plenty of ties, IVs, nature, moves, HP, luck,
 * held items, fainted members) are changed, then onInit runs as EncounterPhase runs it (executeWithSeedOffset(.., waveIndex)) and one
 * option runs as the phases run it: onPreOptionPhase in executeWithSeedOffset(.., getSeedOffset()) (the roll), onOptionPhase in
 * executeWithSeedOffset(.., getSeedOffset() * 500) with the trap's showEncounterText awaited (so what follows it is on the global RNG, as
 * in the game) and initBattleWithEnemyConfig REALLY run. The seeds are searched so that every reward band, both sides of every band edge
 * (29/30, 34/35, 44/45, 74/75) and the trap come up.
 * Per case: what the options need, the RNG before and after onInit, the config onInit made (species, form, level modifier, moves, boss,
 * disableSwitch), the roll, then after the option: the RNG, the rewards it set (tiers, fill), what it said (called order), the party's HP
 * and status, whether it is game over, and for the trap fight the boss that enters the battle (id, form, level, segments, IVs, nature,
 * ability, HP, stats, moves, items).
 * GC_CHE_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { allMoves } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MoveId } from "#enums/move-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { randSeedInt } from "#utils/common";
import * as PokemonUtils from "#mystery-encounters/encounter-pokemon-utils";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import * as Dialogue from "#mystery-encounters/encounter-dialogue-utils";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { timedEventManager } from "#app/global-event-manager";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_CHE_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.BULBASAUR, SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE],
  [SpeciesId.PERSIAN, SpeciesId.GENGAR],
  [SpeciesId.RATTATA, SpeciesId.MAGIKARP, SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.ABRA, SpeciesId.GEODUDE],
  [SpeciesId.PICHU, SpeciesId.CHARIZARD, SpeciesId.MEOWTH, SpeciesId.GRENINJA, SpeciesId.SNEASEL],
  [SpeciesId.SCYTHER, SpeciesId.URSHIFU, SpeciesId.GHOLDENGO, SpeciesId.MAGMAR],
];
const BIOMES = [BiomeId.PLAINS, BiomeId.TOWN, BiomeId.FOREST, BiomeId.SEA, BiomeId.CAVE, BiomeId.VOLCANO, BiomeId.SPACE, BiomeId.LABORATORY,
  BiomeId.GRASS, BiomeId.TALL_GRASS, BiomeId.MEADOW, BiomeId.SWAMP, BiomeId.MOUNTAIN, BiomeId.BEACH, BiomeId.LAKE, BiomeId.JUNGLE,
  BiomeId.GRAVEYARD, BiomeId.DOJO, BiomeId.ICE_CAVE, BiomeId.WASTELAND, BiomeId.ABYSS, BiomeId.RUINS, BiomeId.ISLAND, BiomeId.METROPOLIS];
const STEALING = [MoveId.PLUCK, MoveId.COVET, MoveId.KNOCK_OFF, MoveId.THIEF, MoveId.TRICK, MoveId.SWITCHEROO];
const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "FORM_CHANGE_ITEM": case "RARE_FORM_CHANGE_ITEM": return t.formChangeItem;
    case "EVOLUTION_ITEM": case "RARE_EVOLUTION_ITEM": return t.evolutionItem;
    case "MINT": return t.nature;
    /* game-core numbers the species boosters by their item: Light Ball, Thick Club, Metal Powder, Quick Powder, Deep Sea Tooth, Deep Sea Scale */
    case "SPECIES_STAT_BOOSTER": case "RARE_SPECIES_STAT_BOOSTER":
      return ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_TOOTH", "DEEP_SEA_SCALE"].indexOf(t.key);
  }
  return -1;
};
const lines: string[] = [`V 1 ${N}`];
const cover = { bands: [0, 0, 0, 0, 0], leave: 0, edges: new Set<number>(), gameOver: 0, ties: 0, forms: new Set<number>(), items: 0 };
const BOUNDARY = [0, 29, 30, 34, 35, 44, 45, 74, 75, 99];
const BANDS: Array<[number, number]> = [[75, 99], [45, 74], [35, 44], [30, 34], [0, 29]];

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {

  it(`mysterious chest, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    for (const b of BIOMES) biomes.set(b, [MysteryEncounterType.MYSTERIOUS_CHEST]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.MYSTERIOUS_CHEST, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(2);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((async () => {}) as any);
    const said: string[] = [];
    vi.spyOn(Dialogue, "queueEncounterMessage").mockImplementation(((key: string) => { said.push(/fainted/i.test(key) ? "battle:fainted" : key); }) as any);
    vi.spyOn(Dialogue, "showEncounterText").mockImplementation((async (key: string) => { said.push(key); }) as any);
    /* the chest's sprites: the pre-option phase swaps and plays them */
    encounter.introVisuals = { getSpriteAtIndex: () => [{ setAlpha: () => {} }], spriteConfigs: [{}, {}], playAnim: () => {} };
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
    const enemyList = () => scene.enemyModifiers.map((m: any) => `${m.type.id} ${argOf(m.type)} ${m.stackCount}`);
    const pm = scene.phaseManager;
    const probe = (runSeed: string, wave: number): number => {
      scene.setSeed(runSeed);
      scene.currentBattle.waveIndex = wave;
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      let roll = -1;
      scene.executeWithSeedOffset(() => { roll = randSeedInt(100); }, encounter.getSeedOffset());
      return roll;
    };
    for (let i = 0; i < share; i++) {
      /* slots 0..4: option 1 in a reward band or the trap; 5, 6: option 1 on a band edge; 7: leave */
      const slot = i % 8, k = slot === 7 ? 1 : 0, wave = 15 + rnd(136);
      let runSeed = word(), roll = -1;
      if (k === 0) {
        const want = slot < 5 ? -1 : BOUNDARY[rnd(BOUNDARY.length)];
        for (let tries = 0; tries < 6000; tries++) {
          runSeed = word();
          roll = probe(runSeed, wave);
          if (slot < 5 ? roll >= BANDS[slot][0] && roll <= BANDS[slot][1] : roll === want) break;
        }
      }
      const waveSeed = word();
      scene.setSeed(runSeed); scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.newArena(BIOMES[rnd(BIOMES.length)]);
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = undefined;
      encounter.startOfBattleEffects.length = 0;
      scene.modifiers.length = 0;
      scene.enemyModifiers.length = 0;
      party.length = 0; party.push(...original);
      const mons: string[] = [];
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        mon.fusionSpecies = null;
        mon.level = 1 + rnd(rnd(2) ? 100 : 20);
        mon.nature = rnd(25);
        mon.ivs = mon.ivs.map(() => rnd(32));
        const n = 1 + rnd(4), ids: number[] = [];
        while (ids.length < n) {
          const id = 1 + rnd(allMoves.length - 1);
          if (allMoves[id] && !ids.includes(id)) ids.push(id);
        }
        mon.moveset = ids.map(id => new PokemonMove(id));
        mon.shiny = rnd(4) === 0;
        mon.variant = mon.shiny ? rnd(3) : 0;
        mon.luck = mon.shiny ? mon.variant + 1 : 0;
        mon.status = null;
        mon.calculateStats();
      }
      /* ties at the top: two members share the highest level about half of the time */
      if (rnd(2) === 0 && party.length > 1) {
        const top = Math.max(...party.map((p: any) => p.level));
        party[rnd(party.length)].level = top;
        party.forEach((mon: any) => mon.calculateStats());
      }
      const adds: Array<() => void> = [];
      for (const mon of party) {
        if (rnd(3) === 0) adds.push(mk("LEFTOVERS", mon));
        if (rnd(4) === 0) adds.push(mk("SHELL_BELL", mon));
        if (rnd(4) === 0) adds.push(mk("BASE_STAT_BOOSTER", mon, rnd(6), 1 + rnd(3)));
        if (rnd(4) === 0) adds.push(mk("BERRY", mon, rnd(11), 1 + rnd(3)));
        if (rnd(5) === 0) adds.push(mk("LUCKY_EGG", mon, undefined, 1 + rnd(4)));
        if (rnd(8) === 0) adds.push(mk("SOOTHE_BELL", mon));
      }
      if (rnd(3) === 0) adds.push(mk("EXP_SHARE", undefined, undefined, 1 + rnd(3)));
      if (rnd(4) === 0) adds.push(mk("SHINY_CHARM", undefined, undefined, 1 + rnd(3)));
      if (rnd(4) === 0) adds.push(mk("ABILITY_CHARM", undefined, undefined, 1 + rnd(3)));
      if (rnd(2) === 0) adds.push(mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      scene.pokeballCounts = { 0: rnd(30), 1: rnd(4), 2: rnd(3), 3: rnd(95), 4: rnd(2) };
      for (const mon of party) {
        const max = mon.getMaxHp();
        mon.hp = rnd(5) === 0 ? 0 : 1 + rnd(max);
        mons.push(`${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.hp} ${max} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((mv: any) => mv.moveId).join(" ")} ${mon.luck} ${mon.getAbility().id}`);
      }
      /* a lone survivor makes the trap game over (about one case in four) */
      if (rnd(4) === 0 && party.length > 1) { const keep = rnd(party.length); party.forEach((mon: any, j: number) => { if (j !== keep) mon.hp = 0; }); }
      if (party.every((mon: any) => mon.hp <= 0)) party[rnd(party.length)].hp = 1;
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      const hpAfter = party.map((p: any) => p.hp);
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const before0 = list();
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId}`, `U ${mons.join(" ; ")}`, `H ${hpAfter.join(" ")}`,
        `B ${scene.pokeballCounts[0]} ${scene.pokeballCounts[1]} ${scene.pokeballCounts[2]} ${scene.pokeballCounts[3]} ${scene.pokeballCounts[4]}`,
        `L ${before0.length} ${before0.join(" ")}`, `A ${meets.join(" ")}`);
      if (!meets[k]) { lines.push("E"); continue; }

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`che-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      const rngInit = rng();
      const cfg = encounter.enemyPartyConfigs[0], pc = cfg.pokemonConfigs[0];
      lines.push(`R ${rngBefore}`, `I ${rngInit}`,
        `W ${pc.species.speciesId} ${pc.formIndex} ${cfg.levelAdditiveModifier} ${pc.isBoss ? 1 : 0} ${cfg.disableSwitch ? 1 : 0} ${pc.moveSet.length} ${pc.moveSet.join(" ")} ${pc.modifierConfigs ? pc.modifierConfigs.length : -1}`);

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); rewards.mockClear(); said.length = 0;
      const queued: string[] = [];
      let cleared = 0;
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string) => { queued.push(name); }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      vi.spyOn(pm, "clearPhaseQueue").mockImplementation((() => { cleared++; }) as any);

      /* onPreOptionPhase (only the open option has one) */
      let pre: Promise<boolean> | undefined;
      let preRoll = -1;
      if (k === 0) {
        scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
        await pre;
        preRoll = encounter.misc.roll;
        expect(preRoll).toBe(roll);
        expect(rng()).toBe(rngInit);
      }
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      vi.mocked(pm.unshiftNew).mockRestore();
      vi.mocked(pm.pushNew).mockRestore();
      vi.mocked(pm.clearPhaseQueue).mockRestore();
      const afterOption = rng();
      const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
      const tiers: number[] = given?.guaranteedModifierTiers ?? [];
      const gameOver = queued.includes("GameOverPhase") ? 1 : 0;
      lines.push(`O ${preRoll}`, `Q ${afterOption}`,
        `S ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${tiers.length} ${tiers.join(" ")} ${(given?.guaranteedModifierTypeFuncs ?? []).length} ${(given?.guaranteedModifierTypeOptions ?? []).length} ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] ? 1 : 0) : -1}`,
        `M ${said.length} ${said.map(s => s.replace(/\s+/g, "_")).join(" ")}`,
        `Y ${original.map((p: any) => `${p.hp} ${p.status?.effect ?? 0}`).join(" ")}`,
        `G ${gameOver} ${cleared}`);
      const battling = scene.getEnemyParty().length > 0 && k === 0 && preRoll < 30 && !gameOver;
      if (battling) {
        const battle = scene.currentBattle, e = scene.getEnemyParty()[0];
        expect(scene.getEnemyParty().length).toBe(1);
        const tagNames = e.summonData.tags.map((t: any) => t.tagType).join(",");
        const enemyItems = enemyList();
        lines.push(`D ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${queued.length} ${queued.join(" ")}`,
          `X ${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`,
          `N ${e.status?.effect ?? 0} ${e.status?.sleepTurnsRemaining ?? -1} ${e.aiType}`,
          `T ${tagNames.length ? tagNames : "-"}`,
          `J ${enemyItems.length} ${enemyItems.join(" ")}`,
          `F ${encounter.startOfBattleEffects.length}`);
        cover.items += enemyItems.length > 0 ? 1 : 0;
        cover.forms.add(e.formIndex);
      }
      lines.push("E");
      if (k === 1) cover.leave++;
      else {
        cover.bands[BANDS.findIndex(([lo, hi]) => preRoll >= lo && preRoll <= hi)]++;
        if (BOUNDARY.includes(preRoll)) cover.edges.add(preRoll);
        cover.gameOver += gameOver;
        if (preRoll < 30) {
          const levels = original.map((p: any, j: number) => (hpAfter[j] > 0 ? p.level : -1));
          const top = Math.max(...levels);
          if (levels.filter((l: number) => l === top).length > 1) cover.ties++;
        }
      }
      party.length = 0; party.push(...original);
    }
  }, 1200000);
}

it("writes the mysterious chest trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-chest.txt", lines.join("\n") + "\n");
  console.log(`MYSTERIOUS CHEST ${SEED}: bands=${cover.bands} leave=${cover.leave} edges=${[...cover.edges].sort((a, b) => a - b)} gameOver=${cover.gameOver} ties=${cover.ties} forms=${[...cover.forms]} fights with items=${cover.items}`);
  expect(cover.bands.every(n => n > 0) && cover.leave > 0).toBe(true);
});
