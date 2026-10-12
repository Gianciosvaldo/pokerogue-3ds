/* Mystery encounter action oracle (card 82's harness; the older gen/dump-mystery-actions.test.ts and
 * tests/mystery_diff_test.c stay as they are). The real encounter is started through the game manager, then for each
 * case the run seed, wave seed and wave are changed and the chosen option's real onOptionPhase is run exactly as
 * MysteryEncounterOptionSelectedPhase runs it (executeWithSeedOffset(.., getSeedOffset() * 500)). Records the
 * RNG before and after the option, money, the party's levels, the items owned, whether the encounter left without
 * a battle, the shop it set up (setEncounterRewards' customModifierSettings) and the options that shop shows
 * (SelectModifierPhase.start's own calls). Families: Department Store Sale, Shady Vitamin Dealer (below).
 * GC_MYS_N=400 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { ModifierPoolType } from "#enums/modifier-pool-type";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { MysteryEncounterRewardsPhase } from "#phases/mystery-encounter-phases";
import { SpeciesId } from "#enums/species-id";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { CIVILIZATION_ENCOUNTER_BIOMES } from "#mystery-encounters/mystery-encounter-biomes";
import { regenerateModifierPoolThresholds } from "#modifiers/modifier-type";
import { EggLapsePhase } from "#phases/egg-lapse-phase";
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { Stat } from "#enums/stat";
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
