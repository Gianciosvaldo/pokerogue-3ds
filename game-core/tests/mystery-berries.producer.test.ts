/* The Berries Abound oracle (the harness family of mystery-snorlax.producer.test.ts). The real encounter is started through the game manager; per
 * case the run and wave seeds, the wave, the biome and the party (levels, IVs, nature, held items among them berries of the kind the encounter gives)
 * are changed, then onInit runs as EncounterPhase runs it (executeWithSeedOffset(.., waveIndex)) and one option runs as the phases run it:
 * onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500) with initBattleWithEnemyConfig REALLY run. The callback setEncounterRewards is
 * given (the berries) runs by hand in executeWithSeedOffset(.., wave * 1000), the way the rewards phase does, and the boss's
 * mysteryEncounterBattleEffects by hand, the way PostSummonPhase does.
 * Per case: the config onInit made, the berry count, the fastest Pokemon and the boss's Speed, the RNG before and after, the shop (five berry
 * options), the boss that enters the battle, the enrage effect, the experience of a won race, the callback's messages and the party's items
 * afterwards. GC_BRY_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { allMoves } from "#data/data-lists";
import { audioManager } from "#app/global-audio-manager";
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

const N = Number(process.env.GC_BRY_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
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
const cover = { options: [0, 0, 0], won: 0, lost: 0, enraged: 0, species: new Set<number>(), items: 0, grabbed: new Set<number>(), full: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {

  it(`berries abound, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    for (const b of BIOMES) biomes.set(b, [MysteryEncounterType.BERRIES_ABOUND]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    vi.spyOn(audioManager, "playSound").mockImplementation((() => {}) as any);
    await game.runToMysteryEncounter(MysteryEncounterType.BERRIES_ABOUND, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    const expSet = vi.spyOn(EncounterPhaseUtils, "setEncounterExp").mockImplementation(() => {});
    vi.spyOn(Dialogue, "showEncounterText").mockImplementation((async () => {}) as any);
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
      const k = i % 3, wave = 10 + rnd(171), runSeed = word(), waveSeed = word();
      scene.setSeed(runSeed); scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.newArena(BIOMES[rnd(BIOMES.length)]);
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = undefined;
      scene.modifiers.length = 0;
      scene.enemyModifiers.length = 0;
      party.length = 0; party.push(...original);
      const mons: string[] = [];
      const boost = rnd(3) === 0 ? 60 : 0;   /* some parties are fast enough to win the race */
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        mon.fusionSpecies = null;
        mon.level = Math.min(100, boost + 1 + rnd(rnd(2) ? 100 : 20));
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
        mon.calculateStats();
      }
      const adds: Array<() => void> = [];
      for (const mon of party) {
        if (rnd(3) === 0) adds.push(mk("LEFTOVERS", mon));
        if (rnd(4) === 0) adds.push(mk("BASE_STAT_BOOSTER", mon, rnd(6), 1 + rnd(3)));
        /* berries of every kind, up to and past their caps, so that the callback finds some Pokemon full */
        for (let t = 0; t < 11; t++) if (rnd(4) === 0) adds.push(mk("BERRY", mon, t, 1 + rnd(3)));
        if (rnd(5) === 0) adds.push(mk("LUCKY_EGG", mon, undefined, 1 + rnd(4)));
        if (rnd(6) === 0) adds.push(mk("QUICK_CLAW", mon, undefined, 1 + rnd(3)));
        if (rnd(8) === 0) adds.push(mk("SOOTHE_BELL", mon));
      }
      if (rnd(3) === 0) adds.push(mk("EXP_SHARE", undefined, undefined, 1 + rnd(3)));
      if (rnd(3) === 0) adds.push(mk("EXP_CHARM", undefined, undefined, 1 + rnd(3)));
      if (rnd(4) === 0) adds.push(mk("SHINY_CHARM", undefined, undefined, 1 + rnd(3)));
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
      if (rnd(4) === 0 && party.length > 1) { const keep = rnd(party.length); party.forEach((mon: any, j: number) => { if (j !== keep) mon.hp = 0; }); }
      /* a party with nobody able to fight cannot meet a battle: the run would be over */
      if (party.every((mon: any) => mon.hp <= 0)) party[rnd(party.length)].hp = 1;
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      const hpAfter = party.map((p: any) => p.hp);
      const before0 = list();
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId}`, `U ${mons.join(" ; ")}`, `H ${hpAfter.join(" ")}`,
        `B ${scene.pokeballCounts[0]} ${scene.pokeballCounts[1]} ${scene.pokeballCounts[2]} ${scene.pokeballCounts[3]} ${scene.pokeballCounts[4]}`,
        `L ${before0.length} ${before0.join(" ")}`);

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`bry-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      const rngInit = rng();
      const cfg = encounter.enemyPartyConfigs[0], pc = cfg.pokemonConfigs[0], ds = pc.dataSource;
      lines.push(`R ${rngBefore}`, `I ${rngInit}`,
        `W ${pc.species.speciesId} ${ds.formIndex} ${pc.level} ${pc.isBoss ? 1 : 0} ${ds.ivs.join(" ")} ${ds.nature} ${ds.abilityIndex} ${ds.passive ? 1 : 0} ${ds.shiny ? 1 : 0} ${ds.variant} ${ds.gender} ${ds.friendship} ${ds.id} ${ds.hp} ${ds.stats.join(" ")} ${ds.moveset.length} ${ds.moveset.map((m: any) => m.moveId).join(" ")}`,
        `M ${encounter.misc.numBerries} ${original.indexOf(encounter.misc.fastestPokemon)} ${encounter.misc.enemySpeed} ${encounter.dialogueTokens.fastestPokemon === encounter.misc.fastestPokemon.getNameToRender() ? 1 : 0}`);

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); rewards.mockClear(); expSet.mockClear();
      const shopLine = () => {
        const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
        const opts = (given?.guaranteedModifierTypeOptions ?? []).map((o: any) => `${o.type.id} ${argOf(o.type)} ${o.type.tier} ${o.upgradeCount ?? 0}`);
        const funcs = (given?.guaranteedModifierTypeFuncs ?? []).length;
        return `S ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${funcs} ${rewards.mock.calls.length && rewards.mock.calls[0][2] ? 1 : 0} ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] ? 1 : 0) : -1} ${opts.length} ${opts.join(" ")}`;
      };
      const callbackLines = (cb: any) => {
        /* the rewards phase runs it in executeWithSeedOffset(.., wave * 1000) */
        Phaser.Math.RND.sow([`brb-${SEED}-${ci}-${i}`]);
        const r0 = rng();
        const msgs: string[] = [];
        vi.spyOn(Dialogue, "queueEncounterMessage").mockImplementation(((key: string) => { msgs.push(key); }) as any);
        scene.executeWithSeedOffset(() => cb(), wave * 1000);
        vi.mocked(Dialogue.queueEncounterMessage).mockRestore();
        const r1 = rng();
        const after = list();
        const counts = msgs.map(t => (t.match(/\d+/) ?? ["-1"])[0]);
        return [`Y ${r0} ${r1} ${msgs.length} ${counts.join(" ")}`, `G ${after.length} ${after.join(" ")}`];
      };
      const runBattle = async () => {
        const queued: string[] = [];
        vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string) => { queued.push(name); }) as any);
        vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
        let done: Promise<boolean> | undefined;
        scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
        await done;
        vi.mocked(pm.unshiftNew).mockRestore();
        vi.mocked(pm.pushNew).mockRestore();
        return queued;
      };
      if (k === 2) {
        let done: Promise<boolean> | undefined;
        scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
        await done;
        lines.push(`Q ${rng()}`, shopLine(), "E");
        cover.options[2]++;
        party.length = 0; party.push(...original);
        continue;
      }
      /* option 1, and option 2 when the boss is faster, fight; option 2 otherwise grabs the berries */
      const fastWin = k === 1 && encounter.misc.fastestPokemon.getStat(5) / (encounter.misc.enemySpeed * 1.1) >= 1;
      if (fastWin) {
        let done: Promise<boolean> | undefined;
        scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
        await done;
        const exp = expSet.mock.calls.length ? expSet.mock.calls[0] : undefined;
        const afterFast = rng();
        const cbl = callbackLines(rewards.mock.calls[0][2]);
        lines.push(`Q ${afterFast}`, shopLine(), `K ${expSet.mock.calls.length} ${exp ? original.findIndex((p: any) => p.id === exp[0]) : -1} ${exp ? exp[1] : -1} ${exp ? (exp[2] === false ? 0 : 1) : -1} ${encounter.dialogueTokens.numBerries}`, ...cbl, "E");
        cover.options[1]++; cover.won++;
        cover.grabbed.add(Number(encounter.dialogueTokens.numBerries));
      } else {
        const queued = await runBattle();
        const afterOption = rng();
        const battle = scene.currentBattle, e = scene.getEnemyParty()[0];
        expect(scene.getEnemyParty().length).toBe(1);
        const tagNames = e.summonData.tags.map((t: any) => t.tagType).join(",");
        const enemyItems = enemyList();
        /* the enrage effect of the lost race, called by hand as PostSummonPhase calls it */
        const effects: string[] = [];
        const pcNow = encounter.enemyPartyConfigs[0].pokemonConfigs[0];
        if (pcNow.mysteryEncounterBattleEffects) {
          const msgs: string[] = [], phases: string[] = [];
          vi.spyOn(Dialogue, "queueEncounterMessage").mockImplementation(((key: string) => { msgs.push(key); }) as any);
          vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, args: any) => { phases.push(`${name}:${(args?.changes ?? []).map((c: any) => `${c.stat}x${c.stages}`).join(",")}`); }) as any);
          pcNow.mysteryEncounterBattleEffects(e);
          vi.mocked(Dialogue.queueEncounterMessage).mockRestore();
          vi.mocked(pm.unshiftNew).mockRestore();
          effects.push(`${msgs.length} ${msgs.join(",") || "-"} ${phases.length} ${phases.join(";") || "-"}`);
          cover.enraged++;
        } else effects.push("0 - 0 -");
        const cbl = callbackLines(rewards.mock.calls[0][2]);
        lines.push(`Q ${afterOption} ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${queued.length} ${queued.join(" ")}`,
          shopLine(),
          `X ${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`,
          `T ${tagNames.length ? tagNames : "-"}`,
          `J ${enemyItems.length} ${enemyItems.join(" ")}`,
          `F ${effects[0]}`, ...cbl, "E");
        cover.items += enemyItems.length > 0 ? 1 : 0;
        cover.species.add(e.species.speciesId);
        cover.options[k === 0 ? 0 : 1]++;
        if (k === 1) cover.lost++;
      }
      party.length = 0; party.push(...original);
    }
  }, 1200000);
}

it("writes the berries abound trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-berries.txt", lines.join("\n") + "\n");
  console.log(`BERRIES ABOUND ${SEED}: options=${cover.options} races won=${cover.won} lost=${cover.lost} enraged=${cover.enraged} species=${cover.species.size} with items=${cover.items} grabbed=${[...cover.grabbed].sort().join(",")}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
  expect(cover.won > 0 && cover.lost > 0).toBe(true);
});
