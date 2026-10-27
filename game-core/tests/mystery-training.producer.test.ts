/* The Training Session oracle (the harness family of mystery-snorlax.producer.test.ts). The real encounter, its party picker replaced by a
 * plan (selectPokemonForOption is called with the real callbacks and filter: the picked Pokemon, and for the nature and ability options
 * a list item whose handler runs for real). Per case: the party, the dex data of the picked species, what the picker offered (its
 * choices, which of them are new to the dex), then onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500) with
 * initBattleWithEnemyConfig REALLY run (the clone of the picked Pokemon as a boss with its items; the party without it), the rewards set
 * and the onBeforeRewardsPhase callback they carry run by hand (the IV, nature or ability change, the Pokemon and its items back, the
 * dex data). GC_TRN_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { allMoves } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MoveId } from "#enums/move-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import i18next from "i18next";
import { getStatKey } from "#enums/stat";
import * as Dialogue from "#mystery-encounters/encounter-dialogue-utils";
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
const cover = { options: [0, 0, 0, 0], invalid: 0, nochoice: 0, species: new Set<number>(), items: 0, newn: 0, newa: 0, ivs: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {

  it(`training session, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    for (const b of BIOMES) biomes.set(b, [MysteryEncounterType.TRAINING_SESSION]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.TRAINING_SESSION, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(4);
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
      const k = i % 4, wave = 10 + rnd(171), runSeed = word(), waveSeed = word();
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
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        mon.fusionSpecies = null;
        mon.level = 1 + rnd(rnd(2) ? 100 : 20);
        mon.nature = rnd(25);
        if (mon.customPokemonData) mon.customPokemonData.nature = -1;
        mon.ivs = mon.ivs.map(() => rnd(32));
        const n = 1 + rnd(4), ids: number[] = [];
        if (rnd(3) === 0) ids.push(STEALING[rnd(STEALING.length)]);
        while (ids.length < n) {
          const id = 1 + rnd(allMoves.length - 1);
          if (allMoves[id] && !ids.includes(id)) ids.push(id);
        }
        mon.moveset = ids.map(id => new PokemonMove(id));
        for (const mv of mon.moveset) mv.ppUsed = rnd(3) === 0 ? rnd(4) : 0;
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
        mons.push(`${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.hp} ${max} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((mv: any) => mv.moveId).join(" ")} ${mon.luck} ${mon.getAbility().id} ${mon.abilityIndex} ${mon.gender} ${mon.friendship} ${mon.shiny ? 1 : 0} ${mon.variant} ${mon.teraType} ${mon.passive ? 1 : 0} ${mon.moveset.map((mv: any) => mv.ppUsed).join(" ")}`);
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
      const before0 = list();
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId}`, `U ${mons.join(" ; ")}`, `H ${hpAfter.join(" ")}`,
        `B ${scene.pokeballCounts[0]} ${scene.pokeballCounts[1]} ${scene.pokeballCounts[2]} ${scene.pokeballCounts[3]} ${scene.pokeballCounts[4]}`,
        `L ${before0.length} ${before0.join(" ")}`, `A ${meets.join(" ")} -1`);
      if (!meets[k]) { lines.push("E"); continue; }

      /* the dex data of the picked species and its prevolutions: random, so the picker's "new" marks and the updates vary */
      const chainOf = (mon: any): number[] => {
        const out: number[] = [];
        let sid: any = mon.species.speciesId;
        do { out.push(sid); sid = speciesDataRegistry.getPrevolution(sid); } while (sid != null);
        return out;
      };
      const dexLine = (mon: any) => {
        const chain = chainOf(mon), root = mon.species.getRootSpeciesId(true);
        const sd = scene.gameData.starterData[root];
        return `${chain.length} ${chain.map((s: number) => { const d = scene.gameData.dexData[s]; return `${s} ${d.ivs.join(" ")} ${d.natureAttr} ${d.caughtAttr} ${d.seenAttr}`; }).join(" ")} ${root} ${sd.abilityAttr} ${sd.passiveAttr}`;
      };
      const member = rnd(party.length);
      {
        const mon = party[member];
        for (const s of chainOf(mon)) {
          const d = scene.gameData.dexData[s];
          d.ivs = d.ivs.map(() => rnd(32));
          d.natureAttr = rnd(3) === 0 ? 0 : (rnd(1 << 25) << 1) >>> 0;
        }
        scene.gameData.starterData[mon.species.getRootSpeciesId(true)].abilityAttr = rnd(8);
        scene.gameData.starterData[mon.species.getRootSpeciesId(true)].passiveAttr = rnd(8);
      }

      /* the picker, with the real callbacks and filter */
      const sel = { member, valid: 1, n: 0, slots: [] as number[], flags: [] as number[], chosen: -1 };
      vi.spyOn(EncounterPhaseUtils, "selectPokemonForOption").mockImplementation((async (onSelected: any, _no: any, filter: any) => {
        const mon = original[sel.member];
        if (filter?.(mon)) { sel.valid = 0; return false; }
        const items = onSelected(mon);
        if (Array.isArray(items)) {
          sel.n = items.length;
          for (const it of items) {
            it.handler();
            sel.slots.push(encounter.misc.abilityIndex ?? encounter.misc.chosenNature);
            sel.flags.push(String(it.label).startsWith("(+) ") ? 1 : 0);
          }
          sel.chosen = rnd(items.length);
          items[sel.chosen].handler();
          cover.newn += k === 1 ? sel.flags.reduce((a, b) => a + b, 0) : 0;
          cover.newa += k === 2 ? sel.flags.reduce((a, b) => a + b, 0) : 0;
        }
        return true;
      }) as any);

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); rewards.mockClear();
      const living = party.filter((mon: any, j: number) => j !== member && mon.hp > 0).length;
      lines.push(`D ${dexLine(party[member])}`);
      if (k < 3) {
        encounter.misc = undefined;
        const ok = await option.onPreOptionPhase();
        lines.push(`O ${sel.member} ${ok ? 1 : 0} ${living} ${sel.n} ${sel.chosen} ${sel.slots.map((s, j) => `${s} ${sel.flags[j]}`).join(" ")}`);
        vi.mocked(EncounterPhaseUtils.selectPokemonForOption).mockRestore();
        if (!ok) { cover.invalid++; lines.push("E"); continue; }
        if (living === 0) { cover.nochoice++; lines.push("E"); continue; }
      } else {
        vi.mocked(EncounterPhaseUtils.selectPokemonForOption).mockRestore();
        lines.push(`O -1 1 ${living} 0 -1`);
      }

      Phaser.Math.RND.sow([`trn-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      if (k === 3) {
        let done: Promise<boolean> | undefined;
        scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
        await done;
        lines.push(`R ${rngBefore}`, `Q ${rng()}`, `S ${rewards.mock.calls.length} 0 0 ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] ? 1 : 0) : -1}`, "E");
        cover.options[3]++;
        continue;
      }
      const queued: string[] = [];
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string) => { queued.push(name); }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      let done: Promise<boolean> | undefined;
      const pokemon = encounter.misc.playerPokemon;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      vi.mocked(pm.unshiftNew).mockRestore();
      vi.mocked(pm.pushNew).mockRestore();
      const afterOption = rng();
      const battle = scene.currentBattle, e = scene.getEnemyParty()[0];
      expect(scene.getEnemyParty().length).toBe(1);
      const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
      const callback = rewards.mock.calls.length ? rewards.mock.calls[0][2] : undefined;
      const tagNames = e.summonData.tags.map((t: any) => t.tagType).join(",");
      const enemyItems = enemyList();
      lines.push(`R ${rngBefore}`,
        `Q ${afterOption} ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${queued.length} ${queued.join(" ")}`,
        `S ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${callback ? 1 : 0} ${leave.mock.calls.length}`,
        `X ${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => `${m.moveId} ${m.ppUsed}`).join(" ")} ${e.teraType} ${e.luck} ${e.getNature()}`,
        `N ${e.status?.effect ?? 0} ${e.formIndex}`,
        `T ${tagNames.length ? tagNames : "-"}`,
        `J ${enemyItems.length} ${enemyItems.join(" ")}`,
        `W ${party.length} ${party.map((p: any) => original.indexOf(p)).join(" ")}`,
        `V ${list().length} ${list().join(" ")}`);

      /* onBeforeRewardsPhase, as the rewards phase calls it, on a stream of its own */
      Phaser.Math.RND.sow([`trb-${SEED}-${ci}-${i}`]);
      const r0 = rng();
      const msgs: string[] = [];
      vi.spyOn(Dialogue, "queueEncounterMessage").mockImplementation(((key: string) => { msgs.push(key); }) as any);
      scene.executeWithSeedOffset(() => callback(), wave * 1000);
      vi.mocked(Dialogue.queueEncounterMessage).mockRestore();
      const r1 = rng();
      const stat = (tok: string) => { const idx = [0, 1, 2, 3, 4, 5].find(s => i18next.t(getStatKey(s)) === tok); return tok === "-" ? -1 : (idx ?? -2); };
      lines.push(`Y ${r0} ${r1} ${msgs.length} ${msgs.join(" ")} ${stat(encounter.dialogueTokens.stat1 ?? "-")} ${stat(encounter.dialogueTokens.stat2 ?? "-")}`,
        `a ${pokemon.ivs.join(" ")} ${pokemon.nature} ${pokemon.getNature()} ${pokemon.abilityIndex} ${pokemon.getAbility().id} ${pokemon.hp} ${pokemon.getMaxHp()} ${pokemon.stats.join(" ")} ${pokemon.customPokemonData?.nature ?? -1}`,
        `b ${party.length} ${party.map((p: any) => original.indexOf(p)).join(" ")}`,
        `c ${list().length} ${list().join(" ")}`,
        `d ${dexLine(pokemon)}`, "E");
      cover.options[k]++;
      cover.items += enemyItems.length > 0 ? 1 : 0;
      cover.species.add(e.species.speciesId);
      if (k === 0 && msgs.length) cover.ivs++;
      party.length = 0; party.push(...original);
    }
  }, 1200000);
}

it("writes the training session trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-training.txt", lines.join("\n") + "\n");
  console.log(`TRAINING SESSION ${SEED}: options=${cover.options} invalid=${cover.invalid} nochoice=${cover.nochoice} species=${cover.species.size} with items=${cover.items} new natures=${cover.newn} new abilities=${cover.newa}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
});
