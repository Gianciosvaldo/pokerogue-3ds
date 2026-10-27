/* The Slumbering Snorlax oracle (the harness family of mystery-darkdeal.producer.test.ts). The real encounter is started through the game
 * manager; per case the run and wave seeds, the wave, the biome and the party (levels, IVs, nature, moves among them stealing ones, HP,
 * luck, held items) are changed, then onInit runs as EncounterPhase runs it (executeWithSeedOffset(.., waveIndex)) and one option
 * runs as the phases run it: onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500) with initBattleWithEnemyConfig
 * REALLY run (the boss made from its data source, the tail after its await on the global RNG). The boss's post-summon effect
 * (mysteryEncounterBattleEffects) is called by hand, the way PostSummonPhase calls it.
 * Per case: what the options need (meetsRequirements, the steal option's primary Pokemon), the RNG before and after onInit, the boss
 * onInit made (species, form, level, segments, IVs, nature, ability, shininess, id, moves, stats, HP), the item it picked (id, argument,
 * tier), then for the fight: the RNG after the option, the rewards it set, the boss that enters the battle (id, HP, stats, items,
 * tags) and the stat boost it rolls; for the steal: the rewards, the experience (who, how much) and how it leaves; for leaving: how it leaves.
 * GC_SNO_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { allMoves } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MoveId } from "#enums/move-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
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

const N = Number(process.env.GC_SNO_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
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
const cover = { options: [0, 0, 0], stacks: [0, 0, 0, 0], species: new Set<number>(), items: 0, steal: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {

  it(`slumbering snorlax, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    for (const b of BIOMES) biomes.set(b, [MysteryEncounterType.SLUMBERING_SNORLAX]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.SLUMBERING_SNORLAX, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    const expSet = vi.spyOn(EncounterPhaseUtils, "setEncounterExp").mockImplementation(() => {});
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
      const k = i % 3, wave = 15 + rnd(136), runSeed = word(), waveSeed = word();
      scene.setSeed(runSeed); scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.newArena(BIOMES[rnd(BIOMES.length)]);
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = undefined;
      encounter.startOfBattleEffects.length = 0;
      encounter.options[2].primaryPokemon = undefined;
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
        if (rnd(3) === 0) ids.push(STEALING[rnd(STEALING.length)]);
        while (ids.length < n) {
          const id = 1 + rnd(allMoves.length - 1);
          if (allMoves[id] && !ids.includes(id)) ids.push(id);
        }
        mon.moveset = ids.map(id => new PokemonMove(id));
        mon.shiny = rnd(4) === 0;
        mon.variant = mon.shiny ? rnd(3) : 0;
        mon.luck = mon.shiny ? mon.variant + 1 : 0;
        mon.calculateStats();
      }
      const adds: Array<() => void> = [];
      for (const mon of party) {
        if (rnd(3) === 0) adds.push(mk("LEFTOVERS", mon));
        if (rnd(4) === 0) adds.push(mk("SHELL_BELL", mon));
        if (rnd(4) === 0) adds.push(mk("BASE_STAT_BOOSTER", mon, rnd(6), 1 + rnd(3)));
        if (rnd(4) === 0) adds.push(mk("BERRY", mon, rnd(11), 1 + rnd(3)));
        if (rnd(5) === 0) adds.push(mk("LUCKY_EGG", mon, undefined, 1 + rnd(4)));
        if (rnd(6) === 0) adds.push(mk("QUICK_CLAW", mon, undefined, 1 + rnd(3)));
        if (rnd(6) === 0) adds.push(mk("FOCUS_BAND", mon, undefined, 1 + rnd(3)));
        if (rnd(6) === 0) adds.push(mk("WIDE_LENS", mon, undefined, 1 + rnd(3)));
        if (rnd(8) === 0) adds.push(mk("SOOTHE_BELL", mon));
      }
      if (rnd(3) === 0) adds.push(mk("EXP_SHARE", undefined, undefined, 1 + rnd(3)));
      if (rnd(3) === 0) adds.push(mk("EXP_CHARM", undefined, undefined, 1 + rnd(3)));
      if (rnd(4) === 0) adds.push(mk("SHINY_CHARM", undefined, undefined, 1 + rnd(3)));
      if (rnd(4) === 0) adds.push(mk("ABILITY_CHARM", undefined, undefined, 1 + rnd(3)));
      if (rnd(4) === 0) adds.push(mk("LOCK_CAPSULE"));
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
      // fainted Pokemon do not count for luck or the steal option
      if (rnd(4) === 0 && party.length > 1) { const keep = rnd(party.length); party.forEach((mon: any, j: number) => { if (j !== keep) mon.hp = 0; }); }
      /* a party with nobody able to fight cannot meet a battle: the run would be over */
      if (party.every((mon: any) => mon.hp <= 0)) party[rnd(party.length)].hp = 1;
      /* the shiny roll of every Pokemon made reads these two */
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      const hpAfter = party.map((p: any) => p.hp);
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const primary = encounter.options[2].primaryPokemon ? original.indexOf(encounter.options[2].primaryPokemon) : -1;
      const before0 = list();
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId}`, `U ${mons.join(" ; ")}`, `H ${hpAfter.join(" ")}`,
        `B ${scene.pokeballCounts[0]} ${scene.pokeballCounts[1]} ${scene.pokeballCounts[2]} ${scene.pokeballCounts[3]} ${scene.pokeballCounts[4]}`,
        `L ${before0.length} ${before0.join(" ")}`, `A ${meets.join(" ")} ${primary}`);
      if (!meets[k]) { lines.push("E"); continue; }

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`sno-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      const rngInit = rng();
      const cfg = encounter.enemyPartyConfigs[0], pc = cfg.pokemonConfigs[0];
      const st = Array.isArray(pc.status) ? pc.status : [pc.status, -1];
      lines.push(`R ${rngBefore}`, `I ${rngInit}`,
        `W ${pc.species.speciesId} ${cfg.levelAdditiveModifier} ${pc.isBoss ? 1 : 0} ${pc.shiny ? 1 : 0} ${st[0]} ${st[1]} ${pc.nature} ${pc.moveSet.length} ${pc.moveSet.join(" ")} ${pc.modifierConfigs.length} ${pc.modifierConfigs.map((m: any) => `${argOf(m.modifier)} ${m.stackCount ?? 1}`).join(" ")} ${pc.aiType} ${pc.customPokemonData?.spriteScale ?? -1}`);
      cover.stacks[pc.modifierConfigs[3].stackCount]++;
      cover.stacks[2 + pc.modifierConfigs[4].stackCount]++;

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); rewards.mockClear(); expSet.mockClear();
      const rewardLine = () => {
        const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
        const funcs = (given?.guaranteedModifierTypeFuncs ?? []).map((fn: any) => (fn === (modifierTypes as any).LEFTOVERS ? "LEFTOVERS" : "OTHER"));
        const opts = (given?.guaranteedModifierTypeOptions ?? []).length;
        return `S ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${funcs.length} ${funcs.join(" ")} ${opts} ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] ? 1 : 0) : -1}`;
      };
      if (k === 0) {
        const queued: string[] = [];
        vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string) => { queued.push(name); }) as any);
        vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
        let done: Promise<boolean> | undefined;
        scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
        await done;
        vi.mocked(pm.unshiftNew).mockRestore();
        vi.mocked(pm.pushNew).mockRestore();
        const afterOption = rng();
        const battle = scene.currentBattle, e = scene.getEnemyParty()[0];
        expect(scene.getEnemyParty().length).toBe(1);
        const tagNames = e.summonData.tags.map((t: any) => t.tagType).join(",");
        const enemyItems = enemyList();
        const fx = encounter.startOfBattleEffects.map((x: any) => `${x.sourceBattlerIndex} ${x.targets.length} ${x.targets.join(" ")} ${x.move.moveId} ${x.useMode}`);
        lines.push(`Q ${afterOption} ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${queued.length} ${queued.join(" ")}`,
          rewardLine(),
          `X ${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`,
          `N ${e.status?.effect ?? 0} ${e.status?.sleepTurnsRemaining ?? -1} ${e.customPokemonData?.spriteScale ?? -1} ${e.aiType}`,
          `T ${tagNames.length ? tagNames : "-"}`,
          `J ${enemyItems.length} ${enemyItems.join(" ")}`,
          `F ${fx.length} ${fx.join(" ; ")}`, "E");
        cover.items += enemyItems.length > 0 ? 1 : 0;
        cover.species.add(e.level);
        cover.options[0]++;
      } else if (k === 1) {
        const queued: string[] = [], msgs: string[] = [];
        vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...args: any[]) => { queued.push(`${name}:${args.join(",")}`); }) as any);
        vi.spyOn(Dialogue, "queueEncounterMessage").mockImplementation(((key: string) => { msgs.push(key); }) as any);
        let done: Promise<boolean> | undefined;
        scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
        await done;
        vi.mocked(pm.unshiftNew).mockRestore();
        vi.mocked(Dialogue.queueEncounterMessage).mockRestore();
        lines.push(`Q ${rng()}`, rewardLine(), `K ${expSet.mock.calls.length}`, `D ${queued.length} ${queued.join(" ")} ${msgs.length} ${msgs.join(" ")}`, "E");
        cover.options[1]++;
      } else {
        let done: Promise<boolean> | undefined;
        scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
        await done;
        const exp = expSet.mock.calls.length ? expSet.mock.calls[0] : undefined;
        lines.push(`Q ${rng()}`, rewardLine(),
          `K ${expSet.mock.calls.length} ${exp ? original.findIndex((p: any) => p.id === exp[0]) : -1} ${exp ? exp[1] : -1} ${exp ? (exp[2] === false ? 0 : 1) : -1}`, "E");
        cover.options[2]++;
        cover.steal++;
      }
      party.length = 0; party.push(...original);
    }
  }, 1200000);
}

it("writes the slumbering snorlax trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-snorlax.txt", lines.join("\n") + "\n");
  console.log(`SLUMBERING SNORLAX ${SEED}: options=${cover.options} stacks=${cover.stacks} levels=${cover.species.size} with items=${cover.items} steals=${cover.steal}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
});
