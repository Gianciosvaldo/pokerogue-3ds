/* The Bug-Type Superfan oracle, options 2 and 3 and the requirements (the harness family of mystery-breeder.producer.test.ts). The real encounter is started through the
 * game manager; per case the run and wave seeds, the wave (30..180, never a boss wave or one ending in 1), the party (a random subset of at least three of eight layouts, so
 * from none to six Bug types, levels, fainted members) and its held items (Quick Claw, Grip Claw, Bug and other attack type boosters, Soothe Bell) and the Mega Bracelet and
 * Dynamax Band are changed. The requirements (the encounter's and each option's) are recorded. Option 2's onPreOptionPhase runs in executeWithSeedOffset(.., getSeedOffset()),
 * option 3's party screen is answered by the oracle (the per Pokemon filter and the list of items it offers are recorded, then a Pokemon and an item are picked), and the
 * option's onOptionPhase runs in executeWithSeedOffset(.., getSeedOffset() * 500). setEncounterRewards and leaveEncounterWithoutBattle are recorded.
 * GC_SF_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { SpeciesId } from "#enums/species-id";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { timedEventManager } from "#app/global-event-manager";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_SF_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.BEEDRILL, SpeciesId.BUTTERFREE, SpeciesId.SCYTHER, SpeciesId.PINSIR, SpeciesId.HERACROSS, SpeciesId.VOLCARONA],
  [SpeciesId.CATERPIE, SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.MAGIKARP],
  [SpeciesId.SHEDINJA, SpeciesId.SNORLAX, SpeciesId.CHARIZARD, SpeciesId.MEWTWO, SpeciesId.ARCANINE],
  [SpeciesId.BEEDRILL, SpeciesId.PIKACHU, SpeciesId.SCIZOR, SpeciesId.GENGAR, SpeciesId.DURANT, SpeciesId.EEVEE],
  [SpeciesId.PIKACHU, SpeciesId.SNORLAX, SpeciesId.MEWTWO, SpeciesId.MAGIKARP, SpeciesId.GENGAR],
  [SpeciesId.PARASECT, SpeciesId.YANMA, SpeciesId.ARIADOS, SpeciesId.CHARIZARD, SpeciesId.PIKACHU],
  [SpeciesId.HERACROSS, SpeciesId.PINSIR, SpeciesId.SCYTHER, SpeciesId.SCIZOR, SpeciesId.VOLCARONA],
  [SpeciesId.SCYTHER, SpeciesId.PINSIR, SpeciesId.KARRABLAST, SpeciesId.SHELMET, SpeciesId.HERACROSS, SpeciesId.CATERPIE],
];

const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "MINT": return t.nature;
    case "SPECIES_STAT_BOOSTER": case "RARE_SPECIES_STAT_BOOSTER":
      return ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_TOOTH", "DEEP_SEA_SCALE"].indexOf(t.key);
    case "EVOLUTION_ITEM": case "RARE_EVOLUTION_ITEM": return t.evolutionItem;
    case "FORM_CHANGE_ITEM": case "RARE_FORM_CHANGE_ITEM": return t.formChangeItem;
  }
  return -1;
};
const keyOf = (f: any): string => Object.entries(modifierTypes as any).filter(([, v]) => v === f).map(([key]) => key)[0] ?? "?";
const tokenOf = (s: string | undefined) => (s ?? "").replace(/\s+/g, "_").replace(/\n/g, "|");

const lines: string[] = [`V 1 ${N}`];
const cover = { options: [0, 0, 0], tiers: [0, 0, 0, 0], special: 0, bugs: [0, 0, 0, 0, 0, 0, 0], picks: 0, noitem: 0, spawn: [0, 0], errors: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {
  it(`the bug-type superfan, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(42).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.PLAINS, [MysteryEncounterType.BUG_TYPE_SUPERFAN]]]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.BUG_TYPE_SUPERFAN, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    scene.time.delayedCall = () => ({}) as any;
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
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
    const opt = (o: any) => `${o.type.id} ${argOf(o.type)} ${o.type.tier} ${o.upgradeCount ?? 0} ${o.cost ?? 0}`;
    for (let i = 0; i < share; i++) {
      const k = 1 + (i % 2);
      let wave = 30 + rnd(151);
      while (wave % 10 === 0 || wave % 10 === 1) wave++;
      const runSeed = word(), waveSeed = word();
      scene.setSeed(runSeed); scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.enemyParty = [];
      scene.newArena(BiomeId.PLAINS);
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = null;
      encounter.dialogueTokens = {};
      encounter.doEncounterRewards = undefined;
      scene.modifiers.length = 0;
      scene.enemyModifiers.length = 0;
      /* a random subset of at least three members, in party order */
      const keep = original.map((_: any, m: number) => m);
      /* the two layouts of six Bug types keep their whole party in three cases out of four (the top reward needs all six) */
      while (keep.length > 3 && (ci === 0 || ci === 7 ? rnd(4) === 0 : rnd(3) !== 0)) keep.splice(rnd(keep.length), 1);
      party.length = 0; party.push(...keep.map((m: number) => original[m]));
      scene.party = [...party];
      for (const mon of party) {
        mon.level = 5 + rnd(96);
        mon.status = null;
        mon.calculateStats();
        mon.hp = mon.getMaxHp();
      }
      const adds: Array<() => void> = [];
      for (const mon of party) {
        if (rnd(3) === 0) adds.push(mk("QUICK_CLAW", mon, undefined, 1 + rnd(3)));
        if (rnd(4) === 0) adds.push(mk("GRIP_CLAW", mon, undefined, 1 + rnd(3)));
        if (rnd(3) === 0) adds.push(mk("ATTACK_TYPE_BOOSTER", mon, 6, 1 + rnd(4)));   /* PokemonType.BUG = 6 */
        if (rnd(3) === 0) adds.push(mk("ATTACK_TYPE_BOOSTER", mon, [0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17][rnd(17)], 1 + rnd(3)));
        if (rnd(5) === 0) adds.push(mk("SOOTHE_BELL", mon));
        if (rnd(6) === 0) adds.push(mk("LEFTOVERS", mon));
      }
      if (rnd(3) === 0) adds.push(mk("MEGA_BRACELET"));
      if (rnd(3) === 0) adds.push(mk("DYNAMAX_BAND"));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      for (const mon of party) if (rnd(4) === 0) mon.hp = 0;
      const mons = party.map((mon: any) => `${original.indexOf(mon)} ${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.hp > 0 ? 1 : 0}`);
      const before0 = list();
      const meets = [encounter.meetsRequirements(), ...encounter.options.map((o: any) => o.meetsRequirements())].map((b: boolean) => (b ? 1 : 0));
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k}`, `U ${mons.join(" ; ")}`, `L ${before0.length} ${before0.join(" ")}`, `A ${meets.join(" ")}`);
      cover.spawn[meets[0]]++;

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); rewards.mockClear();
      Phaser.Math.RND.sow([`sf-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      let rngPre = "-", rngAfter = "-";
      const numBug = party.filter((p: any) => p.isOfType(6)).length;
      let filters = "-", offered = "-", picked = "-";
      if (k === 1 && numBug === 0) { lines.push("E"); cover.bugs[0]++; continue; }   /* the option is disabled: it cannot be selected */
      if (k === 1) {
        let pre: any;
        scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); rngPre = rng(); }, encounter.getSeedOffset());
        await pre;
        cover.bugs[Math.min(numBug, 6)]++;
      } else {
        /* the party screen is answered by the oracle: the filter's answer for every member, the items the chosen one offers */
        const spy = vi.spyOn(EncounterPhaseUtils, "selectPokemonForOption").mockImplementation((async (onSelected: any, _notSelected: any, filter: any) => {
          filters = party.map((p: any) => (filter(p) == null ? 1 : 0)).join(" ");
          const able = party.filter((p: any) => filter(p) == null);
          if (!able.length) return false;
          const mon = able[rnd(able.length)];
          const items = onSelected(mon);
          offered = `${party.indexOf(mon)} ${items.length} ${items.map((it: any) => tokenOf(it.label)).join(" ")}`;
          if (!items.length) return false;
          const at = rnd(items.length);
          picked = `${party.indexOf(mon)} ${at}`;
          items[at].handler();
          cover.picks++;
          return true;
        }) as any);
        let pre: any;
        scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); rngPre = rng(); }, encounter.getSeedOffset());
        const go = await pre;
        spy.mockRestore();
        lines.push(`F ${filters}`, `V ${offered}`, `PK ${picked}`, `GO ${go === false ? 0 : 1}`);
        if (picked === "-") { cover.noitem++; lines.push("E"); continue; }
      }
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); rngAfter = rng(); }, encounter.getSeedOffset() * 500);
      await done;
      const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
      const funcs = (given?.guaranteedModifierTypeFuncs ?? []).map(keyOf);
      const options = (given?.guaranteedModifierTypeOptions ?? []).map(opt);
      const selected = (option.dialogue?.selected ?? []).map((s: any) => `${tokenOf(s.speaker)}/${tokenOf(s.text)}`);
      lines.push(`Q ${rngBefore} ${rngPre === "-" ? "- - - -" : rngPre} ${rngAfter === "-" ? "- - - -" : rngAfter}`,
        `S ${numBug} ${tokenOf(encounter.dialogueTokens.numBugTypes) || "-"} ${tokenOf(encounter.dialogueTokens.selectedItem) || "-"} ${selected.length} ${selected.join(" ")}`,
        `T ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${funcs.length} ${funcs.join(" ")} ${options.length} ${options.join(" ; ")} ${(given?.guaranteedModifierTiers ?? []).length} ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] ? 1 : 0) : -1}`);
      const after = list();
      lines.push(`W ${after.length} ${after.join(" ")}`, "E");
      cover.options[k]++;
      if (k === 1) {
        cover.tiers[numBug < 2 ? 0 : numBug < 4 ? 1 : numBug < 6 ? 2 : 3]++;
        cover.special += options.length > 2 ? 1 : 0;
      }
    }
    scene.party = [...original];
  }, 1800000);
}

it("writes the bug-type superfan trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-superfan.txt", lines.join("\n") + "\n");
  console.log(`BUG-TYPE SUPERFAN ${SEED}: options=${cover.options} tiers=${cover.tiers} specials=${cover.special} bugTypes(0..6)=${cover.bugs} picks=${cover.picks} noitem=${cover.noitem} spawn(no,yes)=${cover.spawn}`);
  expect(cover.options[1] > 0 && cover.options[2] > 0).toBe(true);
  expect(cover.errors).toBe(0);
});
