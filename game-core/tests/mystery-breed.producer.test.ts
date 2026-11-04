/* Uncommon Breed oracle (the harness family of mystery-fiery.producer.test.ts). The real encounter is started through the game manager; per case the
 * run and wave seeds, the biome, the wave (10 * n + 2..4), the party (levels, charming moves, fainted members, shiny luck, held berries) are changed,
 * onInit runs as EncounterPhase runs it (in executeWithSeedOffset(.., waveIndex)), and one option runs as the phases run it: onOptionPhase in
 * executeWithSeedOffset(.., getSeedOffset() * 500). initBattleWithEnemyConfig is REALLY run for the fight (no await before it: the option's stream; the item
 * rolls after its await: the global RNG); catchPokemon is replaced by a recorder that adds the Pokemon to the party with the real Pokemon.addToParty
 * (the dex, the texts and the party-full prompt are the UI's); the Pokemon's post-summon effect is called by hand the way PostSummonPhase does.
 * GC_BRD_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { allMoves, modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import * as AnimUtils from "#utils/anim-utils";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import * as Dialogue from "#mystery-encounters/encounter-dialogue-utils";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as EncounterPokemonUtils from "#mystery-encounters/encounter-pokemon-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { timedEventManager } from "#app/global-event-manager";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_BRD_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.PIKACHU, SpeciesId.BULBASAUR, SpeciesId.EEVEE],
  [SpeciesId.BULBASAUR, SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE, SpeciesId.MAGNEMITE],
  [SpeciesId.SLUGMA, SpeciesId.RATTATA, SpeciesId.LEAFEON, SpeciesId.CHERUBI, SpeciesId.SNORLAX],
  [SpeciesId.EEVEE],
  [SpeciesId.LEAFEON, SpeciesId.TROPIUS, SpeciesId.SNORLAX],
  [SpeciesId.RATTATA, SpeciesId.BRONZONG, SpeciesId.PIKACHU, SpeciesId.CHARMANDER],
  [SpeciesId.PIKACHU, SpeciesId.BULBASAUR, SpeciesId.EEVEE, SpeciesId.RATTATA, SpeciesId.SLUGMA, SpeciesId.SNORLAX],
];
const BIOMES = [BiomeId.PLAINS, BiomeId.FOREST, BiomeId.CAVE, BiomeId.LAKE, BiomeId.MOUNTAIN, BiomeId.GRAVEYARD, BiomeId.SPACE, BiomeId.VOLCANO, BiomeId.SEA, BiomeId.TOWN];
const CHARMING = [MoveId.CHARM, MoveId.FLATTER, MoveId.DRAGON_CHEER, MoveId.ALLURING_VOICE, MoveId.ATTRACT, MoveId.SWEET_SCENT, MoveId.CAPTIVATE, MoveId.AROMATIC_MIST];

const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "MINT": return t.nature;
    case "SPECIES_STAT_BOOSTER": case "RARE_SPECIES_STAT_BOOSTER":
      return ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_TOOTH", "DEEP_SEA_SCALE"].indexOf(t.key);
  }
  return -1;
};

const lines: string[] = [`V 1 ${N}`];
const cover = { options: [0, 0, 0], skipped: 0, skipped_full: 0, items: 0, egg_self: 0, egg_attack: 0, no_egg: 0, short_moveset: 0, berries_gone: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

/** a Pokemon as one record: species form level boss segments ivs nature abilityIndex passive shiny variant gender friendship id hp max stats n moves */
const dump = (e: any) => `${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`;

for (let ci = 0; ci < configs.length; ci++) {

  it(`uncommon breed, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(42).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.PLAINS, [MysteryEncounterType.UNCOMMON_BREED]]]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.UNCOMMON_BREED, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    scene.time.delayedCall = () => ({}) as any;
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    const expCalls = vi.spyOn(EncounterPhaseUtils, "setEncounterExp").mockImplementation(() => {});
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((async () => {}) as any);
    vi.spyOn(AnimUtils, "playTween").mockImplementation((async () => {}) as any);
    const caught: string[] = [];
    vi.spyOn(EncounterPokemonUtils, "catchPokemon").mockImplementation((async (pokemon: any, ball: any, type: any, show: any, obtain: any) => {
      const pre = dump(pokemon);
      const np = party.length < 6 ? pokemon.addToParty(type) : null;
      caught.push(`${pre} ; ${ball === null ? 1 : 0} ${type} ${show ? 1 : 0} ${obtain ? 1 : 0} ${np ? 1 : 0} ${np ? `${np.pokeball} ${dump(np)}` : ""}`);
    }) as any);
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
    const pm = scene.phaseManager;
    for (let i = 0; i < share; i++) {
      const k = i % 3, wave = 10 * (1 + rnd(17)) + 2 + rnd(3), runSeed = word(), waveSeed = word(), biome = BIOMES[rnd(BIOMES.length)];
      scene.setSeed(runSeed); scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.enemyParty = [];
      scene.newArena(biome);
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = null;
      encounter.startOfBattleEffects.length = 0;
      scene.modifiers.length = 0;
      scene.enemyModifiers.length = 0;
      party.length = 0; party.push(...original);
      const mons: string[] = [];
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        mon.level = 5 + rnd(96);
        mon.nature = rnd(25);
        mon.ivs = [0, 1, 2, 3, 4, 5].map(() => rnd(32));
        const ids: number[] = [];
        const n = 1 + rnd(4);
        while (ids.length < n) {
          const id = rnd(3) === 0 ? CHARMING[rnd(CHARMING.length)] : 1 + rnd(allMoves.length - 1);
          if (allMoves[id] && !ids.includes(id)) ids.push(id);
        }
        mon.moveset = ids.map(id => new PokemonMove(id));
        mon.shiny = rnd(5) === 0;
        mon.variant = mon.shiny ? rnd(3) : 0;
        mon.luck = mon.shiny ? mon.variant + 1 : 0;
        mon.abilityIndex = rnd(2);
        mon.customPokemonData.ability = AbilityId.BALL_FETCH;   /* nothing that reacts to the summon */
        mon.status = null;
        mon.calculateStats();
      }
      const adds: Array<() => void> = [];
      for (const mon of party) {
        if (rnd(2) === 0) adds.push(mk("BERRY", mon, rnd(11), 1 + rnd(3)));
        if (rnd(3) === 0) adds.push(mk("BERRY", mon, rnd(11), 1 + rnd(2)));
        if (rnd(4) === 0) adds.push(mk("LEFTOVERS", mon));
        if (rnd(4) === 0) adds.push(mk("BASE_STAT_BOOSTER", mon, rnd(6), 1 + rnd(3)));
      }
      if (rnd(3) === 0) adds.push(mk("EXP_SHARE", undefined, undefined, 1 + rnd(3)));
      if (rnd(2) === 0) adds.push(mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      scene.pokeballCounts = { 0: rnd(30), 1: rnd(4), 2: rnd(3), 3: rnd(95), 4: rnd(2) };
      for (const mon of party) mon.hp = rnd(5) === 0 ? 0 : 1 + rnd(mon.getMaxHp());
      if (!party.some((mon: any) => mon.hp > 0)) party[0].hp = 1 + rnd(party[0].getMaxHp());
      for (const mon of party) {
        mons.push(`${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.hp} ${mon.getMaxHp()} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((mv: any) => mv.moveId).join(" ")} ${mon.luck} ${mon.getAbility().id}`);
      }
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      scene.money = rnd(5000);
      const before0 = list();
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const primary3 = encounter.options[2].primaryPokemon ? original.indexOf(encounter.options[2].primaryPokemon) : -1;
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId} ${scene.money}`, `U ${mons.join(" ; ")}`,
        `B ${scene.pokeballCounts[0]} ${scene.pokeballCounts[1]} ${scene.pokeballCounts[2]} ${scene.pokeballCounts[3]} ${scene.pokeballCounts[4]}`,
        `L ${before0.length} ${before0.join(" ")}`, `A ${meets.join(" ")} ${meets[2] ? primary3 : -1}`);
      if (!meets[k]) { lines.push("E"); cover.skipped++; continue; }
      if (k > 0 && party.length >= 6) { lines.push("E"); cover.skipped_full++; continue; }

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`brd-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      let initMsgs = 0;
      vi.spyOn(pm, "queueMessage").mockImplementation((() => { initMsgs++; }) as any);
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      vi.mocked(pm.queueMessage).mockRestore();
      const rngInit = rng();
      const pokemon = encounter.misc.pokemon;
      const egg = encounter.misc.eggMove ?? 0;
      lines.push(`R ${rngBefore}`, `I ${rngInit}`, `W ${initMsgs} ${egg} ${pokemon.getNameToRender().replace(/\s+/g, "_")}`, `X0 ${dump(pokemon)}`);
      if (egg === 0) cover.no_egg++;
      else if (allMoves[egg].is("SelfStatusMove")) cover.egg_self++;
      else cover.egg_attack++;

      const option = encounter.options[k];
      encounter.selectedOption = option;
      const primary = option.primaryPokemon ? original.indexOf(option.primaryPokemon) : -1;
      leave.mockClear(); rewards.mockClear(); expCalls.mockClear(); said.length = 0; caught.length = 0;
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
      const exps = expCalls.mock.calls.map((c: any) => { const pid = Array.isArray(c[0]) ? c[0][0] : c[0]; return `${original.findIndex((p: any) => p.id === pid)} ${c[1]} ${c[2] === undefined ? 1 : c[2] ? 1 : 0}`; });
      lines.push(`Q ${rngPre} ${afterOption}`,
        `T ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${(given?.guaranteedModifierTypeFuncs ?? []).length} ${(given?.guaranteedModifierTypeOptions ?? []).length} ${(given?.guaranteedModifierTiers ?? []).length} ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] ? 1 : 0) : -1} ${rewards.mock.calls.length && rewards.mock.calls[0][2] ? 1 : 0}`,
        `X2 ${exps.length} ${exps.join(" ; ")}`,
        `M ${said.length} ${said.map(s => s.replace(/\s+/g, "_")).join(" ")}`,
        `O ${primary} ${scene.money}`,
        `G ${caught.length} ${caught.join(" ; ")}`,
        `V ${list().length} ${list().join(" ")}`);
      cover.options[k]++;
      if (k === 1) cover.berries_gone += list().length < before0.length ? 1 : 0;
      if (k > 0 && pokemon.moveset.length < 4) cover.short_moveset++;

      if (k === 0) {
        const battle = scene.currentBattle, enemies = scene.getEnemyParty();
        expect(enemies.length).toBe(1);
        const fx = encounter.startOfBattleEffects.map((x: any) => `${x.sourceBattlerIndex} ${x.targets.length} ${x.targets.join(" ")} ${x.move.moveId} ${x.useMode}`);
        const enemyItems = scene.enemyModifiers.map((m: any) => `${enemies.findIndex((p: any) => p.id === m.pokemonId)} ${m.type.id} ${argOf(m.type)} ${m.stackCount}`);
        lines.push(`D ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${queued.length} ${queued.join(" ")}`,
          `F ${fx.length} ${fx.join(" ; ")}`, `J ${enemyItems.length} ${enemyItems.join(" ")}`);
        const e = enemies[0];
        lines.push(`X ${dump(e)}`, `N ${e.status?.effect ?? 0} ${e.aiType} ${e.getTag(BattlerTagType.MYSTERY_ENCOUNTER_POST_SUMMON) ? 1 : 0} ${e.teraType}`);
        /* PostSummonPhase's lapse of the tag: the effect, by hand */
        const effects: string[] = [];
        if (e.mysteryEncounterBattleEffects) {
          vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, p: any) => { effects.push(`${name} ${p.changes.map((c: any) => `${c.stat} ${c.stages}`).join(" ")}`); }) as any);
          said.length = 0;
          const r2 = rng();
          e.mysteryEncounterBattleEffects(e);
          const r3 = rng();
          vi.mocked(pm.unshiftNew).mockRestore();
          lines.push(`K ${r2} ${r3} ${effects.length} ${effects.map(s => s.replace(/ /g, "_")).join(" ")} ${said.length} ${said.map(s => s.replace(/\s+/g, "_")).join(" ")}`);
        } else lines.push(`K ${rng()} ${rng()} 0 0`);
        cover.items += enemyItems.length > 0 ? 1 : 0;
      }
      lines.push("E");
      party.length = 0; party.push(...original);
    }
  }, 1200000);
}

it("writes the uncommon breed trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-breed.txt", lines.join("\n") + "\n");
  console.log(`UNCOMMON BREED ${SEED}: options=${cover.options} skipped=${cover.skipped} full party=${cover.skipped_full} egg moves: self=${cover.egg_self} attack=${cover.egg_attack} none=${cover.no_egg} short movesets=${cover.short_moveset} berries removed=${cover.berries_gone} fights with items=${cover.items}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
});
