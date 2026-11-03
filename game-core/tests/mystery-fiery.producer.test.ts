/* Fiery Fallout oracle (the harness family of mystery-teleport.producer.test.ts). The real encounter is started through the game manager;
 * per case the run and wave seeds, the wave (10 * n + 2..4, 42 on), the weather the wave starts with (none, Sunny, rain, the primal ones...),
 * the party (Fire types, fire resistant abilities by custom ability, fainted members, statuses, Leaf Guard users), the held items are changed,
 * onInit runs as EncounterPhase runs it (with no enemy on the field, as an ME wave has none), and one option runs as the phases run it:
 * onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500). initBattleWithEnemyConfig is REALLY run (no await before it: the option's
 * stream; the item rolls after its await: the global RNG); the callback of setEncounterRewards is called by hand in executeWithSeedOffset(..,
 * waveIndex * 1000) the way MysteryEncounterRewardsPhase does; the Volcarona's post-summon effect by hand the way PostSummonPhase does.
 * GC_FIE_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { Weather } from "#data/weather";
import { Status } from "#data/status-effect";
import { allMoves } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MoveId } from "#enums/move-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { randSeedInt } from "#utils/common";
import * as AnimUtils from "#utils/anim-utils";
import { SpeciesId } from "#enums/species-id";
import { PokemonType } from "#enums/pokemon-type";
import { WeatherType } from "#enums/weather-type";
import { StatusEffect } from "#enums/status-effect";
import { AbilityId } from "#enums/ability-id";
import { PokemonMove } from "#moves/pokemon-move";
import * as Dialogue from "#mystery-encounters/encounter-dialogue-utils";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { timedEventManager } from "#app/global-event-manager";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_FIE_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.PIKACHU, SpeciesId.BULBASAUR, SpeciesId.EEVEE],
  [SpeciesId.BULBASAUR, SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE, SpeciesId.MAGNEMITE],
  [SpeciesId.SLUGMA, SpeciesId.RATTATA, SpeciesId.LEAFEON, SpeciesId.CHERUBI, SpeciesId.SNORLAX, SpeciesId.TORKOAL],
  [SpeciesId.EEVEE],
  [SpeciesId.LEAFEON, SpeciesId.TROPIUS],
  [SpeciesId.RATTATA, SpeciesId.BRONZONG, SpeciesId.PIKACHU, SpeciesId.CHARMANDER],
];
const FIRE_ABILITIES = [AbilityId.FLAME_BODY, AbilityId.FLASH_FIRE, AbilityId.WELL_BAKED_BODY, AbilityId.HEATPROOF, AbilityId.THERMAL_EXCHANGE,
  AbilityId.THICK_FAT, AbilityId.WATER_BUBBLE, AbilityId.MAGMA_ARMOR, AbilityId.WATER_VEIL, AbilityId.STEAM_ENGINE, AbilityId.PRIMORDIAL_SEA];
const WEATHERS = [WeatherType.NONE, WeatherType.NONE, WeatherType.SUNNY, WeatherType.RAIN, WeatherType.SANDSTORM, WeatherType.FOG,
  WeatherType.HEAVY_RAIN, WeatherType.HARSH_SUN, WeatherType.STRONG_WINDS, WeatherType.HAIL, WeatherType.SNOW];
const STATUSES = [StatusEffect.POISON, StatusEffect.TOXIC, StatusEffect.PARALYSIS, StatusEffect.BURN, StatusEffect.SLEEP, StatusEffect.FREEZE];

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
const cover = { options: [0, 0, 0], skipped: 0, fainted_primary: 0, items: 0, burned: 0, burn_refused: 0, damaged: 0, weather_primal: 0, weather_same: 0, weather_set: 0, ability_primary: 0, lead_stack: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {

  it(`fiery fallout, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(42).startingBiome(BiomeId.VOLCANO).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.PLAINS, [MysteryEncounterType.MYSTERIOUS_CHALLENGERS]]]);
    biomes.set(BiomeId.VOLCANO, [MysteryEncounterType.FIERY_FALLOUT]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.FIERY_FALLOUT, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    scene.time.delayedCall = () => ({}) as any;
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    const expCalls = vi.spyOn(EncounterPhaseUtils, "setEncounterExp").mockImplementation(() => {});
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((async () => {}) as any);
    vi.spyOn(AnimUtils, "playTween").mockImplementation((async () => {}) as any);
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
    const weatherOf = () => { const w = scene.arena.weather; return w ? `${w.weatherType} ${w.turnsLeft} ${w.maxDuration}` : "0 0 0"; };
    for (let i = 0; i < share; i++) {
      const k = i % 3, wave = 10 * (4 + rnd(14)) + 2 + rnd(3), runSeed = word(), waveSeed = word();
      scene.setSeed(runSeed); scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.enemyParty = [];
      scene.newArena(BiomeId.VOLCANO);
      const wt = WEATHERS[rnd(WEATHERS.length)];
      scene.arena.weather = wt === WeatherType.NONE ? null : new Weather(wt, rnd(2) ? 0 : 1 + rnd(5));
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
        mon.abilityIndex = rnd(2);
        mon.customPokemonData.ability = rnd(3) === 0 ? FIRE_ABILITIES[rnd(FIRE_ABILITIES.length)] : -1;
        mon.status = rnd(4) === 0 ? new Status(STATUSES[rnd(STATUSES.length)], 0, 3) : null;
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
      /* the lead may already hold boosters of the type that will be drawn (the stack count goes up by one) */
      if (rnd(3) === 0) adds.push(mk("ATTACK_TYPE_BOOSTER", party[0], rnd(18), 1 + rnd(3)));
      if (rnd(3) === 0) adds.push(mk("EXP_SHARE", undefined, undefined, 1 + rnd(3)));
      if (rnd(4) === 0) adds.push(mk("SHINY_CHARM", undefined, undefined, 1 + rnd(3)));
      if (rnd(2) === 0) adds.push(mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      scene.pokeballCounts = { 0: rnd(30), 1: rnd(4), 2: rnd(3), 3: rnd(95), 4: rnd(2) };
      for (const mon of party) {
        mon.hp = rnd(4) === 0 ? 0 : 1 + rnd(mon.getMaxHp());
        /* a Pokemon that fainted carries the Faint status, not the one it had */
        if (mon.hp === 0) mon.status = new Status(StatusEffect.FAINT);
      }
      /* a fight needs one Pokemon that can fight */
      if (!party.some((mon: any) => mon.hp > 0)) party[0].hp = 1 + rnd(party[0].getMaxHp());
      for (const mon of party) {
        const max = mon.getMaxHp();
        mons.push(`${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.ivs.join(" ")} ${mon.hp} ${max} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((mv: any) => mv.moveId).join(" ")} ${mon.luck} ${mon.getAbility().id}`);
      }
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      scene.money = rnd(5000);
      const hpAfter = party.map((p: any) => p.hp);
      const before0 = list();
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      const primary3 = encounter.options[2].primaryPokemon ? original.indexOf(encounter.options[2].primaryPokemon) : -1;
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId} ${scene.money}`, `U ${mons.join(" ; ")}`, `H ${hpAfter.join(" ")}`,
        `S ${party.map((p: any) => p.status?.effect ?? 0).join(" ")}`,
        `B ${scene.pokeballCounts[0]} ${scene.pokeballCounts[1]} ${scene.pokeballCounts[2]} ${scene.pokeballCounts[3]} ${scene.pokeballCounts[4]}`,
        `L ${before0.length} ${before0.join(" ")}`, `A ${meets.join(" ")} ${meets[2] ? primary3 : -1}`);
      if (!meets[k]) { lines.push("E"); cover.skipped++; continue; }

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`fie-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      const weatherBefore = weatherOf();
      let initMsgs = 0, initPhases = 0;
      vi.spyOn(pm, "queueMessage").mockImplementation((() => { initMsgs++; }) as any);
      vi.spyOn(pm, "unshiftNew").mockImplementation((() => { initPhases++; }) as any);
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      vi.mocked(pm.queueMessage).mockRestore();
      vi.mocked(pm.unshiftNew).mockRestore();
      const rngInit = rng();
      lines.push(`R ${rngBefore}`, `I ${rngInit}`, `W ${weatherBefore} ${weatherOf()} ${initMsgs} ${initPhases}`);
      if (wt === WeatherType.SUNNY) cover.weather_same++;
      else if (wt === WeatherType.HEAVY_RAIN || wt === WeatherType.HARSH_SUN || wt === WeatherType.STRONG_WINDS) cover.weather_primal++;
      else cover.weather_set++;

      const option = encounter.options[k];
      encounter.selectedOption = option;
      const primary = option.primaryPokemon ? original.indexOf(option.primaryPokemon) : -1;
      leave.mockClear(); rewards.mockClear(); expCalls.mockClear(); said.length = 0;
      const queued: string[] = [];
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string) => { queued.push(name); }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      const statusBefore = party.map((p: any) => p.status?.effect ?? 0);
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
        `Y ${original.map((p: any) => `${p.hp} ${p.getMaxHp()} ${p.stats.join(" ")} ${p.status?.effect ?? 0} ${p.getAbility().id} ${p.customPokemonData.ability}`).join(" ; ")}`,
        `V ${list().length} ${list().join(" ")}`);
      cover.options[k]++;
      if (k === 2 && primary >= 0 && original[primary].hp <= 0) cover.fainted_primary++;
      if (k === 2 && primary >= 0 && !original[primary].isOfType(PokemonType.FIRE, { includeTeraType: false })) cover.ability_primary++;
      if (k === 1) {
        const now = party.map((p: any) => p.status?.effect ?? 0);
        for (let m = 0; m < now.length; m++) if (statusBefore[m] === 0 && now[m] === StatusEffect.BURN) cover.burned++;
        if (said.some(s => s.endsWith("targetBurned")) === false && hpAfter.some((h: number, m: number) => h > 0)) cover.burn_refused++;
        cover.damaged += original.filter((p: any, m: number) => p.hp < hpAfter[m]).length;
      }

      /* the battle's first SummonPhase swaps a fainted lead with the first Pokemon behind it that can fight, for the second slot of a double battle too when two can fight (the oracle plays no battle, so it is done here) */
      if (k === 0 && party.length > 1) {
        const able = party.filter((p: any) => p.isAllowedInBattle()).length;
        for (let slot = 0; slot < (able > 1 ? 2 : 1); slot++) {
          if (party[slot].isAllowedInBattle()) continue;
          const legal = party.findIndex((p: any, n: number) => n > slot && p.isAllowedInBattle());
          if (legal > slot) [party[slot], party[legal]] = [party[legal], party[slot]];
        }
      }
      /* setEncounterRewards' callback, as MysteryEncounterRewardsPhase runs it */
      {
        const cb = rewards.mock.calls.length ? (rewards.mock.calls[0][2] as any) : undefined;
        if (cb) {
          const items0 = list();
          const stackBefore = items0.filter(s => s.startsWith("ATTACK_TYPE_BOOSTER") && s.split(" ")[2] === "0").length;
          said.length = 0;
          const r0 = rng();
          scene.executeWithSeedOffset(() => { cb(); }, wave * 1000);
          const r1 = rng();
          const items1 = list();
          cover.lead_stack += stackBefore > 0 ? 1 : 0;
          lines.push(`CB ${r0} ${r1} ${said.length} ${said.map(s => s.replace(/\s+/g, "_")).join(" ")} ${items1.length} ${items1.join(" ")}`);
        } else lines.push("CB");
      }

      if (k === 0) {
        const battle = scene.currentBattle, enemies = scene.getEnemyParty();
        expect(enemies.length).toBe(2);
        const fx = encounter.startOfBattleEffects.map((x: any) => `${x.sourceBattlerIndex} ${x.targets.length} ${x.targets.join(" ")} ${x.move.moveId} ${x.useMode}`);
        const enemyItems = scene.enemyModifiers.map((m: any) => `${enemies.findIndex((p: any) => p.id === m.pokemonId)} ${m.type.id} ${argOf(m.type)} ${m.stackCount}`);
        lines.push(`D ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${queued.length} ${queued.join(" ")}`,
          `F ${fx.length} ${fx.join(" ; ")}`, `J ${enemyItems.length} ${enemyItems.join(" ")}`);
        for (let n = 0; n < 2; n++) {
          const e = enemies[n];
          lines.push(`X ${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`,
            `N ${e.status?.effect ?? 0} ${e.aiType} ${e.getTag(BattlerTagType.MYSTERY_ENCOUNTER_POST_SUMMON) ? 1 : 0} ${e.teraType}`);
          /* PostSummonPhase's lapse of the tag: the effect, by hand */
          const effects: string[] = [];
          if (e.mysteryEncounterBattleEffects) {
            vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, p: any) => { effects.push(`${name} ${p.changes.map((c: any) => `${c.stat} ${c.stages}`).join(" ")}`); }) as any);
            const r2 = rng();
            e.mysteryEncounterBattleEffects(e);
            const r3 = rng();
            vi.mocked(pm.unshiftNew).mockRestore();
            lines.push(`K ${r2} ${r3} ${effects.length} ${effects.map(s => s.replace(/ /g, "_")).join(" ")}`);
          } else lines.push(`K ${rng()} ${rng()} 0`);
        }
        cover.items += enemyItems.length > 0 ? 1 : 0;
      }
      lines.push("E");
      party.length = 0; party.push(...original);
    }
  }, 1200000);
}

it("writes the fiery fallout trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-fiery.txt", lines.join("\n") + "\n");
  console.log(`FIERY FALLOUT ${SEED}: options=${cover.options} skipped=${cover.skipped} fainted primaries=${cover.fainted_primary} ability-only primaries=${cover.ability_primary} weather: set=${cover.weather_set} already sunny=${cover.weather_same} primal=${cover.weather_primal} burned=${cover.burned} burn refused=${cover.burn_refused} damaged=${cover.damaged} lead already held boosters=${cover.lead_stack} fights with items=${cover.items}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
});
