import { BattleScene } from "#app/battle-scene";
/* The Global Trade System oracle (card 82's harness, like mystery-salesman.producer.test.ts). The real encounter is started
 * through the game manager; per case the run seed, wave seed, wave, biome, weather, trainer and secret ids, Shiny Charms, money
 * and a fresh random party (species of every kind, forms, levels, fainted ones, passives, held items) are set, then the
 * encounter's own onInit runs again exactly as EncounterPhase runs it (executeWithSeedOffset(.., waveIndex)): the three trade
 * options of every party member are read (Pokemon with every field game-core compares and its moveset), the options'
 * meetsRequirements() are recorded, and the chosen option runs for real: onPreOptionPhase with selectPokemonForOption answering
 * (the Pokemon, the trade option or held item chosen; the selection filter of every Pokemon recorded), then onOptionPhase in
 * executeWithSeedOffset(.., getSeedOffset() * 500) with the trade animation, the dex update (setPokemonCaught), the held-item
 * carry-over and removePokemonFromPlayerParty really run. Nothing is mocked but leaveEncounterWithoutBattle and
 * setEncounterRewards (their arguments are recorded), Math.random (the Poke Ball randInt(4) of the received Pokemon is
 * unseeded upstream: recorded, never compared) and the timed events (switched off: the C port keeps them out).
 * Wonder Trade is run twice from the same RNG state: once to learn the species received (so the dex can be set before), then
 * for real.
 * GC_GTS_N=400 GC_GTS_SEED=777 GC_GTS_OUT=/abs/gts.txt, Node22. */
import { timedEventManager } from "#app/global-event-manager";
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MoveId } from "#enums/move-id";
import { Unlockables } from "#enums/unlockables";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { CIVILIZATION_ENCOUNTER_BIOMES } from "#mystery-encounters/mystery-encounter-biomes";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import i18next from "i18next";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const nativeBattleRng = BattleScene.prototype.randBattleSeedInt;
const N = Number(process.env.GC_GTS_N ?? 400), SEED = Number(process.env.GC_GTS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const BIOMES = [BiomeId.PLAINS, BiomeId.TOWN, BiomeId.FOREST, BiomeId.SEA, BiomeId.CAVE, BiomeId.VOLCANO, BiomeId.SPACE, BiomeId.ISLAND, BiomeId.LABORATORY];
const lines: string[] = [];
const cover = {
  options: [0, 0, 0, 0], disabled: 0, species: new Set<number>(), legendary: 0, regional: 0, forms: 0, shiny: 0, hidden: 0, eggMoves: 0, eggReplaced: 0,
  tiers: new Set<number>(), items: new Set<string>(), nonTransferable: 0, noValidMon: 0, expanded: 0, passive: 0,
};
const ssbKeys = ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_SCALE", "DEEP_SEA_TOOTH"];
const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "SPECIES_STAT_BOOSTER": case "RARE_SPECIES_STAT_BOOSTER": return ssbKeys.indexOf(t.key);
    case "TM_COMMON": case "TM_GREAT": case "TM_ULTRA": return t.moveId;
    case "EVOLUTION_ITEM": case "RARE_EVOLUTION_ITEM": return t.evolutionItem;
    case "FORM_CHANGE_ITEM": case "RARE_FORM_CHANGE_ITEM": return t.formChangeItem;
    case "MINT": return t.nature;
    case "TERA_SHARD": return t.teraType;
    case "MYSTERY_ENCOUNTER_SHUCKLE_JUICE": return t.statModifier;
    case "MYSTERY_ENCOUNTER_OLD_GATEAU": return 0;
  }
  return -1;
};
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

/* the numbers game-core compares of a Pokemon, and its moveset */
const monTokens = (p: any): string =>
  [p.id, p.species.speciesId, p.formIndex, p.level, p.hp, ...p.stats, p.pokeball, p.metWave, p.metLevel, p.metBiome, p.metSpecies, ...p.ivs, p.nature, p.abilityIndex,
    p.gender, +p.shiny, p.variant, p.friendship, p.luck, p.exp, p.teraType, +!!p.passive, +!!p.pokerus].join(" ")
  + ` ${p.moveset.length} ${p.moveset.map((m: any) => m.moveId).join(" ")}`;
const dexTokens = (gameData: any, id: number): string => {
  const d = gameData.dexData[id], sd = gameData.starterData[id];
  return `${id} ${d.caughtAttr} ${d.natureAttr} ${d.caughtCount} ${d.ivs.join(" ")} ${sd?.candyCount ?? -1} ${sd?.abilityAttr ?? -1} ${d.seenAttr} ${d.seenCount}`;
};

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 40).toBe(true);
});

const CHUNK = 100;
const chunks = Math.ceil(N / CHUNK);
for (let ci = 0; ci < chunks; ci++) {
  it(`global trade system, cases ${ci * CHUNK}..`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    CIVILIZATION_ENCOUNTER_BIOMES.forEach(b => biomes.set(b, [MysteryEncounterType.GLOBAL_TRADE_SYSTEM]));
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    await game.runToMysteryEncounter(MysteryEncounterType.GLOBAL_TRADE_SYSTEM, [SpeciesId.LAPRAS, SpeciesId.GENGAR, SpeciesId.ABRA]);
    BattleScene.prototype.randBattleSeedInt = nativeBattleRng;
    vi.spyOn(timedEventManager, "isEventActive").mockReturnValue(false);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party = scene.getPlayerParty(), gameData = scene.gameData;
    expect(encounter.options.length).toBe(4);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    const select = vi.spyOn(EncounterPhaseUtils, "selectPokemonForOption");
    const keys: string[] = [];
    let traderKey = "-";
    const realT = i18next.t.bind(i18next);
    vi.spyOn(i18next, "t").mockImplementation(((k: any, o?: any) => {
      const key = String(k);
      const m = /^mysteryEncounters\/globalTradeSystem:(pokemonTradeSelected|pokemonTradeGoodbye|tradeReceived|itemTradeSelected)$/.exec(key);
      if (m) keys.push(m[1]);
      if (/^trainersCommon:[A-Za-z]+(\.(male|female))?\.\d+$/.test(key)) traderKey = key;
      return realT(k, o);
    }) as any);
    const random = vi.spyOn(Math, "random");
    const all = speciesDataRegistry.getAllSpecies();
    const legends = all.filter((s: any) => s.legendary || s.subLegendary || s.mythical);
    const regionals = all.filter((s: any) => s.speciesId >= 2000);
    const bst = (s: any) => s.getBaseStatTotal();
    const ordinary = all.filter((s: any) => !(s.legendary || s.subLegendary || s.mythical));
    const sorted = [...ordinary].sort((a: any, b: any) => bst(a) - bst(b) || a.speciesId - b.speciesId);
    const extremes = [...sorted.slice(0, 14), ...sorted.slice(-14)];
    const pickSpecies = (): any => {
      const r = rnd(100);
      if (r < 16) return legends[rnd(legends.length)];
      if (r < 26) return regionals[rnd(regionals.length)];
      if (r < 36) return extremes[rnd(extremes.length)];
      return all[rnd(all.length)];
    };
    const baseDex = new Map<number, any>();
    const snapshot = (id: number) => {
      if (!baseDex.has(id)) baseDex.set(id, { dex: structuredClone(gameData.dexData[id]), starter: gameData.starterData[id] ? structuredClone(gameData.starterData[id]) : null });
      const b = baseDex.get(id);
      Object.assign(gameData.dexData[id], structuredClone(b.dex));
      if (b.starter) Object.assign(gameData.starterData[id], structuredClone(b.starter));
    };
    const heldPool: Array<[string, boolean, () => number | undefined]> = [
      ["LEFTOVERS", false, () => undefined], ["SHELL_BELL", false, () => undefined], ["SOUL_DEW", false, () => undefined], ["FOCUS_BAND", false, () => undefined],
      ["QUICK_CLAW", false, () => undefined], ["KINGS_ROCK", false, () => undefined], ["WIDE_LENS", false, () => undefined], ["MULTI_LENS", false, () => undefined],
      ["SCOPE_LENS", false, () => undefined], ["WHITE_HERB", false, () => undefined], ["LUCKY_EGG", false, () => undefined], ["GOLDEN_EGG", false, () => undefined],
      ["GRIP_CLAW", false, () => undefined], ["TOXIC_ORB", false, () => undefined], ["FLAME_ORB", false, () => undefined], ["MINI_BLACK_HOLE", false, () => undefined],
      ["REVIVER_SEED", false, () => undefined], ["EVIOLITE", false, () => undefined], ["BATON", false, () => undefined], ["IV_SCANNER", false, () => undefined],
      ["BERRY", true, () => rnd(11)], ["BASE_STAT_BOOSTER", true, () => rnd(6)], ["ATTACK_TYPE_BOOSTER", true, () => rnd(18)],
      ["SPECIES_STAT_BOOSTER", true, () => rnd(6)], ["RARE_SPECIES_STAT_BOOSTER", true, () => rnd(6)],
      ["FORM_CHANGE_ITEM", true, () => 1 + rnd(40)], ["RARE_FORM_CHANGE_ITEM", true, () => 1 + rnd(40)],
      ["EVOLUTION_TRACKER_GIMMIGHOUL", false, () => undefined], ["MYSTERY_ENCOUNTER_SHUCKLE_JUICE", true, () => (rnd(2) ? 10 : -15)],
      ["MYSTERY_ENCOUNTER_OLD_GATEAU", false, () => undefined], ["MYSTERY_ENCOUNTER_MACHO_BRACE", false, () => undefined],
    ];
    const funcs = (modifierTypes as any);
    const mk = (id: string, mon: any, arg: number | undefined, stack: number): boolean => {
      try {
        const gen = funcs[id];
        if (!gen) return false;
        let type: any;
        if (arg === undefined) type = gen();
        else if (id === "SPECIES_STAT_BOOSTER" || id === "RARE_SPECIES_STAT_BOOSTER") type = EncounterPhaseUtils.generateModifierType(gen, [ssbKeys[arg]]) as any;
        else if (id === "FORM_CHANGE_ITEM" || id === "RARE_FORM_CHANGE_ITEM") type = EncounterPhaseUtils.generateModifierType(gen, [arg]) as any;
        else type = EncounterPhaseUtils.generateModifierType(gen, [arg]) as any;
        if (!type) return false;
        if (!type.id) type.id = id;
        const modifier = type.newModifier(mon);
        if (!modifier) return false;
        modifier.stackCount = Math.max(1, Math.min(stack, modifier.getMaxStackCount()));   // never past the maximum, as the game cannot reach it
        scene.addModifier(modifier, true);
        return true;
      } catch {
        return false;
      }
    };
    const share = Math.min(CHUNK, N - ci * CHUNK);
    for (let i = 0; i < share; i++) {
      const caseNo = ci * CHUNK + i;
      const k = [0, 1, 2, 0, 1, 2, 3][caseNo % 7];
      const wave = 10 + rnd(171), runSeed = word(), waveSeed = word();
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.enemyParty.length = 0;
      scene.arena.biomeId = BIOMES[rnd(BIOMES.length)];
      scene.waveCycleOffset = rnd(40);
      const weather = rnd(2) ? 0 : 1 + rnd(9);
      scene.arena.weather = weather ? ({ weatherType: weather, isImmutable: () => false, turnsLeft: 0, maxDuration: 0, isEffectSuppressed: () => false } as any) : null;
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.doEncounterExp = undefined;
      encounter.doEncounterRewards = undefined;
      encounter.misc = undefined;
      encounter.dialogueTokens = {};
      scene.modifiers.length = 0;
      party.length = 0;
      Phaser.Math.RND.sow([`gts-build-${SEED}-${caseNo}`]);
      const nParty = [1, 2, 2, 3, 3, 3, 4, 4, 5, 6, 6, 6][rnd(12)];
      for (let j = 0; j < nParty; j++) {
        const sp = j > 0 && rnd(8) === 0 ? party[rnd(j)].species : pickSpecies();
        const level = 1 + rnd(rnd(2) ? 100 : 25);
        const mon = scene.addPlayerPokemon(sp, level);
        if (sp.forms.length > 1 && rnd(10) < 4) mon.formIndex = rnd(sp.forms.length);
        mon.fusionSpecies = null;
        mon.nature = rnd(25);
        mon.pokerus = rnd(4) === 0;
        mon.passive = rnd(2) === 0;
        mon.moveset = [];
        const nm = 1 + rnd(4);
        const ids: number[] = [];
        while (ids.length < nm) { const id = 1 + rnd(MoveId.PSYCHIC_NOISE); if (!ids.includes(id)) ids.push(id); }
        mon.moveset = ids.map(id => new PokemonMove(id));
        mon.calculateStats();
        party.push(mon);
      }
      // held items: a few kinds each, stacked now and then; shared items too
      for (const mon of party) {
        const n = rnd(3) === 0 ? 0 : 1 + rnd(4);
        for (let a = 0; a < n; a++) {
          const [id, takesArg, argf] = heldPool[rnd(heldPool.length)];
          mk(id, mon, takesArg ? argf() : undefined, 1 + rnd(rnd(3) === 0 ? 4 : 2));
        }
      }
      const charms = rnd(3) === 0 ? 1 + rnd(4) : 0;
      if (charms) mk("SHINY_CHARM", undefined, undefined, charms);
      if (rnd(2)) mk("ABILITY_CHARM", undefined, undefined, 1 + rnd(3));
      if (rnd(2)) mk("AMULET_COIN", undefined, undefined, 1 + rnd(3));
      if (rnd(3) === 0) mk("EXP_SHARE", undefined, undefined, 1 + rnd(3));
      party.forEach((mon: any) => mon.calculateStats());
      const money = rnd(3) === 0 ? Number.MAX_SAFE_INTEGER - rnd(5000) : 12345 + rnd(100000);
      scene.money = money;
      const balls0 = rnd(3) === 0 ? 90 + rnd(10) : rnd(30);
      scene.pokeballCounts = { 0: balls0, 1: rnd(4), 2: 0, 3: 0, 4: 0 };
      gameData.trainerId = rnd(65536);
      gameData.secretId = rnd(65536);
      for (const mon of party) mon.hp = rnd(6) === 0 ? 0 : 1 + rnd(mon.getMaxHp());
      if (rnd(10) !== 0) party[0].hp = Math.max(1, party[0].hp);
      const ids = party.map((p: any) => p.id);
      const holder = (m: any) => (m.pokemonId == null ? -1 : (ids.indexOf(m.pokemonId) >= 0 ? ids.indexOf(m.pokemonId) : -2));
      const list = () => scene.modifiers.map((m: any) => `${m.type.id} ${argOf(m.type)} ${holder(m)} ${m.stackCount}`);
      const listX = () => scene.modifiers.map((m: any) => `${m.type.id} ${argOf(m.type)} ${holder(m)} ${m.stackCount} ${typeof m.isTransferable === "boolean" ? +m.isTransferable : -1}`);

      // the encounter's onInit, as EncounterPhase runs it
      Phaser.Math.RND.sow([`gts-${SEED}-${caseNo}`]);
      const rngStart = rng();
      scene.executeWithSeedOffset(() => { encounter.onInit(); }, wave);
      const rngInit = rng();
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const mapOf = (mon: any) => encounter.misc.tradeOptionsMap.get(mon.id) as any[];
      const optionLines = party.map((mon: any) => `O ${mapOf(mon).length} ${mapOf(mon).map((e: any) => monTokens(e)).join(" ")}`);
      const levelsLine = party.map((p: any) => `${p.species.speciesId} ${p.formIndex} ${p.level} ${p.hp > 0 ? 1 : 0} ${+!!p.passive} ${p.gender}`);
      const mons = party.map((p: any) => `${p.species.speciesId} ${p.formIndex} ${p.level} ${p.nature} ${p.ivs.join(" ")} ${p.pokerus ? 1 : 0} ${p.getAbility().id} ${p.getPassiveAbility().id} ${p.hasPassive() ? 1 : 0} ${p.hp} ${p.getMaxHp()} ${p.stats.join(" ")} ${p.moveset.length} ${p.moveset.map((mv: any) => mv.moveId).join(" ")}`);
      for (const mon of party) for (const e of mapOf(mon)) cover.species.add(e.species.speciesId);
      for (const mon of party) {
        const s = mon.species;
        cover.legendary += s.legendary || s.subLegendary || s.mythical ? 1 : 0;
        cover.regional += s.speciesId >= 2000 ? 1 : 0;
        cover.forms += mon.formIndex > 0 ? 1 : 0;
        cover.passive += mon.passive ? 1 : 0;
      }
      for (const m of scene.modifiers) { cover.items.add(m.type.id); if (m.isTransferable === false) cover.nonTransferable++; }

      // the Pokemon (and held item) the case picks
      const xferOf = (mon: any) => mon.getHeldItems().filter((m: any) => m.isTransferable);
      let pickMon = rnd(party.length), pickSub = rnd(3);
      let runs = meets[k] === 1;
      if (k === 2) {
        const withItems = party.map((p: any, j: number) => (xferOf(p).length ? j : -1)).filter((j: number) => j >= 0);
        if (withItems.length) { pickMon = withItems[rnd(withItems.length)]; pickSub = rnd(xferOf(party[pickMon]).length); runs = true; } else { runs = false; cover.noValidMon++; }
      }
      let filterFlags = party.map(() => 1);
      if (k === 2) filterFlags = party.map((p: any) => (xferOf(p).length ? 1 : 0));
      if (k === 3) runs = true;
      lines.push(`C ${caseNo} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.money} ${balls0} ${gameData.trainerId} ${gameData.secretId} ${charms} ${scene.arena.biomeId} ${weather} ${party.length} ${runs ? 1 : 0} ${pickMon} ${pickSub} ${scene.waveCycleOffset} ${+gameData.isUnlocked(Unlockables.MINI_BLACK_HOLE)} ${+gameData.isUnlocked(Unlockables.EVIOLITE)}`,
        `U ${mons.join(" ")}`, `P ${levelsLine.join(" ")}`, `L ${list().length} ${listX().join(" ")}`,
        `A ${meets.join(" ")} ${filterFlags.join(" ")} ${scene.getMaxExpLevel()}`, `Z ${rngStart} ${rngInit}`, ...optionLines);
      if (!runs) { cover.disabled++; lines.push("E"); continue; }

      const option = encounter.options[k];
      encounter.selectedOption = option;
      const chosenModifier = k === 2 ? xferOf(party[pickMon])[pickSub] : null;
      let tierSeen = -1;
      if (chosenModifier) {
        const orig = chosenModifier.type.withTierFromPool.bind(chosenModifier.type);
        chosenModifier.type.withTierFromPool = (...a: any[]) => { const r = orig(...a); tierSeen = r.tier ?? -1; return r; };
      }
      select.mockImplementation(((cb: any) => new Promise<boolean>(res => setTimeout(() => {
        const r = cb(party[pickMon]);
        if (Array.isArray(r) && r.length) r[pickSub].handler();
        res(true);
      }, 0))) as any);
      const doPre = async (): Promise<void> => {
        let pre: Promise<boolean> | undefined;
        if (option.onPreOptionPhase) scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
        await pre;
      };
      Phaser.Math.RND.sow([`gts-run-${SEED}-${caseNo}`]);
      const chain: number[] = [];
      if (k === 1) {
        // a dry run from the same RNG state: which species does Wonder Trade hand over?
        const saved = Phaser.Math.RND.state();
        await doPre();
        const sp = encounter.misc.receivedPokemon.species;
        for (let id: number | null = sp.speciesId; id != null; id = speciesDataRegistry.getPrevolution(id)) chain.push(id);
        const root = sp.getRootSpeciesId(true);
        if (!chain.includes(root)) chain.push(root);
        Phaser.Math.RND.state(saved);
        encounter.misc.tradedPokemon = undefined; encounter.misc.receivedPokemon = undefined;
      } else if (k === 0) {
        const sp = mapOf(party[pickMon])[pickSub].species;
        for (let id: number | null = sp.speciesId; id != null; id = speciesDataRegistry.getPrevolution(id)) chain.push(id);
        const root = sp.getRootSpeciesId(true);
        if (!chain.includes(root)) chain.push(root);
      }
      for (const id of chain) {
        snapshot(id);
        const d = gameData.dexData[id], sd = gameData.starterData[id];
        const r = rnd(4);
        d.caughtAttr = r === 0 ? 0n : r === 1 ? 157n : r === 2 ? 3n : d.caughtAttr;
        d.seenAttr = d.caughtAttr ? d.caughtAttr : (rnd(2) ? 157n : 0n);
        d.seenCount = rnd(5);
        d.natureAttr = rnd(3) === 0 ? 0 : 1 << (1 + rnd(24));
        d.caughtCount = rnd(5);
        d.ivs = Array.from({ length: 6 }, () => rnd(32));
        if (sd) { sd.candyCount = rnd(40); sd.abilityAttr = rnd(8); }
      }
      const dexBefore = chain.map(id => dexTokens(gameData, id));
      random.mockReturnValue((rnd(4) + 0.5) / 4);
      keys.length = 0; traderKey = "-";
      leave.mockClear(); rewards.mockClear();
      const before = rng(), money0 = scene.money;
      await doPre();
      const afterPre = rng();
      const received = encounter.misc?.receivedPokemon;
      const receivedTokens = received ? monTokens(received) : "-";
      let done: Promise<void> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      const afterOption = rng();
      expect(leave).toHaveBeenCalledTimes(k === 3 || k < 3 ? 1 : 0);
      const order = party.map((p: any) => ids.indexOf(p.id));
      const fresh = party.filter((p: any) => !ids.includes(p.id));
      const setRewards = rewards.mock.calls[0]?.[0] as any;
      const rewardLine = setRewards
        ? `${setRewards.guaranteedModifierTypeOptions?.length ?? 0} ${(setRewards.guaranteedModifierTypeOptions ?? []).map((o: any) => `${o.type.id} ${argOf(o.type)} ${o.type.tier ?? -1} ${o.upgradeCount}`).join(" ")} ${setRewards.fillRemaining ? 1 : 0}`
        : "-1";
      lines.push(`R ${before}`, `B ${afterPre}`, `Q ${afterOption} ${scene.money} ${leave.mock.calls[0][0] ? 1 : 0} ${money0}`,
        `N ${keys.length} ${keys.join(" ")}`, `T ${traderKey} ${encounter.dialogueTokens.tradeTrainerName ?? "-"}`, `S ${rewardLine}`, `G ${tierSeen}`,
        `W ${order.length} ${order.join(" ")}`, ...fresh.map((p: any) => `M ${monTokens(p)}`),
        `V ${list().length} ${list().join(" ")}`,
        `D ${chain.length} ${chain.map(id => dexTokens(gameData, id)).join(" ")}`, `Y ${dexBefore.join(" ")}`, "E");
      cover.options[k]++;
      if (fresh.length) {
        const n = fresh[0];
        cover.shiny += n.shiny ? 1 : 0;
        cover.hidden += n.abilityIndex === 2 ? 1 : 0;
      }
      if (rewardLine !== "-1") {
        const m = / (\d+) \d+$/.exec(rewardLine.split(" ").slice(0, -1).join(" ") + " 0");
        void m;
        const parts = rewardLine.split(" ");
        if (parts.length > 3) cover.tiers.add(Number(parts[3]));
      }
      void MoveId;
    }
  }, Number(process.env.GC_GTS_TIMEOUT ?? 3600000));
}

it("writes the gts trace", () => {
  lines.unshift(`V 1 ${N}`);
  lines.push(`Z ${N}`);
  fs.writeFileSync(process.env.GC_GTS_OUT ?? "/tmp/gts.txt", lines.join("\n") + "\n");
  console.log(`GTS ${SEED}: options=${cover.options} disabled=${cover.disabled} no mon with an item=${cover.noValidMon} species offered=${cover.species.size} legendary/mythical in party=${cover.legendary} regional=${cover.regional} forms=${cover.forms} passive=${cover.passive} shiny=${cover.shiny} hidden=${cover.hidden} tiers=${[...cover.tiers].sort()} item kinds=${cover.items.size} non-transferable=${cover.nonTransferable}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
});
