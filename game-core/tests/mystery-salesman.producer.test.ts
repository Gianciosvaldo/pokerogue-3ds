/* The Pokemon Salesman oracle (card 82's harness, like mystery-actions.producer.test.ts). The real encounter is started
 * through the game manager; for each case the run seed, wave seed, wave, biome, money, trainer and secret ids, Shiny Charms
 * and the party are changed, then the encounter's own onInit runs again exactly as EncounterPhase runs it
 * (executeWithSeedOffset(.., waveIndex)): the Pokemon on sale and its price are read, the option's meetsRequirements() is
 * recorded, and the chosen option's real onOptionPhase is run as MysteryEncounterOptionSelectedPhase runs it
 * (executeWithSeedOffset(.., getSeedOffset() * 500)) with catchPokemon really run: the dex, the party, the messages, the
 * party-full prompt answered through the real UI handlers, the VictoryPhase it unshifts really started. Nothing is mocked
 * but the dialogue texts, the intro visuals' transition and leaveEncounterWithoutBattle (its argument is recorded).
 * Timed events are switched off (the C port keeps them out). The moveset (generateAndPopulateMoveset) is not recorded:
 * game-core has no port of it for a player's Pokemon yet. A third of the cases are made shiny by choosing the trainer id
 * once the Pokemon's own id is known, since shininess draws nothing.
 * GC_MYS_N=400 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { timedEventManager } from "#app/global-event-manager";
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { SpeciesId } from "#enums/species-id";
import { UiMode } from "#enums/ui-mode";
import { Button } from "#enums/buttons";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as EncounterDialogueUtils from "#mystery-encounters/encounter-dialogue-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { CIVILIZATION_ENCOUNTER_BIOMES } from "#mystery-encounters/mystery-encounter-biomes";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { VictoryPhase } from "#phases/victory-phase";
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
const dbg = (m: string) => { if (process.env.GC_SAL_DEBUG) fs.appendFileSync(process.env.GC_SAL_DEBUG, `${m}\n`); };
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");
const BIOMES = [BiomeId.PLAINS, BiomeId.TOWN, BiomeId.FOREST, BiomeId.SEA, BiomeId.CAVE, BiomeId.VOLCANO, BiomeId.SPACE, BiomeId.ISLAND, BiomeId.LABORATORY];
const configs: SpeciesId[][] = [
  [SpeciesId.BULBASAUR],
  [SpeciesId.LAPRAS, SpeciesId.GENGAR, SpeciesId.ABRA],
  [SpeciesId.RATTATA, SpeciesId.MAGIKARP, SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.MEW],
  [SpeciesId.RATTATA, SpeciesId.MAGIKARP, SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.MEW, SpeciesId.GROUDON],
  [SpeciesId.DUGTRIO, SpeciesId.WOBBUFFET, SpeciesId.MEW, SpeciesId.DRAGONITE, SpeciesId.GHOLDENGO, SpeciesId.OCTILLERY],
];
const lines: string[] = [];
const cover = { options: [0, 0], disabled: 0, magikarp: 0, shiny: 0, shinyVariant: 0, hidden: 0, full: [0, 0, 0], started: 0, newCaught: 0, priceMax: 0, species: new Set<number>(), forms: 0, exp: 0 };

/* the numbers game-core compares of a Pokemon (never its moveset) */
const monTokens = (p: any): string =>
  [p.id, p.species.speciesId, p.formIndex, p.level, p.hp, ...p.stats, p.pokeball, p.metWave, p.metLevel, p.metBiome, p.metSpecies, ...p.ivs, p.nature, p.abilityIndex,
    p.gender, +p.shiny, p.variant, p.friendship, p.luck, p.exp, p.teraType, +!!p.passive, +!!p.pokerus].join(" ");
const dexTokens = (gameData: any, id: number): string => {
  const d = gameData.dexData[id], sd = gameData.starterData[id];
  return `${id} ${d.caughtAttr} ${d.natureAttr} ${d.caughtCount} ${d.ivs.join(" ")} ${sd?.candyCount ?? -1} ${sd?.abilityAttr ?? -1} ${d.seenAttr} ${d.seenCount}`;
};

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 40).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {
  it(`salesman, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(37).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    CIVILIZATION_ENCOUNTER_BIOMES.forEach(b => biomes.set(b, [MysteryEncounterType.THE_POKEMON_SALESMAN]));
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    await game.runToMysteryEncounter(MysteryEncounterType.THE_POKEMON_SALESMAN, configs[ci]);
    vi.spyOn(timedEventManager, "isEventActive").mockReturnValue(false);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party = scene.getPlayerParty(), gameData = scene.gameData;
    expect(encounter.options.length).toBe(2);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((() => Promise.resolve()) as any);
    vi.spyOn(EncounterDialogueUtils, "showEncounterDialogue").mockImplementation((() => Promise.resolve()) as any);
    const keys: string[] = [];
    const realT = i18next.t.bind(i18next);
    vi.spyOn(i18next, "t").mockImplementation(((k: any, o?: any) => {
      const key = String(k);
      if (/^battle:(pokemonObtained|addedAsAStarter|partyFull|pokemonCaught)$/.test(key)) keys.push(key.replace(/^.*:/, ""));
      else if (/^partyUiHandler:(goodbye|byebye|farewell|soLong|thisIsWhereWePart|illMissYou|illNeverForgetYou|untilWeMeetAgain|sayonara|smellYaLater)$/.test(key)) keys.push("farewell");
      return realT(k, o);
    }) as any);
    const original = [...party];
    const spriteConfigs = encounter.spriteConfigs.length;
    const baseDex = new Map<number, any>();
    const snapshot = (id: number) => {
      if (!baseDex.has(id)) baseDex.set(id, { dex: structuredClone(gameData.dexData[id]), starter: gameData.starterData[id] ? structuredClone(gameData.starterData[id]) : null });
      const b = baseDex.get(id);
      Object.assign(gameData.dexData[id], structuredClone(b.dex));
      if (b.starter) Object.assign(gameData.starterData[id], structuredClone(b.starter));
    };
    lines.push(`P ${party.length}`);
    const share = Math.floor(N / configs.length);
    const init = (wave: number) => {
      encounter.misc = undefined;
      encounter.spriteConfigs.length = spriteConfigs;
      scene.executeWithSeedOffset(() => { encounter.onInit(); }, wave);
    };
    for (let i = 0; i < share; i++) {
      const k = i % 2, wave = 10 + rnd(171), runSeed = word(), waveSeed = word();
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.enemyParty.length = 0;
      scene.arena.biomeId = BIOMES[rnd(BIOMES.length)];
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.doEncounterExp = undefined;
      encounter.doEncounterRewards = undefined;
      scene.modifiers.length = 0;
      party.length = 0; party.push(...original);
      const charms = rnd(3) === 0 ? 1 + rnd(4) : 0;
      if (charms) {
        const mod = (modifierTypes as any).SHINY_CHARM().newModifier();
        mod.stackCount = charms;
        scene.addModifier(mod, true);
      }
      gameData.trainerId = rnd(65536);
      gameData.secretId = rnd(65536);
      // the party: levels and a fainted one now and then
      const levels: number[] = [];
      for (const mon of party) {
        mon.level = 1 + rnd(rnd(2) ? 100 : 20);
        mon.calculateStats();
        mon.hp = rnd(5) === 0 ? 0 : mon.getMaxHp();
        levels.push(mon.level);
      }
      party[0].hp = party[0].getMaxHp();
      init(wave);
      // a third get the trainer id that makes (trainerId ^ secretId ^ the id's halves) 0 or one of the three values around the
      // shiny threshold (64, doubled 1 + charms times), so an off-by-one in either direction flips the shininess
      if (rnd(3) === 0) {
        const id = encounter.misc.pokemon.id;
        const threshold = 64 * (charms ? 2 ** (1 + charms) : 1);
        const target = [0, threshold - 1, threshold, threshold + 1][rnd(4)];
        gameData.secretId = 0;
        gameData.trainerId = (((id >>> 16) ^ (id & 0xffff)) ^ target) & 0xffff;
        init(wave);
      }
      const mon = encounter.misc.pokemon, price = encounter.misc.price;
      // the dex entries of the species' chain, part-filled
      const chain: number[] = [];
      for (let id: number | null = mon.species.speciesId; id != null; id = speciesDataRegistry.getPrevolution(id)) chain.push(id);
      const root = mon.species.getRootSpeciesId(true);
      if (!chain.includes(root)) chain.push(root);
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
      const required = scene.getWaveMoneyAmount(4);
      const moneys = [required - 1, required, required, price, price + 1, 5 * required, 12345];
      scene.money = Math.max(0, moneys[rnd(moneys.length)]);
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const runs = meets[k] === 1;
      const answer = party.length === 6 ? (rnd(4) === 0 ? -1 : rnd(6)) : -2, release = k === 0 ? answer : -2;
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.money} ${gameData.trainerId} ${gameData.secretId} ${charms} ${scene.arena.biomeId} ${release} ${party.length} ${runs ? 1 : 0}`,
        `U ${party.map((p: any, j: number) => `${p.species.speciesId} ${p.formIndex} ${levels[j]} ${p.hp > 0 ? 1 : 0}`).join(" ")}`,
        `A ${meets.join(" ")} ${price} ${scene.getMaxExpLevel()} ${monTokens(mon)}`,
        `D ${chain.length} ${chain.map(id => dexTokens(gameData, id)).join(" ")}`);
      cover.species.add(mon.species.speciesId);
      if (!runs) { cover.disabled++; lines.push("E"); continue; }

      // a released Pokemon is destroyed: the party holds a copy of it (same id) for that, so the original lives on
      if (release >= 0) {
        const src = original[release];
        party[release] = scene.addPlayerPokemon(src.species, src.level, src.abilityIndex, src.formIndex, src.gender, src.shiny, src.variant, src.ivs, src.nature, src);
      }
      const isNew = (p: any) => !original.some((o: any) => o.id === p.id);
      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); keys.length = 0;
      Phaser.Math.RND.sow([`salesman-${SEED}-${ci}-${i}`]);
      const before = rng(), money0 = scene.money;
      const pm = scene.phaseManager, queued: string[] = [], expPhases: string[] = [];
      let victoryId = -1;
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...a: any[]) => {
        queued.push(name);
        if (name === "VictoryPhase") victoryId = a[0];
      }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      vi.spyOn(pm, "removeAllPhasesOfType").mockImplementation((() => {}) as any);
      const ui = scene.ui;
      vi.spyOn(ui, "showText").mockImplementation(((_t: string, _d: any, cb?: () => void) => { cb?.(); }) as any);
      dbg(`case ${i} option ${k} species ${mon.species.speciesId} start`);
      let done: Promise<boolean> | undefined;
      let settled = false;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      void done!.then(() => { settled = true; });
      // the full party's prompt, answered through the real handlers like the catch oracle does: Yes opens the party
      // screen in release mode (the slot, then Release, then a key for the farewell), No lets the Pokemon go
      const tick = () => new Promise<void>(r => setTimeout(r, 0));
      const waitFor = async (cond: () => boolean) => { for (let t = 0; t < 3000 && !settled && !cond(); t++) await tick(); };
      if (release !== -2) {
        await waitFor(() => ui.mode === UiMode.OPTION_SELECT);
        expect(ui.mode).toBe(UiMode.OPTION_SELECT);
        let h = ui.getHandler();
        h.setCursor(release === -1 ? 3 : 2);
        h.processInput(Button.ACTION);
        if (release >= 0) {
          await waitFor(() => ui.mode === UiMode.PARTY);
          expect(ui.mode).toBe(UiMode.PARTY);
          h = ui.getHandler();
          h.setCursor(release);
          h.processInput(Button.ACTION);   // the slot
          h.processInput(Button.ACTION);   // Release, the first of its options
          await waitFor(() => !!(ui.getHandler() as any).awaitingActionInput);
          ui.getHandler().processInput(Button.ACTION);   // the farewell text waits for a key
        }
      }
      await done;
      dbg(`case ${i} option settled`);
      void settled;
      const afterOption = rng();
      expect(leave).toHaveBeenCalledTimes(1);
      // the VictoryPhase it unshifted, really started
      if (victoryId >= 0) {
        vi.spyOn(pm, "unshiftPhase").mockImplementation(((ph: any) => { expPhases.push(`${ph.phaseName === "ExpPhase" ? 0 : 1} ${ph.index} ${ph.exp}`); }) as any);
        vi.spyOn(pm, "create").mockImplementation(((name: string, index: number, exp: number) => ({ phaseName: name, index, exp })) as any);
        const victory = new VictoryPhase(victoryId, true);
        vi.spyOn(victory, "end").mockImplementation(() => {});
        victory.start();
      }
      for (const fn of [pm.unshiftNew, pm.pushNew, pm.removeAllPhasesOfType, pm.unshiftPhase, pm.create]) (fn as any)?.mockRestore?.();
      vi.mocked(ui.showText).mockRestore();
      const afterVictory = rng();
      const order = party.map((p: any) => original.findIndex((o: any) => o.id === p.id));
      lines.push(`R ${before}`, `Q ${afterOption} ${afterVictory} ${scene.money} ${leave.mock.calls[0][0] ? 1 : 0}`,
        `N ${keys.length} ${keys.join(" ")}`, `X ${queued.length} ${queued.join(" ")} ${victoryId < 0 ? "-1" : victoryId === mon.id ? "id" : "other"}`,
        `F ${expPhases.length} ${expPhases.join(" ")}`,
        `W ${order.length} ${order.join(" ")}`, ...party.filter(isNew).map((p: any) => `M ${monTokens(p)}`),
        `V ${chain.map(id => dexTokens(gameData, id)).join(" ")}`, "E");
      void money0;
      cover.options[k]++;
      cover.exp += expPhases.length;
      if (k === 0) {
        cover.magikarp += mon.species.speciesId === SpeciesId.MAGIKARP ? 1 : 0;
        cover.shiny += mon.shiny ? 1 : 0;
        cover.shinyVariant += mon.shiny && mon.variant > 0 ? 1 : 0;
        cover.hidden += mon.abilityIndex === 2 ? 1 : 0;
        cover.full[release === -2 ? 0 : release === -1 ? 1 : 2]++;
        cover.started += keys.includes("addedAsAStarter") ? 1 : 0;
        cover.forms += mon.formIndex > 0 ? 1 : 0;
        cover.priceMax += price === scene.getWaveMoneyAmount(4) ? 1 : 0;
      }
      party.length = 0; party.push(...original);
      scene.currentBattle.enemyParty.length = 0;
    }
  }, Number(process.env.GC_SAL_TIMEOUT ?? 1200000));
}

it("writes the salesman trace", () => {
  lines.unshift(`V 1 ${N}`);
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-salesman.txt", lines.join("\n") + "\n");
  console.log(`SALESMAN ${SEED}: options=${cover.options} disabled=${cover.disabled} species=${cover.species.size} magikarp=${cover.magikarp} shiny=${cover.shiny} shiny variants=${cover.shinyVariant} hidden=${cover.hidden} party room/no/release=${cover.full} started=${cover.started} max price=${cover.priceMax} exp phases=${cover.exp}`);
  expect(cover.options.every(n => n > 0) && cover.shiny > 0 && cover.magikarp > 0).toBe(true);
});
