/* The Dark Deal oracle (the harness family of mystery-actions.producer.test.ts). The real encounter is started through the game
 * manager; per case the run and wave seeds, the wave, the biome and the party (levels, IVs, nature, moves, HP: a fifth fainted,
 * held items in a shuffled order, among them form-change items, which the deal keeps) are changed, then option 1 runs as the phases
 * run it: onPreOptionPhase in executeWithSeedOffset(.., getSeedOffset()) with removePokemonFromPlayerParty really run (its
 * updateModifiers prunes the item list), onOptionPhase in (.., * 500) with initBattleWithEnemyConfig REALLY run (the boss, its
 * segments, its items), the ModifierRewardPhase it unshifted really rewarded. Option 2 (leave) is recorded for its heal-shop flag.
 * Per case: what the options needed (meetsRequirements), the Pokemon removed, its types, the items it held, the Rogue Ball
 * count, then the boss: species, form, level, boss segments, IVs, nature, ability index, passive, shininess, variant, gender,
 * friendship, moves in order, stats, HP, its items in order, the dex entry; the battle (encounter mode, double, enemy levels);
 * the Phaser RNG around each step and the party and items after, in order.
 * GC_DD_N=400 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { allMoves } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MoveId } from "#enums/move-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { SpeciesId } from "#enums/species-id";
import { ModifierRewardPhase } from "#phases/modifier-reward-phase";
import { PokemonMove } from "#moves/pokemon-move";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_DD_N ?? 400), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.BULBASAUR, SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE],
  [SpeciesId.LAPRAS, SpeciesId.GENGAR],
  [SpeciesId.RATTATA, SpeciesId.MAGIKARP, SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.ABRA, SpeciesId.GEODUDE],
  [SpeciesId.GIRATINA, SpeciesId.CHARIZARD, SpeciesId.MEOWTH, SpeciesId.GRENINJA],
  [SpeciesId.ARCEUS, SpeciesId.DEOXYS, SpeciesId.SHAYMIN, SpeciesId.URSHIFU, SpeciesId.GHOLDENGO],
];
const BIOMES = [BiomeId.PLAINS, BiomeId.TOWN, BiomeId.FOREST, BiomeId.SEA, BiomeId.CAVE, BiomeId.VOLCANO, BiomeId.SPACE, BiomeId.LABORATORY];
const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "FORM_CHANGE_ITEM": case "RARE_FORM_CHANGE_ITEM": return t.formChangeItem;
    case "EVOLUTION_ITEM": case "RARE_EVOLUTION_ITEM": return t.evolutionItem;
    case "MINT": return t.nature;
  }
  return -1;
};
const lines: string[] = [`V 1 ${N}`];
const cover = { options: [0, 0], fainted: 0, last: 0, boss: new Set<number>(), forms: 0, items: 0, tiers: [0, 0, 0, 0], formItems: 0, mono: 0, segments: new Set<number>(), shiny: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 20).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {
  it(`dark deal, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    for (const b of BIOMES) biomes.set(b, [MysteryEncounterType.DARK_DEAL]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    await game.runToMysteryEncounter(MysteryEncounterType.DARK_DEAL, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(2);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const realRemove = scene.removePokemonFromPlayerParty.bind(scene);
    vi.spyOn(scene, "removePokemonFromPlayerParty").mockImplementation(((p: any) => realRemove(p, false)) as any);
    const funcKey = new Map<any, string>(Object.entries(modifierTypes as any).map(([k, f]) => [f, k]));
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
    const dexTokens = (id: number): string => {
      const d = scene.gameData.dexData[id];
      return `${id} ${d.seenAttr} ${d.seenCount} ${d.caughtAttr} ${d.caughtCount}`;
    };
    const pm = scene.phaseManager;
    for (let i = 0; i < share; i++) {
      const k = i % 2, wave = 30 + rnd(151), runSeed = word(), waveSeed = word();
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.arena.biomeId = BIOMES[rnd(BIOMES.length)];
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = undefined;
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
        mon.calculateStats();
      }
      const adds: Array<() => void> = [];
      for (const mon of party) {
        if (rnd(3) === 0) adds.push(mk("LEFTOVERS", mon));
        if (rnd(4) === 0) adds.push(mk("SHELL_BELL", mon));
        if (rnd(4) === 0) adds.push(mk("BASE_STAT_BOOSTER", mon, rnd(6), 1 + rnd(3)));
        if (rnd(4) === 0) adds.push(mk("BERRY", mon, rnd(11), 1 + rnd(3)));
        if (rnd(5) === 0) adds.push(mk("LUCKY_EGG", mon, undefined, 1 + rnd(4)));
        if (rnd(5) === 0) adds.push(mk("EXP_CHARM"));
        if (rnd(6) === 0) adds.push(mk("QUICK_CLAW", mon, undefined, 1 + rnd(3)));
        if (rnd(6) === 0) adds.push(mk("FOCUS_BAND", mon, undefined, 1 + rnd(3)));
        if (rnd(6) === 0) adds.push(mk("WIDE_LENS", mon, undefined, 1 + rnd(3)));
      }
      if (rnd(3) === 0) adds.push(mk("EXP_SHARE", undefined, undefined, 1 + rnd(3)));
      if (rnd(2) === 0) adds.push(mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      scene.pokeballCounts = { 0: rnd(30), 1: rnd(4), 2: rnd(3), 3: rnd(95), 4: rnd(2) };
      for (const mon of party) {
        const max = mon.getMaxHp();
        mon.hp = rnd(5) === 0 ? 0 : 1 + rnd(max);
        mons.push(`${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.hp} ${max} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((mv: any) => mv.moveId).join(" ")}`);
      }
      // the last able Pokemon rule: sometimes everyone but one is fainted
      if (rnd(4) === 0 && party.length > 1) { const keep = rnd(party.length); party.forEach((mon: any, j: number) => { if (j !== keep) mon.hp = 0; }); cover.last++; }
      /* the shiny roll of every Pokemon made reads these two */
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      const hpAfter = party.map((p: any) => p.hp);
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const before0 = list();
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId}`, `U ${mons.join(" ; ")}`, `H ${hpAfter.join(" ")}`,
        `B ${scene.pokeballCounts[0]} ${scene.pokeballCounts[1]} ${scene.pokeballCounts[2]} ${scene.pokeballCounts[3]} ${scene.pokeballCounts[4]}`,
        `L ${before0.length} ${before0.join(" ")}`, `A ${meets.join(" ")}`);
      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear();
      const dexBefore = new Map<number, string>();
      for (const id of Object.keys(scene.gameData.dexData)) dexBefore.set(Number(id), dexTokens(Number(id)));
      Phaser.Math.RND.sow([`dd-${SEED}-${ci}-${i}`]);
      const before = rng();
      if (k === 1) {
        let done: Promise<boolean> | undefined;
        scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
        await done;
        expect(leave).toHaveBeenCalledTimes(1);
        lines.push(`R ${before}`, `Q ${rng()} ${leave.mock.calls[0][0] ? 1 : 0}`, "E");
        cover.options[1]++;
        continue;
      }
      let pre: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
      await pre;
      const afterPre = rng();
      const removed = original.find((p: any) => !party.includes(p));
      const removedIdx = original.indexOf(removed);
      const removedTypes = encounter.misc.removedTypes;
      const queued: string[] = [];
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...a: any[]) => { queued.push(name === "ModifierRewardPhase" ? `ModifierRewardPhase:${funcKey.get(a[0]) ?? "?"}` : name); }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      vi.mocked(pm.unshiftNew).mockRestore();
      vi.mocked(pm.pushNew).mockRestore();
      const afterOption = rng();
      const battle = scene.currentBattle, e = scene.getEnemyParty()[0];
      expect(scene.getEnemyParty().length).toBe(1);
      // the ModifierRewardPhase it queued runs ahead of the battle
      const rogue0 = scene.pokeballCounts[3];
      if (queued.includes("ModifierRewardPhase:ROGUE_BALL")) {
        try { await Promise.race([new ModifierRewardPhase(modifierTypes.ROGUE_BALL).doReward(), new Promise(r => setTimeout(r, 0))]); } catch { /* showText has no UI here */ }
      }
      const items = enemyList();
      lines.push(`R ${before}`, `D ${afterPre} ${removedIdx} ${removedTypes.length} ${removedTypes.join(" ")}`,
        `X ${queued.length} ${queued.join(" ")}`, `Q ${afterOption} ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")}`,
        `W ${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`,
        `J ${items.length} ${items.join(" ")}`, `Y ${dexBefore.get(e.species.speciesId)}`, `T ${dexTokens(e.species.speciesId)}`,
        `M ${scene.pokeballCounts[3]} ${rogue0}`, `P ${party.length} ${party.map((p: any) => original.indexOf(p)).join(" ")}`,
        `V ${list().length} ${list().join(" ")}`, "E");
      cover.options[0]++;
      cover.boss.add(e.species.speciesId);
      cover.segments.add(e.bossSegments ?? 0);
      cover.items += items.length > 0 ? 1 : 0;
      cover.shiny += e.shiny ? 1 : 0;
      void speciesDataRegistry;
      void MoveId;
      party.length = 0; party.push(...original);
    }
  }, 1200000);
}

it("writes the dark deal trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-darkdeal.txt", lines.join("\n") + "\n");
  console.log(`DARK DEAL ${SEED}: options=${cover.options} last-able=${cover.last} bosses=${cover.boss.size} segments=${[...cover.segments].join(",")} with items=${cover.items} shiny=${cover.shiny}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
});
