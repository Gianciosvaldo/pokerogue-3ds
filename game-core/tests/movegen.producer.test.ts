/* The generic moveset generator's oracle (game-core/src/gc_movegen.c, tests/movegen_diff_test.c): upstream's own
 * generateAndPopulateMoveset (EnemyPokemon's: Smeargle and Eternatus special-cased, the rest src/ai/ai-moveset-gen.ts
 * generateMoveset) for a wild, non-boss, trainerless Pokemon, run in executeWithSeedOffset(.., offset, seed) on a Pokemon made
 * from a random species, form, level, ability slot, gender, nature, IVs and the arena's weather. Per case it records the
 * Pokemon, the moveset, the stats and the next two draws of the RNG (so the RNG used is compared as well), and for the "E"
 * records the Move.calculateEffectivePower(pokemon) of every damaging move for the same kind of Pokemon with a given
 * ability. GC_MG_N=1500 GC_MG_E=60 GC_MG_SEED=777 GC_MG_OUT=/abs/movegen.txt, Node22. */
import { allAbilities, allMoves } from "#data/data-lists";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { generateMoveset } from "#ai/ai-moveset-gen";
import { globalScene } from "#app/global-scene";
import { randSeedInt } from "#utils/common";
import { MoveCategory } from "#enums/move-category";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

const N = Number(process.env.GC_MG_N ?? 1500), NE = Number(process.env.GC_MG_E ?? 60), SEED = Number(process.env.GC_MG_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
});

it("movegen oracle", async () => {
  const game = new GameManager(phaserGame);
  game.override.startingWave(45).startingBiome(1);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const out = process.env.GC_MG_OUT ?? "/tmp/movegen.txt";
  const lines: string[] = ["V 1"];
  const allSpecies = speciesDataRegistry.getAllSpecies();
  const e = scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR), 20, 0, false, true);
  const damaging: number[] = [];
  for (const m of allMoves) if (m && m.id > 0 && m.category !== MoveCategory.STATUS && m.power > 0) damaging.push(m.id);
  const powerAbilities: number[] = [];
  for (const ab of allAbilities) if (ab.hasAttr("AiMovegenMoveStatsAbAttr") || ab.hasAttr("VariableMovePowerAbAttr")) powerAbilities.push(ab.id);
  const speciesIds = allSpecies.filter(s => s != null && s.speciesId > 0).map(s => s.speciesId);

  /* species that learn the rare moves (the ones filterUselessMoves and the category adjustments act on), so the random draw reaches them */
  const rareIds = new Set<number>([MoveId.AURORA_VEIL, MoveId.SOLAR_BEAM, MoveId.SOLAR_BLADE, MoveId.VENOM_DRENCH, MoveId.RAIN_DANCE, MoveId.SUNNY_DAY,
    MoveId.SANDSTORM, MoveId.HAIL, MoveId.SNOWSCAPE, MoveId.CURSE, MoveId.BULK_UP, MoveId.HONE_CLAWS, MoveId.CALM_MIND, MoveId.TAKE_HEART]);
  for (const m of allMoves) {
    if (m && m.id > 0 && (m.hasAttr("DefAtkAttr") || m.hasAttr("PhotonGeyserCategoryAttr") || m.hasAttr("ShellSideArmCategoryAttr") || m.hasAttr("WeatherChangeAttr")
      || m.hasAttr("TargetAtkUserAtkAttr") || m.hasAttr("ThunderAccuracyAttr") || m.hasAttr("StormAccuracyAttr") || m.hasAttr("MultiHitPowerIncrementAttr")
      || m.hasAttr("DelayedAttackAttr") || m.hasAttr("RechargeAttr") || m.hasAttr("VariableMoveTypeAttr"))) rareIds.add(m.id);
  }
  const weatherMoves = new Set<number>([MoveId.RAIN_DANCE, MoveId.SUNNY_DAY, MoveId.SANDSTORM, MoveId.HAIL, MoveId.SNOWSCAPE, MoveId.AURORA_VEIL, MoveId.SOLAR_BEAM,
    MoveId.SOLAR_BLADE, MoveId.WEATHER_BALL]);
  const focus: number[] = [], weatherFocus: number[] = [];
  for (const sid of speciesIds) {
    try {
      e.species = speciesDataRegistry.getSpecies(sid);
      e.formIndex = 0;
      e.fusionSpecies = null;
      const rows = e.getLevelMoves({ startingLevel: 1, includeEvolutionMoves: true, includePrevolutionMoves: true } as any);
      if (rows.some((r: any) => rareIds.has(r[1]))) focus.push(sid);
      if (rows.some((r: any) => weatherMoves.has(r[1]))) weatherFocus.push(sid);
    } catch {
      /* not a species with a moveset */
    }
  }
  /* abilities the generator looks at by id, the weather summoners, and the ones that change a move's power: forced onto a third of the Pokemon */
  const relevant = new Set<number>(powerAbilities);
  for (const a of [AbilityId.RAIN_DISH, AbilityId.FORECAST, AbilityId.SWIFT_SWIM, AbilityId.DRY_SKIN, AbilityId.CHLOROPHYLL, AbilityId.FLOWER_GIFT, AbilityId.PROTOSYNTHESIS,
    AbilityId.HARVEST, AbilityId.SOLAR_POWER, AbilityId.SLUSH_RUSH, AbilityId.ICE_FACE, AbilityId.SNOW_CLOAK, AbilityId.ICE_BODY, AbilityId.SAND_FORCE, AbilityId.SAND_RUSH,
    AbilityId.SAND_VEIL, AbilityId.SNOW_WARNING, AbilityId.SHEER_FORCE, AbilityId.NEUROFORCE]) relevant.add(a);
  for (const ab of allAbilities) if (ab.hasAttr("PostSummonWeatherChangeAbAttr") || ab.hasAttr("PostDefendWeatherChangeAbAttr") || ab.hasAttr("PreAttackWeatherOverrideAbAttr") || ab.hasAttr("SuppressWeatherEffectAbAttr")) relevant.add(ab.id);
  const relevantList = [...relevant].filter(a => a > 0);
  /* the abilities filterUselessMoves keeps a weather move for, and the weather summoners */
  const weatherAbilities = [AbilityId.RAIN_DISH, AbilityId.FORECAST, AbilityId.SWIFT_SWIM, AbilityId.DRY_SKIN, AbilityId.CHLOROPHYLL, AbilityId.FLOWER_GIFT, AbilityId.PROTOSYNTHESIS,
    AbilityId.HARVEST, AbilityId.SOLAR_POWER, AbilityId.SLUSH_RUSH, AbilityId.ICE_FACE, AbilityId.SNOW_CLOAK, AbilityId.ICE_BODY, AbilityId.SAND_FORCE, AbilityId.SAND_RUSH,
    AbilityId.SAND_VEIL, AbilityId.SNOW_WARNING, AbilityId.DROUGHT, AbilityId.DRIZZLE, AbilityId.SAND_STREAM, AbilityId.SAND_SPIT, AbilityId.SNOW_WARNING, AbilityId.ORICHALCUM_PULSE, AbilityId.DROUGHT];
  const setWeather = (w: number) => {
    scene.arena.weather = w ? ({ weatherType: w, isImmutable: () => false, turnsLeft: 0, maxDuration: 0, isEffectSuppressed: () => false } as any) : null;
  };
  /* a random Pokemon of a random species, in the same object: nothing is rolled from the RNG under test */
  const shape = (forcedAbility: number, wf = false) => {
    const sid = wf ? weatherFocus[rnd(weatherFocus.length)] : focus.length && rnd(2) ? focus[rnd(focus.length)] : speciesIds[rnd(speciesIds.length)];
    const sp = speciesDataRegistry.getSpecies(sid);
    const nForms = sp.forms.length;
    const form = nForms > 1 && rnd(3) ? rnd(nForms) : 0;
    const level = rnd(4) === 0 ? 1 + rnd(150) : 1 + rnd(100);
    const abilityIndex = rnd(3);
    const gender = [-1, 0, 1][rnd(3)];
    const nature = rnd(25);
    const ivs = Array.from({ length: 6 }, () => rnd(32));
    const weather = rnd(2) ? 0 : rnd(10);
    e.species = sp;
    e.fusionSpecies = null;
    e.formIndex = form;
    e.level = level;
    e.abilityIndex = abilityIndex;
    e.customPokemonData.ability = forcedAbility;
    e.gender = gender;
    e.nature = nature;
    e.ivs = ivs;
    e.moveset = [];
    e.calculateStats();
    e.hp = e.stats[0];
    setWeather(weather);
    return { sp, form, level, abilityIndex, gender, nature, ivs, weather };
  };

  let skipped = 0;
  const reasons = new Map<string, number>();
  for (let i = 0; i < N; i++) {
    let s;
    try {
      const wf = rnd(4) === 0;
      s = wf ? shape(weatherAbilities[rnd(weatherAbilities.length)], true) : shape(rnd(3) === 0 ? relevantList[rnd(relevantList.length)] : -1);
    } catch (err) {
      skipped++;
      reasons.set(`shape: ${String(err).slice(0, 80)}`, (reasons.get(`shape: ${String(err).slice(0, 80)}`) ?? 0) + 1);
      continue;
    }
    if (e.hasPassive() || e.isBoss()) { skipped++; reasons.set("passive/boss", (reasons.get("passive/boss") ?? 0) + 1); continue; }
    const offset = rnd(2000), waveSeed = word();
    let moves: number[] = [], d1 = 0, d2 = 0;
    try {
      scene.executeWithSeedOffset(() => {
        e.generateAndPopulateMoveset();
        moves = e.moveset.map((m: any) => m.moveId);
        d1 = randSeedInt(1000000);
        d2 = randSeedInt(1000000);
      }, offset, waveSeed);
    } catch (err) {
      skipped++;
      reasons.set(`gen: ${String(err).slice(0, 80)}`, (reasons.get(`gen: ${String(err).slice(0, 80)}`) ?? 0) + 1);
      continue;
    }
    lines.push(`M ${i} ${s.sp.speciesId} ${s.form} ${s.level} ${s.abilityIndex} ${e.customPokemonData.ability} ${e.getAbility().id} ${s.gender} ${s.nature} ${s.ivs.join(" ")} ${s.weather} ${offset} ${waveSeed} ${e.stats.join(" ")} ${moves.length} ${moves.join(" ")} ${d1} ${d2}`);
  }
  /* effective power: the same kind of Pokemon with a given ability, every damaging move */
  for (let i = 0; i < NE; i++) {
    const ability = i % 3 === 0 ? allAbilities[1 + rnd(allAbilities.length - 1)].id : powerAbilities[rnd(powerAbilities.length)];
    let s;
    try {
      s = shape(ability);
    } catch {
      skipped++;
      continue;
    }
    if (rnd(2)) {
      /* the weather the ability's power rows (Sand Force, Solar Power, Rivalry's gender, ...) answer to */
      s.weather = 1 + rnd(9);
      setWeather(s.weather);
    }
    globalScene.movesetGenInProgress = true;
    const powers = damaging.map(id => allMoves[id].calculateEffectivePower(e));
    globalScene.movesetGenInProgress = false;
    lines.push(`E ${i} ${s.sp.speciesId} ${s.form} ${s.level} ${s.abilityIndex} ${ability} ${s.gender} ${s.nature} ${s.ivs.join(" ")} ${s.weather} ${e.stats.join(" ")} ${powers.length} ${damaging.map((id, k) => `${id} ${powers[k]}`).join(" ")}`);
  }
  console.log(`SKIPPED ${JSON.stringify([...reasons])}`);
  fs.writeFileSync(out, `${lines.join("\n")}\nZ ${skipped}\n`);
}, 3600000);
