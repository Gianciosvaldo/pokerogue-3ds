/* Teleporting Hijinks oracle (the harness family of mystery-strong.producer.test.ts). The real encounter is started through the game manager;
 * per case the run and wave seeds, the wave (10 * n + 2..4), the biome, the party (Electric and Steel Pokemon, fainted members), the money
 * (short of the price, exactly the price, plenty) and the held items are changed, onInit runs as EncounterPhase runs it, and one option runs as
 * the phases run it: onPreOptionPhase in executeWithSeedOffset(.., getSeedOffset()), onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500).
 * The biome change (newArena) is really run, the tweens and asset loading are replaced; initBattleWithEnemyConfig is REALLY run after the
 * awaits (so on the global RNG); the boss's post-summon effect (mysteryEncounterBattleEffects) is called by hand the way PostSummonPhase does.
 * GC_TLP_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { allMoves } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MoveId } from "#enums/move-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { randSeedInt } from "#utils/common";
import * as AnimUtils from "#utils/anim-utils";
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

const N = Number(process.env.GC_TLP_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.PIKACHU, SpeciesId.BULBASAUR, SpeciesId.EEVEE],
  [SpeciesId.BULBASAUR, SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE, SpeciesId.MAGNEMITE],
  [SpeciesId.MAGNEMITE, SpeciesId.ARON, SpeciesId.RATTATA, SpeciesId.JOLTEON, SpeciesId.LUCARIO, SpeciesId.EEVEE],
  [SpeciesId.EEVEE],
  [SpeciesId.STEELIX, SpeciesId.PICHU],
  [SpeciesId.RATTATA, SpeciesId.RATTATA, SpeciesId.GHOLDENGO, SpeciesId.PIKACHU],
];
const BIOMES = [BiomeId.PLAINS, BiomeId.TOWN, BiomeId.FOREST, BiomeId.SEA, BiomeId.CAVE, BiomeId.VOLCANO, BiomeId.SPACE, BiomeId.LABORATORY,
  BiomeId.GRASS, BiomeId.TALL_GRASS, BiomeId.MEADOW, BiomeId.SWAMP, BiomeId.MOUNTAIN, BiomeId.BEACH, BiomeId.LAKE, BiomeId.JUNGLE,
  BiomeId.GRAVEYARD, BiomeId.DOJO, BiomeId.FAIRY_CAVE, BiomeId.ICE_CAVE, BiomeId.WASTELAND, BiomeId.ABYSS, BiomeId.RUINS, BiomeId.ISLAND, BiomeId.METROPOLIS];
const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "FORM_CHANGE_ITEM": case "RARE_FORM_CHANGE_ITEM": return t.formChangeItem;
    case "EVOLUTION_ITEM": case "RARE_EVOLUTION_ITEM": return t.evolutionItem;
    case "MINT": return t.nature;
    case "MYSTERY_ENCOUNTER_SHUCKLE_JUICE": return t.statModifier;
    /* game-core numbers the species boosters by their item: Light Ball, Thick Club, Metal Powder, Quick Powder, Deep Sea Tooth, Deep Sea Scale */
    case "SPECIES_STAT_BOOSTER": case "RARE_SPECIES_STAT_BOOSTER":
      return ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_TOOTH", "DEEP_SEA_SCALE"].indexOf(t.key);
  }
  return -1;
};
const lines: string[] = [`V 1 ${N}`];
const cover = { options: [0, 0, 0], skipped: 0, fainted_primary: 0, items: 0, high: 0, same_biome: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {

  it(`teleporting hijinks, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(32).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    for (const b of BIOMES) biomes.set(b, [MysteryEncounterType.TELEPORTING_HIJINKS]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.TELEPORTING_HIJINKS, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    scene.time.delayedCall = () => ({}) as any;
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    const expCalls = vi.spyOn(EncounterPhaseUtils, "setEncounterExp").mockImplementation(() => {});
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((async () => {}) as any);
    vi.spyOn(AnimUtils, "playTween").mockImplementation((async () => {}) as any);
    scene.loadBiomeAssets = async () => {};
    scene.clearBiomeAssets = () => {};
    scene.updateBiomeWaveText = () => {};
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
    const startBiome = scene.arena.biomeId;
    for (let i = 0; i < share; i++) {
      const k = i % 3, wave = 10 * (1 + rnd(17)) + 2 + rnd(3), runSeed = word(), waveSeed = word();
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
      const adds: Array<() => void> = [];
      for (const mon of party) {
        if (rnd(3) === 0) adds.push(mk("LEFTOVERS", mon));
        if (rnd(4) === 0) adds.push(mk("BASE_STAT_BOOSTER", mon, rnd(6), 1 + rnd(3)));
        if (rnd(4) === 0) adds.push(mk("BERRY", mon, rnd(11), 1 + rnd(3)));
        if (rnd(5) === 0) adds.push(mk("LUCKY_EGG", mon, undefined, 1 + rnd(4)));
        if (rnd(8) === 0) adds.push(mk("SOOTHE_BELL", mon));
      }
      if (rnd(3) === 0) adds.push(mk("EXP_SHARE", undefined, undefined, 1 + rnd(3)));
      if (rnd(4) === 0) adds.push(mk("SHINY_CHARM", undefined, undefined, 1 + rnd(3)));
      if (rnd(2) === 0) adds.push(mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      scene.pokeballCounts = { 0: rnd(30), 1: rnd(4), 2: rnd(3), 3: rnd(95), 4: rnd(2) };
      for (const mon of party) mon.hp = rnd(4) === 0 ? 0 : 1 + rnd(mon.getMaxHp());
      /* a fight needs one Pokemon that can fight */
      if (!party.some((mon: any) => mon.hp > 0)) party[0].hp = 1 + rnd(party[0].getMaxHp());
      for (const mon of party) {
        const max = mon.getMaxHp();
        mons.push(`${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.hp} ${max} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((mv: any) => mv.moveId).join(" ")} ${mon.luck} ${mon.getAbility().id}`);
      }
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      const price = scene.getWaveMoneyAmount(1.75);
      const roll = rnd(6);
      scene.money = roll === 0 ? Math.max(price - 1 - rnd(40), 0) : roll === 1 ? price : price + rnd(5000);
      const hpAfter = party.map((p: any) => p.hp);
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const before0 = list();
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId} ${scene.money}`, `U ${mons.join(" ; ")}`, `H ${hpAfter.join(" ")}`,
        `B ${scene.pokeballCounts[0]} ${scene.pokeballCounts[1]} ${scene.pokeballCounts[2]} ${scene.pokeballCounts[3]} ${scene.pokeballCounts[4]}`,
        `L ${before0.length} ${before0.join(" ")}`, `A ${meets.join(" ")}`);
      if (!meets[k]) { lines.push("E"); cover.skipped++; continue; }

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`tlp-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      const rngInit = rng();
      lines.push(`R ${rngBefore}`, `I ${rngInit}`, `W ${encounter.misc.price}`);

      const option = encounter.options[k];
      encounter.selectedOption = option;
      const primary = option.primaryPokemon ? original.indexOf(option.primaryPokemon) : -1;
      const biomeBefore = scene.arena.biomeId;
      leave.mockClear(); rewards.mockClear(); expCalls.mockClear(); said.length = 0;
      const queued: string[] = [];
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string) => { queued.push(name); }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      let pre: Promise<any> | undefined;
      if (option.onPreOptionPhase) scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
      await pre;
      const rngPre = rng();
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      vi.mocked(pm.unshiftNew).mockRestore();
      vi.mocked(pm.pushNew).mockRestore();
      const afterOption = rng();
      const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
      const opts = (given?.guaranteedModifierTypeOptions ?? []).map((o: any) => `${o.type.id} ${argOf(o.type)} ${o.type.tier} ${o.upgradeCount ?? 0}`);
      const exps = expCalls.mock.calls.map((c: any) => `${original.findIndex((p: any) => p.id === c[0])} ${c[1]} ${c[2] === undefined ? 1 : c[2] ? 1 : 0}`);
      const arena = scene.arena;
      /* a boss with a weather or terrain ability changes the arena when it is summoned: the port plays that summon, the oracle does not */
      const e0 = scene.getEnemyParty()[0];
      const setter = [e0.getAbility(), e0.getPassiveAbility()].some((a: any) => a.attrs.some((at: any) => /^PostSummon(Weather|Terrain)/.test(at.constructor.name)));
      lines.push(`Q ${rngPre} ${afterOption}`,
        `S ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${(given?.guaranteedModifierTypeFuncs ?? []).length} ${opts.length} ${(given?.guaranteedModifierTiers ?? []).length} ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] ? 1 : 0) : -1}`,
        `O ${opts.length} ${opts.join(" ; ")}`,
        `X2 ${exps.length} ${exps.join(" ; ")}`,
        `M ${said.length} ${said.map(s => s.replace(/\s+/g, "_")).join(" ")}`,
        `T ${primary} ${scene.money}`,
        `G ${arena.biomeId} ${setter ? -1 : arena.weather?.weatherType ?? 0} ${setter ? -1 : arena.terrain?.terrainType ?? 0} ${arena.tags.length} ${arena.playerFaints ?? 0} ${arena.playerTerasUsed ?? 0}`,
        `Y ${original.map((p: any) => `${p.hp} ${p.getMaxHp()} ${p.stats.join(" ")}`).join(" ; ")}`,
        `V ${list().length} ${list().join(" ")}`);
      cover.options[k]++;
      if (k === 1 && primary >= 0 && original[primary].hp <= 0) cover.fainted_primary++;
      if (wave >= 50) cover.high++;
      {
        const battle = scene.currentBattle, e = scene.getEnemyParty()[0];
        expect(scene.getEnemyParty().length).toBe(1);
        const enemyItems = enemyList();
        const fx = encounter.startOfBattleEffects.map((x: any) => `${x.sourceBattlerIndex} ${x.targets.length} ${x.targets.join(" ")} ${x.move.moveId} ${x.useMode}`);
        lines.push(`D ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${queued.length} ${queued.join(" ")}`,
          `X ${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`,
          `N ${e.status?.effect ?? 0} ${e.aiType}`,
          `J ${enemyItems.length} ${enemyItems.join(" ")}`,
          `F ${fx.length} ${fx.join(" ; ")}`);
        /* PostSummonPhase's lapse of the tag: the effect, by hand (option 3's boss has none) */
        const effects: string[] = [];
        let r0 = "", r1 = "";
        if (e.mysteryEncounterBattleEffects) {
          vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, p: any) => { effects.push(`${name} ${p.changes.map((c: any) => `${c.stat} ${c.stages}`).join(" ")}`); }) as any);
          vi.spyOn(Dialogue, "queueEncounterMessage").mockImplementation(((key: string) => { effects.push(`MSG ${key}`); }) as any);
          r0 = rng();
          e.mysteryEncounterBattleEffects(e);
          r1 = rng();
          vi.mocked(pm.unshiftNew).mockRestore();
          vi.mocked(Dialogue.queueEncounterMessage).mockRestore();
          vi.spyOn(Dialogue, "queueEncounterMessage").mockImplementation(((key: string) => { said.push(key); }) as any);
        } else { r0 = r1 = rng(); }
        const mult = e.hasAbility(86) ? 2 : e.hasAbility(126) ? -1 : 1;
        lines.push(`K ${r0} ${r1} ${effects.length} ${effects.map(s => s.replace(/ /g, "_")).join(" ")}`,
          /* StatStageChangePhase multiplies the stages by the holder's Simple (x2, id 86) or Contrary (x-1, id 126), ability or passive */
          `Z ${mult} ${e.bossSegmentIndex}`);
        cover.items += enemyItems.length > 0 ? 1 : 0;
        if (biomeBefore === arena.biomeId) cover.same_biome++;
      }
      lines.push("E");
      party.length = 0; party.push(...original);
      scene.newArena(startBiome);
    }
  }, 1200000);
}

it("writes the teleporting hijinks trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-teleport.txt", lines.join("\n") + "\n");
  console.log(`TELEPORTING HIJINKS ${SEED}: options=${cover.options} skipped=${cover.skipped} fainted primaries=${cover.fainted_primary} waves>=50=${cover.high} fights with items=${cover.items} same biome after=${cover.same_biome}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
});
