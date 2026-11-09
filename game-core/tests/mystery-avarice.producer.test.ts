/* Absolute Avarice oracle (the harness family of mystery-strong.producer.test.ts and mystery-dance.producer.test.ts). The real encounter is started through the
 * game manager; per case the run and wave seeds, the wave (20..180), the biome and the party (levels, IVs, nature, moves, HP, fainted members, held items, and the
 * Berries each member holds: types, stacks) are changed, then onInit runs as EncounterPhase runs it (executeWithSeedOffset(.., waveIndex)), onVisualsStart really
 * runs (the berries leave the party), and one option runs as the phases run it: onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500).
 * Option 1 awaits transitionMysteryEncounterIntroVisuals, so initBattleWithEnemyConfig (REALLY run) is on the global RNG; the rewards callback is called by hand
 * the way the rewards phase calls it, and the boss's post-summon effect (mysteryEncounterBattleEffects) the way PostSummonPhase calls it. Option 2 is really run (the
 * shuffle is Phaser.Math.RND's, on the option's stream). Option 3 builds the Greedent in the option's stream; catchPokemon is replaced by a recorder that adds the
 * Pokemon to the party with the real Pokemon.addToParty.
 * Per case: what the options need, the RNG before and after onInit, the config onInit made (species, level modifier, bars, shiny lock, moves, the item for each stack)
 * and the berries it stole (per holder), the party's items after the visuals, then after the option: the RNG, the rewards it set, what it said, the party's items, and
 * for the fight the boss that enters (id, level, IVs, stats, moves, items), the start-of-battle effects, the rewards callback's effect, and the post-summon stat changes;
 * for the catch the Pokemon as it was caught and as it joined.
 * GC_AVA_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { allMoves, modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MoveId } from "#enums/move-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import * as Dialogue from "#mystery-encounters/encounter-dialogue-utils";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as EncounterPokemonUtils from "#mystery-encounters/encounter-pokemon-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { timedEventManager } from "#app/global-event-manager";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_AVA_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.BULBASAUR, SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE],
  [SpeciesId.PERSIAN, SpeciesId.GENGAR],
  [SpeciesId.RATTATA, SpeciesId.MAGIKARP, SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.ABRA],
  [SpeciesId.PICHU, SpeciesId.CHARIZARD, SpeciesId.MEOWTH, SpeciesId.GRENINJA, SpeciesId.SNEASEL],
  [SpeciesId.SCYTHER, SpeciesId.URSHIFU, SpeciesId.GHOLDENGO, SpeciesId.MAGMAR, SpeciesId.PIKACHU],
  [SpeciesId.PIKACHU],
  [SpeciesId.RATTATA, SpeciesId.MAGIKARP, SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.ABRA, SpeciesId.GEODUDE],
];
const BIOMES = [BiomeId.PLAINS, BiomeId.TOWN, BiomeId.FOREST, BiomeId.SEA, BiomeId.CAVE, BiomeId.VOLCANO, BiomeId.SPACE, BiomeId.LABORATORY,
  BiomeId.GRASS, BiomeId.TALL_GRASS, BiomeId.MEADOW, BiomeId.SWAMP, BiomeId.MOUNTAIN, BiomeId.BEACH, BiomeId.LAKE, BiomeId.JUNGLE,
  BiomeId.GRAVEYARD, BiomeId.DOJO, BiomeId.ICE_CAVE, BiomeId.WASTELAND, BiomeId.ABYSS, BiomeId.RUINS, BiomeId.ISLAND, BiomeId.METROPOLIS];
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
const berryCap = (type: number) => (type === 0 || type === 1 || type === 2 || type === 10 ? 2 : 3);
const lines: string[] = [`V 1 ${N}`];
const cover = { options: [0, 0, 0], items: 0, merged: 0, overflow: 0, returned: 0, skippedFull: 0, realVisuals: 0, contrary: 0, noBerries: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

/** a Pokemon as one record: species form level boss segments ivs nature abilityIndex passive shiny variant gender friendship id hp max stats n moves */
const dump = (e: any) => `${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`;

for (let ci = 0; ci < configs.length; ci++) {

  it(`absolute avarice, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    for (const b of BIOMES) biomes.set(b, [MysteryEncounterType.ABSOLUTE_AVARICE]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.ABSOLUTE_AVARICE, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((async () => {}) as any);
    scene.time.delayedCall = () => ({}) as any;
    const caught: string[] = [];
    vi.spyOn(EncounterPokemonUtils, "catchPokemon").mockImplementation((async (pokemon: any, ball: any, type: any, show: any, obtain: any) => {
      const pre = dump(pokemon);
      const np = party.length < 6 ? pokemon.addToParty(type) : null;
      caught.push(`${pre} ; ${ball === null ? 1 : 0} ${type} ${show ? 1 : 0} ${obtain ? 1 : 0} ${np ? 1 : 0} ${np ? `${np.pokeball} ${dump(np)}` : ""}`);
    }) as any);
    const said: string[] = [];
    vi.spyOn(Dialogue, "queueEncounterMessage").mockImplementation(((key: string) => { said.push(key); }) as any);
    vi.spyOn(Dialogue, "showEncounterText").mockImplementation((async (key: string) => { said.push(key); }) as any);
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
    for (let i = 0; i < share; i++) {
      const k = i % 3, wave = 20 + rnd(161), runSeed = word(), waveSeed = word();
      scene.setSeed(runSeed); scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.enemyParty = [];
      scene.newArena(BIOMES[rnd(BIOMES.length)]);
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = undefined;
      encounter.startOfBattleEffects.length = 0;
      encounter.dialogueTokens = {};
      scene.modifiers.length = 0;
      scene.enemyModifiers.length = 0;
      party.length = 0; party.push(...original);
      const mons: string[] = [];
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        mon.fusionSpecies = null;
        mon.level = 5 + rnd(rnd(2) ? 96 : 20);
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
      const adds: Array<() => void> = [];
      let nBerries = 0;
      /* a case in eight has few or no Berries; the others are generous, so that stacks of one type from several holders meet on the boss */
      const lean = rnd(8) === 0;
      for (const mon of party) {
        const kinds = lean ? rnd(2) : rnd(5);
        const used: number[] = [];
        for (let b = 0; b < kinds; b++) {
          const type = rnd(11);
          if (used.includes(type)) continue;
          used.push(type);
          const stack = 1 + rnd(berryCap(type));
          nBerries += stack;
          adds.push(mk("BERRY", mon, type, stack));
        }
        if (rnd(3) === 0) adds.push(mk("LEFTOVERS", mon));
        if (rnd(4) === 0) adds.push(mk("SHELL_BELL", mon));
        if (rnd(4) === 0) adds.push(mk("BASE_STAT_BOOSTER", mon, rnd(6), 1 + rnd(3)));
        if (rnd(5) === 0) adds.push(mk("LUCKY_EGG", mon, undefined, 1 + rnd(4)));
        if (rnd(8) === 0) adds.push(mk("SOOTHE_BELL", mon));
        /* an item that already revives: the Reviver Seed callback skips its holder */
        if (rnd(4) === 0) adds.push(mk("REVIVER_SEED", mon));
      }
      if (rnd(3) === 0) adds.push(mk("EXP_SHARE", undefined, undefined, 1 + rnd(3)));
      if (rnd(4) === 0) adds.push(mk("SHINY_CHARM", undefined, undefined, 1 + rnd(3)));
      if (rnd(2) === 0) adds.push(mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      if (nBerries === 0) cover.noBerries++;
      scene.pokeballCounts = { 0: rnd(30), 1: rnd(4), 2: rnd(3), 3: rnd(95), 4: rnd(2) };
      for (const mon of party) {
        const max = mon.getMaxHp();
        mon.hp = rnd(5) === 0 ? 0 : 1 + rnd(max);
        mons.push(`${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.hp} ${max} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((mv: any) => mv.moveId).join(" ")} ${mon.luck} ${mon.getAbility().id}`);
      }
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
      if (k === 2 && party.length >= 6) { lines.push("E"); cover.skippedFull++; continue; }

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`ava-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      const rngInit = rng();
      const cfg = encounter.enemyPartyConfigs[0], pc = cfg.pokemonConfigs[0];
      lines.push(`R ${rngBefore}`, `I ${rngInit}`,
        `W ${pc.species.speciesId} ${cfg.levelAdditiveModifier} ${pc.isBoss ? 1 : 0} ${pc.bossSegments} ${pc.shiny === false ? 0 : 1} ${pc.moveSet.length} ${pc.moveSet.join(" ")} ${pc.tags.length} ${pc.tags.join(",")} ${pc.modifierConfigs.length} ${pc.modifierConfigs.map((m: any) => `${argOf(m.modifier)} ${m.stackCount ?? 1}`).join(" ")} ${cfg.disableSwitch ? 1 : 0} ${typeof pc.mysteryEncounterBattleEffects === "function" ? 1 : 0}`);
      /* the stolen berries, by holder (in party order): a count of modifiers, then type and stack for each */
      const stolenMap: Map<number, any[]> = encounter.misc.berryItemsMap;
      lines.push(`M ${party.map((p: any) => { const s = stolenMap.get(p.id) ?? []; return `${s.length} ${s.map((b: any) => `${b.berryType} ${b.stackCount}`).join(" ")}`; }).join(" ; ")}`);
      /* the intro token */
      lines.push(`G ${String(encounter.dialogueTokens.greedentName ?? "-").replace(/\s+/g, "_")}`);

      /* onVisualsStart: the berries leave the party */
      let realVisuals = true;
      try { encounter.onVisualsStart(); } catch (e) {
        realVisuals = false;
        (scene.findModifiers((m: any) => m.constructor.name === "BerryModifier") as any[]).forEach((m: any) => scene.removeModifier(m));
        scene.updateModifiers(true);
      }
      if (realVisuals) cover.realVisuals++;
      const afterVisuals = list();
      lines.push(`T ${afterVisuals.length} ${afterVisuals.join(" ")}`);

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); rewards.mockClear(); said.length = 0; caught.length = 0;
      const queued: string[] = [];
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...args: any[]) => { queued.push(args.length && args.every(a => typeof a === "number") ? `${name}_${args.join("_")}` : name); }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      if (option.onPreOptionPhase) {
        let pre: Promise<any> | undefined;
        scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
        await pre;
      }
      const rngPre = rng();
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      vi.mocked(pm.unshiftNew).mockRestore();
      vi.mocked(pm.pushNew).mockRestore();
      const afterOption = rng();
      const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
      lines.push(`Q ${rngPre} ${afterOption}`,
        `S ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${(given?.guaranteedModifierTypeFuncs ?? []).length} ${(given?.guaranteedModifierTypeOptions ?? []).length} ${(given?.guaranteedModifierTiers ?? []).length} ${rewards.mock.calls.length && rewards.mock.calls[0][2] ? 1 : 0} ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] ? 1 : 0) : -1}`,
        `X2 ${said.length} ${said.map(s => s.replace(/\s+/g, "_")).join(" ")}`,
        `Y ${caught.length} ${caught.join(" ; ")}`,
        `V ${list().length} ${list().join(" ")}`);
      cover.options[k]++;

      if (k === 0) {
        const battle = scene.currentBattle, e = scene.getEnemyParty()[0];
        expect(scene.getEnemyParty().length).toBe(1);
        const tagNames = e.summonData.tags.map((t: any) => t.tagType).join(",");
        const enemyItems = enemyList();
        const fx = encounter.startOfBattleEffects.map((x: any) => `${x.sourceBattlerIndex} ${x.targets.length} ${x.targets.join(" ")} ${x.move.moveId} ${x.useMode}`);
        lines.push(`D ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${queued.length} ${queued.join(" ")}`,
          `X ${dump(e)}`,
          `N ${e.status?.effect ?? 0} ${e.aiType} ${tagNames.length ? tagNames : "-"} ${e.teraType}`,
          `J ${enemyItems.length} ${enemyItems.join(" ")}`,
          `F ${fx.length} ${fx.join(" ; ")}`,
          `Tk ${String(encounter.dialogueTokens.foodReward ?? "-").replace(/\s+/g, "_")}`);
        /* the rewards callback, as the rewards phase calls it */
        const cb = rewards.mock.calls.length ? (rewards.mock.calls[0][2] as any) : undefined;
        said.length = 0;
        if (cb) cb();
        lines.push(`Cb ${said.length} ${said.map(s => s.replace(/\s+/g, "_")).join(" ")}`, `Va ${list().length} ${list().join(" ")}`);
        /* PostSummonPhase's lapse of the tag: the effect, by hand */
        const effects: string[] = [];
        vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, p: any) => { effects.push(`${name} ${p.changes.map((c: any) => `${c.stat} ${c.stages}`).join(" ")}`); }) as any);
        vi.spyOn(Dialogue, "queueEncounterMessage").mockImplementation(((key: string) => { effects.push(`MSG ${key}`); }) as any);
        const r0 = rng();
        e.mysteryEncounterBattleEffects(e);
        const r1 = rng();
        vi.mocked(pm.unshiftNew).mockRestore();
        vi.mocked(Dialogue.queueEncounterMessage).mockRestore();
        vi.spyOn(Dialogue, "queueEncounterMessage").mockImplementation(((key: string) => { said.push(key); }) as any);
        const mult = e.hasAbility(86) ? 2 : e.hasAbility(126) ? -1 : 1;
        lines.push(`K ${r0} ${r1} ${effects.length} ${effects.map(s => s.replace(/ /g, "_")).join(" ")}`, `Z ${mult} ${e.bossSegmentIndex}`);
        cover.items += enemyItems.length > 0 ? 1 : 0;
        cover.contrary += mult === -1 ? 1 : 0;
        const stacks = enemyItems.length;
        if (stacks > 0) cover.merged++;
        const sumStolen = [...stolenMap.values()].reduce((s, l) => s + l.reduce((a: number, b: any) => a + b.stackCount, 0), 0);
        const sumBoss = scene.enemyModifiers.reduce((s: number, m: any) => s + m.stackCount, 0);
        if (sumBoss < sumStolen) cover.overflow++;
      } else if (k === 1) {
        const stolen = [...stolenMap.values()].reduce((s, l) => s + l.reduce((a: number, b: any) => a + b.stackCount, 0), 0);
        const back = scene.modifiers.filter((m: any) => m.type.id === "BERRY").reduce((s: number, m: any) => s + m.stackCount, 0);
        cover.returned += back > 0 ? 1 : 0;
        expect(back).toBeLessThanOrEqual(stolen);
      }
      lines.push("E");
      vi.mocked(Dialogue.queueEncounterMessage).mockRestore?.();
      vi.spyOn(Dialogue, "queueEncounterMessage").mockImplementation(((key: string) => { said.push(key); }) as any);
      party.length = 0; party.push(...original);
    }
  }, 1200000);
}

it("writes the absolute avarice trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-avarice.txt", lines.join("\n") + "\n");
  console.log(`ABSOLUTE AVARICE ${SEED}: options=${cover.options} fights with items=${cover.items} with berries=${cover.merged} overflowing=${cover.overflow} returned=${cover.returned} no berries=${cover.noBerries} full party=${cover.skippedFull} real visuals=${cover.realVisuals} contrary=${cover.contrary}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
});
