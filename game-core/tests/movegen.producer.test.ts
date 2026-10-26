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
import { getSpeciesDeniedOffensiveStat } from "#balance/off-stat-denylist";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

const NB = Number(process.env.GC_MG_B ?? 800);
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
  /* a passive that changes the move's type (Normalize, the -ate abilities, Liquid Voice) next to an ability whose power depends on that type
   * is not modelled: no species has such a pair (the exhaustive pass below checks every species, form and ability slot) */
  const typeChangers = new Set<number>(allAbilities.filter(a => a.id > 0 && a.hasAttr("MoveTypeChangeAbAttr")).map(a => a.id));
  const passiveList = relevantList.filter(a => !typeChangers.has(a) && !allAbilities[a].hasAttr("PreAttackWeatherOverrideAbAttr") && !allAbilities[a].hasAttr("SuppressWeatherEffectAbAttr"));
  /* the abilities filterUselessMoves keeps a weather move for, and the weather summoners */
  const weatherAbilities = [AbilityId.RAIN_DISH, AbilityId.FORECAST, AbilityId.SWIFT_SWIM, AbilityId.DRY_SKIN, AbilityId.CHLOROPHYLL, AbilityId.FLOWER_GIFT, AbilityId.PROTOSYNTHESIS,
    AbilityId.HARVEST, AbilityId.SOLAR_POWER, AbilityId.SLUSH_RUSH, AbilityId.ICE_FACE, AbilityId.SNOW_CLOAK, AbilityId.ICE_BODY, AbilityId.SAND_FORCE, AbilityId.SAND_RUSH,
    AbilityId.SAND_VEIL, AbilityId.SNOW_WARNING, AbilityId.DROUGHT, AbilityId.DRIZZLE, AbilityId.SAND_STREAM, AbilityId.SAND_SPIT, AbilityId.SNOW_WARNING, AbilityId.ORICHALCUM_PULSE, AbilityId.DROUGHT];
  const setWeather = (w: number) => {
    scene.arena.weather = w ? ({ weatherType: w, isImmutable: () => false, turnsLeft: 0, maxDuration: 0, isEffectSuppressed: () => false } as any) : null;
  };
  /* a random Pokemon of a random species, in the same object: nothing is rolled from the RNG under test */
  /* species on the worse-offensive-stat denylist: bosses are the ones it acts on */
  const denyFocus = speciesIds.filter(sid => {
    const sp = speciesDataRegistry.getSpecies(sid);
    for (let f = 0; f < Math.max(sp.forms.length, 1); f++) if (getSpeciesDeniedOffensiveStat(sid, f) !== undefined) return true;
    return false;
  });
  const shape = (forcedAbility: number, wf = false, boss = false, fixed?: { sid: number; form: number; slot: number; weather: number; passive?: number }) => {
    const sid = fixed ? fixed.sid : wf ? weatherFocus[rnd(weatherFocus.length)]
      : boss && rnd(3) === 0 ? denyFocus[rnd(denyFocus.length)]
      : focus.length && rnd(2) ? focus[rnd(focus.length)] : speciesIds[rnd(speciesIds.length)];
    const sp = speciesDataRegistry.getSpecies(sid);
    const nForms = sp.forms.length;
    const form = fixed ? fixed.form : nForms > 1 && rnd(3) ? rnd(nForms) : 0;
    const level = boss && rnd(2) ? 55 + rnd(100) : rnd(4) === 0 ? 1 + rnd(150) : 1 + rnd(100);
    const abilityIndex = fixed ? fixed.slot : rnd(3);
    const gender = [-1, 0, 1][rnd(3)];
    const nature = rnd(25);
    const ivs = Array.from({ length: 6 }, () => rnd(32));
    const weather0 = rnd(2) ? 0 : rnd(10);
    const weather = fixed ? fixed.weather : weather0;
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
    /* a boss has its passive and, in doubles, the doubles-only moves; everything else is a plain wild Pokemon */
    const segments = boss ? 1 + rnd(4) : 0;
    /* the weather-focus bosses get a weather passive half the time (Weather Ball's type follows the last summoner, the passive's) */
    const passive = fixed?.passive ?? (boss && !fixed && (wf ? rnd(2) === 0 : rnd(3) === 0) ? (wf ? weatherAbilities[rnd(weatherAbilities.length)] : passiveList[rnd(passiveList.length)]) : -1);
    const double = boss && rnd(4) === 0;   /* the draws above stay in step for fixed shapes */
    e.bossSegments = segments;
    e.customPokemonData.passive = passive;
    scene.currentBattle.double = double;
    e.calculateStats();
    e.hp = e.stats[0];
    setWeather(weather);
    return { sp, form, level, abilityIndex, gender, nature, ivs, weather, segments, passive, double };
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
  /* bosses: the same, with boss segments (so a passive), a forced passive on a third, and a double battle on a quarter */
  const emitBoss = (i: number, s: any) => {
    if (!e.isBoss() || !e.hasPassive()) { skipped++; return; }
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
      reasons.set(`boss gen: ${String(err).slice(0, 80)}`, (reasons.get(`boss gen: ${String(err).slice(0, 80)}`) ?? 0) + 1);
      return;
    }
    lines.push(`B ${i} ${s.sp.speciesId} ${s.form} ${s.level} ${s.abilityIndex} ${e.customPokemonData.ability} ${e.getAbility().id} ${s.gender} ${s.nature} ${s.ivs.join(" ")} ${s.weather} ${offset} ${waveSeed} ${s.segments} ${s.passive} ${e.getPassiveAbility().id} ${s.double ? 1 : 0} ${e.stats.join(" ")} ${moves.length} ${moves.join(" ")} ${d1} ${d2}`);
    if (i % 6 === 0) {
      /* the effective power of every damaging move for this boss (its passive's attributes apply after its ability's) */
      globalScene.movesetGenInProgress = true;
      const powers = damaging.map(id => allMoves[id].calculateEffectivePower(e));
      globalScene.movesetGenInProgress = false;
      lines.push(`F ${i} ${powers.length} ${damaging.map((id, k) => `${id} ${powers[k]}`).join(" ")}`);
    }
  };
  /* forced abilities that rewrite the move's type or the weather would pair with the species' passive in a way no species has */
  const interacting = new Set<number>(allAbilities.filter(a => a.id > 0 && (a.hasAttr("MoveTypeChangeAbAttr") || a.hasAttr("PreAttackWeatherOverrideAbAttr")
    || a.hasAttr("SuppressWeatherEffectAbAttr"))).map(a => a.id));
  const terrainSummoners = allAbilities.filter(a => a.id > 0 && a.hasAttr("PostSummonTerrainChangeAbAttr")).map(a => a.id);
  const forcedList = relevantList.filter(a => !interacting.has(a));
  for (let i = 0; i < NB; i++) {
    let s;
    try {
      const wf = rnd(5) === 0;
      s = wf ? shape(weatherAbilities[rnd(weatherAbilities.length)], true, true) : shape(rnd(3) === 0 ? forcedList[rnd(forcedList.length)] : -1, false, true);
    } catch (err) {
      skipped++;
      continue;
    }
    emitBoss(i, s);
  }
  /* every species, form and ability slot as a boss with the abilities it really has (the passive of its form): the pairs of an
   * ability and a passive that a species can have, which is where the two attribute lists could interact */
  if (process.env.GC_MG_X !== "0") {
    let n = 100000;
    for (const sid of speciesIds) {
      const sp = speciesDataRegistry.getSpecies(sid);
      for (let form = 0; form < Math.max(sp.forms.length, 1); form++) {
        for (let slot = 0; slot < 3; slot++) {
          let s;
          try {
            s = shape(-1, false, true, { sid, form, slot, weather: n % 10 });
          } catch {
            skipped++;
            continue;
          }
          emitBoss(n++, s);
          /* a Levitate or Eelevate ability with a terrain summoner's passive: the summoners' boost is off for it */
          if ([AbilityId.LEVITATE, AbilityId.EELEVATE].includes(e.getAbility().id)) {
            for (const t of terrainSummoners) {
              try {
                s = shape(-1, false, true, { sid, form, slot, weather: n % 10, passive: t });
              } catch {
                skipped++;
                continue;
              }
              emitBoss(n++, s);
            }
          }
        }
      }
    }
  }
  e.bossSegments = 0;
  e.customPokemonData.passive = -1;
  scene.currentBattle.double = false;
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
