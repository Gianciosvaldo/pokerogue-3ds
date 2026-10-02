/*
 * Upstream half of the differential test. Copy into an upstream checkout as
 * test/tests/zz-dump/difftest.producer.test.ts and run with
 *   DIFF_OUT=/abs/cases.txt DIFF_N=5000 npx vitest run test/tests/zz-dump/difftest.producer.test.ts
 * (DIFF_SEED picks the case stream, DIFF_ABILITIES=0 keeps every ability at Ball Fetch,
 * DIFF_FOCUS=1 favours rare item, tag and ability interactions).
 * It builds random attacker/defender pairs inside a real upstream battle, asks
 * upstream for stats, crit rolls and damage, and writes cases + expected answers.
 * build/difftest replays each case through the C port and compares.
 */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getArenaTag, SuppressAbilitiesTag } from "#data/arena-tag";
import {
  AutotomizedTag,
  CritBoostTag,
  getBattlerTag,
  HighestStatBoostTag,
  StockpilingTag,
  SupremeOverlordTag,
} from "#data/battler-tags";
import { allAbilities, allMoves, modifierTypes } from "#data/data-lists";
import { Status } from "#data/status-effect";
import { Terrain } from "#data/terrain";
import { Weather } from "#data/weather";
import { AbilityId } from "#enums/ability-id";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { ArenaTagType } from "#enums/arena-tag-type";
import { BattlerTagType } from "#enums/battler-tag-type";
import { Command } from "#enums/command";
import { MoveCategory } from "#enums/move-category";
import { HitResult } from "#enums/hit-result";
import { MoveId } from "#enums/move-id";
import { MoveResult } from "#enums/move-result";
import { MoveUseMode } from "#enums/move-use-mode";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { StatusEffect } from "#enums/status-effect";
import { TrainerSlot } from "#enums/trainer-slot";
import { PokemonMove } from "#moves/pokemon-move";
import { generateModifierType } from "#mystery-encounters/encounter-phase-utils";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("difftest producer", async () => {
  const N = Number(process.env.DIFF_N ?? 2000);
  const ABILITIES = process.env.DIFF_ABILITIES !== "0";
  // DIFF_FOCUS=1 draws the focus moves, focus abilities and damage-path tags far more often,
  // for interactions that need several rare picks at once (Unburden's tag + ability + a speed move)
  const FOCUS = process.env.DIFF_FOCUS === "1";
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override
    .battleStyle("single")
    .ability(AbilityId.BALL_FETCH)
    .enemyAbility(AbilityId.BALL_FETCH)
    .startingWave(5)
    .enemySpecies(SpeciesId.MAGIKARP)
    .enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);

  // GameManager pins battle rolls to their maximum; put upstream's real body back.
  (BattleScene.prototype as any).randBattleSeedInt = function (this: any, range: number, min = 0) {
    return this.currentBattle?.randSeedInt(range, min);
  };
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  const A = game.field.getPlayerPokemon() as any;
  const D = game.field.getEnemyPokemon() as any;

  // Deterministic case generator (independent of Phaser's RNG).
  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const ids = Object.keys(speciesDataRegistry.data).map(Number);
  // Every implemented damaging move, including the ones whose power is -1 until an
  // attribute sets it (weight-, HP- and speed-based moves, fixed damage).
  const attacks = allMoves.filter((m: any) => m && m.category !== MoveCategory.STATUS && !m.isUnimplemented);

  const lines: string[] = [];
  const thrown: Record<string, number> = {};
  // Battle RNG state before each case, so one unported early-return can't desync the rest.
  const rngLine = () => {
    const st = battle.battleSeedState;
    return st ? "S " + st.split(",").slice(1).join(" ") : `Z ${battle.battleSeed} ${battle.turn}`;
  };

  // Real upstream items, built through the game's own modifier types. Each case gives
  // both Pokemon a few random held items (sometimes pointing at another Pokemon, or at
  // any Pokemon with id -1) and each side a few side-wide ones, in random list order.
  const item = (key: string, pregen?: any[]) => generateModifierType((modifierTypes as any)[key], pregen) as any;
  const coverSpecies = [
    SpeciesId.FARFETCHD, SpeciesId.GALAR_FARFETCHD, SpeciesId.SIRFETCHD, SpeciesId.PIKACHU, SpeciesId.CUBONE,
    SpeciesId.MAROWAK, SpeciesId.ALOLA_MAROWAK, SpeciesId.DITTO, SpeciesId.CLAMPERL, SpeciesId.EEVEE, SpeciesId.MEOWTH,
  ];
  const speciesItems = ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_SCALE", "DEEP_SEA_TOOTH"];
  const plainHeld = [
    "LEFTOVERS", "SOOTHE_BELL", "FOCUS_BAND", "QUICK_CLAW", "KINGS_ROCK", "SHELL_BELL", "LUCKY_EGG", "WIDE_LENS",
    "GRIP_CLAW", "TOXIC_ORB", "FLAME_ORB",
  ];
  const focus = [
    MoveId.KNOCK_OFF, MoveId.ACROBATICS, MoveId.SUPER_FANG, MoveId.RUINATION, MoveId.NATURES_MADNESS,
    MoveId.SEISMIC_TOSS, MoveId.DRAGON_RAGE, MoveId.ENDEAVOR, MoveId.FLY, MoveId.DIVE, MoveId.SOLAR_BEAM,
    MoveId.ROLLOUT, MoveId.ICE_BALL, MoveId.UPROAR, MoveId.DOUBLE_KICK, MoveId.BULLET_SEED, MoveId.EXPLOSION,
    MoveId.TACKLE, MoveId.QUICK_ATTACK, MoveId.EMBER, MoveId.WATER_GUN, MoveId.VINE_WHIP, MoveId.THUNDER_SHOCK,
    // moves whose damage reads a tag-dependent value: speed, weight, stockpiles, Minimize and
    // semi-invulnerable targets, variable category, sound (Substitute), Ground vs Flying
    MoveId.GYRO_BALL, MoveId.ELECTRO_BALL, MoveId.LOW_KICK, MoveId.GRASS_KNOT, MoveId.HEAVY_SLAM, MoveId.HEAT_CRASH,
    MoveId.SPIT_UP, MoveId.STOMP, MoveId.BODY_SLAM, MoveId.GUST, MoveId.TWISTER, MoveId.EARTHQUAKE, MoveId.SURF,
    MoveId.WHIRLPOOL, MoveId.PHOTON_GEYSER, MoveId.SHELL_SIDE_ARM, MoveId.BOOMBURST, MoveId.HYPER_VOICE,
    MoveId.THOUSAND_ARROWS, MoveId.MUD_SHOT,
    // the party (Beat Up), the global RNG (Present), priority (Grassy Glide vs Queenly Majesty)
    MoveId.BEAT_UP, MoveId.PRESENT, MoveId.GRASSY_GLIDE, MoveId.QUICK_ATTACK, MoveId.AQUA_JET, MoveId.BRAVE_BIRD,
    MoveId.DRAIN_PUNCH,
    // moves that ask hasAbility (Comatose), so a passive Comatose shows
    MoveId.HEX, MoveId.WAKE_UP_SLAP,
    // moves that read the turn history: the user's move history, the attacks it received this
    // turn, the last turn order
    MoveId.ROLLOUT, MoveId.ICE_BALL, MoveId.FURY_CUTTER, MoveId.ECHOED_VOICE, MoveId.STOMPING_TANTRUM,
    MoveId.TEMPER_FLARE, MoveId.REVENGE, MoveId.AVALANCHE, MoveId.COUNTER, MoveId.MIRROR_COAT, MoveId.METAL_BURST,
    MoveId.COMEUPPANCE, MoveId.FUSION_FLARE, MoveId.FUSION_BOLT,
  ]
    .map(id => allMoves[id])
    .filter(m => attacks.includes(m));
  // Abilities that meet tags: screens and Substitute (Infiltrator), Neutralizing Gas and the
  // abilities it can't suppress, contact through a Substitute, Protect, Autotomize-like weight
  const focusAbilities = [
    AbilityId.INFILTRATOR, AbilityId.NEUTRALIZING_GAS, AbilityId.FLUFFY, AbilityId.DISGUISE, AbilityId.ICE_FACE,
    AbilityId.UNSEEN_FIST, AbilityId.PIERCING_DRILL, AbilityId.UNBURDEN, AbilityId.COMATOSE, AbilityId.TERA_SHELL,
    AbilityId.LEVITATE, AbilityId.SCRAPPY, AbilityId.LONG_REACH, AbilityId.TOUGH_CLAWS, AbilityId.HEAVY_METAL,
    AbilityId.LIGHT_METAL, AbilityId.FLASH_FIRE, AbilityId.SOUNDPROOF, AbilityId.GALVANIZE, AbilityId.NORMALIZE,
    AbilityId.AERILATE, AbilityId.LIQUID_VOICE, AbilityId.STURDY,
    // the move queue (Analytic), later hits (Parental Bond), priority vs Queenly Majesty and co.
    AbilityId.ANALYTIC, AbilityId.PARENTAL_BOND, AbilityId.PRANKSTER, AbilityId.GALE_WINGS, AbilityId.TRIAGE,
    AbilityId.QUEENLY_MAJESTY, AbilityId.DAZZLING, AbilityId.ARMOR_TAIL, AbilityId.WONDER_GUARD,
    // the attacks received this turn (Sheer Force hits turn off Color Change and co.)
    AbilityId.SHEER_FORCE, AbilityId.COLOR_CHANGE, AbilityId.BERSERK, AbilityId.ANGER_SHELL, AbilityId.WIMP_OUT,
  ];
  const focusOdds = (n: number) => rnd(FOCUS ? 2 : n) === 0;
  // Moves whose priority an ability can raise, met with the abilities that block priority moves
  const priorityMoves = [
    MoveId.GRASSY_GLIDE, MoveId.BRAVE_BIRD, MoveId.DRAIN_PUNCH, MoveId.QUICK_ATTACK, MoveId.AQUA_JET, MoveId.BODY_SLAM,
  ].map(id => allMoves[id]);
  const priorityRaisers = [AbilityId.GALE_WINGS, AbilityId.TRIAGE, AbilityId.PRANKSTER];
  const priorityBlockers = [AbilityId.QUEENLY_MAJESTY, AbilityId.DAZZLING, AbilityId.ARMOR_TAIL];
  let caseMove: any;
  const giveItems = (p: any) => {
    const list = p.isPlayer() ? scene.modifiers : scene.enemyModifiers;
    const n = rnd(3) === 0 ? 0 : 1 + rnd(4);
    for (let k = 0; k < n; k++) {
      let key: string;
      let pregen: any[] | undefined;
      let stacks = 1;
      switch (rnd(15)) {
        case 0: key = "SCOPE_LENS"; break;
        case 1: key = "LEEK"; break;
        case 2: key = "EVIOLITE"; break;
        case 3: key = "SPECIES_STAT_BOOSTER"; pregen = [speciesItems[rnd(speciesItems.length)]]; break;
        case 4:
        case 5:
          key = "ATTACK_TYPE_BOOSTER";
          pregen = [rnd(2) === 0 ? caseMove.type : rnd(18)];
          stacks = 1 + rnd(99);
          break;
        case 6: key = "BASE_STAT_BOOSTER"; pregen = [rnd(6)]; stacks = 1 + rnd(31); break;
        case 7: key = "MULTI_LENS"; stacks = 1 + rnd(3); break;
        case 8: key = "SOUL_DEW"; stacks = 1 + rnd(10); break;
        case 9: key = "MYSTERY_ENCOUNTER_SHUCKLE_JUICE"; pregen = [rnd(2) === 0 ? 10 : -15]; stacks = 1 + rnd(2); break;
        case 10: key = "MYSTERY_ENCOUNTER_OLD_GATEAU"; break;
        case 11: key = "MYSTERY_ENCOUNTER_MACHO_BRACE"; stacks = rnd(3) === 0 ? 50 : 1 + rnd(50); break;
        case 12: key = "BERRY"; pregen = [rnd(4)]; stacks = 1 + rnd(3); break;
        default: key = plainHeld[rnd(plainHeld.length)]; stacks = 1 + rnd(3);
      }
      const m = item(key, pregen).newModifier(p);
      m.stackCount = stacks;
      if (rnd(8) === 0) m.virtualStackCount = 1 + rnd(2);
      const who = rnd(12);
      if (who === 0) m.pokemonId = -1;
      else if (who === 1) m.pokemonId = 424242;
      list.splice(rnd(list.length + 1), 0, m);
    }
    // Side-wide items: X items, Dire Hit, a non-battle item, and the enemy's tokens
    const side: any[] = [];
    if (rnd(4) === 0) side.push(item("TEMP_STAT_STAGE_BOOSTER", [1 + rnd(6)]).newModifier());
    if (rnd(6) === 0) side.push(item("DIRE_HIT").newModifier());
    if (rnd(6) === 0) side.push(item("EXP_CHARM").newModifier());
    if (!p.isPlayer()) {
      for (const key of [
        "ENEMY_DAMAGE_BOOSTER",
        "ENEMY_DAMAGE_REDUCTION",
        "ENEMY_HEAL",
        "ENEMY_ATTACK_POISON_CHANCE",
        "ENEMY_ATTACK_PARALYZE_CHANCE",
        "ENEMY_ATTACK_BURN_CHANCE",
        "ENEMY_STATUS_EFFECT_HEAL_CHANCE",
        "ENEMY_ENDURE_CHANCE",
        "ENEMY_FUSED_CHANCE",
      ]) {
        if (rnd(3) === 0) {
          const m = item(key).newModifier();
          const maxStack = m.getMaxStackCount ? m.getMaxStackCount() : 10;
          m.stackCount = key === "ENEMY_HEAL"
            ? (rnd(3) === 0 ? 1 + rnd(10) : 1 + rnd(5))
            : (maxStack === 10 ? 1 + rnd(10) : (rnd(6) === 0 ? 1 + rnd(999) : 1 + rnd(40)));
          side.push(m);
        }
      }
    }
    for (const m of side) list.splice(rnd(list.length + 1), 0, m);
  };
  const chainOf = (m: any) => {
    const c: string[] = [];
    for (let o = Object.getPrototypeOf(m); o && o !== Object.prototype; o = Object.getPrototypeOf(o)) {
      c.push(o.constructor.name);
    }
    return c.join("<");
  };
  const modLine = (sideNo: number, m: any) => {
    const stats: number[] = Array.isArray(m.stats) ? m.stats : [];
    const species: number[] = Array.isArray(m.species) ? m.species : [];
    const mult = m.multiplier ?? m.boostMultiplier ?? m.boost ?? m.damageMultiplier ?? 0;
    return [
      "M", sideNo, chainOf(m), m.pokemonId ?? 0, m.stackCount, m.virtualStackCount, m.isTransferable ? 1 : 0,
      typeof m.stat === "number" ? m.stat : -1, stats.reduce((b, st) => b | (1 << st), 0), mult,
      typeof m.moveType === "number" ? m.moveType : -1, m.stageIncrement ?? m.statModifier ?? 0,
      species.length, ...species,
    ].join(" ");
  };

  // Real upstream tags, built by the game's own factories (getBattlerTag / getArenaTag) and put
  // straight into the tag lists, as loading a save does, so no onAdd side effect runs; the
  // fields onAdd would set are set here. Half the picks are tags the damage path reads, the
  // other half any tag type, so a read the port doesn't know about shows up as a mismatch.
  const allBattlerTags = Object.values(BattlerTagType) as BattlerTagType[];
  const hotBattlerTags = [
    BattlerTagType.FIRE_BOOST, BattlerTagType.CHARGED, BattlerTagType.HELPING_HAND, BattlerTagType.SUPREME_OVERLORD,
    BattlerTagType.CRIT_BOOST, BattlerTagType.DRAGON_CHEER, BattlerTagType.ALWAYS_CRIT, BattlerTagType.TAR_SHOT,
    BattlerTagType.PROTOSYNTHESIS, BattlerTagType.QUARK_DRIVE, BattlerTagType.IGNORE_GHOST, BattlerTagType.IGNORE_DARK,
    BattlerTagType.STOCKPILING, BattlerTagType.AUTOTOMIZED, BattlerTagType.SUBSTITUTE, BattlerTagType.ELECTRIFIED,
    BattlerTagType.SLOW_START, BattlerTagType.UNBURDEN, BattlerTagType.IGNORE_FLYING, BattlerTagType.FLOATING,
    BattlerTagType.TELEKINESIS, BattlerTagType.FLYING, BattlerTagType.UNDERGROUND, BattlerTagType.UNDERWATER,
    BattlerTagType.HIDDEN, BattlerTagType.RECEIVE_DOUBLE_DAMAGE, BattlerTagType.MINIMIZED, BattlerTagType.STURDY,
    BattlerTagType.CONFUSED, BattlerTagType.PROTECTED, BattlerTagType.KINGS_SHIELD, BattlerTagType.BANEFUL_BUNKER,
  ];
  const giveTags = (p: any) => {
    // Unburden's tag only ever sits on an Unburden Pokemon (it's added when its item goes)
    if ((p.summonData.ability === AbilityId.UNBURDEN || p.customPokemonData.passive === AbilityId.UNBURDEN) && rnd(2) === 0) {
      p.summonData.tags.push(getBattlerTag(BattlerTagType.UNBURDEN, 1, caseMove.id, p.id));
    }
    const n = rnd(2) === 0 ? 0 : 1 + rnd(3);
    for (let k = 0; k < n; k++) {
      const type = rnd(FOCUS ? 4 : 2) !== 0 ? hotBattlerTags[rnd(hotBattlerTags.length)] : allBattlerTags[rnd(allBattlerTags.length)];
      if (p.getTag(type)) {
        continue; // addTag never holds two tags of one type
      }
      const tag: any = getBattlerTag(type, 1 + rnd(5), caseMove.id, rnd(2) === 0 ? A.id : D.id);
      if (!tag) {
        continue;
      }
      if (tag instanceof CritBoostTag) {
        (tag as any).critStages = 1 + rnd(2);
      } else if (tag instanceof SupremeOverlordTag) {
        (tag as any).faintCount = rnd(6);
      } else if (tag instanceof HighestStatBoostTag) {
        tag.stat = 1 + rnd(5);
        tag.multiplier = tag.stat === Stat.SPD ? 1.5 : 1.3;
      } else if (tag instanceof StockpilingTag) {
        tag.stockpiledCount = rnd(4);
      } else if (tag instanceof AutotomizedTag) {
        tag.autotomizeCount = rnd(4);
      }
      p.summonData.tags.push(tag);
    }
  };
  const allArenaTags = (Object.values(ArenaTagType) as ArenaTagType[]).filter(t => t !== ArenaTagType.NONE);
  const hotArenaTags = [
    ArenaTagType.REFLECT, ArenaTagType.LIGHT_SCREEN, ArenaTagType.AURORA_VEIL, ArenaTagType.MUD_SPORT,
    ArenaTagType.WATER_SPORT, ArenaTagType.NO_CRIT, ArenaTagType.NEUTRALIZING_GAS, ArenaTagType.ION_DELUGE,
    ArenaTagType.GRAVITY, ArenaTagType.TAILWIND, ArenaTagType.GRASS_WATER_PLEDGE,
  ];
  const giveArenaTags = () => {
    scene.arena.tags.length = 0;
    const n = rnd(2) === 0 ? 0 : 1 + rnd(3);
    for (let k = 0; k < n; k++) {
      const type = rnd(FOCUS ? 4 : 2) !== 0 ? hotArenaTags[rnd(hotArenaTags.length)] : allArenaTags[rnd(allArenaTags.length)];
      const side = [ArenaTagSide.BOTH, ArenaTagSide.PLAYER, ArenaTagSide.ENEMY][rnd(3)];
      if (scene.arena.getTagOnSide(type, side)) {
        continue; // addTag would only overlap the existing tag
      }
      const tag: any = getArenaTag(type, 1 + rnd(5), caseMove.id, rnd(2) === 0 ? A.id : D.id, side);
      if (!tag) {
        continue;
      }
      if (tag instanceof SuppressAbilitiesTag) {
        (tag as any).sourceCount = 1 + rnd(2);
      }
      scene.arena.tags.push(tag);
    }
  };
  const tagLine = (who: number, t: any) =>
    [
      "T", who, t.tagType, chainOf(t), t.turnCount, t.sourceMove ?? 0, t.sourceId ?? -1,
      t instanceof CritBoostTag ? t.critStages : 0,
      t instanceof StockpilingTag ? t.stockpiledCount : 0,
      t instanceof AutotomizedTag ? t.autotomizeCount : 0,
      t instanceof SupremeOverlordTag ? t.faintCount : 0,
      t instanceof HighestStatBoostTag ? t.stat : -1,
      t instanceof HighestStatBoostTag ? t.multiplier : 0,
    ].join(" ");
  const arenaLine = (t: any) =>
    [
      "A", t.tagType, chainOf(t), t.side, t.turnCount, t.maxDuration, t.sourceMove ?? 0, t.sourceId ?? -1,
      t instanceof SuppressAbilitiesTag ? t.sourceCount : 0, t.beingRemoved ? 1 : 0,
    ].join(" ");

  // Bench Pokemon for the parties (Beat Up walks the whole party): made once, given a random
  // species and status per case. The Pokemon on the field stays first in its party.
  const benchPool = (player: boolean) =>
    Array.from({ length: 5 }, () =>
      player
        ? scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5)
        : scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA), 5, TrainerSlot.NONE),
    );
  const playerBench = benchPool(true);
  const enemyBench = benchPool(false);
  const benchStatuses = [
    StatusEffect.NONE, StatusEffect.NONE, StatusEffect.NONE, StatusEffect.BURN, StatusEffect.POISON,
    StatusEffect.SLEEP, StatusEffect.FAINT,
  ];
  const giveParty = (p: any, pool: any[], party: any[]) => {
    const n = rnd(3) === 0 ? 0 : 1 + rnd(pool.length);
    party.length = 0;
    party.push(p);
    for (const b of pool.slice(0, n)) {
      b.species = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
      const st = benchStatuses[rnd(benchStatuses.length)];
      b.status = st ? new Status(st) : null;
      party.push(b);
    }
  };
  const partyLine = (side: number, party: any[]) =>
    ["P", side, party.length, ...party.flatMap((m: any) => [m.id, m.species.speciesId, m.status?.effect ?? 0])].join(" ");

  // Turn history: summonData.moveHistory (oldest first) and turnData.attacksReceived (newest
  // first), set as the move and damage phases leave them. Histories run from empty to longer than
  // the port keeps (8 entries), and favour the moves the readers compare with: the case move,
  // Rollout's combo move Defense Curl, Fusion Flare and Bolt.
  const moveResults = [
    MoveResult.SUCCESS, MoveResult.SUCCESS, MoveResult.SUCCESS, MoveResult.FAIL, MoveResult.MISS, MoveResult.OTHER,
    MoveResult.PENDING, undefined,
  ];
  const useModes = Object.values(MoveUseMode).filter(v => typeof v === "number") as number[];
  const damageResults = [
    HitResult.EFFECTIVE, HitResult.EFFECTIVE, HitResult.SUPER_EFFECTIVE, HitResult.NOT_VERY_EFFECTIVE,
    HitResult.EXTREMELY_EFFECTIVE, HitResult.MOSTLY_INEFFECTIVE, HitResult.ONE_HIT_KO, HitResult.CONFUSION,
    HitResult.INDIRECT, HitResult.INDIRECT_KO,
  ];
  const historyMove = () => {
    const hot = [caseMove.id, MoveId.DEFENSE_CURL, MoveId.FUSION_FLARE, MoveId.FUSION_BOLT];
    return rnd(FOCUS ? 4 : 2) !== 0
      ? hot[rnd(hot.length)]
      : rnd(2) === 0
        ? attacks[rnd(attacks.length)].id
        : rnd(allMoves.length);
  };
  const giveHistory = (p: any) => {
    const r = rnd(8);
    const n = r < 3 ? 0 : r < 7 ? 1 + rnd(4) : 5 + rnd(6);
    // One history in 3 is a streak: the case move (or Defense Curl) used again and again, as
    // Rollout, Fury Cutter and Echoed Voice count, with the odd failure
    const streak = rnd(3) === 0;
    for (let k = 0; k < n; k++) {
      p.summonData.moveHistory.push({
        move: streak ? (rnd(6) === 0 ? MoveId.DEFENSE_CURL : caseMove.id) : historyMove(),
        targets: [],
        useMode: useModes[rnd(useModes.length)],
        result: streak && rnd(6) !== 0 ? MoveResult.SUCCESS : moveResults[rnd(moveResults.length)],
      });
    }
    const other = p === A ? D : A;
    const m = rnd(2) === 0 ? 0 : rnd(16) === 0 ? 9 + rnd(3) : 1 + rnd(3);
    for (let k = 0; k < m; k++) {
      const pick = rnd(8);
      const source = pick < 5 ? other : pick < 7 ? p : null; // null: a Pokemon not on the field
      p.turnData.attacksReceived.push({
        move: rnd(3) === 0 ? rnd(allMoves.length) : attacks[rnd(attacks.length)].id,
        result: damageResults[rnd(damageResults.length)],
        damage: rnd(4) === 0 ? 0 : 1 + rnd(300),
        critical: rnd(8) === 0,
        sourceId: source ? source.id : 424242,
        sourceBattlerIndex: source && rnd(4) !== 0 ? source.getBattlerIndex() : rnd(5) - 1,
      });
    }
  };
  const historyLines = (who: number, p: any) => [
    ["Hm", who, p.summonData.moveHistory.length,
      ...p.summonData.moveHistory.flatMap((t: any) => [t.move, t.useMode, t.result ?? -1])].join(" "),
    ["Ha", who, p.turnData.attacksReceived.length,
      ...p.turnData.attacksReceived.flatMap((r: any) =>
        [r.move, r.result, r.critical ? 1 : 0, r.damage, r.sourceId, r.sourceBattlerIndex])].join(" "),
  ];

  const setup = (p: any) => {
    // Non-simulated calls can leave per-turn state behind (e.g. Tera Shell stores
    // turnData.moveEffectiveness), so every case starts from fresh transient data.
    p.resetSummonData();
    p.resetTurnData();
    p.resetBattleAndWaveData();
    const sid = rnd(12) === 0 ? coverSpecies[rnd(coverSpecies.length)] : ids[rnd(ids.length)];
    const sp = speciesDataRegistry.getSpecies(sid);
    const form = sp.forms.length ? rnd(sp.forms.length) : 0;
    p.species = sp;
    p.formIndex = form;
    p.fusionSpecies = null;
    p.level = 1 + rnd(100);
    p.ivs = [0, 0, 0, 0, 0, 0].map(() => rnd(32));
    p.nature = rnd(25);
    p.summonData.types = [];
    if (p === D) {
      p.bossSegments = rnd(5) === 0 ? 2 : 0;
    }
    // Passives: Pokemon.passive (bosses always have one), with the species' own passive or a
    // random one in customPokemonData. Set before calculateStats (a Wonder Guard passive sets HP to 1).
    p.passive = ABILITIES && rnd(2) === 0;
    p.customPokemonData.passive = !ABILITIES
      ? AbilityId.BALL_FETCH
      : rnd(3) === 0
        ? -1
        : focusOdds(8)
          ? focusAbilities[rnd(focusAbilities.length)]
          : 1 + rnd(allAbilities.length - 1);
    // Active ability via summonData (wins over the Ball Fetch overrides, which still
    // govern the permanent ability that calculateStats looks at). DIFF_ABILITIES=0 keeps Ball Fetch.
    let ability =
      ABILITIES && rnd(2) === 0
        ? focusOdds(8)
          ? focusAbilities[rnd(focusAbilities.length)]
          : 1 + rnd(allAbilities.length - 1)
        : AbilityId.BALL_FETCH;
    if (ABILITIES && FOCUS && priorityMoves.includes(caseMove) && rnd(2) === 0) {
      ability = (p === A ? priorityRaisers : priorityBlockers)[rnd(3)];
      // an Aerilate passive turns a Normal move Flying, which Gale Wings reads through getMoveType
      if (ability === AbilityId.GALE_WINGS && rnd(2) === 0) {
        p.passive = true;
        p.customPokemonData.passive = AbilityId.AERILATE;
      }
    }
    p.summonData.ability = ability;
    // now and then the passive is the ability itself (applyAbAttrs then skips the passive)
    if (ABILITIES && rnd(8) === 0) {
      p.customPokemonData.passive = ability;
    }
    giveItems(p);
    p.calculateStats();
    p.hp = rnd(3) === 0 ? p.getMaxHp() : 1 + rnd(p.getMaxHp());
    const stages = [0, 0, 0, 0, 0, 0, 0].map(() => (rnd(3) === 0 ? rnd(13) - 6 : 0));
    stages.forEach((v, i) => p.setStatStage(i + 1, v));
    const s = [
      StatusEffect.NONE,
      StatusEffect.NONE,
      StatusEffect.BURN,
      StatusEffect.PARALYSIS,
      StatusEffect.POISON,
      StatusEffect.TOXIC,
      StatusEffect.SLEEP,
      StatusEffect.FREEZE,
    ][rnd(8)];
    p.status = s ? new Status(s) : null;
    p.isTerastallized = rnd(6) === 0;
    p.teraType = rnd(3) === 0 ? caseMove.type : rnd(19);
    p.gender = rnd(3) - 1; // GENDERLESS -1, MALE 0, FEMALE 1
    p.stellarTypesBoosted = rnd(2) === 0 ? [] : [rnd(19), rnd(19)];
    p.friendship = rnd(256);
    // Per-turn and per-summon state read on the damage path
    p.summonData.hitCount = rnd(4) === 0 ? rnd(8) : 0;
    p.turnData.acted = rnd(2) === 0;
    p.turnData.statStagesDecreased = rnd(3) === 0;
    p.turnData.joinedRound = rnd(4) === 0;
    p.turnData.damageTaken = rnd(3) === 0 ? rnd(200) : 0;
    p.turnData.combiningPledge =
      rnd(4) === 0 ? [MoveId.FIRE_PLEDGE, MoveId.WATER_PLEDGE, MoveId.GRASS_PLEDGE][rnd(3)] : undefined;
    p.turnData.hitCount = rnd(4);
    p.turnData.hitsLeft = rnd(4) - 1;
    giveTags(p);
    giveHistory(p);
    return [sid, form, p.level, ...p.ivs, p.nature, ...stages, s, p.isTerastallized ? 1 : 0, p.teraType, p.hp, ability, p.gender];
  };
  // Fields read after the move is known (the moveset may hold it)
  const late = (p: any) => {
    const mask = p.stellarTypesBoosted.reduce((m: number, t: number) => m | (1 << t), 0);
    const moves = [0, 1, 2, 3].map(i => p.moveset[i]?.moveId ?? 0);
    const pp = [0, 1, 2, 3].map(i => p.moveset[i]?.ppUsed ?? 0);
    return [
      p.isBoss() ? 1 : 0,
      mask,
      p.summonData.hitCount,
      p.getMoveHistory().length,
      p.turnData.attacksReceived.length,
      p.turnData.acted ? 1 : 0,
      p.turnData.statStagesDecreased ? 1 : 0,
      p.turnData.joinedRound ? 1 : 0,
      p.turnData.damageTaken,
      p.turnData.combiningPledge ?? 0,
      p.turnData.hitCount,
      p.turnData.hitsLeft,
      p.summonData.tags.length,
      p.friendship,
      p.hasTrainer() ? 1 : 0,
      ...moves,
      ...pp,
      p.id,
      p.passive ? 1 : 0,
      p.customPokemonData.passive,
    ];
  };

  for (let i = 0; i < N; i++) {
    battle.turn = 1 + rnd(30);
    // One case in 8 (1 in 2 with DIFF_FOCUS=1) uses a move with rare item or tag interactions
    const move = focusOdds(8) ? focus[rnd(focus.length)] : attacks[rnd(attacks.length)];
    caseMove = move;
    scene.modifiers.length = 0;
    scene.enemyModifiers.length = 0;
    const a = setup(A);
    const d = setup(D);
    const weather = [0, 0, 0, 1, 2, 3, 5, 9][rnd(8)];
    scene.arena.weather = weather ? new Weather(weather, 5) : null;
    const terrain = [0, 0, 0, 1, 2, 3, 4][rnd(7)];
    scene.arena.terrain = terrain ? new Terrain(terrain, 5) : null;
    giveArenaTags();
    giveParty(A, playerBench, scene.getPlayerParty());
    giveParty(D, enemyBench, scene.getEnemyParty());
    // Sometimes a MovePhase is still queued this turn (for either Pokemon), as Analytic checks
    let queued: any = null;
    if (rnd(4) === 0) {
      const who = rnd(2) === 0 ? A : D;
      const at = who === A ? D : A;
      queued = scene.phaseManager.create("MovePhase", who, [at.getBattlerIndex()], new PokemonMove(MoveId.TACKLE), MoveUseMode.NORMAL);
      scene.phaseManager.dynamicQueueManager.queueDynamicPhase(queued);
    }
    scene.arena.playerFaints = rnd(3) === 0 ? rnd(5) : 0;
    battle.enemyFaints = rnd(3) === 0 ? rnd(5) : 0;
    battle.playerFaintsHistory = rnd(3) === 0 ? [{ pokemon: A, turn: battle.turn - 1 - rnd(2) }] : [];
    battle.enemyFaintsHistory = rnd(3) === 0 ? [{ pokemon: D, turn: battle.turn - 1 - rnd(2) }] : [];
    battle.turnCommands = {};
    for (const p of [A, D]) {
      battle.turnCommands[p.getBattlerIndex()] = { command: rnd(3) === 0 ? Command.BALL : Command.FIGHT };
    }
    const crit = rnd(4) === 0;
    const swap = rnd(4) === 0; // the enemy attacks the player
    const src = swap ? D : A;
    const def = swap ? A : D;
    src.moveset = rnd(3) === 0 ? [new PokemonMove(move.id, rnd(move.pp + 1))] : [];
    def.moveset = [];
    // The last turn order: the Pokemon whose MovePhase started this turn, usually ending with
    // the attacker (the Pokemon moving now)
    const turnOrder = scene.phaseManager.dynamicQueueManager.getLastTurnOrder();
    turnOrder.length = 0;
    // Fusion Flare and Bolt look back through it for each other, passing over failed moves, so
    // for them it is more often long enough to hold a failure between the two
    const fusion = move.id === MoveId.FUSION_FLARE || move.id === MoveId.FUSION_BOLT;
    const partner = move.id === MoveId.FUSION_FLARE ? MoveId.FUSION_BOLT : MoveId.FUSION_FLARE;
    const nOrder = fusion && rnd(2) === 0 ? 2 + rnd(3) : rnd(3) === 0 ? 0 : 1 + rnd(4);
    for (let k = 0; k < nOrder; k++) {
      turnOrder.push(k === nOrder - 1 && rnd(4) !== 0 ? src : [A, D][rnd(2)]);
    }
    // A Pokemon that moved earlier this turn usually has that move as its newest history entry
    for (const p of turnOrder.slice(0, -1)) {
      if (rnd(4) !== 0) {
        p.summonData.moveHistory.push({
          move: rnd(2) === 0 ? (fusion ? partner : [MoveId.FUSION_FLARE, MoveId.FUSION_BOLT][rnd(2)]) : historyMove(),
          targets: [],
          useMode: useModes[rnd(useModes.length)],
          result: [MoveResult.SUCCESS, MoveResult.SUCCESS, MoveResult.FAIL, MoveResult.FAIL, MoveResult.MISS,
            MoveResult.OTHER, undefined][rnd(7)],
        });
      }
    }
    const faintTurn = (h: any[]) => (h.length ? h.at(-1).turn : -1);
    const rl = rngLine();
    const globalRng = `G ${Phaser.Math.RND.state().split(",").slice(1).join(" ")}`;
    const queuedIds: number[] = [];
    scene.phaseManager.hasPhaseOfType("MovePhase", (ph: any) => {
      queuedIds.push(ph.pokemon.id);
      return false;
    });
    // Super Fang's lens hits read the HP its attribute kept from an earlier first hit
    const halfHpAttr: any = move.attrs.find((at: any) => at.constructor.name === "TargetHalfHpDamageAttr");
    const halfHpLine = halfHpAttr ? `H ${halfHpAttr.initialHp === undefined ? "NaN" : halfHpAttr.initialHp}` : null;
    const partyLines = [partyLine(0, scene.getPlayerParty()), partyLine(1, scene.getEnemyParty())];
    const worldLine =
      `W ${battle.turn} ${scene.waveSeed} ${terrain} ${scene.arena.playerFaints} ${battle.enemyFaints}`
        + ` ${faintTurn(battle.playerFaintsHistory)} ${faintTurn(battle.enemyFaintsHistory)}`
        + ` ${scene.phaseManager.dynamicQueueManager.getLastTurnOrder().length} ${scene.arena.tags.length}`
        + ` ${scene.modifiers.length} ${scene.enemyModifiers.length} ${scene.getMaxExpLevel()}`
        + ` ${[0, 1, 2, 3].map(k => battle.turnCommands[k]?.command ?? -1).join(" ")}`;
    const caseLine =
      `C ${a.join(" ")} ${late(A).join(" ")} ${d.join(" ")} ${late(D).join(" ")} ${weather} ${move.id} ${crit ? 1 : 0} ${swap ? 1 : 0}`;
    // Turn order and histories as they stand before the calls
    const turnLines = [
      `O ${turnOrder.length} ${turnOrder.map((p: any) => p.id).join(" ")}`.trim(),
      ...historyLines(0, A),
      ...historyLines(1, D),
    ];
    // Tags as they stand before the calls (a non-simulated call may add some, e.g. Sturdy's)
    const tagLines = [
      ...A.summonData.tags.map((t: any) => tagLine(0, t)),
      ...D.summonData.tags.map((t: any) => tagLine(1, t)),
      ...scene.arena.tags.map((t: any) => arenaLine(t)),
    ];
    let sim: any;
    let critRoll: boolean;
    let real: any;
    try {
      sim = def.getAttackDamage({ source: src, move, isCritical: crit, simulated: true });
      critRoll = def.getCriticalHitResult(src, move);
      real = def.getAttackDamage({ source: src, move, isCritical: crit, simulated: false });
    } catch (e) {
      // Some random states can't happen in a real battle and crash upstream (e.g. Beat Up
      // with inconsistent hit counters). Such cases are left out and counted.
      thrown[move.name] = (thrown[move.name] ?? 0) + 1;
      continue;
    } finally {
      if (queued) {
        scene.phaseManager.dynamicQueueManager.removePhase("MovePhase", (ph: any) => ph === queued);
      }
    }
    lines.push(rl, worldLine, globalRng, ...partyLines, `Q ${queuedIds.length} ${queuedIds.join(" ")}`.trim());
    lines.push(...turnLines);
    if (halfHpLine) {
      lines.push(halfHpLine);
    }
    scene.modifiers.forEach((m: any) => lines.push(modLine(0, m)));
    scene.enemyModifiers.forEach((m: any) => lines.push(modLine(1, m)));
    lines.push(...tagLines);
    lines.push(caseLine);
    lines.push(
      `E ${A.stats.join(" ")} ${D.stats.join(" ")} ${sim.damage} ${sim.result} ${critRoll ? 1 : 0} ${real.damage} ${real.result}`
        // what the calls left in the attacker's turn data (Present's heal ends the move's other hits)
        + ` ${src.turnData.hitCount} ${src.turnData.hitsLeft}`,
    );
  }
  lines.push(`# upstream threw (case left out): ${JSON.stringify(thrown)}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
