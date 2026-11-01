/* The Delibird-y oracle (card 82's harness, like mystery-gts.producer.test.ts). The real encounter is started through the game manager;
 * per case the run seed, wave seed, wave, money, trainer and secret ids and a fresh random party (species of every kind, levels, fainted
 * ones, held items of every kind with stacks up to the maximum, the Amulet Coin / Candy Jar / Berry Pouch / Healing Charm at every stack
 * from none to the maximum) are set. Recorded: the encounter's primary requirement (Some(option 2's, option 3's HeldItemRequirement)) and
 * its scene requirement (the money), every option's meetsRequirements(), the party screen's selectableFilter of every Pokemon for options 2
 * and 3 and the list onPokemonSelected answers for it (the held items it names, in order). Then the chosen option runs as the phases run
 * it: onPreOptionPhase in executeWithSeedOffset(.., getSeedOffset()) (the money; the party screen answering with the Pokemon and the item
 * chosen), onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500) with the phases it unshifts recorded (the
 * ModifierRewardPhase and the modifier type it was given) and the texts it says. Nothing is mocked but leaveEncounterWithoutBattle (its
 * argument is recorded), selectPokemonForOption (answers as the trace says), the phase manager's queue and the timed events (switched
 * off: the C port keeps them out).
 * GC_DELI_N=400 GC_DELI_SEED=777 GC_DELI_OUT=/abs/delibirdy.txt, Node22. */
import { timedEventManager } from "#app/global-event-manager";
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { CIVILIZATION_ENCOUNTER_BIOMES } from "#mystery-encounters/mystery-encounter-biomes";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import i18next from "i18next";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_DELI_N ?? 400), SEED = Number(process.env.GC_DELI_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const lines: string[] = [];
const cover = {
  options: [0, 0, 0], disabled: 0, tooPoor: 0, noItem: 0, shellBell: [0, 0, 0], shellBellFull: 0, berry: 0, seed: 0, items: new Set<string>(), nonTransferable: 0,
  notSpawnable: 0, fainted: 0, amulet: 0,
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

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 40).toBe(true);
});

const CHUNK = 100;
const chunks = Math.ceil(N / CHUNK);
for (let ci = 0; ci < chunks; ci++) {
  it(`delibirdy, cases ${ci * CHUNK}..`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    CIVILIZATION_ENCOUNTER_BIOMES.forEach(b => biomes.set(b, [MysteryEncounterType.DELIBIRDY]));
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    await game.runToMysteryEncounter(MysteryEncounterType.DELIBIRDY, [SpeciesId.LAPRAS, SpeciesId.GENGAR, SpeciesId.ABRA]);
    vi.spyOn(timedEventManager, "isEventActive").mockReturnValue(false);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "getDelibirdyBuff").mockReturnValue([]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party = scene.getPlayerParty(), gameData = scene.gameData;
    expect(encounter.options.length).toBe(3);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const select = vi.spyOn(EncounterPhaseUtils, "selectPokemonForOption");
    const keys: string[] = [];
    const realT = i18next.t.bind(i18next);
    vi.spyOn(i18next, "t").mockImplementation(((k: any, o?: any) => {
      const key = String(k);
      if (key === "battle:rewardGain") keys.push(`rewardGain:${String(o?.modifierName).toUpperCase().replace(/ /g, "_")}`);
      else if (/^mysteryEncounterMessages:/.test(key)) keys.push(key.split(":")[1]);
      return realT(k, o);
    }) as any);
    const pm = scene.phaseManager;
    vi.spyOn(pm, "queueMessage").mockImplementation((() => {}) as any);
    const funcKey = new Map<any, string>(Object.entries(modifierTypes as any).map(([k, f]) => [f, k]));
    const all = speciesDataRegistry.getAllSpecies();
    const heldPool: Array<[string, boolean, () => number | undefined]> = [
      ["LEFTOVERS", false, () => undefined], ["SHELL_BELL", false, () => undefined], ["SOUL_DEW", false, () => undefined], ["FOCUS_BAND", false, () => undefined],
      ["QUICK_CLAW", false, () => undefined], ["KINGS_ROCK", false, () => undefined], ["WIDE_LENS", false, () => undefined], ["MULTI_LENS", false, () => undefined],
      ["SCOPE_LENS", false, () => undefined], ["WHITE_HERB", false, () => undefined], ["LUCKY_EGG", false, () => undefined], ["GOLDEN_EGG", false, () => undefined],
      ["GRIP_CLAW", false, () => undefined], ["TOXIC_ORB", false, () => undefined], ["FLAME_ORB", false, () => undefined], ["MINI_BLACK_HOLE", false, () => undefined],
      ["EVIOLITE", false, () => undefined], ["BATON", false, () => undefined],
      ["REVIVER_SEED", false, () => undefined], ["REVIVER_SEED", false, () => undefined], ["REVIVER_SEED", false, () => undefined],
      ["BERRY", true, () => rnd(11)], ["BERRY", true, () => rnd(11)], ["BERRY", true, () => rnd(11)], ["BERRY", true, () => rnd(11)],
      ["BASE_STAT_BOOSTER", true, () => rnd(6)], ["ATTACK_TYPE_BOOSTER", true, () => rnd(18)],
      ["SPECIES_STAT_BOOSTER", true, () => rnd(6)], ["RARE_SPECIES_STAT_BOOSTER", true, () => rnd(6)],
      ["FORM_CHANGE_ITEM", true, () => 1 + rnd(40)], ["RARE_FORM_CHANGE_ITEM", true, () => 1 + rnd(40)],
      ["EVOLUTION_TRACKER_GIMMIGHOUL", false, () => undefined], ["MYSTERY_ENCOUNTER_SHUCKLE_JUICE", true, () => (rnd(2) ? 10 : -15)],
      ["MYSTERY_ENCOUNTER_OLD_GATEAU", false, () => undefined], ["MYSTERY_ENCOUNTER_MACHO_BRACE", false, () => undefined],
      ["MYSTERY_ENCOUNTER_BLACK_SLUDGE", false, () => undefined],
    ];
    const funcs = modifierTypes as any;
    const mk = (id: string, mon: any, arg: number | undefined, stack: number): boolean => {
      try {
        const gen = funcs[id];
        if (!gen) return false;
        let type: any;
        if (arg === undefined) type = gen();
        else if (id === "SPECIES_STAT_BOOSTER" || id === "RARE_SPECIES_STAT_BOOSTER") type = EncounterPhaseUtils.generateModifierType(gen, [ssbKeys[arg]]) as any;
        else type = EncounterPhaseUtils.generateModifierType(gen, [arg]) as any;
        if (!type) return false;
        if (!type.id) type.id = id;
        const modifier = type.newModifier(mon);
        if (!modifier) return false;
        modifier.stackCount = Math.max(1, Math.min(stack, modifier.getMaxStackCount()));   // never past the maximum, as the game cannot reach it
        // a held item that cannot be transferred (the held-modifier config of an encounter can say so: battle-scene.ts addModifier), food included
        if (modifier.isTransferable === true && rnd(9) === 0) { modifier.isTransferable = false; modifier.lockedHere = true; }
        scene.addModifier(modifier, true);
        return true;
      } catch {
        return false;
      }
    };
    const persistent: Array<[string, number]> = [["AMULET_COIN", 5], ["CANDY_JAR", 99], ["BERRY_POUCH", 3], ["HEALING_CHARM", 5]];
    const share = Math.min(CHUNK, N - ci * CHUNK);
    for (let i = 0; i < share; i++) {
      const caseNo = ci * CHUNK + i;
      const k = caseNo % 3;
      const wave = 10 + rnd(171), runSeed = word(), waveSeed = word();
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.enemyParty.length = 0;
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.doEncounterExp = undefined;
      encounter.doEncounterRewards = undefined;
      encounter.misc = undefined;
      encounter.dialogueTokens = {};
      scene.modifiers.length = 0;
      party.length = 0;
      Phaser.Math.RND.sow([`deli-build-${SEED}-${caseNo}`]);
      const nParty = [1, 2, 2, 3, 3, 3, 4, 4, 5, 6, 6, 6][rnd(12)];
      for (let j = 0; j < nParty; j++) {
        const sp = all[rnd(all.length)];
        const mon = scene.addPlayerPokemon(sp, 1 + rnd(rnd(2) ? 100 : 25));
        if (sp.forms.length > 1 && rnd(10) < 4) mon.formIndex = rnd(sp.forms.length);
        mon.fusionSpecies = null;
        mon.nature = rnd(25);
        mon.calculateStats();
        party.push(mon);
      }
      // held items: a few kinds each, stacked now and then; a Gimmighoul tracker may sit on any species
      for (const mon of party) {
        const n = rnd(4) === 0 ? 0 : 1 + rnd(4);
        for (let a = 0; a < n; a++) {
          const [id, takesArg, argf] = heldPool[rnd(heldPool.length)];
          mk(id, mon, takesArg ? argf() : undefined, 1 + rnd(rnd(3) === 0 ? 7 : 3));
        }
        if (rnd(7) === 0) mk("SHELL_BELL", mon, undefined, rnd(2) ? 6 : 1 + rnd(5));   // a Shell Bell now and then at its maximum
      }
      // the gifts' own modifiers: none, some, or full
      const full = [rnd(3) === 0, rnd(3) === 0, rnd(3) === 0, rnd(3) === 0];
      persistent.forEach(([id, max], j) => {
        if (full[j]) mk(id, undefined, undefined, max);
        else if (rnd(2)) mk(id, undefined, undefined, 1 + rnd(max - 1));
      });
      if (rnd(2)) mk("SHINY_CHARM", undefined, undefined, 1 + rnd(3));
      if (rnd(3) === 0) mk("EXP_SHARE", undefined, undefined, 1 + rnd(3));
      party.forEach((mon: any) => mon.calculateStats());
      for (const mon of party) mon.hp = rnd(6) === 0 ? 0 : 1 + rnd(mon.getMaxHp());
      const ids = party.map((p: any) => p.id);
      const holder = (m: any) => (m.pokemonId == null ? -1 : (ids.indexOf(m.pokemonId) >= 0 ? ids.indexOf(m.pokemonId) : -2));
      const list = () => scene.modifiers.map((m: any) => `${m.type.id} ${argOf(m.type)} ${holder(m)} ${m.stackCount}`);
      const listX = () => scene.modifiers.map((m: any) => `${m.type.id} ${argOf(m.type)} ${holder(m)} ${m.stackCount} ${m.lockedHere ? 2 : typeof m.isTransferable === "boolean" ? +m.isTransferable : -1}`);   // 2: locked here, 1 and 0: as the game has it
      // the money: short of the price now and then, above it mostly, huge now and then
      const price = scene.getWaveMoneyAmount(2);
      const moneyKind = rnd(8);
      scene.money = moneyKind === 0 ? Math.max(0, price - 1 - rnd(price + 1)) : moneyKind === 1 ? price : moneyKind === 2 ? Number.MAX_SAFE_INTEGER - rnd(5000) : price + rnd(100000);
      gameData.trainerId = rnd(65536);
      gameData.secretId = rnd(65536);

      // the requirements, as the encounter and its options work them out
      Phaser.Math.RND.sow([`deli-${SEED}-${caseNo}`]);
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const spawnPrimary = encounter.meetsPrimaryRequirementAndPrimaryPokemonSelected() ? 1 : 0;
      const spawnScene = encounter.requirements.every((r: any) => r.meetsRequirement()) ? 1 : 0;
      // the party screen as the options' own onPreOptionPhase drives it: the filter it gives, and the list onPokemonSelected answers for the
      // Pokemon (each entry's handler stores the modifier it names)
      const screenOf = (opt: number, mon: any): { selectable: number, items: any[] } => {
        let selectable = 0, items: any[] = [];
        select.mockImplementation(((cb: any, _none: any, filter: any) => {
          selectable = filter(mon) === null ? 1 : 0;
          const r = cb(mon);
          items = Array.isArray(r) ? r.map((x: any) => { x.handler(); return encounter.misc.chosenModifier; }) : [];
          return Promise.resolve(true);
        }) as any);
        void encounter.options[opt].onPreOptionPhase();
        return { selectable, items };
      };
      const screens1 = party.map((p: any) => screenOf(1, p)), screens2 = party.map((p: any) => screenOf(2, p));
      const f1 = screens1.map((s2: any) => s2.selectable), f2 = screens2.map((s2: any) => s2.selectable);
      const lists1 = screens1.map((s2: any) => s2.items), lists2 = screens2.map((s2: any) => s2.items);
      const fmtList = (l: any[]) => `${l.length} ${l.map((m: any) => `${m.type.id} ${argOf(m.type)} ${m.stackCount}`).join(" ")}`;
      const mons = party.map((p: any) => `${p.species.speciesId} ${p.formIndex} ${p.level} ${p.hp > 0 ? 1 : 0}`);

      // the Pokemon and the item the case picks
      let pickMon = 0, pickSub = 0, runs = meets[k] === 1;
      if (k > 0) {
        const lists = k === 1 ? lists1 : lists2;
        const withItems = lists.map((l: any[], j: number) => (l.length ? j : -1)).filter((j: number) => j >= 0);
        if (withItems.length) { pickMon = withItems[rnd(withItems.length)]; pickSub = rnd(lists[pickMon].length); } else runs = false;
      }
      // which gift the case gives and whether its stack is full (the Shell Bell then goes to the field's Pokemon: a fainted lead would throw upstream)
      const chosenType = k === 0 ? "AMULET_COIN" : k === 2 ? "HEALING_CHARM" : lists1[pickMon]?.[pickSub]?.constructor.name === "BerryModifier" ? "CANDY_JAR" : "BERRY_POUCH";
      const stackOf = (id: string) => scene.modifiers.filter((m: any) => m.type.id === id).reduce((n: number, m: any) => n + m.stackCount, 0);
      const maxOf = persistent.find(([id]) => id === chosenType)![1];
      const isFull = stackOf(chosenType) >= maxOf;
      if (runs && isFull && k < 2 && party[0].hp <= 0) party[0].hp = 1;
      party.forEach((mon: any, j: number) => { mons[j] = `${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.hp > 0 ? 1 : 0}`; });
      const before0 = list();
      lines.push(`C ${caseNo} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.money} ${party.length} ${runs ? 1 : 0} ${pickMon} ${pickSub}`,
        `U ${mons.join(" ")}`, `L ${before0.length} ${listX().join(" ")}`,
        `A ${meets.join(" ")} ${spawnPrimary} ${spawnScene} ${f1.join(" ")} ${f2.join(" ")}`,
        `H ${lists1.map(fmtList).join(" ")}`, `I ${lists2.map(fmtList).join(" ")}`, `Z ${rng()}`);
      for (const m of scene.modifiers) { cover.items.add(m.type.id); if (m.isTransferable === false) cover.nonTransferable++; }
      cover.fainted += party[0].hp <= 0 ? 1 : 0;
      cover.notSpawnable += spawnPrimary ? 0 : 1;
      if (!runs) {
        cover.disabled++;
        cover.tooPoor += k === 0 ? 1 : 0;
        cover.noItem += k > 0 ? 1 : 0;
        lines.push("E");
        continue;
      }

      const option = encounter.options[k];
      encounter.selectedOption = option;
      const chosenModifier = k === 0 ? null : (k === 1 ? lists1 : lists2)[pickMon][pickSub];
      select.mockImplementation(((cb: any) => new Promise<boolean>(res => setTimeout(() => {
        const r = cb(party[pickMon]);
        if (Array.isArray(r) && r.length) r[pickSub].handler();
        res(true);
      }, 0))) as any);
      const queued: string[] = [];
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...a: any[]) => {
        queued.push(name === "ModifierRewardPhase" ? `ModifierRewardPhase:${funcKey.get(a[0]) ?? "?"}` : name);
      }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      keys.length = 0; leave.mockClear();
      Phaser.Math.RND.sow([`deli-run-${SEED}-${caseNo}`]);
      const before = rng(), money0 = scene.money;
      let pre: Promise<boolean> | undefined;
      if (option.onPreOptionPhase) scene.executeWithSeedOffset(() => { pre = option.onPreOptionPhase(); }, encounter.getSeedOffset());
      await pre;
      const afterPre = rng();
      const chosenToken = encounter.dialogueTokens.chosenItem ?? "-";
      let done: Promise<void> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      const afterOption = rng();
      for (const fn of [pm.unshiftNew, pm.pushNew]) vi.mocked(fn).mockRestore();
      expect(leave).toHaveBeenCalledTimes(1);
      const shellBell = keys.some(x => x.startsWith("rewardGain"));
      lines.push(`R ${before}`, `B ${afterPre}`, `Q ${afterOption} ${scene.money} ${leave.mock.calls[0][0] ? 1 : 0} ${money0}`,
        `N ${keys.length} ${keys.join(" ")}`, `X ${queued.length} ${queued.join(" ")}`, `T ${chosenToken.replace(/ /g, "_")}`,
        `V ${list().length} ${list().join(" ")}`, "E");
      cover.options[k]++;
      cover.shellBell[k] += shellBell ? 1 : 0;
      cover.amulet += k === 0 && stackOf("AMULET_COIN") > 0 ? 1 : 0;
      if (chosenModifier) {
        if (chosenModifier.constructor.name === "BerryModifier") cover.berry++; else if (k === 1) cover.seed++;
      }
      void shellBell;
      if (isFull) cover.shellBellFull++;
    }
  }, Number(process.env.GC_DELI_TIMEOUT ?? 3600000));
}

it("writes the delibirdy trace", () => {
  lines.unshift(`V 1 ${N}`);
  lines.push(`Z ${N}`);
  fs.writeFileSync(process.env.GC_DELI_OUT ?? "/tmp/delibirdy.txt", lines.join("\n") + "\n");
  console.log(`DELIBIRDY ${SEED}: options=${cover.options} disabled=${cover.disabled} (too poor ${cover.tooPoor}, no item ${cover.noItem}) shell bell given=${cover.shellBell} stacks full=${cover.shellBellFull} berries=${cover.berry} seeds=${cover.seed} not spawnable=${cover.notSpawnable} fainted lead=${cover.fainted} item kinds=${cover.items.size} non-transferable=${cover.nonTransferable}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
});
