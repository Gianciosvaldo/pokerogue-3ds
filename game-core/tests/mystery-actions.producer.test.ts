/* Mystery encounter action oracle (card 82's harness; the older gen/dump-mystery-actions.test.ts and
 * tests/mystery_diff_test.c stay as they are). The real encounter is started through the game manager, then for each
 * case the run seed, wave seed and wave are changed and the chosen option's real onOptionPhase is run exactly as
 * MysteryEncounterOptionSelectedPhase runs it (executeWithSeedOffset(.., getSeedOffset() * 500)). Records the
 * RNG before and after the option, money, the party's levels, the items owned, whether the encounter left without
 * a battle, the shop it set up (setEncounterRewards' customModifierSettings) and the options that shop shows
 * (SelectModifierPhase.start's own calls). Families: Department Store Sale, Shady Vitamin Dealer, Lost at Sea, Field Trip, Part Timer, An Offer You Can't Refuse (below).
 * GC_MYS_N=400 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { ModifierPoolType } from "#enums/modifier-pool-type";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { MysteryEncounterRewardsPhase } from "#phases/mystery-encounter-phases";
import { SpeciesId } from "#enums/species-id";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as EncounterDialogueUtils from "#mystery-encounters/encounter-dialogue-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { CIVILIZATION_ENCOUNTER_BIOMES } from "#mystery-encounters/mystery-encounter-biomes";
import { regenerateModifierPoolThresholds } from "#modifiers/modifier-type";
import { EggLapsePhase } from "#phases/egg-lapse-phase";
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { Stat } from "#enums/stat";
import { MoveId } from "#enums/move-id";
import { AbilityId } from "#enums/ability-id";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { PartyExpPhase } from "#phases/party-exp-phase";
import { ModifierRewardPhase } from "#phases/modifier-reward-phase";
import { allMoves } from "#data/data-lists";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import i18next from "i18next";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_MYS_N ?? 400), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const biomeMap = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
CIVILIZATION_ENCOUNTER_BIOMES.forEach(b => biomeMap.set(b, [MysteryEncounterType.DEPARTMENT_STORE_SALE]));
const configs: SpeciesId[][] = [
  [SpeciesId.BULBASAUR], [SpeciesId.LAPRAS, SpeciesId.GENGAR, SpeciesId.ABRA], [SpeciesId.MEW],
  [SpeciesId.RATTATA, SpeciesId.MAGIKARP, SpeciesId.PIKACHU, SpeciesId.EEVEE],
];
const lines = [`V 1 ${N}`];
const cover = { options: [0, 0, 0, 0], keys: new Set<string>(), tiers: new Set<number>() };
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
  }
  return -1;
};
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 40).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {
  it(`department store sale, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomeMap);
    await game.runToMysteryEncounter(MysteryEncounterType.DEPARTMENT_STORE_SALE, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party = scene.getPlayerParty();
    expect(encounter.options.length).toBe(4);
    const funcKey = new Map<any, string>(Object.entries(modifierTypes as any).map(([k, f]) => [f, k]));
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards");
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    lines.push(`P ${party.length} ${party.map((p: any) => `${p.species.speciesId} ${p.formIndex} ${p.level} ${p.moveset.length} ${p.moveset.map((m: any) => m.moveId).join(" ")}`).join(" ")}`);
    const share = Math.floor(N / configs.length);
    for (let i = 0; i < share; i++) {
      const k = i % 4, wave = 10 + rnd(91), runSeed = word(), waveSeed = word();
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.money = 12345 + rnd(1000);
      scene.modifiers.length = 0;
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      const option = encounter.options[k];
      encounter.selectedOption = option;
      rewards.mockClear(); leave.mockClear();
      Phaser.Math.RND.sow([`mys-${SEED}-${ci}-${i}`]);
      const before = rng(), money = scene.money, itemsBefore = scene.modifiers.length;
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      const afterOption = rng();
      expect(leave).toHaveBeenCalledTimes(1);
      expect(rewards).toHaveBeenCalledTimes(1);
      const settings: any = rewards.mock.calls[0][0];
      const funcs: string[] = (settings.guaranteedModifierTypeFuncs ?? []).map((f: any) => funcKey.get(f) ?? "?");
      expect(funcs.includes("?")).toBe(false);
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${money}`, `R ${before}`,
        `Q ${afterOption}`,
        `F ${funcs.length} ${funcs.join(" ")} ${settings.fillRemaining ? 1 : 0} ${settings.rerollMultiplier === undefined ? "nan" : settings.rerollMultiplier} ${settings.allowLuckUpgrades === undefined ? -1 : +settings.allowLuckUpgrades} ${settings.guaranteedModifierTiers?.length ?? 0} ${settings.guaranteedModifierTypeOptions?.length ?? 0}`,
        `M ${scene.money} ${scene.modifiers.length - itemsBefore} ${leave.mock.calls.length}`);
      // SelectModifierPhase.start() for that shop
      const phase: any = new SelectModifierPhase(0, undefined, settings);
      phase.updateSeed();
      regenerateModifierPoolThresholds(scene.getPlayerParty(), ModifierPoolType.PLAYER, 0);
      const count = phase.getModifierCount();
      const opts = phase.getModifierTypeOptions(count);
      lines.push(`O ${count} ${opts.length} ${opts.map((o: any) => `${o.type.id} ${argOf(o.type)} ${o.type.tier ?? -1} ${o.upgradeCount}`).join(" ")}`,
        `S ${rng()}`, "E");
      cover.options[k]++; funcs.forEach(f => cover.keys.add(f)); opts.forEach((o: any) => cover.tiers.add(o.type.tier));
    }
  }, 600000);
}


/* ---- Shady Vitamin Dealer ----
 * Per case: a party with random levels, IVs, natures (some with a Mint's custom nature), vitamins already held (up to
 * the IV cap) and HP around the 0.51 boundary; the wave and the money around both prices. The options' own
 * meetsRequirements() and the party screen filters give what the app may offer; then the chosen option runs the way
 * the phases run it: onPreOptionPhase inside executeWithSeedOffset(.., getSeedOffset()) (the filter and the callback
 * are captured from selectPokemonForOption), the party screen's callback called after it returns (the run's own RNG:
 * traced to run outside any seed offset), onOptionPhase in (.., * 500), then MysteryEncounterRewardsPhase's own
 * doEncounterRewardsAndContinue() (what it pushes: the heal-only shop, the PostMysteryEncounterPhase, and whether
 * any experience phase) and onPostOptionPhase in (.., * 2000). Eggs are lapsed with EggLapsePhase.start(). */
const shadyConfigs: SpeciesId[][] = [
  [SpeciesId.BULBASAUR], [SpeciesId.LAPRAS, SpeciesId.GENGAR, SpeciesId.ABRA], [SpeciesId.MEW, SpeciesId.SHEDINJA],
  [SpeciesId.RATTATA, SpeciesId.MAGIKARP, SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.SNORLAX, SpeciesId.DITTO],
];
const shadyLines: string[] = [];
const shadyCover = { options: [0, 0, 0], picks: 0, noPick: 0, maxStack: 0, cap0: 0, custom: 0, boundary: 0, disabled: 0 };
const statIds = [Stat.HP, Stat.ATK, Stat.DEF, Stat.SPATK, Stat.SPDEF, Stat.SPD];
for (let ci = 0; ci < shadyConfigs.length; ci++) {
  it(`shady vitamin dealer, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    CIVILIZATION_ENCOUNTER_BIOMES.forEach(b => biomes.set(b, [MysteryEncounterType.SHADY_VITAMIN_DEALER]));
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    await game.runToMysteryEncounter(MysteryEncounterType.SHADY_VITAMIN_DEALER, shadyConfigs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    // the party screens' filters, and the callbacks, as selectPokemonForOption receives them
    let captured: { cb: (p: any) => void; filter: (p: any) => string | null } | undefined;
    vi.spyOn(EncounterPhaseUtils, "selectPokemonForOption").mockImplementation(((cb: any, _no: any, filter: any) => {
      captured = { cb, filter };
      return Promise.resolve(true);
    }) as any);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const keys: string[] = [];
    const realT = i18next.t.bind(i18next);
    vi.spyOn(i18next, "t").mockImplementation(((k: any, o?: any) => {
      const key = String(k);
      if (/paidMoney|cheapSideEffects|noBadEffects/.test(key)) keys.push(key.replace(/^.*[:]/, "") + (o?.amount !== undefined ? `:${o.amount}` : ""));
      return realT(k, o);
    }) as any);
    const filters: Array<(p: any) => string | null> = [];
    for (const k of [0, 1]) {
      captured = undefined;
      encounter.selectedOption = encounter.options[k];
      scene.executeWithSeedOffset(() => { void encounter.options[k].onPreOptionPhase(); }, encounter.getSeedOffset());
      expect(captured).toBeDefined();
      filters.push(captured!.filter);
    }
    shadyLines.push(`P ${party.length} ${party.map((p: any) => `${p.species.speciesId} ${p.formIndex} ${p.level} ${p.moveset.length} ${p.moveset.map((m: any) => m.moveId).join(" ")}`).join(" ")}`);
    const state = (mon: any) => {
      const stacks = statIds.map(st => {
        const m = scene.modifiers.find((x: any) => x.type?.id === "BASE_STAT_BOOSTER" && x.pokemonId === mon.id && x.stat === st);
        return m ? m.stackCount : 0;
      });
      return `${mon.hp} ${mon.getMaxHp()} ${mon.stats.join(" ")} ${mon.getNature()} ${mon.customPokemonData.nature} ${stacks.join(" ")}`;
    };
    const share = Math.floor(N / shadyConfigs.length);
    const noRound = [0, 0.5, 0.504, 0.505, 0.5049, 0.509, 0.51, 0.514, 0.515, 0.52, 0.8, 1];
    for (let i = 0; i < share; i++) {
      const k = i % 3, wave = 10 + rnd(171), runSeed = word(), waveSeed = word();
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = undefined;
      encounter.doEncounterExp = undefined;
      scene.modifiers.length = 0;
      const cheap = scene.getWaveMoneyAmount(1.5), dear = scene.getWaveMoneyAmount(5);
      const moneys = [cheap - 1, cheap, cheap + 1, dear - 1, dear, dear + 1, 50 * dear, 12345];
      scene.money = Math.max(0, moneys[rnd(moneys.length)]);
      const mons: string[] = [];
      for (const mon of party) {
        mon.level = 5 + rnd(rnd(2) ? 90 : 20);
        mon.nature = rnd(25);
        mon.customPokemonData.nature = rnd(5) === 0 ? rnd(25) : -1;
        mon.ivs = mon.ivs.map(() => (rnd(5) === 0 ? 0 : rnd(5) === 0 ? 31 : rnd(32)));
        statIds.forEach(st => {
          if (rnd(4) !== 0 || mon.ivs[st] === 0) return;
          const stack = rnd(3) === 0 ? mon.ivs[st] : 1 + rnd(mon.ivs[st]);
          const type = EncounterPhaseUtils.generateModifierType(modifierTypes.BASE_STAT_BOOSTER, [st]) as any;
          const modifier = type.newModifier(mon);
          modifier.stackCount = stack;
          scene.addModifier(modifier, true);
        });
      }
      party.forEach((mon: any) => mon.calculateStats());
      for (const mon of party) {
        const max = mon.getMaxHp(), ratio = rnd(4) === 0 ? noRound[rnd(noRound.length)] : rnd(101) / 100;
        mon.hp = Math.min(max, Math.max(0, Math.round(ratio * max) + (rnd(3) - 1)));
        shadyCover.boundary += mon.getHpRatio() >= 0.5 && mon.getHpRatio() <= 0.52 ? 1 : 0;
        mons.push(`${mon.level} ${mon.nature} ${mon.customPokemonData.nature} ${mon.getAbility(true).id} ${mon.hasPassive() ? mon.getPassiveAbility().id : 0} ${mon.ivs.join(" ")} ${statIds.map(st => scene.modifiers.find((x: any) => x.type?.id === "BASE_STAT_BOOSTER" && x.pokemonId === mon.id && x.stat === st)?.stackCount ?? 0).join(" ")} ${mon.hp} ${mon.getMaxHp()} ${mon.stats.join(" ")}`);
        shadyCover.custom += mon.customPokemonData.nature >= 0 ? 1 : 0;
      }
      const eggs = Array.from({ length: rnd(3) }, () => 2 + rnd(40));
      scene.gameData.eggs = eggs.map(w => ({ hatchWaves: w }));
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const masks = [0, 1].map(k2 => party.reduce((m: number, mon: any, idx: number) => m | (filters[k2](mon) === null ? 1 << idx : 0), 0));
      const valid = k < 2 ? party.map((_: any, idx: number) => idx).filter((idx: number) => (masks[k] >> idx) & 1) : [];
      const runs = meets[k] === 1 && (k === 2 || valid.length > 0);
      const pick = runs && k < 2 ? valid[rnd(valid.length)] : -1;
      shadyLines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.money} ${pick} ${runs ? 1 : 0}`,
        `U ${mons.join(" ")}`, `G ${eggs.length} ${eggs.join(" ")}`, `A ${meets.join(" ")} ${masks.join(" ")}`);
      shadyCover.options[k] += runs ? 1 : 0;
      shadyCover.disabled += runs ? 0 : 1;
      if (!runs) { shadyCover.noPick += k < 2 ? 1 : 0; shadyLines.push("E"); continue; }

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); keys.length = 0;
      Phaser.Math.RND.sow([`shady-${SEED}-${ci}-${i}`]);
      const before = rng();
      let afterPre = before, afterPick = before, moneyPick = scene.money, stats = "-";
      if (option.onPreOptionPhase) {
        captured = undefined;
        let pre: Promise<boolean> | undefined;
        scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
        await pre;
        afterPre = rng();
        // the party screen's callback: outside any seed offset
        captured!.cb(party[pick]);
        afterPick = rng();
        moneyPick = scene.money;
        stats = encounter.misc.modifiers.map((m: any) => m.stat).join(" ");
      }
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      const afterOption = rng();
      expect(leave).toHaveBeenCalledTimes(1);
      const owned = scene.modifiers.filter((m: any) => m.type?.id === "BASE_STAT_BOOSTER").length;
      const afterOptionState = party.map(state).join(" ");
      // MysteryEncounterRewardsPhase.doEncounterRewardsAndContinue() with what it would queue recorded
      const pm = scene.phaseManager, queued: string[] = [];
      let shop = "-";
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...a: any[]) => {
        queued.push(name);
        if (name === "SelectModifierPhase") shop = `${a[0]} ${a[1] === undefined ? "u" : a[1]} ${a[2]?.fillRemaining ? 1 : 0} ${a[2]?.rerollMultiplier ?? "nan"}`;
      }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      vi.spyOn(pm, "removeAllPhasesOfType").mockImplementation((() => {}) as any);
      const rewards = new MysteryEncounterRewardsPhase(leave.mock.calls[0][0]);
      vi.spyOn(rewards, "end").mockImplementation(() => {});
      rewards.doEncounterRewardsAndContinue();
      vi.mocked(pm.unshiftNew).mockRestore(); vi.mocked(pm.pushNew).mockRestore(); vi.mocked(pm.removeAllPhasesOfType).mockRestore();
      // EggLapsePhase.start(): nothing hatches (every egg has more than 1 wave left), so the wave counters go down
      const lapse = new EggLapsePhase();
      vi.spyOn(lapse, "end").mockImplementation(() => {});
      lapse.start();
      // PostMysteryEncounterPhase: onPostOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 2000)
      let post = "-";
      if (option.onPostOptionPhase) {
        let pdone: Promise<boolean> | undefined;
        scene.executeWithSeedOffset(() => { pdone = option.onPostOptionPhase(); }, encounter.getSeedOffset() * 2000);
        await pdone;
        post = party.map(state).join(" ");
      }
      const afterPost = rng();
      // doEncounterExp set by the post phase is never called afterwards: the phases queued, if it were
      const expSet = encounter.doEncounterExp ? 1 : 0;
      shadyLines.push(`R ${before}`, `B ${afterPre}`, `Y ${afterPick} ${moneyPick} ${stats}`, `Q ${afterOption} ${scene.money} ${owned} ${leave.mock.calls[0][0] ? 1 : 0}`,
        `W ${afterOptionState}`, `X ${queued.length} ${queued.join(" ")} ${shop}`, `T ${afterPost} ${expSet}`,
        `V ${post}`, `N ${keys.length} ${keys.join(" ")}`, `H ${scene.gameData.eggs.map((e: any) => e.hatchWaves).join(" ") || "-"}`, "E");
      shadyCover.picks += k < 2 ? 1 : 0;
    }
  }, 1200000);
}

it("writes the trace", () => {
  for (const n of cover.options) expect(n).toBeGreaterThan(0);
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync(process.env.GC_MYS_OUT ?? "/tmp/mystery.txt", lines.join("\n") + "\n");
  console.log(`MYSTERY ${SEED}: ${Math.floor(N / configs.length) * configs.length} cases options=${cover.options} keys=${[...cover.keys].sort()} tiers=${[...cover.tiers].sort()}`);
});

it("writes the shady trace", () => {
  shadyLines.unshift(`V 1 ${N}`);
  shadyLines.push(`Z ${Math.floor(N / shadyConfigs.length) * shadyConfigs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-shady.txt", shadyLines.join("\n") + "\n");
  console.log(`SHADY ${SEED}: options run=${shadyCover.options} picks=${shadyCover.picks} disabled-or-no-pick=${shadyCover.disabled} hp-ratio-boundary mons=${shadyCover.boundary} custom-nature mons=${shadyCover.custom}`);
  expect(shadyCover.options.every(n => n > 0)).toBe(true);
});


/* ---- Lost at Sea ----
 * Per case: a party with random levels (some at the exact level a Surf / Fly level move is learned), HP (some fainted),
 * Pokerus, some fused with a Pokemon of another species, and the experience items (Exp. Share, Exp. Balance, Oval
 * Charm, Lucky / Golden Egg); the wave and the eggs. Records which Pokemon the options' CanLearnMoveRequirement lets
 * through (queryParty over the whole party) with each option's meetsRequirements() and the primary Pokemon it picks,
 * then runs the chosen option's real onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500), and the rewards
 * phase's own doEncounterRewardsAndContinue(): doEncounterExp's PartyExpPhase is really started with the phases it
 * would unshift recorded (an ExpPhase or ShowPartyExpBarPhase, party index, experience), then EggLapsePhase.start(). */
const lostConfigs: SpeciesId[][] = [
  [SpeciesId.SQUIRTLE, SpeciesId.PIDGEOT, SpeciesId.DITTO, SpeciesId.GENGAR, SpeciesId.SNORLAX, SpeciesId.ABRA],
  [SpeciesId.LAPRAS, SpeciesId.CHARIZARD, SpeciesId.MEW],
  [SpeciesId.RATTATA, SpeciesId.DITTO, SpeciesId.SHEDINJA],
  [SpeciesId.GLISCOR, SpeciesId.GHOLDENGO, SpeciesId.MIRAIDON, SpeciesId.MAGIKARP, SpeciesId.ZUBAT, SpeciesId.DRAGONITE],
  [SpeciesId.BULBASAUR],
];
const fusionPool = [SpeciesId.LAPRAS, SpeciesId.PIDGEOT, SpeciesId.GYARADOS, SpeciesId.MAGIKARP, SpeciesId.DITTO, SpeciesId.RATTATA, SpeciesId.SQUIRTLE];
const lostLines: string[] = [];
const lostCover = { options: [0, 0, 0], disabled: 0, faintedPrimary: 0, levelOnly: 0, tmOnly: 0, eggOnly: 0, fusionTmOnly: 0, shareRuns: 0, balanceRuns: 0, expMons: 0, shown: 0 };
const learnLevels = new Map<string, number[]>();
for (let ci = 0; ci < lostConfigs.length; ci++) {
  it(`lost at sea, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    CIVILIZATION_ENCOUNTER_BIOMES.forEach(b => biomes.set(b, [MysteryEncounterType.LOST_AT_SEA]));
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    await game.runToMysteryEncounter(MysteryEncounterType.LOST_AT_SEA, lostConfigs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    lostLines.push(`P ${party.length} ${party.map((p: any) => `${p.species.speciesId} ${p.formIndex}`).join(" ")}`);
    const moves = [MoveId.SURF, MoveId.FLY];
    // the levels at which a species learns Surf / Fly by level (the requirement's level moves are the ones learned at the current level)
    const levelsFor = (mon: any): number[] => {
      const key = `${mon.species.speciesId}-${mon.fusionSpecies?.speciesId ?? 0}`;
      if (!learnLevels.has(key)) {
        const out: number[] = [], saved = mon.level;
        for (let l = 1; l <= 100; l++) {
          mon.level = l;
          if (mon.getLevelMoves().some((lm: any) => moves.includes(lm[1]))) out.push(l);
        }
        mon.level = saved;
        learnLevels.set(key, out);
      }
      return learnLevels.get(key)!;
    };
    const share = Math.floor(N / lostConfigs.length);
    for (let i = 0; i < share; i++) {
      const k = i % 3, wave = 10 + rnd(171), runSeed = word(), waveSeed = word();
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = undefined;
      encounter.doEncounterExp = undefined;
      scene.modifiers.length = 0;
      scene.money = 12345;
      const mons: string[] = [];
      for (const mon of party) {
        mon.fusionSpecies = null;
        mon.fusionFormIndex = 0;
        if (rnd(3) === 0) { mon.fusionSpecies = speciesDataRegistry.getSpecies(fusionPool[rnd(fusionPool.length)]); }
        mon.level = 5 + rnd(rnd(2) ? 90 : 20);
        const exact = levelsFor(mon);
        if (exact.length && rnd(2) === 0) mon.level = exact[rnd(exact.length)];
        mon.nature = rnd(25);
        mon.ivs = mon.ivs.map(() => rnd(32));
        mon.pokerus = rnd(4) === 0;
      }
      party.forEach((mon: any) => mon.calculateStats());
      const exps = { share: 0, balance: 0, oval: 0 };
      const addItem = (id: string, stack: number, mon?: any) => {
        const type = (modifierTypes as any)[id]();
        const modifier = type.newModifier(mon);
        modifier.stackCount = stack;
        scene.addModifier(modifier, true);
      };
      if (rnd(3) === 0) { exps.share = 1 + rnd(3); addItem("EXP_SHARE", exps.share); }
      if (rnd(4) === 0) { exps.balance = 1 + rnd(4); addItem("EXP_BALANCE", exps.balance); }
      if (rnd(3) === 0) { exps.oval = 1 + rnd(2); addItem("OVAL_CHARM", exps.oval); }
      const eggItems: string[] = [];
      for (let m = 0; m < party.length; m++) {
        if (rnd(4) === 0) { const st = 1 + rnd(4); addItem("LUCKY_EGG", st, party[m]); eggItems.push(`${m} 40 ${st}`); }
        if (rnd(6) === 0) { const st = 1 + rnd(3); addItem("GOLDEN_EGG", st, party[m]); eggItems.push(`${m} 100 ${st}`); }
      }
      for (const mon of party) {
        const max = mon.getMaxHp();
        mon.hp = rnd(5) === 0 ? 0 : 1 + rnd(max);
        mons.push(`${mon.species.speciesId} ${mon.formIndex} ${mon.fusionSpecies?.speciesId ?? 0} ${mon.fusionFormIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.pokerus ? 1 : 0} ${mon.getAbility(true).id} ${mon.hasPassive() ? mon.getPassiveAbility().id : 0} ${mon.hp} ${max} ${mon.stats.join(" ")}`);
      }
      const eggs = Array.from({ length: rnd(3) }, () => 2 + rnd(40));
      scene.gameData.eggs = eggs.map(w => ({ hatchWaves: w }));
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const masks = [0, 1].map(k2 => encounter.options[k2].primaryPokemonRequirements[0].queryParty(party).reduce((m: number, mon: any) => m | (1 << party.indexOf(mon)), 0));
      const primaries = [0, 1].map(k2 => (meets[k2] ? party.indexOf(encounter.options[k2].primaryPokemon) : -1));
      lostLines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${meets[k]}`, `U ${mons.join(" ")}`,
        `I ${exps.share} ${exps.balance} ${exps.oval} ${eggItems.length} ${eggItems.join(" ")}`, `G ${eggs.length} ${eggs.join(" ")}`,
        `A ${meets.join(" ")} ${masks.join(" ")} ${primaries.join(" ")} ${scene.getMaxExpLevel()}`);
      if (!meets[k]) { lostCover.disabled++; lostLines.push("E"); continue; }
      lostCover.options[k]++;
      if (k < 2) {
        const primary = party[primaries[k]];
        lostCover.faintedPrimary += primary.hp <= 0 ? 1 : 0;
        const why = {
          level: primary.getLevelMoves().some((lm: any) => lm[1] === moves[k]),
          tm: primary.species.getTms(primary.getFormKey()).includes(moves[k]),
          egg: !!primary.getEggMoves()?.includes(moves[k]),
          fusion: !!primary.fusionSpecies?.getTms(primary.getFusionFormKey() ?? undefined).includes(moves[k]),
        };
        lostCover.levelOnly += why.level && !why.tm && !why.egg && !why.fusion ? 1 : 0;
        lostCover.tmOnly += !why.level && why.tm && !why.egg && !why.fusion ? 1 : 0;
        lostCover.eggOnly += !why.level && !why.tm && why.egg && !why.fusion ? 1 : 0;
        lostCover.fusionTmOnly += !why.level && !why.tm && !why.egg && why.fusion ? 1 : 0;
        lostCover.shareRuns += exps.share ? 1 : 0;
        lostCover.balanceRuns += exps.balance ? 1 : 0;
      }
      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear();
      Phaser.Math.RND.sow([`lost-${SEED}-${ci}-${i}`]);
      const before = rng();
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      const afterOption = rng();
      expect(leave).toHaveBeenCalledTimes(1);
      const hpAfter = party.map((p: any) => p.hp);
      // MysteryEncounterRewardsPhase.doEncounterRewardsAndContinue(), the PartyExpPhase it queues really started
      const pm = scene.phaseManager, queued: string[] = [], expPhases: string[] = [];
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...a: any[]) => {
        queued.push(name);
        if (name === "PartyExpPhase") {
          const phase = new PartyExpPhase(a[0], a[1], a[2]);
          vi.spyOn(phase, "end").mockImplementation(() => {});
          phase.start();
        }
      }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      vi.spyOn(pm, "removeAllPhasesOfType").mockImplementation((() => {}) as any);
      vi.spyOn(pm, "create").mockImplementation(((name: string, index: number, exp: number) => ({ phaseName: name, index, exp })) as any);
      vi.spyOn(pm, "unshiftPhase").mockImplementation(((ph: any) => { expPhases.push(`${ph.phaseName === "ExpPhase" ? 0 : 1} ${ph.index} ${ph.exp}`); }) as any);
      const rewards = new MysteryEncounterRewardsPhase(leave.mock.calls[0][0]);
      vi.spyOn(rewards, "end").mockImplementation(() => {});
      rewards.doEncounterRewardsAndContinue();
      for (const fn of [pm.unshiftNew, pm.pushNew, pm.removeAllPhasesOfType, pm.create, pm.unshiftPhase]) vi.mocked(fn).mockRestore();
      const lapse = new EggLapsePhase();
      vi.spyOn(lapse, "end").mockImplementation(() => {});
      lapse.start();
      lostCover.expMons += expPhases.length;
      lostLines.push(`R ${before}`, `Q ${afterOption} ${scene.money} ${leave.mock.calls[0][0] ? 1 : 0}`, `W ${hpAfter.join(" ")}`,
        `X ${queued.length} ${queued.join(" ")}`, `F ${expPhases.length} ${expPhases.join(" ")}`,
        `H ${scene.gameData.eggs.map((e: any) => e.hatchWaves).join(" ") || "-"}`, "E");
    }
  }, 1200000);
}

it("writes the lost at sea trace", () => {
  lostLines.unshift(`V 1 ${N}`);
  lostLines.push(`Z ${Math.floor(N / lostConfigs.length) * lostConfigs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-lost.txt", lostLines.join("\n") + "\n");
  console.log(`LOST ${SEED}: options run=${lostCover.options} disabled=${lostCover.disabled} fainted-primary=${lostCover.faintedPrimary} level-only=${lostCover.levelOnly} tm-only=${lostCover.tmOnly} egg-only=${lostCover.eggOnly} fusion-tm-only=${lostCover.fusionTmOnly} exp-share runs=${lostCover.shareRuns} exp-balance runs=${lostCover.balanceRuns} exp phases=${lostCover.expMons}`);
  expect(lostCover.options.every(n => n > 0)).toBe(true);
});


/* ---- Field Trip ----
 * Per case: a party with random levels, HP (a fifth fainted, and any of them can be picked: the party screen has no
 * filter), Pokerus, the experience items and 1-4 random moves each; the option (physical / special / status), the Pokemon
 * and the move picked. Runs the option as the phases do: onPreOptionPhase in executeWithSeedOffset(.., getSeedOffset())
 * with selectPokemonForOption captured, the party screen's callback with the Pokemon, the handler of the move picked
 * (pokemonAndMoveChosen: sets doEncounterExp and encounter.misc), onOptionPhase in (.., * 500) (setEncounterRewards for a
 * correct move), then MysteryEncounterRewardsPhase's own doEncounterRewardsAndContinue() with the PartyExpPhase really
 * started (the experience phases it unshifts) and the phases it queues in order, the shop opened with its own
 * SelectModifierPhase calls, and EggLapsePhase.start(). */
const tripConfigs: SpeciesId[][] = [
  [SpeciesId.SQUIRTLE, SpeciesId.PIDGEOT, SpeciesId.DITTO, SpeciesId.GENGAR, SpeciesId.SNORLAX, SpeciesId.ABRA],
  [SpeciesId.LAPRAS, SpeciesId.CHARIZARD, SpeciesId.MEW],
  [SpeciesId.RATTATA, SpeciesId.DITTO, SpeciesId.SHEDINJA, SpeciesId.MAGIKARP],
  [SpeciesId.BULBASAUR],
];
const tripLines: string[] = [];
const tripCover = { options: [0, 0, 0], correct: 0, wrong: 0, fainted: 0, shops: [0, 0, 0], expPhases: 0, oval: 0, shown: 0 };
for (let ci = 0; ci < tripConfigs.length; ci++) {
  it(`field trip, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    CIVILIZATION_ENCOUNTER_BIOMES.forEach(b => biomes.set(b, [MysteryEncounterType.FIELD_TRIP]));
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    await game.runToMysteryEncounter(MysteryEncounterType.FIELD_TRIP, tripConfigs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    let captured: { cb: (p: any) => any } | undefined;
    vi.spyOn(EncounterPhaseUtils, "selectPokemonForOption").mockImplementation(((cb: any) => {
      captured = { cb };
      return Promise.resolve(true);
    }) as any);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    tripLines.push(`P ${party.length} ${party.map((p: any) => `${p.species.speciesId} ${p.formIndex}`).join(" ")}`);
    const share = Math.floor(N / tripConfigs.length);
    const addItem = (id: string, stack: number, mon?: any) => {
      const type = (modifierTypes as any)[id]();
      const modifier = type.newModifier(mon);
      modifier.stackCount = stack;
      scene.addModifier(modifier, true);
    };
    for (let i = 0; i < share; i++) {
      const k = i % 3, wave = 10 + rnd(91), runSeed = word(), waveSeed = word();
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = undefined;
      encounter.doEncounterExp = undefined;
      encounter.doEncounterRewards = undefined;
      scene.modifiers.length = 0;
      scene.money = 12345;
      const mons: string[] = [];
      for (const mon of party) {
        mon.fusionSpecies = null;
        mon.level = 5 + rnd(rnd(2) ? 90 : 20);
        mon.nature = rnd(25);
        mon.ivs = mon.ivs.map(() => rnd(32));
        mon.pokerus = rnd(4) === 0;
        const n = 1 + rnd(4), ids: number[] = [];
        while (ids.length < n) {
          const id = 1 + rnd(allMoves.length - 1);
          if (allMoves[id] && !ids.includes(id)) ids.push(id);
        }
        mon.moveset = ids.map(id => new PokemonMove(id));
      }
      party.forEach((mon: any) => mon.calculateStats());
      const exps = { share: 0, balance: 0, oval: 0 };
      if (rnd(3) === 0) { exps.share = 1 + rnd(3); addItem("EXP_SHARE", exps.share); }
      if (rnd(4) === 0) { exps.balance = 1 + rnd(4); addItem("EXP_BALANCE", exps.balance); }
      if (rnd(2) === 0) { exps.oval = 1 + rnd(2); addItem("OVAL_CHARM", exps.oval); }
      const eggItems: string[] = [];
      for (let m = 0; m < party.length; m++) {
        if (rnd(4) === 0) { const st = 1 + rnd(4); addItem("LUCKY_EGG", st, party[m]); eggItems.push(`${m} 40 ${st}`); }
        if (rnd(6) === 0) { const st = 1 + rnd(3); addItem("GOLDEN_EGG", st, party[m]); eggItems.push(`${m} 100 ${st}`); }
      }
      for (const mon of party) {
        const max = mon.getMaxHp();
        mon.hp = rnd(5) === 0 ? 0 : 1 + rnd(max);
        mons.push(`${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.pokerus ? 1 : 0} ${mon.getAbility(true).id} ${mon.hasPassive() ? mon.getPassiveAbility().id : 0} ${mon.hp} ${max} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((m: any) => m.moveId).join(" ")}`);
      }
      const eggs = Array.from({ length: rnd(3) }, () => 2 + rnd(40));
      scene.gameData.eggs = eggs.map(w => ({ hatchWaves: w }));
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      expect(meets).toEqual([1, 1, 1]);
      const pick = rnd(party.length), slot = rnd(party[pick].moveset.length);
      tripLines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${pick} ${slot}`, `U ${mons.join(" ")}`,
        `I ${exps.share} ${exps.balance} ${exps.oval} ${eggItems.length} ${eggItems.join(" ")}`, `G ${eggs.length} ${eggs.join(" ")}`,
        `A ${scene.getMaxExpLevel()}`);
      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear();
      Phaser.Math.RND.sow([`trip-${SEED}-${ci}-${i}`]);
      const before = rng();
      captured = undefined;
      let pre: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
      await pre;
      const afterPre = rng();
      expect(captured).toBeDefined();
      const choices = captured!.cb(party[pick]);
      expect(choices.length).toBe(party[pick].moveset.length);
      choices[slot].handler();
      const afterPick = rng();
      const correct = encounter.misc.correctMove ? 1 : 0;
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      const afterOption = rng();
      expect(leave).toHaveBeenCalledTimes(1);
      const pm = scene.phaseManager, queued: string[] = [], expPhases: string[] = [];
      let shopSettings: any, shopArgs = "-";
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...a: any[]) => {
        queued.push(name);
        if (name === "PartyExpPhase") {
          const phase = new PartyExpPhase(a[0], a[1], a[2]);
          vi.spyOn(phase, "end").mockImplementation(() => {});
          phase.start();
        } else if (name === "SelectModifierPhase") {
          shopSettings = a[2];
          shopArgs = `${a[0]} ${a[1] === undefined ? "u" : a[1]} ${a[2]?.fillRemaining ? 1 : 0} ${a[2]?.rerollMultiplier ?? "nan"} ${a[2]?.guaranteedModifierTypeOptions?.length ?? 0} ${a[2]?.allowLuckUpgrades === undefined ? -1 : +a[2].allowLuckUpgrades}`;
        }
      }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      vi.spyOn(pm, "removeAllPhasesOfType").mockImplementation((() => {}) as any);
      vi.spyOn(pm, "create").mockImplementation(((name: string, index: number, exp: number) => ({ phaseName: name, index, exp })) as any);
      vi.spyOn(pm, "unshiftPhase").mockImplementation(((ph: any) => { expPhases.push(`${ph.phaseName === "ExpPhase" ? 0 : 1} ${ph.index} ${ph.exp}`); }) as any);
      const rewards = new MysteryEncounterRewardsPhase(leave.mock.calls[0][0]);
      vi.spyOn(rewards, "end").mockImplementation(() => {});
      rewards.doEncounterRewardsAndContinue();
      for (const fn of [pm.unshiftNew, pm.pushNew, pm.removeAllPhasesOfType, pm.create, pm.unshiftPhase]) vi.mocked(fn).mockRestore();
      // the shop SelectModifierPhase.start() would open (the heal-only one shows no items)
      let shop = "0 0";
      let afterShop = rng();
      if (shopSettings) {
        const phase: any = new SelectModifierPhase(0, undefined, shopSettings);
        phase.updateSeed();
        regenerateModifierPoolThresholds(scene.getPlayerParty(), ModifierPoolType.PLAYER, 0);
        const count = phase.getModifierCount();
        const opts = phase.getModifierTypeOptions(count);
        shop = `${count} ${opts.length} ${opts.map((o: any) => `${o.type.id} ${argOf(o.type)} ${o.type.tier ?? -1} ${o.upgradeCount}`).join(" ")}`;
        afterShop = rng();
      }
      const lapse = new EggLapsePhase();
      vi.spyOn(lapse, "end").mockImplementation(() => {});
      lapse.start();
      tripLines.push(`R ${before}`, `B ${afterPre}`, `Y ${afterPick} ${correct} ${choices.length}`, `Q ${afterOption} ${scene.money} ${leave.mock.calls[0][0] ? 1 : 0}`,
        `X ${queued.length} ${queued.join(" ")}`, `T ${shopArgs}`, `F ${expPhases.length} ${expPhases.join(" ")}`,
        `O ${shop}`, `S ${afterShop}`, `H ${scene.gameData.eggs.map((e: any) => e.hatchWaves).join(" ") || "-"}`, "E");
      tripCover.options[k]++;
      tripCover.correct += correct;
      tripCover.wrong += correct ? 0 : 1;
      tripCover.fainted += party[pick].hp <= 0 ? 1 : 0;
      if (correct) tripCover.shops[k]++;
      tripCover.expPhases += expPhases.length;
      tripCover.oval += exps.oval && !correct ? 1 : 0;
    }
  }, 1200000);
}

it("writes the field trip trace", () => {
  tripLines.unshift(`V 1 ${N}`);
  tripLines.push(`Z ${Math.floor(N / tripConfigs.length) * tripConfigs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-trip.txt", tripLines.join("\n") + "\n");
  console.log(`TRIP ${SEED}: options=${tripCover.options} correct=${tripCover.correct} wrong=${tripCover.wrong} fainted picks=${tripCover.fainted} shops by option=${tripCover.shops} exp phases=${tripCover.expPhases} wrong with oval charm=${tripCover.oval}`);
  expect(tripCover.shops.every(n => n > 0) && tripCover.wrong > 0).toBe(true);
});


/* ---- Part Timer ----
 * Per case: a party with random levels, natures, IVs, HP (a fifth fainted), Pokerus, the experience items, an Amulet Coin,
 * and 1-4 random moves each (a third with a charming move among them) with random PP used, PP Ups and the odd maximum
 * override; the wave and the money. The options' own meetsRequirements() and the party screen filter give what the app may
 * offer, and the primary Pokemon of the third option. Then the chosen option runs the way the phases run it:
 * onPreOptionPhase in executeWithSeedOffset(.., getSeedOffset()) with selectPokemonForOption captured and the party
 * screen's callback called after it (outside any seed offset), onOptionPhase in (.., * 500) with the dialogue it shows
 * recorded, then MysteryEncounterRewardsPhase's own doEncounterRewardsAndContinue() with the PartyExpPhase really started
 * (the experience phases it unshifts), the shop it queues opened with SelectModifierPhase's own calls, and
 * EggLapsePhase.start(). */
const timerConfigs: SpeciesId[][] = [
  [SpeciesId.SHUCKLE, SpeciesId.NINJASK, SpeciesId.SNORLAX, SpeciesId.BLISSEY, SpeciesId.DITTO, SpeciesId.MAGIKARP],
  [SpeciesId.REGIELEKI, SpeciesId.MEW, SpeciesId.SHEDINJA],
  [SpeciesId.RATTATA, SpeciesId.ABRA, SpeciesId.GROUDON, SpeciesId.PIKACHU],
  [SpeciesId.BULBASAUR],
];
const charming = [MoveId.CHARM, MoveId.FLATTER, MoveId.DRAGON_CHEER, MoveId.ALLURING_VOICE, MoveId.ATTRACT, MoveId.SWEET_SCENT, MoveId.CAPTIVATE, MoveId.AROMATIC_MIST];
const timerLines: string[] = [];
const timerCover = { options: [0, 0, 0], noPick: 0, disabled: 0, good: [0, 0, 0], bad: [0, 0, 0], low: 0, high: 0, fainted: 0, amulet: 0, ppCut: 0, shops: 0, expPhases: 0, oval: 0, charmFirstFainted: 0 };
for (let ci = 0; ci < timerConfigs.length; ci++) {
  it(`part timer, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    CIVILIZATION_ENCOUNTER_BIOMES.forEach(b => biomes.set(b, [MysteryEncounterType.PART_TIMER]));
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    await game.runToMysteryEncounter(MysteryEncounterType.PART_TIMER, timerConfigs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    let captured: { cb: (p: any) => void; filter: (p: any) => string | null } | undefined;
    vi.spyOn(EncounterPhaseUtils, "selectPokemonForOption").mockImplementation(((cb: any, _no: any, filter: any) => {
      captured = { cb, filter };
      return Promise.resolve(true);
    }) as any);
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((() => Promise.resolve(true)) as any);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const keys: string[] = [];
    vi.spyOn(EncounterDialogueUtils, "showEncounterDialogue").mockImplementation(((key: string) => { keys.push(key.replace(/^.*:/, "")); return Promise.resolve(); }) as any);
    vi.spyOn(EncounterDialogueUtils, "showEncounterText").mockImplementation(((text: string) => {
      if (/^mysteryEncounters\/partTimer:/.test(text)) keys.push(text.replace(/^.*:/, ""));
      return Promise.resolve();
    }) as any);
    const realT = i18next.t.bind(i18next);
    vi.spyOn(i18next, "t").mockImplementation(((k: any, o?: any) => {
      const key = String(k);
      if (/receiveMoney/.test(key)) keys.push(`receiveMoney:${o?.amount}`);
      return realT(k, o);
    }) as any);
    const filters: Array<(p: any) => string | null> = [];
    for (const k of [0, 1]) {
      captured = undefined;
      encounter.selectedOption = encounter.options[k];
      scene.executeWithSeedOffset(() => { void encounter.options[k].onPreOptionPhase(); }, encounter.getSeedOffset());
      expect(captured).toBeDefined();
      filters.push(captured!.filter);
    }
    timerLines.push(`P ${party.length} ${party.map((p: any) => `${p.species.speciesId} ${p.formIndex}`).join(" ")}`);
    const share = Math.floor(N / timerConfigs.length);
    const addItem = (id: string, stack: number, mon?: any) => {
      const type = (modifierTypes as any)[id]();
      // getModifierType(func) names the type after its key; the shop pool's stack check finds owned items by that id
      if (!type.id) type.id = id;
      const modifier = type.newModifier(mon);
      modifier.stackCount = stack;
      scene.addModifier(modifier, true);
    };
    const pps = () => party.map((mon: any) => mon.moveset.map((m: any) => m.ppUsed).join(" ")).join(" ");
    for (let i = 0; i < share; i++) {
      const k = i % 3, wave = 10 + rnd(171), runSeed = word(), waveSeed = word();
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = undefined;
      encounter.doEncounterExp = undefined;
      encounter.doEncounterRewards = undefined;
      scene.modifiers.length = 0;
      const mons: string[] = [];
      for (const mon of party) {
        mon.fusionSpecies = null;
        mon.level = 1 + rnd(rnd(2) ? 100 : 20);
        mon.nature = rnd(25);
        mon.ivs = mon.ivs.map(() => rnd(32));
        mon.pokerus = rnd(4) === 0;
        const n = 1 + rnd(4), ids: number[] = [];
        while (ids.length < n) {
          const id = 1 + rnd(allMoves.length - 1);
          if (allMoves[id] && !ids.includes(id)) ids.push(id);
        }
        if (rnd(3) === 0) ids[rnd(n)] = charming[rnd(charming.length)];
        mon.moveset = ids.map(id => {
          const mv = new PokemonMove(id, 0, rnd(4) === 0 ? 1 + rnd(3) : 0, rnd(12) === 0 ? 1 + rnd(40) : undefined);
          const max = mv.getMovePp();
          mv.ppUsed = rnd(3) === 0 ? max : rnd(max + 1);
          return mv;
        });
      }
      party.forEach((mon: any) => mon.calculateStats());
      const exps = { share: 0, balance: 0, oval: 0, amulet: 0 };
      if (rnd(3) === 0) { exps.share = 1 + rnd(3); addItem("EXP_SHARE", exps.share); }
      if (rnd(4) === 0) { exps.balance = 1 + rnd(4); addItem("EXP_BALANCE", exps.balance); }
      if (rnd(2) === 0) { exps.oval = 1 + rnd(2); addItem("OVAL_CHARM", exps.oval); }
      if (rnd(2) === 0) { exps.amulet = 1 + rnd(5); addItem("AMULET_COIN", exps.amulet); }
      const eggItems: string[] = [];
      for (let m = 0; m < party.length; m++) {
        if (rnd(4) === 0) { const st = 1 + rnd(4); addItem("LUCKY_EGG", st, party[m]); eggItems.push(`${m} 40 ${st}`); }
        if (rnd(6) === 0) { const st = 1 + rnd(3); addItem("GOLDEN_EGG", st, party[m]); eggItems.push(`${m} 100 ${st}`); }
      }
      scene.money = rnd(3) === 0 ? Number.MAX_SAFE_INTEGER - rnd(5000) : 12345 + rnd(100000);
      for (const mon of party) {
        const max = mon.getMaxHp();
        mon.hp = rnd(5) === 0 ? 0 : 1 + rnd(max);
        mons.push(`${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.pokerus ? 1 : 0} ${mon.getAbility(true).id} ${mon.hasPassive() ? mon.getPassiveAbility().id : 0} ${mon.hp} ${max} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((m: any) => `${m.moveId} ${m.ppUsed} ${m.ppUp} ${m.maxPpOverride ?? -1}`).join(" ")}`);
      }
      const eggs = Array.from({ length: rnd(3) }, () => 2 + rnd(40));
      scene.gameData.eggs = eggs.map(w => ({ hatchWaves: w }));
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const primary = meets[2] ? party.indexOf(encounter.options[2].primaryPokemon) : -1;
      const mask = party.reduce((m: number, mon: any, idx: number) => m | (filters[0](mon) === null ? 1 << idx : 0), 0);
      const valid = k < 2 ? party.map((_: any, idx: number) => idx).filter((idx: number) => (mask >> idx) & 1) : [];
      const runs = k === 2 ? meets[2] === 1 : valid.length > 0;
      const pick = k === 2 ? primary : runs ? valid[rnd(valid.length)] : -1;
      timerLines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.money} ${pick} ${runs ? 1 : 0}`, `U ${mons.join(" ")}`,
        `I ${exps.share} ${exps.balance} ${exps.oval} ${exps.amulet} ${eggItems.length} ${eggItems.join(" ")}`, `G ${eggs.length} ${eggs.join(" ")}`,
        `A ${meets.join(" ")} ${mask} ${primary} ${scene.getMaxExpLevel()}`);
      if (!runs) { timerCover.disabled++; timerCover.noPick += k < 2 ? 1 : 0; timerLines.push("E"); continue; }

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); keys.length = 0;
      Phaser.Math.RND.sow([`timer-${SEED}-${ci}-${i}`]);
      const before = rng(), money0 = scene.money, ppBefore = pps();
      captured = undefined;
      let pre: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
      await pre;
      const afterPre = rng();
      if (k < 2) {
        expect(captured).toBeDefined();
        captured!.cb(party[pick]);   // the party screen's callback: outside any seed offset
      }
      const afterPick = rng();
      const multiplier = k < 2 ? encounter.misc.moneyMultiplier : 2.5;
      const ppAfter = pps();
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      const afterOption = rng();
      expect(leave).toHaveBeenCalledTimes(1);
      const pm = scene.phaseManager, queued: string[] = [], expPhases: string[] = [];
      let shopSettings: any, shopArgs = "-";
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...a: any[]) => {
        queued.push(name);
        if (name === "PartyExpPhase") {
          const phase = new PartyExpPhase(a[0], a[1], a[2]);
          vi.spyOn(phase, "end").mockImplementation(() => {});
          phase.start();
        } else if (name === "SelectModifierPhase") {
          shopSettings = a[2];
          shopArgs = `${a[0]} ${a[1] === undefined ? "u" : a[1]} ${a[2]?.fillRemaining ? 1 : 0} ${a[2]?.rerollMultiplier ?? "nan"} ${a[2]?.guaranteedModifierTypeOptions?.length ?? 0} ${a[2]?.allowLuckUpgrades === undefined ? -1 : +a[2].allowLuckUpgrades}`;
        }
      }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      vi.spyOn(pm, "removeAllPhasesOfType").mockImplementation((() => {}) as any);
      vi.spyOn(pm, "create").mockImplementation(((name: string, index: number, exp: number) => ({ phaseName: name, index, exp })) as any);
      vi.spyOn(pm, "unshiftPhase").mockImplementation(((ph: any) => { expPhases.push(`${ph.phaseName === "ExpPhase" ? 0 : 1} ${ph.index} ${ph.exp}`); }) as any);
      const rewards = new MysteryEncounterRewardsPhase(leave.mock.calls[0][0]);
      vi.spyOn(rewards, "end").mockImplementation(() => {});
      rewards.doEncounterRewardsAndContinue();
      for (const fn of [pm.unshiftNew, pm.pushNew, pm.removeAllPhasesOfType, pm.create, pm.unshiftPhase]) vi.mocked(fn).mockRestore();
      let shop = "0 0";
      let afterShop = rng();
      if (shopSettings) {
        const phase: any = new SelectModifierPhase(0, undefined, shopSettings);
        phase.updateSeed();
        regenerateModifierPoolThresholds(scene.getPlayerParty(), ModifierPoolType.PLAYER, 0);
        const count = phase.getModifierCount();
        const opts = phase.getModifierTypeOptions(count);
        shop = `${count} ${opts.length} ${opts.map((o: any) => `${o.type.id} ${argOf(o.type)} ${o.type.tier ?? -1} ${o.upgradeCount}`).join(" ")}`;
        afterShop = rng();
      }
      const lapse = new EggLapsePhase();
      vi.spyOn(lapse, "end").mockImplementation(() => {});
      lapse.start();
      timerLines.push(`R ${before}`, `B ${afterPre}`, `Y ${afterPick} ${multiplier}`, `W ${ppAfter}`, `Q ${afterOption} ${scene.money} ${leave.mock.calls[0][0] ? 1 : 0}`,
        `N ${keys.length} ${keys.join(" ")}`, `X ${queued.length} ${queued.join(" ")}`, `T ${shopArgs}`, `F ${expPhases.length} ${expPhases.join(" ")}`,
        `O ${shop}`, `S ${afterShop}`, `H ${scene.gameData.eggs.map((e: any) => e.hatchWaves).join(" ") || "-"}`, "E");
      void money0; void ppBefore;
      timerCover.options[k]++;
      (keys[0] === "jobCompleteGood" ? timerCover.good : timerCover.bad)[k]++;
      timerCover.low += k < 2 && multiplier === 1 ? 1 : 0;
      timerCover.high += k < 2 && multiplier === 4 ? 1 : 0;
      timerCover.amulet += exps.amulet ? 1 : 0;
      timerCover.ppCut += ppAfter !== ppBefore ? 1 : 0;
      timerCover.shops += shopSettings ? 1 : 0;
      timerCover.expPhases += expPhases.length;
      timerCover.oval += exps.oval ? 1 : 0;
    }
  }, 1200000);
}

it("writes the part timer trace", () => {
  timerLines.unshift(`V 1 ${N}`);
  timerLines.push(`Z ${Math.floor(N / timerConfigs.length) * timerConfigs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-timer.txt", timerLines.join("\n") + "\n");
  console.log(`TIMER ${SEED}: options=${timerCover.options} no-pick=${timerCover.noPick} disabled=${timerCover.disabled} good=${timerCover.good} bad=${timerCover.bad} clamped low=${timerCover.low} high=${timerCover.high} amulet=${timerCover.amulet} pp changed=${timerCover.ppCut} exp phases=${timerCover.expPhases} oval=${timerCover.oval}`);
  expect(timerCover.options.every(n => n > 0) && timerCover.low > 0 && timerCover.high > 0).toBe(true);
});


/* ---- An Offer You Can't Refuse ----
 * Per case: a party of two to six with random levels (twins with the same level, IVs and nature to make ties in the
 * stat total), HP (a fifth fainted), Pokerus, abilities and passives (some enabled), one to four random moves each (a
 * quarter with an extortion move), and the items in a shuffled order: held Leftovers / Shell Bell / vitamins / berries,
 * Gimmighoul's evolution tracker (a few on a Pokemon that is not a Gimmighoul), Exp. Share, Exp. Balance, Oval Charm,
 * Amulet Coin, Lucky / Golden Eggs and 0-4 Shiny Charms; the wave, money, Poke Ball count and eggs. The encounter's own
 * onInit runs again (the strongest Pokemon and its price), the options' meetsRequirements() and the Pokemon option 2 picks
 * are recorded, then the chosen option runs as the phases run it: onPreOptionPhase (sell) in executeWithSeedOffset(..,
 * getSeedOffset()) with removePokemonFromPlayerParty really run (its updateModifiers prunes the list), onOptionPhase in
 * (.., * 500) with the phases it unshifts recorded, the ModifierRewardPhase it queued really rewarded (doReward), then
 * MysteryEncounterRewardsPhase's own doEncounterRewardsAndContinue() with the PartyExpPhase really started, and
 * EggLapsePhase.start(). The item list is recorded before and after, in order. */
const offerConfigs: SpeciesId[][] = [
  [SpeciesId.GIMMIGHOUL, SpeciesId.RATTATA, SpeciesId.RATTATA, SpeciesId.GYARADOS, SpeciesId.MUK],
  [SpeciesId.DUGTRIO, SpeciesId.WOBBUFFET, SpeciesId.MEW, SpeciesId.DRAGONITE, SpeciesId.GHOLDENGO, SpeciesId.OCTILLERY],
  [SpeciesId.MAGIKARP, SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.SHEDINJA, SpeciesId.GROUDON],
  [SpeciesId.MIRAIDON, SpeciesId.LAPRAS, SpeciesId.ARCANINE],
  [SpeciesId.GIMMIGHOUL, SpeciesId.GIMMIGHOUL, SpeciesId.RATTATA],
];
const extortion = [MoveId.BIND, MoveId.CLAMP, MoveId.INFESTATION, MoveId.SAND_TOMB, MoveId.SNAP_TRAP, MoveId.THUNDER_CAGE, MoveId.WRAP, MoveId.SPIRIT_SHACKLE,
  MoveId.MEAN_LOOK, MoveId.JAW_LOCK, MoveId.BLOCK, MoveId.SPIDER_WEB, MoveId.ANCHOR_SHOT, MoveId.OCTOLOCK, MoveId.PURSUIT, MoveId.CONSTRICT, MoveId.BEAT_UP,
  MoveId.COIL, MoveId.WRING_OUT, MoveId.STRING_SHOT];
const extortionAbilities = [AbilityId.INTIMIDATE, AbilityId.ARENA_TRAP, AbilityId.SHADOW_TAG, AbilityId.SUCTION_CUPS, AbilityId.STICKY_HOLD];
const offerLines: string[] = [];
const offerCover = { options: [0, 0, 0], disabled: 0, byMove: 0, byAbility: 0, ties: 0, orphans: 0, doubleCuts: 0, full: 0, fallback: 0, exp: 0, prices: new Set<number>() };
for (let ci = 0; ci < offerConfigs.length; ci++) {
  it(`offer, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    CIVILIZATION_ENCOUNTER_BIOMES.forEach(b => biomes.set(b, [MysteryEncounterType.AN_OFFER_YOU_CANT_REFUSE]));
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    await game.runToMysteryEncounter(MysteryEncounterType.AN_OFFER_YOU_CANT_REFUSE, offerConfigs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const realRemove = scene.removePokemonFromPlayerParty.bind(scene);
    vi.spyOn(scene, "removePokemonFromPlayerParty").mockImplementation(((p: any) => realRemove(p, false)) as any);
    const keys: string[] = [];
    const realT = i18next.t.bind(i18next);
    vi.spyOn(i18next, "t").mockImplementation(((k: any, o?: any) => {
      const key = String(k);
      if (/receiveMoney/.test(key)) keys.push(`receiveMoney:${o?.amount}`);
      else if (/battle:rewardGain/.test(key)) keys.push("rewardGain");
      else if (/battle:itemStackFull/.test(key)) keys.push("itemStackFull");
      return realT(k, o);
    }) as any);
    const pmAll = scene.phaseManager;
    vi.spyOn(pmAll, "queueMessage").mockImplementation((() => {}) as any);
    const funcKey = new Map<any, string>(Object.entries(modifierTypes as any).map(([k, f]) => [f, k]));
    const original = [...party];
    offerLines.push(`P ${party.length} ${party.map((p: any) => `${p.species.speciesId} ${p.formIndex}`).join(" ")}`);
    const share = Math.floor(N / offerConfigs.length);
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
    for (let i = 0; i < share; i++) {
      const k = i % 3, wave = 10 + rnd(171), runSeed = word(), waveSeed = word();
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = undefined;
      encounter.doEncounterExp = undefined;
      encounter.doEncounterRewards = undefined;
      scene.modifiers.length = 0;
      party.length = 0; party.push(...original);
      const mons: string[] = [];
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        mon.fusionSpecies = null;
        mon.level = 1 + rnd(rnd(2) ? 100 : 20);
        mon.nature = rnd(25);
        mon.ivs = mon.ivs.map(() => rnd(32));
        mon.pokerus = rnd(4) === 0;
        mon.passive = rnd(2) === 0;
        const twin = party.findIndex((q: any, j: number) => j < m && q.species.speciesId === mon.species.speciesId);
        if (twin >= 0 && rnd(2) === 0) { mon.level = party[twin].level; mon.nature = party[twin].nature; mon.ivs = [...party[twin].ivs]; }
        const n = 1 + rnd(4), ids: number[] = [];
        while (ids.length < n) {
          const id = 1 + rnd(allMoves.length - 1);
          if (allMoves[id] && !ids.includes(id)) ids.push(id);
        }
        if (rnd(4) === 0) ids[rnd(n)] = extortion[rnd(extortion.length)];
        mon.customPokemonData.passive = rnd(5) === 0 ? extortionAbilities[rnd(extortionAbilities.length)] : -1;
        mon.customPokemonData.ability = rnd(12) === 0 ? extortionAbilities[rnd(extortionAbilities.length)] : -1;
        mon.moveset = ids.map(id => new PokemonMove(id));
      }
      party.forEach((mon: any) => mon.calculateStats());
      const adds: Array<() => void> = [];
      for (const mon of party) {
        if (rnd(3) === 0) adds.push(mk("LEFTOVERS", mon));
        if (rnd(4) === 0) adds.push(mk("SHELL_BELL", mon));
        if (rnd(4) === 0) adds.push(mk("BASE_STAT_BOOSTER", mon, rnd(6), 1 + rnd(3)));
        if (rnd(5) === 0) adds.push(mk("BERRY", mon, rnd(11), 1 + rnd(3)));
        if (mon.species.speciesId === SpeciesId.GIMMIGHOUL ? rnd(4) !== 0 : rnd(8) === 0) adds.push(mk("EVOLUTION_TRACKER_GIMMIGHOUL", mon, undefined, 1 + rnd(9)));
        if (rnd(5) === 0) adds.push(mk("LUCKY_EGG", mon, undefined, 1 + rnd(4)));
        if (rnd(6) === 0) adds.push(mk("GOLDEN_EGG", mon, undefined, 1 + rnd(3)));
      }
      if (rnd(3) === 0) adds.push(mk("EXP_SHARE", undefined, undefined, 1 + rnd(3)));
      if (rnd(4) === 0) adds.push(mk("EXP_BALANCE", undefined, undefined, 1 + rnd(4)));
      if (rnd(2) === 0) adds.push(mk("OVAL_CHARM", undefined, undefined, 1 + rnd(2)));
      if (rnd(2) === 0) adds.push(mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)));
      const charms = rnd(5);
      if (charms) adds.push(mk("SHINY_CHARM", undefined, undefined, charms));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());   // the vitamins count
      scene.money = rnd(3) === 0 ? Number.MAX_SAFE_INTEGER - rnd(5000) : 12345 + rnd(100000);
      const balls = [rnd(3) === 0 ? 90 + rnd(10) : rnd(30), 0, 0, 0, 0];
      scene.pokeballCounts = { 0: balls[0], 1: 0, 2: 0, 3: 0, 4: 0 };
      for (const mon of party) {
        const max = mon.getMaxHp();
        mon.hp = rnd(5) === 0 ? 0 : 1 + rnd(max);
        mons.push(`${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.pokerus ? 1 : 0} ${mon.getAbility().id} ${mon.getPassiveAbility().id} ${mon.hasPassive() ? 1 : 0} ${mon.hp} ${max} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((mv: any) => mv.moveId).join(" ")}`);
      }
      const eggs = Array.from({ length: rnd(3) }, () => 2 + rnd(40));
      scene.gameData.eggs = eggs.map(w => ({ hatchWaves: w }));
      encounter.onInit();
      const strongest = party.indexOf(encounter.misc.pokemon), price = encounter.misc.price;
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const primary = meets[1] ? party.indexOf(encounter.options[1].primaryPokemon) : -1;
      const runs = meets[k] === 1;
      const before0 = list();
      offerLines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.money} ${balls[0]} ${runs ? 1 : 0}`, `U ${mons.join(" ")}`,
        `L ${before0.length} ${before0.join(" ")}`, `G ${eggs.length} ${eggs.join(" ")}`,
        `A ${meets.join(" ")} ${primary} ${strongest} ${price} ${scene.getMaxExpLevel()}`);
      if (!runs) { offerCover.disabled++; offerLines.push("E"); continue; }

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); keys.length = 0;
      Phaser.Math.RND.sow([`offer-${SEED}-${ci}-${i}`]);
      const before = rng(), money0 = scene.money;
      let pre: Promise<boolean> | undefined;
      if (option.onPreOptionPhase) scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
      await pre;
      const afterPre = rng();
      const pm = scene.phaseManager, queued: string[] = [], expPhases: string[] = [];
      let shopArgs = "-";
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...a: any[]) => {
        if (name === "ModifierRewardPhase") queued.push(`ModifierRewardPhase:${funcKey.get(a[0]) ?? "?"}`);
        else queued.push(name);
        if (name === "PartyExpPhase") {
          const phase = new PartyExpPhase(a[0], a[1], a[2]);
          vi.spyOn(phase, "end").mockImplementation(() => {});
          phase.start();
        } else if (name === "SelectModifierPhase") {
          shopArgs = `${a[0]} ${a[1] === undefined ? "u" : a[1]} ${a[2]?.fillRemaining ? 1 : 0} ${a[2]?.rerollMultiplier ?? "nan"} ${a[2]?.guaranteedModifierTypeOptions?.length ?? 0} ${a[2]?.allowLuckUpgrades === undefined ? -1 : +a[2].allowLuckUpgrades}`;
        }
      }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      vi.spyOn(pm, "removeAllPhasesOfType").mockImplementation((() => {}) as any);
      vi.spyOn(pm, "create").mockImplementation(((name: string, index: number, exp: number) => ({ phaseName: name, index, exp })) as any);
      vi.spyOn(pm, "unshiftPhase").mockImplementation(((ph: any) => { expPhases.push(`${ph.phaseName === "ExpPhase" ? 0 : 1} ${ph.index} ${ph.exp}`); }) as any);
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      const afterOption = rng();
      expect(leave).toHaveBeenCalledTimes(1);
      // the ModifierRewardPhase it queued runs first (it is unshifted ahead of the rewards phase)
      const stacksBefore = scene.modifiers.filter((m: any) => m.type?.id === "SHINY_CHARM").reduce((n: number, m: any) => n + m.stackCount, 0);
      if (queued.includes("ModifierRewardPhase:SHINY_CHARM")) {
        const keysBefore = keys.length;
        try { await Promise.race([new ModifierRewardPhase(modifierTypes.SHINY_CHARM).doReward(), new Promise(r => setTimeout(r, 0))]); } catch { /* showText has no UI here */ }
        const got = keys.splice(keysBefore);
        // the text of the phase first, then the message addModifier queued when the stack was full
        keys.push(...got.filter(x => x === "rewardGain"), ...got.filter(x => x === "itemStackFull"));
        offerCover.full += got.includes("itemStackFull") ? 1 : 0;
      }
      const rewards = new MysteryEncounterRewardsPhase(leave.mock.calls[0][0]);
      vi.spyOn(rewards, "end").mockImplementation(() => {});
      rewards.doEncounterRewardsAndContinue();
      for (const fn of [pm.unshiftNew, pm.pushNew, pm.removeAllPhasesOfType, pm.create, pm.unshiftPhase]) vi.mocked(fn).mockRestore();
      const afterShop = rng();
      const lapse = new EggLapsePhase();
      vi.spyOn(lapse, "end").mockImplementation(() => {});
      lapse.start();
      const after = list();
      const order = party.map((p: any) => original.indexOf(p));
      offerLines.push(`R ${before}`, `B ${afterPre}`, `Q ${afterOption} ${scene.money} ${leave.mock.calls[0][0] ? 1 : 0}`,
        `N ${keys.length} ${keys.join(" ")}`, `X ${queued.length} ${queued.join(" ")}`, `T ${shopArgs}`, `F ${expPhases.length} ${expPhases.join(" ")}`,
        `S ${afterShop}`, `W ${order.length} ${order.join(" ")}`, `M ${scene.pokeballCounts[0]} ${stacksBefore}`, `V ${after.length} ${after.join(" ")}`,
        `H ${scene.gameData.eggs.map((e: any) => e.hatchWaves).join(" ") || "-"}`, "E");
      void money0;
      offerCover.options[k]++;
      offerCover.prices.add(price);
      if (k === 1) { const byMove = encounter.options[1].primaryPokemon.moveset.some((mv: any) => extortion.includes(mv.moveId)); offerCover.byMove += byMove ? 1 : 0; offerCover.byAbility += byMove ? 0 : 1; }
      if (k === 0) {
        const lost = original.find((p: any) => !party.includes(p));
        offerCover.orphans += before0.some(l => l.split(" ")[2] === String(original.indexOf(lost))) ? 1 : 0;
        offerCover.doubleCuts += before0.length - after.length > before0.filter(l => l.split(" ")[2] === String(original.indexOf(lost))).length ? 1 : 0;
        offerCover.fallback += keys.includes("itemStackFull") ? 1 : 0;
      }
      offerCover.exp += expPhases.length;
      party.length = 0; party.push(...original);
    }
  }, 1200000);
}

it("writes the offer trace", () => {
  offerLines.unshift(`V 1 ${N}`);
  offerLines.push(`Z ${Math.floor(N / offerConfigs.length) * offerConfigs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-offer.txt", offerLines.join("\n") + "\n");
  console.log(`OFFER ${SEED}: options=${offerCover.options} disabled=${offerCover.disabled} extort by move=${offerCover.byMove} by ability=${offerCover.byAbility} stack-full=${offerCover.full} items cut beyond the seller's=${offerCover.doubleCuts} sold with held items=${offerCover.orphans} prices=${offerCover.prices.size} exp phases=${offerCover.exp}`);
  expect(offerCover.options.every(n => n > 0) && offerCover.byMove > 0 && offerCover.byAbility > 0).toBe(true);
});
