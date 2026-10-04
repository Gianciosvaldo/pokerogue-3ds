/*
 * Upstream half of game-core's reward adapter test (tools/rewarddiff.sh copies it into the upstream checkout).
 * Each case is a classic run at a random wave with a party of 1 to 6 real PlayerPokemon of the kind game-core
 * can hold (random species, forms, a fusion now and then, levels, HP, statuses, natures, genders, abilities,
 * passives, shininess, movesets with used PP and PP Ups, starter or caught, egg moves unlocked), random held and
 * persistent items, balls, unlocks, dex entries, biome, time of day and weather. It records the inputs (so
 * build/reward_diff_test can set the same state up in game-core) and what upstream makes of them: for each
 * member the values reward-core is given by its caller (canApplyAbility, canSetStatus, the learnsets
 * getLevelMoves reads, the unlocked egg moves, the item-spawn types of its moves...), then the screens
 * SelectModifierPhase makes in the order a run does: a first roll (or the no-victory screen), up to two paid
 * rerolls (the Lock Capsule's lock on or off), and the copy of the last screen made after a TM or the Memory
 * Mushroom. For each screen: the options, the reroll price and the RNG state after.
 *
 * Left out, because game-core doesn't model it: challenges, other game modes, lures (their battle counts),
 * usedTMs, summon data overrides, fusion gender and luck (a fusion's are 0 here). An ability override
 * (customPokemonData.ability) reaches game-core as the Pokemon's current ability next to its base one.
 */
import { timedEventManager } from "#app/global-event-manager";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { speciesEggMoves } from "#balance/egg-moves";
import { allMoves, modifierTypes } from "#data/data-lists";
import { Status } from "#data/status-effect";
import { Terrain, TerrainType } from "#data/terrain";
import { Weather } from "#data/weather";
import { AbilityId } from "#enums/ability-id";
import { BerryType } from "#enums/berry-type";
import { BiomeId } from "#enums/biome-id";
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
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("game-core reward adapter producer", async () => {
  const N = Number(process.env.RD_N ?? 1000);
  const OUT = process.env.RD_OUT ?? "/tmp/gc-reward-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  scene.gameMode = getGameMode(GameModes.CLASSIC);
  battle.gameMode = scene.gameMode;
  let fusionsBoosted = false;
  vi.spyOn(timedEventManager, "areFusionsBoosted").mockImplementation(() => fusionsBoosted);

  let seed = Number(process.env.RD_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const pick = <T>(v: readonly T[]): T => v[rnd(v.length)];
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const state = () => Phaser.Math.RND.state().split(",").slice(1).join(" ");
  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  const moveIds = allMoves.map((m: any) => m.id).filter((id: number) => id > 0);
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

  const hotSpecies = [
    SpeciesId.PIKACHU, SpeciesId.CUBONE, SpeciesId.MAROWAK, SpeciesId.ALOLA_MAROWAK, SpeciesId.DITTO, SpeciesId.CLAMPERL,
    SpeciesId.FARFETCHD, SpeciesId.GALAR_FARFETCHD, SpeciesId.SIRFETCHD, SpeciesId.TERAPAGOS, SpeciesId.OGERPON,
    SpeciesId.SHEDINJA, SpeciesId.TYROGUE, SpeciesId.GIMMIGHOUL, SpeciesId.ARCEUS, SpeciesId.SILVALLY, SpeciesId.NECROZMA,
    SpeciesId.KYUREM, SpeciesId.CALYREX, SpeciesId.EEVEE, SpeciesId.SLOWPOKE, SpeciesId.KIRLIA, SpeciesId.SNORUNT,
    SpeciesId.SHELMET, SpeciesId.KARRABLAST, SpeciesId.CHARIZARD, SpeciesId.VENUSAUR, SpeciesId.GENGAR, SpeciesId.MORPEKO,
    SpeciesId.RAYQUAZA, SpeciesId.GROUDON, SpeciesId.KYOGRE, SpeciesId.GIRATINA, SpeciesId.GENESECT, SpeciesId.ZACIAN,
    SpeciesId.URSHIFU, SpeciesId.MELMETAL, SpeciesId.GOLURK, SpeciesId.GALAR_SLOWPOKE, SpeciesId.APPLIN, SpeciesId.POLTEAGEIST,
    SpeciesId.SINISTEA, SpeciesId.PANCHAM, SpeciesId.GOOMY, SpeciesId.RIOLU, SpeciesId.MIME_JR, SpeciesId.HAPPINY,
    SpeciesId.GLIGAR, SpeciesId.SNEASEL, SpeciesId.MILCERY, SpeciesId.DIPPLIN, SpeciesId.POLIWHIRL, SpeciesId.GLOOM,
    SpeciesId.BULBASAUR, SpeciesId.IVYSAUR, SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE, SpeciesId.PICHU, SpeciesId.RAICHU,
  ];
  const hotMoves = [
    MoveId.FACADE, MoveId.PSYCHO_SHIFT, MoveId.SUNNY_DAY, MoveId.RAIN_DANCE, MoveId.SANDSTORM, MoveId.SNOWSCAPE, MoveId.HAIL,
    MoveId.CHILLY_RECEPTION, MoveId.ELECTRIC_TERRAIN, MoveId.PSYCHIC_TERRAIN, MoveId.GRASSY_TERRAIN, MoveId.MISTY_TERRAIN,
    MoveId.LOW_SWEEP, MoveId.MACH_PUNCH, MoveId.RAPID_SPIN, MoveId.TACKLE, MoveId.WEATHER_BALL, MoveId.JUDGMENT,
    MoveId.MULTI_ATTACK, MoveId.HIDDEN_POWER, MoveId.TERA_BLAST, MoveId.REVELATION_DANCE, MoveId.NATURAL_GIFT,
    MoveId.DOUBLE_HIT, MoveId.DRAGON_PULSE, MoveId.MIMIC, MoveId.ROLLOUT, MoveId.STOMP, MoveId.AURA_WHEEL,
    MoveId.RAGING_BULL, MoveId.IVY_CUDGEL, MoveId.TECHNO_BLAST, MoveId.TRI_ATTACK,
  ];
  const hotAbilities = [
    AbilityId.QUICK_FEET, AbilityId.GUTS, AbilityId.MARVEL_SCALE, AbilityId.MAGIC_GUARD, AbilityId.TOXIC_BOOST,
    AbilityId.POISON_HEAL, AbilityId.FLARE_BOOST, AbilityId.DROUGHT, AbilityId.DRIZZLE, AbilityId.SAND_STREAM,
    AbilityId.PIXILATE, AbilityId.REFRIGERATE, AbilityId.AERILATE, AbilityId.GALVANIZE, AbilityId.NORMALIZE,
    AbilityId.LIQUID_VOICE, AbilityId.MULTITYPE, AbilityId.RKS_SYSTEM, AbilityId.WONDER_GUARD, AbilityId.COMATOSE,
    AbilityId.PURIFYING_SALT, AbilityId.WATER_VEIL, AbilityId.LEAF_GUARD, AbilityId.FLOWER_VEIL, AbilityId.SWEET_VEIL,
    AbilityId.PASTEL_VEIL, AbilityId.LIMBER, AbilityId.IMMUNITY, AbilityId.MAGMA_ARMOR, AbilityId.WATER_BUBBLE,
    AbilityId.SHIELDS_DOWN, AbilityId.INSOMNIA, AbilityId.VITAL_SPIRIT, AbilityId.OWN_TEMPO,
  ];
  // Moves whose item-spawn type depends on the user, with the species that makes it matter (-1 = any)
  const specials: [number, number][] = [
    [MoveId.AURA_WHEEL, SpeciesId.MORPEKO], [MoveId.TERA_BLAST, -1], [MoveId.HIDDEN_POWER, -1],
    [MoveId.JUDGMENT, SpeciesId.ARCEUS], [MoveId.MULTI_ATTACK, SpeciesId.SILVALLY], [MoveId.TECHNO_BLAST, SpeciesId.GENESECT],
    [MoveId.RAGING_BULL, SpeciesId.TAUROS], [MoveId.IVY_CUDGEL, SpeciesId.OGERPON],
    [MoveId.REVELATION_DANCE, SpeciesId.ORICORIO], [MoveId.WEATHER_BALL, -1], [MoveId.NATURAL_GIFT, -1],
    [MoveId.TERRAIN_PULSE, -1],
  ];
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
  // (no lures: game-core doesn't keep their battle counts)
  const persistentKeys = [
    "EXP_CHARM", "SUPER_EXP_CHARM", "GOLDEN_EXP_CHARM", "GOLDEN_POKEBALL", "SILVER_POKEBALL", "MEGA_BRACELET",
    "DYNAMAX_BAND", "TERA_ORB", "MAP", "IV_SCANNER", "AMULET_COIN", "LOCK_CAPSULE", "CATCHING_CHARM", "ABILITY_CHARM",
    "SHINY_CHARM", "HEALING_CHARM", "CANDY_JAR", "BERRY_POUCH", "EXP_SHARE", "EXP_BALANCE", "OVAL_CHARM",
    "TEMP_STAT_STAGE_BOOSTER", "DIRE_HIT", "COIN_CASE",
  ];
  const caughtSpecies = [
    SpeciesId.SHELMET, SpeciesId.KARRABLAST, SpeciesId.RESHIRAM, SpeciesId.ZEKROM, SpeciesId.SOLGALEO, SpeciesId.LUNALA,
    SpeciesId.GLASTRIER, SpeciesId.SPECTRIER,
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
  const rows = (list: any[]) => [list.length, ...list.flatMap((x: any) => [x[0] ?? -1, x[1], x[2]])];

  const lines: string[] = [`V 1 ${N}`];
  let thrown = 0;
  for (let i = 0; i < N; i++) {
    try {
      scene.seed = seedStr();
      const hot = [1, 10, 50, 150, 165, 179, 180, 188, 189, 190, 198, 199];
      const wave = rnd(6) === 0 ? pick(hot) : 1 + rnd(199);
      battle.waveIndex = wave;
      scene.waveCycleOffset = rnd(40);
      scene.gameData.unlocks[Unlockables.EVIOLITE] = rnd(2) === 0;
      scene.gameData.unlocks[Unlockables.MINI_BLACK_HOLE] = rnd(2) === 0;
      fusionsBoosted = false;
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
      realArena.terrain = rnd(3) === 0 ? new Terrain(pick([TerrainType.MISTY, TerrainType.ELECTRIC, TerrainType.GRASSY, TerrainType.PSYCHIC]), 5) : null;
      const balls = [0, 1, 2, 3, 4].map(() => (rnd(4) === 0 ? 99 + rnd(3) : rnd(99)));
      balls.forEach((v, k) => {
        scene.pokeballCounts[k] = v;
      });

      // the party. One case in 8 stacks attack moves of one type on every member (the booster caps a type at 3).
      const stacked = rnd(8) === 0;
      const size = 1 + rnd(6);
      party.length = 0;
      const eggBits = new Map<number, number>();
      for (let k = 0; k < size; k++) {
        const p = pool[k];
        p.resetSummonData();
        p.resetBattleAndWaveData();
        const special = rnd(6) === 0 ? pick(specials) : null;
        const sid = special && special[1] > 0 && rnd(2) === 0 ? special[1] : rnd(2) === 0 ? pick(hotSpecies) : pick(speciesIds);
        const sp = speciesDataRegistry.getSpecies(sid);
        p.species = sp;
        p.formIndex = sp.forms.length ? rnd(sp.forms.length) : 0;
        const partner = special && special[1] > 0 && sid !== special[1] && rnd(2) === 0;
        if (partner || rnd(5) === 0) {
          const fs2 = speciesDataRegistry.getSpecies(partner ? special![1] : rnd(2) === 0 ? pick(hotSpecies) : pick(speciesIds));
          p.fusionSpecies = fs2;
          p.fusionFormIndex = fs2.forms.length ? rnd(fs2.forms.length) : 0;
          p.fusionAbilityIndex = rnd(2);
          p.fusionGender = 0;
          p.fusionShiny = false;
          p.fusionVariant = 0;
          p.fusionLuck = 0;
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
        p.customPokemonData.ability = rnd(4) === 0 ? pick(hotAbilities) : -1;
        p.customPokemonData.passive = -1;
        p.passive = rnd(2) === 0;
        p.pauseEvolutions = rnd(4) === 0;
        // a starter (metBiome -1: egg moves count, metSpecies its root) or a Pokemon caught on the way
        const starter = rnd(3) !== 0;
        p.metBiome = starter ? -1 : 0;
        p.metSpecies = starter ? sp.getRootSpeciesId(true) : sid;
        p.usedTMs = [];
        const nm = 1 + rnd(4);
        p.moveset = [];
        for (let m = 0; m < nm; m++) {
          const mv = new PokemonMove(rnd(2) === 0 ? pick(hotMoves) : pick(moveIds), 0, rnd(4));
          const pp = mv.getMovePp();
          mv.ppUsed = rnd(3) === 0 ? 0 : rnd(pp + 1);
          p.moveset.push(mv);
        }
        if (special) {
          p.moveset[rnd(p.moveset.length)] = new PokemonMove(special[0], 0, rnd(4));
        }
        if (stacked) {
          const normal = [MoveId.TACKLE, MoveId.BODY_SLAM, MoveId.HYPER_BEAM, MoveId.QUICK_ATTACK, MoveId.SLAM, MoveId.HEADBUTT];
          const fire = [MoveId.EMBER, MoveId.FLAMETHROWER, MoveId.FIRE_BLAST, MoveId.FLAME_WHEEL];
          p.moveset = [0, 1, 2, 3].map(m => new PokemonMove(m < 3 || rnd(2) ? pick(normal) : pick(fire)));
        }
        p.calculateStats();
        const max = p.getMaxHp();
        const r = rnd(8);
        p.hp = r < 2 ? max : r === 2 ? 0 : 1 + rnd(max);
        const st = p.hp === 0 ? (rnd(2) === 0 ? StatusEffect.FAINT : 0) : rnd(3) === 0 ? pick(statuses) : 0;
        p.status = st ? new Status(st) : null;
        // the egg moves the player has unlocked for the species getUnlockedEggMoves reads
        const eggSpecies = p.metSpecies in speciesEggMoves ? p.metSpecies : p.getSpeciesForm(true).getRootSpeciesId(true);
        if (!eggBits.has(eggSpecies)) {
          eggBits.set(eggSpecies, rnd(16));
        }
        scene.gameData.starterData[eggSpecies].eggMoves = eggBits.get(eggSpecies);
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
        scene.modifiers.push(m);
      }

      // the run's state and what reward-core is handed from it
      const luck = getPartyLuckValue(party);
      lines.push(
        [
          "H", scene.seed, wave, biome, realArena.weatherType ?? 0, realArena.terrainType, realArena.getTimeOfDay(), scene.waveCycleOffset,
          scene.gameData.unlocks[Unlockables.EVIOLITE] ? 1 : 0, scene.gameData.unlocks[Unlockables.MINI_BLACK_HOLE] ? 1 : 0,
          caught.length, ...caught, ...balls, luck, party.length, eggBits.size, ...[...eggBits].flat(),
        ].join(" "),
      );
      for (const p of party) {
        const ms = p.getMoveset();
        const at = attackTypes(p);
        const types = p.getTypes({ includeTeraType: false, bypassSummonData: true, ignoreThirdType: true });
        const learnable = p.getLearnableLevelMoves();
        lines.push(
          [
            "P", p.id, p.species.speciesId, p.formIndex, p.fusionSpecies ? p.fusionSpecies.speciesId : -1, p.fusionFormIndex,
            p.gender, p.level, p.hp, p.getMaxHp(), p.status?.effect ?? 0, p.nature, p.teraType, p.friendship, p.abilityIndex,
            p.getAbility(true).id, p.getAbility().id, p.hasPassive() ? p.getPassiveAbility().id : 0, p.passive ? 1 : 0,
            p.shiny ? 1 : 0, p.variant, p.pauseEvolutions ? 1 : 0, p.metBiome === -1 ? 1 : 0, ...p.ivs, ...p.stats,
            p.canApplyAbility() ? 1 : 0, p.canApplyAbility(true) ? 1 : 0, p.isAllowedInChallenge() ? 1 : 0,
            p.canSetStatus(StatusEffect.TOXIC, true, true, null, true) ? 1 : 0,
            p.canSetStatus(StatusEffect.BURN, true, true, null, true) ? 1 : 0, learnable.length > 0 ? 1 : 0,
            p.getPersistentTreasureCount(), types.length, ...types, ms.length,
            ...ms.flatMap((m: any) => [m.moveId, m.ppUsed, m.ppUp, m.getMovePp()]),
            ...rows(p.getLevelMoves({ includeEvolutionMoves: true, includeRelearnerMoves: true })),
            ...rows(p.getLevelMoves({ startingLevel: 1, includeEvolutionMoves: true, includePrevolutionMoves: true,
              includeRelearnerMoves: true })),
            ...rows(learnable), p.metBiome === -1 ? p.getUnlockedEggMoves().length : 0,
            ...(p.metBiome === -1 ? p.getUnlockedEggMoves() : []), at.length, ...at,
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
          ]),
        ].join(" "),
      );

      // the screens, in the order a run makes them
      const record = (kind: number, phase: any, before: string, lock: boolean) => {
        const count = phase.getModifierCount();
        const opts = phase.getModifierTypeOptions(count);
        phase.typeOptions = opts;
        const cost = phase.getRerollCost(lock);
        lines.push(`S ${kind} ${phase.rerollCount} ${lock ? 1 : 0} ${before}`);
        lines.push(
          ["O", count, opts.length, ...opts.flatMap((o: any) => [keyOf(o.type), argOf(o.type), o.type.tier ?? -1, o.upgradeCount]),
            cost, state()].join(" "),
        );
      };
      let phase: any;
      const restricted = rnd(8) === 0;
      scene.lockModifierTiers = false;
      Phaser.Math.RND.sow([seedStr()]);
      let before = state();
      phase = new SelectModifierPhase(0, undefined, restricted ? { fillRemaining: false, rerollMultiplier: -1 } : undefined, false);
      scene.resetSeed();
      regenerateModifierPoolThresholds(party, ModifierPoolType.PLAYER, 0);
      record(restricted ? 1 : 0, phase, before, false);
      const rerolls = restricted ? 0 : rnd(3);
      for (let r = 0; r < rerolls; r++) {
        const lock = rnd(2) === 0;
        scene.lockModifierTiers = lock;
        Phaser.Math.RND.sow([seedStr()]);
        before = state();
        const tiers = phase.typeOptions.map((o: any) => o.type?.tier).filter((t: any) => t !== undefined);
        phase = new SelectModifierPhase(phase.rerollCount + 1, tiers);
        regenerateModifierPoolThresholds(party, ModifierPoolType.PLAYER, phase.rerollCount);
        record(2, phase, before, lock);
      }
      if (rnd(2) === 0) {
        const lock = rnd(2) === 0;
        scene.lockModifierTiers = lock;
        Phaser.Math.RND.sow([seedStr()]);
        before = state();
        phase = phase.copy();
        record(3, phase, before, lock);
      }
      lines.push("E");
    } catch (e) {
      thrown++;
      lines.push(`X ${String(e).slice(0, 80).replace(/\s+/g, "_")}`);
      if (process.env.RD_STACKS) {
        console.error((e as any)?.stack);
      }
    }
  }
  party.length = 0;
  scene.modifiers.length = 0;
  lines.push(`Z ${N - thrown} ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
  console.log(`game-core reward producer: ${N} cases, ${thrown} thrown`);
}, 1_800_000);
