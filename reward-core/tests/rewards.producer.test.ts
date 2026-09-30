/*
 * Upstream half of reward-core's reward option test (tools/difftest.sh copies it into the upstream checkout).
 * Each case sets a random game mode (a challenge run gets random challenges), run seed and wave, unlocks,
 * the fusion event flag, the dex entries the conditions read, the arena's biome and weather, the ball counts,
 * and a party of 1 to 6 real PlayerPokemon with random species, forms, fusions, levels, HP, statuses, natures,
 * genders, abilities, passives, movesets with used PP, used TMs and shininess, plus random held items and
 * persistent items (stacks up to their limits, lures part-way through). It records what reward-core reads
 * (the "caller" fields asked with upstream's own calls) and then runs what SelectModifierPhase.start runs, up
 * to three screens in a row: a first roll (resetSeed), a reroll (rerollCount 1 to 3, with the Lock Capsule's
 * locked tiers or not) and a copy of the screen (isCopy: the old thresholds), some with random
 * CustomModifierSettings. For each screen: the options (item key, generated argument, tier, upgrade count and
 * English name) and the RNG state after. build/rewardstest replays the cases.
 */
import { timedEventManager } from "#app/global-event-manager";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allAbilities, allMoves, modifierTypes } from "#data/data-lists";
import { Status } from "#data/status-effect";
import { Weather } from "#data/weather";
import { AbilityId } from "#enums/ability-id";
import { BerryType } from "#enums/berry-type";
import { BiomeId } from "#enums/biome-id";
import { Challenges } from "#enums/challenges";
import { GameModes } from "#enums/game-modes";
import { ModifierPoolType } from "#enums/modifier-pool-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { Unlockables } from "#enums/unlockables";
import { WeatherType } from "#enums/weather-type";
import { PokemonHeldItemModifier } from "#modifiers/modifier";
import { getPartyLuckValue, regenerateModifierPoolThresholds } from "#modifiers/modifier-type";
import { PokemonMove } from "#moves/pokemon-move";
import { getModifierPoolForType } from "#utils/modifier-utils";
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("rewards producer", async () => {
  const N = Number(process.env.DIFF_N ?? 3000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-rewards-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  let fusionsBoosted = false;
  vi.spyOn(timedEventManager, "areFusionsBoosted").mockImplementation(() => fusionsBoosted);

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const pick = <T>(v: readonly T[]): T => v[rnd(v.length)];
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const state = () => Phaser.Math.RND.state().split(",").slice(1).join(" ");
  const modes = [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.DAILY, GameModes.CHALLENGE];
  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  const moveIds = allMoves.map((m: any) => m.id).filter((id: number) => id > 0);
  const keys = Object.keys(modifierTypes);
  const keyOf = (t: any) => t.id as string;
  const ssbKeys = ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_SCALE", "DEEP_SEA_TOOTH"];
  // A generated type's argument, as reward-core keeps it
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
  const nameOf = (t: any) => String(t.name).replace(/\s+/g, "_");
  // A type from modifierTypes[key], with the generator's pregenerated argument (as the overrides make them)
  const makeType = (key: string, arg?: number) => {
    const fn = (modifierTypes as any)[key];
    let t = fn();
    if (t.generateType) {
      t = t.generateType([], arg === undefined ? undefined : [arg]);
    }
    return t && t.withIdFromFunc(fn);
  };

  // The attack type booster generator's input, asked as it asks it
  const attackTypes = (p: any): number[] => {
    const out: number[] = [];
    for (const pm of p.getMoveset()) {
      const move = pm.getMove();
      if (!move.is("AttackMove")) {
        continue;
      }
      const attr = move.getAttrs("VariableMoveTypeAttr")[0];
      out.push(...(attr ? attr.getTypesForItemSpawn(p, move) : p.getMoveTypeForItemSpawn(move)));
    }
    return out;
  };

  // Species the pools and generators single out, drawn half the time
  const hotSpecies = [
    SpeciesId.PIKACHU, SpeciesId.CUBONE, SpeciesId.MAROWAK, SpeciesId.ALOLA_MAROWAK, SpeciesId.DITTO, SpeciesId.CLAMPERL,
    SpeciesId.FARFETCHD, SpeciesId.GALAR_FARFETCHD, SpeciesId.SIRFETCHD, SpeciesId.TERAPAGOS, SpeciesId.OGERPON,
    SpeciesId.SHEDINJA, SpeciesId.TYROGUE, SpeciesId.GIMMIGHOUL, SpeciesId.ARCEUS, SpeciesId.SILVALLY, SpeciesId.NECROZMA,
    SpeciesId.KYUREM, SpeciesId.CALYREX, SpeciesId.EEVEE, SpeciesId.SLOWPOKE, SpeciesId.KIRLIA, SpeciesId.SNORUNT,
    SpeciesId.SHELMET, SpeciesId.KARRABLAST, SpeciesId.CHARIZARD, SpeciesId.VENUSAUR, SpeciesId.GENGAR,
    SpeciesId.RAYQUAZA, SpeciesId.GROUDON, SpeciesId.KYOGRE, SpeciesId.GIRATINA, SpeciesId.GENESECT, SpeciesId.ZACIAN,
    SpeciesId.URSHIFU, SpeciesId.MELMETAL, SpeciesId.GOLURK, SpeciesId.GALAR_SLOWPOKE, SpeciesId.APPLIN, SpeciesId.POLTEAGEIST,
    SpeciesId.SINISTEA, SpeciesId.PANCHAM, SpeciesId.GOOMY, SpeciesId.RIOLU, SpeciesId.MIME_JR, SpeciesId.HAPPINY,
    SpeciesId.GLIGAR, SpeciesId.SNEASEL, SpeciesId.MILCERY, SpeciesId.DIPPLIN, SpeciesId.POLIWHIRL, SpeciesId.GLOOM,
  ];
  const hotMoves = [
    MoveId.FACADE, MoveId.PSYCHO_SHIFT, MoveId.SUNNY_DAY, MoveId.RAIN_DANCE, MoveId.SANDSTORM, MoveId.SNOWSCAPE, MoveId.HAIL,
    MoveId.CHILLY_RECEPTION, MoveId.ELECTRIC_TERRAIN, MoveId.PSYCHIC_TERRAIN, MoveId.GRASSY_TERRAIN, MoveId.MISTY_TERRAIN,
    MoveId.LOW_SWEEP, MoveId.MACH_PUNCH, MoveId.RAPID_SPIN, MoveId.TACKLE, MoveId.WEATHER_BALL, MoveId.JUDGMENT,
    MoveId.MULTI_ATTACK, MoveId.HIDDEN_POWER, MoveId.TERA_BLAST, MoveId.REVELATION_DANCE, MoveId.NATURAL_GIFT,
    MoveId.DOUBLE_HIT, MoveId.DRAGON_PULSE, MoveId.MIMIC, MoveId.ROLLOUT, MoveId.STOMP,
  ];
  const hotAbilities = [
    AbilityId.QUICK_FEET, AbilityId.GUTS, AbilityId.MARVEL_SCALE, AbilityId.MAGIC_GUARD, AbilityId.TOXIC_BOOST,
    AbilityId.POISON_HEAL, AbilityId.FLARE_BOOST, AbilityId.DROUGHT, AbilityId.ORICHALCUM_PULSE, AbilityId.DRIZZLE,
    AbilityId.SAND_STREAM, AbilityId.SAND_SPIT, AbilityId.SNOW_WARNING, AbilityId.ELECTRIC_SURGE, AbilityId.HADRON_ENGINE,
    AbilityId.PSYCHIC_SURGE, AbilityId.GRASSY_SURGE, AbilityId.SEED_SOWER, AbilityId.MISTY_SURGE, AbilityId.MULTITYPE,
    AbilityId.RKS_SYSTEM, AbilityId.WONDER_GUARD, AbilityId.COMATOSE, AbilityId.PURIFYING_SALT, AbilityId.WATER_VEIL,
  ];
  // Items a member may hold (with their arguments) and persistent items
  const berries = Object.values(BerryType).filter(v => typeof v === "number") as number[];
  const heldPicks: (() => [string, number | undefined])[] = [
    () => ["BERRY", rnd(3) === 0 ? BerryType.LEPPA : pick(berries)],
    () => ["TOXIC_ORB", undefined],
    () => ["FLAME_ORB", undefined],
    () => ["EVIOLITE", undefined],
    () => ["LEEK", undefined],
    () => ["MYSTICAL_ROCK", undefined],
    () => [rnd(2) ? "SPECIES_STAT_BOOSTER" : "RARE_SPECIES_STAT_BOOSTER", rnd(6)],
    () => [rnd(2) ? "FORM_CHANGE_ITEM" : "RARE_FORM_CHANGE_ITEM", 1 + rnd(260)],
    () => ["EVOLUTION_TRACKER_GIMMIGHOUL", undefined],
    () => ["REVIVER_SEED", undefined],
    () => ["LEFTOVERS", undefined],
    () => ["GOLDEN_PUNCH", undefined],
    () => ["ATTACK_TYPE_BOOSTER", rnd(18)],
    () => ["BASE_STAT_BOOSTER", rnd(6)],
    () => ["SOUL_DEW", undefined],
    () => ["MULTI_LENS", undefined],
  ];
  const persistentKeys = [
    "EXP_CHARM", "SUPER_EXP_CHARM", "GOLDEN_EXP_CHARM", "GOLDEN_POKEBALL", "SILVER_POKEBALL", "LURE", "SUPER_LURE",
    "MAX_LURE", "MEGA_BRACELET", "DYNAMAX_BAND", "TERA_ORB", "MAP", "IV_SCANNER", "AMULET_COIN", "LOCK_CAPSULE",
    "CATCHING_CHARM", "ABILITY_CHARM", "SHINY_CHARM", "HEALING_CHARM", "CANDY_JAR", "BERRY_POUCH", "EXP_SHARE",
    "EXP_BALANCE", "OVAL_CHARM", "TEMP_STAT_STAGE_BOOSTER", "DIRE_HIT", "COIN_CASE",
  ];
  const caughtSpecies = [
    SpeciesId.SHELMET, SpeciesId.KARRABLAST, SpeciesId.RESHIRAM, SpeciesId.ZEKROM, SpeciesId.SOLGALEO, SpeciesId.LUNALA,
    SpeciesId.GLASTRIER, SpeciesId.SPECTRIER,
  ];
  const otherChallenges = [
    Challenges.SINGLE_GENERATION, Challenges.SINGLE_TYPE, Challenges.LOWER_MAX_STARTER_COST, Challenges.LOWER_STARTER_POINTS,
    Challenges.INVERSE_BATTLE, Challenges.FLIP_STAT, Challenges.LIMITED_CATCH, Challenges.LIMITED_SUPPORT, Challenges.PASSIVES,
  ];
  const biomes = Object.values(BiomeId).filter(v => typeof v === "number") as number[];
  const weathers = Object.values(WeatherType).filter(v => typeof v === "number" && v) as number[];
  const statuses = [
    StatusEffect.POISON, StatusEffect.TOXIC, StatusEffect.PARALYSIS, StatusEffect.SLEEP, StatusEffect.FREEZE,
    StatusEffect.BURN,
  ];

  const pool = Array.from({ length: 6 }, () => scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5));
  const party = scene.getPlayerParty();
  const realArena = scene.arena;

  const lines: string[] = [];
  let thrown = 0;
  for (let i = 0; i < N; i++) {
    try {
      const gm = getGameMode(modes[rnd(modes.length)]);
      let challenges = 0;
      if (gm.modeId === GameModes.CHALLENGE) {
        const set = (c: number, v: number) => {
          gm.setChallengeValue(c, v);
          challenges |= 1 << c;
        };
        if (rnd(3) === 0) {
          set(Challenges.FRESH_START, 1);
        }
        if (rnd(3) === 0) {
          set(Challenges.HARDCORE, 1);
        }
        if (rnd(4) === 0) {
          const c = pick(otherChallenges);
          set(c, c === Challenges.SINGLE_TYPE ? 1 + rnd(18) : c === Challenges.SINGLE_GENERATION ? 1 + rnd(9) : 1);
        }
        if (rnd(20) === 0) {
          set(Challenges.MOVESET_RANDOMIZER, 1);
        }
      }
      scene.gameMode = gm;
      battle.gameMode = gm;
      scene.seed = seedStr();
      const hot = [1, 10, 165, 179, 180, 188, 189, 190, 198, 199, 200, 50, 150, 250];
      const wave = rnd(6) === 0 ? pick(hot) : rnd(10) === 0 ? 1 + rnd(1200) : 1 + rnd(210);
      battle.waveIndex = wave;
      scene.gameData.unlocks[Unlockables.EVIOLITE] = rnd(2) === 0;
      scene.gameData.unlocks[Unlockables.MINI_BLACK_HOLE] = rnd(2) === 0;
      fusionsBoosted = rnd(3) === 0;
      const caught: number[] = [];
      for (const s of caughtSpecies) {
        const c = rnd(2) === 0;
        scene.gameData.dexData[s].caughtAttr = c ? 1n : 0n;
        if (c) {
          caught.push(s);
        }
      }
      const biome = pick(biomes);
      Object.defineProperty(realArena, "biomeId", { value: biome, writable: true, configurable: true });
      realArena.weather = rnd(3) === 0 ? new Weather(pick(weathers), 5) : null;
      const balls = [0, 1, 2, 3, 4].map(() => (rnd(4) === 0 ? 99 + rnd(3) : rnd(99)));
      balls.forEach((v, k) => {
        scene.pokeballCounts[k] = v;
      });

      // the party. One case in 8 stacks attack moves of one type on every member (the booster caps a type at 3).
      const stacked = rnd(8) === 0;
      const size = 1 + rnd(6);
      party.length = 0;
      for (let k = 0; k < size; k++) {
        const p = pool[k];
        p.resetSummonData();
        p.resetBattleAndWaveData();
        const sid = rnd(2) === 0 ? pick(hotSpecies) : pick(speciesIds);
        const sp = speciesDataRegistry.getSpecies(sid);
        p.species = sp;
        p.formIndex = sp.forms.length ? rnd(sp.forms.length) : 0;
        if (rnd(4) === 0) {
          const fs2 = speciesDataRegistry.getSpecies(rnd(2) === 0 ? pick(hotSpecies) : pick(speciesIds));
          p.fusionSpecies = fs2;
          p.fusionFormIndex = fs2.forms.length ? rnd(fs2.forms.length) : 0;
          p.fusionAbilityIndex = rnd(2);
          p.fusionGender = rnd(3) - 1;
          p.fusionShiny = rnd(6) === 0;
          p.fusionVariant = rnd(3);
          p.fusionLuck = p.fusionShiny ? 1 + p.fusionVariant : 0;
        } else {
          p.fusionSpecies = null;
          p.fusionFormIndex = 0;
        }
        p.level = rnd(4) === 0 ? 1 + rnd(20) : 1 + rnd(100);
        p.gender = rnd(3) - 1;
        p.nature = rnd(25);
        p.teraType = rnd(18);
        p.friendship = rnd(256);
        p.shiny = rnd(6) === 0;
        p.variant = rnd(3);
        p.luck = p.shiny ? 1 + p.variant : 0;
        p.abilityIndex = rnd(3);
        p.customPokemonData.ability = rnd(3) === 0 ? pick(hotAbilities) : -1;
        p.passive = rnd(2) === 0;
        p.customPokemonData.passive = rnd(3) === 0 ? pick(hotAbilities) : rnd(3) === 0 ? 1 + rnd(allAbilities.length - 1) : -1;
        p.pauseEvolutions = rnd(4) === 0;
        p.metBiome = rnd(3) === 0 ? -1 : 0;
        const nm = 1 + rnd(4);
        p.moveset = [];
        for (let m = 0; m < nm; m++) {
          const mv = new PokemonMove(rnd(2) === 0 ? pick(hotMoves) : pick(moveIds), 0, rnd(4));
          const pp = mv.getMovePp();
          mv.ppUsed = rnd(3) === 0 ? 0 : rnd(pp + 1);
          p.moveset.push(mv);
        }
        if (stacked) {
          const normal = [MoveId.TACKLE, MoveId.BODY_SLAM, MoveId.HYPER_BEAM, MoveId.QUICK_ATTACK, MoveId.SLAM, MoveId.HEADBUTT];
          const fire = [MoveId.EMBER, MoveId.FLAMETHROWER, MoveId.FIRE_BLAST, MoveId.FLAME_WHEEL];
          p.moveset = [0, 1, 2, 3].map(m => new PokemonMove(m < 3 || rnd(2) ? pick(normal) : pick(fire)));
        }
        p.usedTMs = rnd(3) === 0 ? Array.from({ length: 1 + rnd(4) }, () => pick(moveIds)) : [];
        p.calculateStats();
        const max = p.getMaxHp();
        const r = rnd(8);
        p.hp = r < 2 ? max : r === 2 ? 0 : 1 + rnd(max);
        const st = p.hp === 0 ? (rnd(2) === 0 ? StatusEffect.FAINT : 0) : rnd(3) === 0 ? pick(statuses) : 0;
        p.status = st ? new Status(st) : null;
        party.push(p);
      }

      // the items
      scene.modifiers.length = 0;
      const seen = new Set<string>();
      for (const p of party) {
        for (let n = rnd(4); n > 0; n--) {
          const [key, arg] = pick(heldPicks)();
          const tag = `${p.id}/${key}/${arg}`;
          const t = makeType(key, arg);
          if (!t || seen.has(tag)) {
            continue;
          }
          seen.add(tag);
          const m = t.newModifier(p);
          if (!m) {
            continue;
          }
          const maxHeld = m.getMaxHeldItemCount(p);
          m.stackCount = rnd(2) === 0 ? maxHeld : 1 + rnd(Math.max(maxHeld, 1));
          scene.modifiers.push(m);
        }
      }
      for (let n = rnd(9); n > 0; n--) {
        const key = pick(persistentKeys);
        if (seen.has(key)) {
          continue;
        }
        seen.add(key);
        const t = makeType(key, key === "TEMP_STAT_STAGE_BOOSTER" ? 1 + rnd(6) : undefined);
        const m = t.newModifier();
        const max = m.getMaxStackCount();
        m.stackCount = rnd(2) === 0 ? max : 1 + rnd(Math.max(max, 1));
        if (m.battleCount !== undefined && m.getMaxBattles) {
          m.battleCount = 1 + rnd(m.getMaxBattles());
        }
        scene.modifiers.push(m);
      }

      // what reward-core reads
      const luck = getPartyLuckValue(party);
      const head = [
        "R", gm.modeId, scene.seed, wave, challenges, luck, biome, realArena.getTimeOfDay(), realArena.weatherType ?? 0,
        scene.gameData.unlocks[Unlockables.EVIOLITE] ? 1 : 0, scene.gameData.unlocks[Unlockables.MINI_BLACK_HOLE] ? 1 : 0,
        fusionsBoosted ? 1 : 0, caught.length, ...caught, ...balls, party.length,
      ];
      lines.push(head.join(" "));
      for (const p of party) {
        const ms = p.getMoveset();
        const lm = p.getLevelMoves({ includeEvolutionMoves: true, includeRelearnerMoves: true }).map((x: any) => x[1]);
        const at = attackTypes(p);
        lines.push(
          [
            "P", p.id, p.species.speciesId, p.formIndex, p.fusionSpecies ? p.fusionSpecies.speciesId : -1, p.fusionFormIndex,
            p.gender, p.fusionGender ?? 0, p.level, p.hp, p.getMaxHp(), p.status?.effect ?? 0, p.getNature(), p.teraType,
            p.friendship, p.getAbility(true).id, p.getAbility().id, p.hasPassive() ? p.getPassiveAbility().id : 0,
            p.canApplyAbility() ? 1 : 0, p.canApplyAbility(true) ? 1 : 0, p.pauseEvolutions ? 1 : 0,
            p.isAllowedInChallenge() ? 1 : 0, p.canSetStatus(StatusEffect.TOXIC, true, true, null, true) ? 1 : 0,
            p.canSetStatus(StatusEffect.BURN, true, true, null, true) ? 1 : 0, p.getLearnableLevelMoves().length > 0 ? 1 : 0,
            p.getPersistentTreasureCount(), ms.length, ...ms.flatMap((m: any) => [m.moveId, m.ppUsed, m.getMovePp()]),
            p.getMoveset(true).length, ...p.getMoveset(true).map((m: any) => m.moveId), lm.length, ...lm, p.usedTMs.length,
            ...p.usedTMs, at.length, ...at,
          ].join(" "),
        );
      }
      const mods = scene.modifiers;
      lines.push(
        [
          "I", mods.length,
          ...mods.flatMap((m: any) => [
            keyOf(m.type), argOf(m.type), m.stackCount,
            m instanceof PokemonHeldItemModifier ? party.findIndex((p: any) => p.id === m.pokemonId) : -1,
            m.getBattleCount ? m.getBattleCount() : 0, m.active ? 1 : 0,
          ]),
        ].join(" "),
      );

      // the screens
      let prevTiers: number[] = [];
      let prevOpts: any[] = [];
      const steps = 1 + rnd(3);
      for (let s = 0; s < steps; s++) {
        const reroll = s === 0 ? (rnd(4) === 0 ? 1 + rnd(3) : 0) : s === 1 ? 1 + rnd(3) : 0;
        const isCopy = s === 2;
        const lock = reroll > 0 && rnd(2) === 0;
        scene.lockModifierTiers = lock;
        const tiers = lock ? prevTiers : undefined;
        let custom: any;
        const cf: string[] = [];
        // a copy is the screen again after a TM or Memory Mushroom: copy() passes the old options
        const copyOpts = isCopy && rnd(2) === 0;
        if (copyOpts) {
          custom = { guaranteedModifierTypeOptions: prevOpts, rerollMultiplier: undefined, allowLuckUpgrades: false };
        } else if (rnd(6) === 0) {
          custom = {};
          if (rnd(2) === 0) {
            custom.guaranteedModifierTiers = Array.from({ length: rnd(4) }, () => rnd(5));
          }
          if (rnd(2) === 0) {
            const funcs = Array.from({ length: 1 + rnd(2) }, () => pick(keys));
            custom.guaranteedModifierTypeFuncs = funcs.map(k => (modifierTypes as any)[k]);
            cf.push(...funcs);
          }
          if (rnd(2) === 0) {
            custom.fillRemaining = true;
          }
          if (rnd(3) === 0) {
            custom.allowLuckUpgrades = false;
          }
        }
        Phaser.Math.RND.sow([seedStr()]);
        const before = state();
        const phase: any = new SelectModifierPhase(reroll, tiers, custom, isCopy);
        if (!reroll && !isCopy) {
          scene.resetSeed();
        }
        if (process.env.DIFF_ONLY && Number(process.env.DIFF_ONLY) === i + 1 && !(globalThis as any).__genWrapped) {
          (globalThis as any).__genWrapped = true;
          const proto: any = Object.getPrototypeOf((modifierTypes as any).BERRY());
          const real = proto.generateType;
          proto.generateType = function (this: any, pty: any, pre: any) {
            const r = real.call(this, pty, pre);
            console.error(`G ${this.id} ${r ? argOf(r) : "null"}`);
            return r;
          };
        }
        if (process.env.DIFF_ONLY && Number(process.env.DIFF_ONLY) === i + 1) {
          // debug: every PLAYER pool entry's weight function and the existing-item rule's inputs
          const pl: any = getModifierPoolForType(ModifierPoolType.PLAYER);
          for (const t of Object.keys(pl)) {
            pl[t].forEach((w: any, k: number) => {
              const ex = scene.findModifiers((m: any) => m.type.id === w.modifierType.id, true);
              const raw = typeof w.weight === "function" ? w.weight(party, reroll) : w.weight;
              const under = ex.some((m: any) => m.stackCount < m.getMaxStackCount(true));
              console.error(`W ${t} ${k} ${w.modifierType.id} ${raw} ${ex.length} ${under ? 1 : 0}`);
            });
          }
        }
        if (!isCopy) {
          regenerateModifierPoolThresholds(party, ModifierPoolType.PLAYER, reroll);
        }
        const count = phase.getModifierCount();
        const opts = phase.getModifierTypeOptions(count);
        lines.push(
          [
            "S", reroll, isCopy ? 1 : 0, lock ? 1 : 0, tiers?.length ?? 0, ...(tiers ?? []).map(t => t ?? -1), custom ? 1 : 0,
            custom?.guaranteedModifierTiers?.length ?? 0, ...(custom?.guaranteedModifierTiers ?? []), cf.length, ...cf,
            custom?.fillRemaining ? 1 : 0, custom?.allowLuckUpgrades === false ? 0 : 1, copyOpts ? 1 : 0, before,
          ].join(" "),
        );
        lines.push(
          [
            "O", count, opts.length,
            ...opts.flatMap((o: any) => [keyOf(o.type), argOf(o.type), o.type.tier ?? -1, o.upgradeCount, nameOf(o.type)]),
            state(),
          ].join(" "),
        );
        prevTiers = opts.map((o: any) => o.type.tier);
        prevOpts = opts;
      }
      lines.push("E");
    } catch (e) {
      thrown++;
      lines.push(`X ${String(e).slice(0, 80).replace(/\s+/g, "_")}`);
      if (process.env.DIFF_STACKS) {
        console.error((e as any)?.stack);
      }
    }
  }
  party.length = 0;
  scene.modifiers.length = 0;
  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
