/*
 * Upstream half of the whole-turn differential test. Copy into an upstream checkout as
 * test/tests/zz-dump/turntest.producer.test.ts and run with
 *   TURN_OUT=/abs/turns.txt TURN_N=2000 npx vitest run test/tests/zz-dump/turntest.producer.test.ts
 * (TURN_SEED picks the case stream; TURN_RICH=1 adds random abilities and statuses, much of
 * which the C engine doesn't cover yet and reports as such; TURN_COND=1 draws most moves from
 * the ones with their own failure conditions and gives each Pokemon a short move history and
 * a wave turn count, so those conditions get exercised; TURN_PROBE=1 does what TURN_COND does
 * with random abilities and statuses, and instead of running a turn calls every move's own
 * conditions, move.applyConditions(user, target, seq) for seq 2, 3 and 4, both ways, writing
 * a PROBE line with the result and the RNG state after each call, after calling the
 * StatStageChangeAttr conditions of the moves that have one, as seq=0 PROBE lines; TURN_STAT=1 aims at the
 * stat-change phase: most moves change stat stages, half the abilities react to stat changes
 * (Simple, Contrary, Clear Body, Mirror Armor, Defiant, Opportunist...), stat stages start near
 * the caps two cases in three, a quarter of the Pokemon are terastallized (Stellar included),
 * one in eight has a status, one case in eight gives both sides the same ability, and a quarter of
 * the cases have Mist on one side); TURN_STATUS=1 aims at statuses: most moves set one (Thunder
 * Wave, Scald, Tri Attack...) or thaw the user, half the abilities deal with statuses (Limber,
 * Sweet Veil, Flower Veil, Corrosion, Synchronize, Early Bird, Poison Heal, Infiltrator...), a
 * quarter of the Pokemon start with a status, a fifth are terastallized and a quarter of the cases
 * have Safeguard on one side; TURN_SPROBE=1 sets up states as TURN_PROBE does (weather, terrain,
 * arena and battler tags) with those abilities and some pending statuses, and instead of running a
 * turn calls pokemon.canSetStatus(effect, quiet=true, overrideStatus, source, ignoreField) for
 * both Pokemon, every effect and three sources (the other one, itself, none), writing an SPROBE
 * line with the result, the abilities applied so far and the RNG state after each call;
 * TURN_ONHIT=1 aims at the abilities that react to a hit (Static, Flame Body, Effect Spore, Rough
 * Skin, Justified, Weak Armor, Berserk, Anger Point, Color Change, Cotton Down, Poison Touch,
 * Stench...): two abilities in three come from them or from the ones they check (Magic Guard,
 * Overcoat, Shield Dust, Long Reach), a quarter of the cases give one side one of those checked
 * abilities or Mirror Armor, most moves are attacks that make contact, one Pokemon in six starts
 * with a status and one in six is terastallized, and a quarter have random stat stages;
 * TURN_CHARGE=1 aims at two-turn moves (Fly, Dig, Solar Beam, Skull Bash...): most moves are
 * two-turn moves or moves that reach a semi-invulnerable Pokemon (Thunder, Earthquake, Surf...),
 * some Pokemon have No Guard, half the Pokemon holding a two-turn move start on its second
 * turn, as MoveChargePhase and the command phases leave it, and half the Pokemon facing one that
 * knows Fly or Bounce use Smack Down, Thousand Arrows or Gravity;
 * TURN_TAGS=1 aims at the battler tags (flinch, confusion, Outrage's lock-in, Mean Look's trap, No
 * Retreat, Minimize, Charge, Tar Shot, Autotomize, Glaive Rush, Lock-On, Laser Focus, Perish Song,
 * Roost, Throat Chop, Heal Block, Aqua Ring, Ingrain, Octolock, Disable, Taunt, Torment, Encore,
 * Attract, Salt Cure, Curse, Nightmare, Yawn, Powder, Magic Coat, Destiny Bond, Grudge, Stockpile,
 * Power Trick, Burn Up...): most moves add one of them (Curse, Destiny Bond, Spit Up and Swallow
 * included) or read one (Stomp, Electric and Fire attacks, weight-based moves, sound moves, the
 * moves Heal Block stops, inaccurate attacks), a quarter of the Pokemon have an ability that reacts
 * to them or adds one (Steadfast, Inner Focus, Own Tempo, Cursed Body, Cute Charm, Magic Bounce,
 * Truant, Slow Start, Sturdy, Flash Fire, Gorilla Tactics, Supreme Overlord, Lightning Rod...), a
 * third start with some of those tags (Disable and Taunt with 1 to 4 turns left; half the time
 * a Pokemon already infatuated faces Attract, one already cursed a Ghost-type Curse, a powdered
 * one uses a Fire move and a stockpiling one Spit Up or Swallow), a tenth have Truant with its
 * tag, half the Pokemon holding a rampage move start in the middle of one (FRENZY, the queued
 * move, a successful history entry), a sixth of the cases have a terrain (Electric Terrain for
 * half the Yawn users) and an eighth rain or heavy rain;
 * TURN_HEAL=1 aims at recoil, drain, recharge and the heal phase: most moves have recoil, drain or
 * recharge (Struggle and Chloroblast included) or are attacks, half the abilities block recoil,
 * turn drain into damage, heal from a status or from a move's type (Rock Head, Magic Guard, Liquid
 * Ooze, Poison Heal, Volt Absorb, Water Absorb...), a third of the Pokemon are poisoned, and half
 * the Pokemon that know a recharge move start the turn recharging, as the command phases leave it;
 * TURN_WEATHER=1 aims at weather and terrain over a turn: seven cases in eight have a weather (the
 * primal ones included; a third last forever, the others have 1 to 5 turns left) and half have a
 * terrain, most moves are Water or Fire moves, two-turn moves (Dig, Dive...), moves that read the
 * weather or terrain (Thunder, Blizzard, Weather Ball, Solar Beam, Grassy Glide...), moves that
 * confuse (Misty Terrain blocks it) or moves whose type can change to or from Fire or Water (Hidden
 * Power, Revelation Dance, Tera Blast...), half the abilities deal with weather or terrain (Cloud
 * Nine, Overcoat, Sand Veil, Rain Dish, Dry Skin, Solar Power, Forecast, Protosynthesis, Mimicry,
 * Magic Guard, Levitate, Swift Swim, Hydration, Normalize, Liquid Voice...),
 * a quarter of the Pokemon have a status, a sixth are terastallized and a quarter start with
 * Protosynthesis's or Quark Drive's stat boost;
 * TURN_BOOST=1 is TURN_WEATHER aimed at Protosynthesis and Quark Drive picking their stat: three
 * Pokemon in four have one of them, half have random stat stages to compare, and a third of the
 * moves set a weather or terrain (Sunny Day, Electric Terrain, Rain Dance...);
 * TURN_TRAPS=1 aims at the damaging traps (Bind, Wrap, Fire Spin, Whirlpool, Clamp, Sand Tomb,
 * Magma Storm, Snap Trap, Thunder Cage, Infestation): most moves add one, free the user (Rapid
 * Spin, Mortal Spin) or clear arena tags (Brick Break, Defog, Tidy Up...), a quarter of the
 * abilities are Magic Guard, half the Pokemon start trapped by the other one (1 to 5 turns left)
 * and some of the rest by Mean Look, a third of the cases have entry hazards, screens, Mist or
 * Safeguard on one side, a quarter a terrain and an eighth fog;
 * TURN_PROTECT=1 aims at Leech Seed, Substitute and the protections: most moves are Protect-family
 * moves (Endure included), Substitute, Shed Tail, Tidy Up, Leech Seed, Rapid Spin, Feint, or
 * attacks that hit several times, are sound-based, go through a Substitute or a protection, or
 * make contact, and status moves aimed at the other Pokemon; a third of the abilities are Infiltrator,
 * Unseen Fist, Long Reach, Liquid Ooze, Magic Guard, Skill Link, Sap Sipper, Rough Skin or
 * Soundproof; a third of the Pokemon start seeded by the other one, a third behind a Substitute
 * with some of its HP left, and a third of those knowing a protection used it successfully on the
 * turns before (so the next use rolls its chance);
 * TURN_ARENA=1 aims at arena tags being added, stacked, swapped, cleared and counted down: most
 * moves lay one (screens, hazards, Tailwind, Gravity, Trick Room, Quick Guard, Wide Guard, Mat
 * Block, Crafty Shield, Lucky Chant, the sports, Ion Deluge, Happy Hour, Fairy Lock, Stone Axe,
 * Ceaseless Edge), swap or clear them (Court Change, Defog, Brick Break, Rapid Spin...), or are what
 * the side-wide protections and Safeguard look at (priority moves, spread moves, status moves aimed
 * at the other Pokemon, confusing moves, moves Gravity stops); a third of the abilities are
 * Infiltrator, Prankster, Gale Wings, Triage, Wind Rider, Wind Power, Levitate, Magic Guard, Serene
 * Grace or Own Tempo (Wind Rider or Wind Power for half the Tailwind users, half the Wind Power
 * ones already charged); up to three arena tags are already up, on a random side (hazards with a
 * random number of layers, the pledge fields included, half of them with part of a fixed duration
 * gone), and half the time the tag a Pokemon's move lays is already where it would go (two of
 * the side-wide protections on one side a quarter of the time; Quick Guard facing half the
 * Prankster, Gale Wings and Triage users, Safeguard half the confusing ones, half of those with
 * Infiltrator; Serene Grace or Sheer Force for half the Stone Axe and Ceaseless Edge users). The status moves
 * aimed at everyone or at the user's side, and the ones that ignore protections, are among the
 * moves the side-wide protections look at.
 * TURN_EFFECTS=1 aims at the smaller move effects and what they read: most moves set a weather or
 * terrain, ground (Smack Down), expose (Foresight, Miracle Eye), add a type (Forest's Curse),
 * scatter coins, take or eat items, burn or confuse a Pokemon whose stats rose (Burning Jealousy,
 * Alluring Voice), lock jaws, crash on a miss (High Jump Kick), heal, faint the user (Explosion,
 * Memento, Mind Blown), reset or swap stat stages, change a type (Soak), or only print a message
 * (Splash), else they are attacks, wind moves half the time; a third of the abilities are Sand
 * Spit, Seed Sower, Toxic Debris, Wind Power, Electromorphosis, Sticky Hold, Magic Guard,
 * Levitate, Mega Launcher, Damp, Serene Grace, Sheer Force, Own Tempo or Simple; a third of the
 * cases start with a weather (primal ones included) and a third with a terrain; half the targets
 * of Burning Jealousy and Alluring Voice already raised a stat this turn, half those of Smack
 * Down are airborne (Levitate or Flying), a third of those of the crash moves have maximum
 * evasion and a third are Ghost; a quarter of the Pokemon start with a type added, some exposed,
 * grounded or charged, a quarter of the cases with Toxic Spikes down, and half the Water Shuriken
 * users are Greninja with Battle Bond.
 * TURN_CALLED=1 aims at the moves that call moves: most moves are Metronome, Mirror Move,
 * Copycat, Sleep Talk, Assist, Nature Power or Instruct, the rest attacks or any move; half the
 * Sleep Talk users are asleep; each Pokemon has up to three earlier moves in its history (mostly
 * its own, some called or used indirectly, some failed), the battle's last move is random, the
 * bench has random movesets (Assist), the biome is random and a third of the cases have a
 * terrain (Nature Power), a sixth have Gravity, and an eighth of the time Sleep Talk or Assist
 * still holds a pick an earlier condition made.
 * TURN_HEADER=1 aims at the moves with a header phase: most moves are Focus Punch, Shell Trap or
 * Beak Blast, the rest contact attacks, priority or multi-hit moves, status moves aimed at the
 * other Pokemon, protections and Substitute; a third of the abilities are ones Beak Blast's burn
 * or its contact check reads (Long Reach, Water Veil, Water Bubble, Thermal Exchange, Comatose,
 * Purifying Salt...) or ones that hit back on contact; a quarter of the Pokemon have a status, a
 * sixth are behind a Substitute, and an eighth of the header moves have no PP left.
 * TURN_DELAYED=1 aims at Future Sight, Doom Desire and Wish: most moves are one of them, the rest
 * attacks or protections; two cases in three start with one or two of their tags already pending
 * (on either Pokemon's slot or an empty one, 1 to 3 turns left, half the delayed attacks landing
 * this turn; a delayed attack from either Pokemon, a bench Pokemon, the target itself or no
 * Pokemon at all; a wish of 1 HP to the target's max HP), and a third of the Pokemon are at low HP.
 * TURN_SWITCH=1 aims at the moves that switch a Pokemon out (Roar, Whirlwind, Dragon Tail, Circle
 * Throw, U-turn, Volt Switch, Flip Turn, Teleport, Baton Pass, Shed Tail, Parting Shot, Chilly
 * Reception): most moves are one of them, the rest protections, Substitute, Mist or attacks; a
 * third of the abilities are ones those moves check (Suction Cups, Guard Dog, Wimp Out, Emergency
 * Exit, Soundproof, Infiltrator, Contrary, the stat-protecting ones) or Rough Skin; the player's
 * bench has 0 to 5 Pokemon, a third of them fainted; the wave is often a multiple of 10 or of 5,
 * the game mode classic, endless or daily; a sixth of the Pokemon are behind a Substitute, a sixth
 * of the sides have Mist, a quarter of the cases have snow, a quarter of the Pokemon have Attack
 * and Sp. Atk near the limits, a third are just above half HP, and a third of the Baton Pass users
 * hold Power Trick.
 * TURN_SUMMON=1 aims at the switch itself: the player switches as its command in a third of the
 * cases (and uses the switching moves in another third), the wild Pokemon often drags the player's
 * out (Roar, Whirlwind, Dragon Tail, Circle Throw) or knocks it out; the player's side has entry
 * hazards (Spikes, Toxic Spikes, Stealth Rock, Sticky Web) half the time; the bench has 1 to 5
 * Pokemon, a quarter fainted, half of them with an ability that acts on entry, some with a status,
 * an eighth shiny; the Pokemon on the field has Natural Cure or Regenerator a sixth of the time.
 *
 * Every mode: a SwitchPhase's party screen picks the n-th number of the case's PICK list modulo
 * the Pokemon it can send in (the bench, not fainted), the same rule the C test applies.
 *
 * Each case builds a random singles state inside a real upstream battle, gives both sides a
 * FIGHT command the way the command phases do, then runs upstream's own phases from
 * TurnStartPhase until the turn is over (the next TurnInitPhase) or a phase the turn engine
 * stops at (VictoryPhase, SwitchPhase, SwitchSummonPhase, GameOverPhase, BattleEndPhase). It writes the state before, every phase
 * that ran, what is left queued, and the state after. build/turntest replays each case
 * through the C engine and compares all of it.
 */
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allAbilities, allMoves } from "#data/data-lists";
import { EntryHazardTag, getArenaTag } from "#data/arena-tag";
import { getBattlerTag } from "#data/battler-tags";
import { Status } from "#data/status-effect";
import { Terrain, TerrainType } from "#data/terrain";
import { Weather } from "#data/weather";
import { AbilityId } from "#enums/ability-id";
import { BattleType } from "#enums/battle-type";
import { BiomeId } from "#enums/biome-id";
import { GameModes } from "#enums/game-modes";
import { Command } from "#enums/command";
import { MoveCategory } from "#enums/move-category";
import { MoveFlags } from "#enums/move-flags";
import { MoveTarget } from "#enums/move-target";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { ArenaTagType } from "#enums/arena-tag-type";
import { BattlerTagLapseType } from "#enums/battler-tag-lapse-type";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { MoveResult } from "#enums/move-result";
import { MoveUseMode } from "#enums/move-use-mode";
import { PokemonType } from "#enums/pokemon-type";
import { PositionalTagType } from "#enums/positional-tag-type";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { StatusEffect } from "#enums/status-effect";
import { TrainerSlot } from "#enums/trainer-slot";
import { UiMode } from "#enums/ui-mode";
import { WeatherType } from "#enums/weather-type";
import { getMoveTargets } from "#moves/move-utils";
import { healBlockedMoves } from "#moves/invalid-moves";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import { PartyOption } from "#ui/party-ui-handler";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("turntest producer", async () => {
  const N = Number(process.env.TURN_N ?? 2000);
  const RICH = process.env.TURN_RICH === "1" || process.env.TURN_PROBE === "1" || process.env.TURN_SPROBE === "1";
  const PROBE = process.env.TURN_PROBE === "1" || process.env.TURN_SPROBE === "1";
  const COND = PROBE || process.env.TURN_COND === "1";
  const STAT = process.env.TURN_STAT === "1";
  const SPROBE = process.env.TURN_SPROBE === "1";
  const STATUS = process.env.TURN_STATUS === "1";
  const STATUS_POOL = STATUS || SPROBE;
  const ONHIT = process.env.TURN_ONHIT === "1";
  const CHARGE = process.env.TURN_CHARGE === "1";
  const TAGS = process.env.TURN_TAGS === "1";
  const HEAL = process.env.TURN_HEAL === "1";
  const BOOST = process.env.TURN_BOOST === "1";
  const WEATHER = process.env.TURN_WEATHER === "1" || BOOST;
  const TRAPS = process.env.TURN_TRAPS === "1";
  const PROTECT = process.env.TURN_PROTECT === "1";
  const ARENA = process.env.TURN_ARENA === "1";
  const EFFECTS = process.env.TURN_EFFECTS === "1";
  const CALLED = process.env.TURN_CALLED === "1";
  const HEADER = process.env.TURN_HEADER === "1";
  const DELAYED = process.env.TURN_DELAYED === "1";
  const SWITCH = process.env.TURN_SWITCH === "1";
  const SUMMON = process.env.TURN_SUMMON === "1";
  const OUT = process.env.TURN_OUT ?? "/tmp/pr-turns.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override
    .battleStyle("single")
    // TURN_SUMMON: no override, so the Pokemon sent in keep their own abilities
    .ability(SUMMON ? AbilityId.NONE : AbilityId.BALL_FETCH)
    .enemyAbility(AbilityId.BALL_FETCH)
    .battleType(BattleType.WILD)
    .startingWave(3)
    .enemySpecies(SpeciesId.MAGIKARP);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);

  // GameManager pins battle rolls to their maximum; put upstream's real body back.
  (BattleScene.prototype as any).randBattleSeedInt = function (this: any, range: number, min = 0) {
    return this.currentBattle?.randSeedInt(range, min);
  };
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  const pm = scene.phaseManager as any;
  const A = game.field.getPlayerPokemon() as any;
  const D = game.field.getEnemyPokemon() as any;

  let seed = Number(process.env.TURN_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const ids = Object.keys(speciesDataRegistry.data).map(Number);
  const usable = allMoves.filter((m: any) => m && m.id !== MoveId.NONE && !m.isUnimplemented);
  const attacks = usable.filter((m: any) => m.category !== MoveCategory.STATUS);
  const probeBattlerTags = [
    BattlerTagType.SEEDED, BattlerTagType.CRIT_BOOST, BattlerTagType.DRAGON_CHEER, BattlerTagType.SUBSTITUTE,
    BattlerTagType.STOCKPILING, BattlerTagType.IGNORE_FLYING, BattlerTagType.TRAPPED, BattlerTagType.NO_RETREAT,
    BattlerTagType.INGRAIN, BattlerTagType.HEAL_BLOCK, BattlerTagType.TAUNT, BattlerTagType.FLOATING,
    BattlerTagType.PERISH_SONG, BattlerTagType.DROWSY, BattlerTagType.AQUA_RING, BattlerTagType.IGNORE_GHOST,
    BattlerTagType.MAGIC_COAT, BattlerTagType.TORMENT, BattlerTagType.POWDER, BattlerTagType.OCTOLOCK,
    BattlerTagType.FLYING, BattlerTagType.BIND,
  ];
  const probeArenaTags = [
    ArenaTagType.REFLECT, ArenaTagType.LIGHT_SCREEN, ArenaTagType.AURORA_VEIL, ArenaTagType.SAFEGUARD,
    ArenaTagType.MIST, ArenaTagType.SPIKES, ArenaTagType.STEALTH_ROCK, ArenaTagType.FAIRY_LOCK,
    ArenaTagType.GRAVITY, ArenaTagType.TAILWIND, ArenaTagType.NO_CRIT,
  ];
  const withConditions = usable.filter(
    (m: any) => m.conditions.length || m.conditionsSeq2.length || m.conditionsSeq3.length,
  );
  const abilityIds = allAbilities.filter((a: any) => a && a.id !== AbilityId.NONE).map((a: any) => a.id);
  const boostTags = [BattlerTagType.PROTOSYNTHESIS, BattlerTagType.QUARK_DRIVE];
  // TURN_BOOST: the moves that start the weathers and terrains that add or remove those boosts
  const fieldSetMoves = [
    MoveId.SUNNY_DAY, MoveId.SUNNY_DAY, MoveId.ELECTRIC_TERRAIN, MoveId.ELECTRIC_TERRAIN, MoveId.RAIN_DANCE,
    MoveId.SANDSTORM, MoveId.SNOWSCAPE, MoveId.GRASSY_TERRAIN, MoveId.MISTY_TERRAIN, MoveId.PSYCHIC_TERRAIN,
  ];
  // TURN_STAT pools, by class name up the prototype chain
  const isA = (o: any, names: string[]) => {
    for (let c = o?.constructor; c && c !== Object; c = Object.getPrototypeOf(c)) {
      if (names.includes(c.name)) {
        return true;
      }
    }
    return false;
  };
  const statMoves = usable.filter((m: any) => m.attrs.some((a: any) => a.constructor.name === "StatStageChangeAttr"));
  // the ones whose change has its own condition (Rototiller, Flower Shield, Venom Drench, Magnetic Flux, Gear Up, Tera Blast)
  const hasStatCond = (a: any) =>
    a.constructor.name === "StatStageChangeAttr" && a.options.condition.toString() !== "() => true";
  const statCondMoves = usable.filter((m: any) => m.attrs.some(hasStatCond));
  const statAbilityIds = allAbilities
    .filter((a: any) =>
      a?.attrs.some((x: any) =>
        isA(x, [
          "StatStageChangeMultiplierAbAttr",
          "ProtectStatAbAttr",
          "ConditionalUserFieldProtectStatAbAttr",
          "ReflectStatStageChangeAbAttr",
          "StatStageChangeCopyAbAttr",
          "PostStatStageChangeAbAttr",
        ]),
      ),
    )
    .map((a: any) => a.id)
    .concat([AbilityId.INFILTRATOR, AbilityId.PLUS, AbilityId.MINUS, AbilityId.LEVITATE]);
  // TURN_STATUS / TURN_SPROBE pools
  const statusSetMoves = usable.filter((m: any) =>
    m.attrs.some((a: any) => ["StatusEffectAttr", "MultiStatusEffectAttr"].includes(a.constructor.name)),
  );
  const thawMoves = usable.filter((m: any) =>
    m.attrs.some((a: any) => a.selfTarget && a.constructor.name === "HealStatusEffectAttr" && a.isOfEffect(StatusEffect.FREEZE)),
  );
  const statusAbilityIds = allAbilities
    .filter((a: any) =>
      a?.attrs.some((x: any) =>
        isA(x, [
          "StatusEffectImmunityAbAttr",
          "UserFieldStatusEffectImmunityAbAttr",
          "IgnoreTypeStatusEffectImmunityAbAttr",
          "SynchronizeStatusAbAttr",
          "ReduceStatusEffectDurationAbAttr",
          "BlockStatusDamageAbAttr",
          "ReduceBurnDamageAbAttr",
          "BlockNonDirectDamageAbAttr",
          "InfiltratorAbAttr",
          "MoveEffectChanceMultiplierAbAttr",
          "IgnoreMoveEffectsAbAttr",
        ]),
      ),
    )
    .map((a: any) => a.id)
    .concat([AbilityId.LEVITATE]);
  // TURN_ONHIT pools
  const contactAttacks = attacks.filter((m: any) => m.flags & 1); // MoveFlags.MAKES_CONTACT
  const onHitAbilityIds = allAbilities
    .filter((a: any) =>
      a?.attrs.some((x: any) =>
        isA(x, ["PostDefendAbAttr", "PostAttackAbAttr", "PostReceiveCritStatStageChangeAbAttr", "BlockNonDirectDamageAbAttr"]),
      ),
    )
    .map((a: any) => a.id)
    .concat([AbilityId.OVERCOAT, AbilityId.SHIELD_DUST, AbilityId.LONG_REACH]);
  // TURN_CHARGE pools: two-turn moves, and the moves that hit a semi-invulnerable Pokemon
  const chargeMoves = usable.filter((m: any) => m.isChargingMove());
  const hitsTagMoves = usable.filter((m: any) =>
    m.attrs.some((a: any) => ["HitsTagAttr", "HitsTagForDoubleDamageAttr"].includes(a.constructor.name)),
  );
  // TURN_TAGS pools: moves adding a tag the C port has, moves reading one, abilities reacting to one
  const portedTags = [
    BattlerTagType.FLINCHED, BattlerTagType.CONFUSED, BattlerTagType.FRENZY, BattlerTagType.TRAPPED,
    BattlerTagType.NO_RETREAT, BattlerTagType.ALWAYS_GET_HIT, BattlerTagType.RECEIVE_DOUBLE_DAMAGE,
    BattlerTagType.MINIMIZED, BattlerTagType.CHARGED, BattlerTagType.TAR_SHOT, BattlerTagType.AUTOTOMIZED,
    BattlerTagType.IGNORE_ACCURACY, BattlerTagType.ALWAYS_CRIT, BattlerTagType.PERISH_SONG, BattlerTagType.ROOSTED,
    BattlerTagType.THROAT_CHOPPED, BattlerTagType.HEAL_BLOCK, BattlerTagType.AQUA_RING, BattlerTagType.INGRAIN,
    BattlerTagType.OCTOLOCK, BattlerTagType.DISABLED, BattlerTagType.TAUNT, BattlerTagType.TORMENT, BattlerTagType.ENCORE,
    BattlerTagType.INFATUATED, BattlerTagType.SALT_CURED, BattlerTagType.CURSED, BattlerTagType.NIGHTMARE,
    BattlerTagType.DROWSY, BattlerTagType.POWDER, BattlerTagType.FLOATING, BattlerTagType.TELEKINESIS,
    BattlerTagType.CRIT_BOOST, BattlerTagType.DRAGON_CHEER, BattlerTagType.SYRUP_BOMB, BattlerTagType.MAGIC_COAT,
    BattlerTagType.ELECTRIFIED, BattlerTagType.DESTINY_BOND, BattlerTagType.GRUDGE, BattlerTagType.STOCKPILING,
    BattlerTagType.BURNED_UP, BattlerTagType.DOUBLE_SHOCKED, BattlerTagType.POWER_TRICK, BattlerTagType.INTERRUPTED,
    BattlerTagType.TRUANT, BattlerTagType.SLOW_START, BattlerTagType.STURDY, BattlerTagType.ENDURE_TOKEN,
    BattlerTagType.UNBURDEN, BattlerTagType.GORILLA_TACTICS, BattlerTagType.SUPREME_OVERLORD, BattlerTagType.FIRE_BOOST,
  ];
  const tagMoves = usable.filter(
    (m: any) =>
      m.attrs.some((a: any) => isA(a, ["AddBattlerTagAttr"]) && portedTags.includes(a.tagType))
      || m.id === MoveId.CURSE
      || m.id === MoveId.DESTINY_BOND
      || m.id === MoveId.SPIT_UP
      || m.id === MoveId.SWALLOW,
  );
  const tagReaders = attacks.filter(
    (m: any) =>
      m.type === PokemonType.ELECTRIC
      || m.type === PokemonType.FIRE
      || m.type === PokemonType.GROUND
      || m.attrs.some((a: any) =>
        ["HitsTagForDoubleDamageAttr", "WeightPowerAttr", "CompareWeightPowerAttr"].includes(a.constructor.name),
      ),
  ).concat(
    // what Throat Chop and Heal Block stop, and what Lock-On makes sure of
    usable.filter(
      (m: any) =>
        m.hasFlag(MoveFlags.SOUND_BASED)
        || healBlockedMoves.has(m.id)
        || (m.category !== MoveCategory.STATUS && m.accuracy > 0 && m.accuracy < 80),
    ),
  );
  const tagAbilityIds = [
    AbilityId.STEADFAST, AbilityId.INNER_FOCUS, AbilityId.OWN_TEMPO, AbilityId.SHIELD_DUST, AbilityId.SERENE_GRACE,
    AbilityId.NO_GUARD, AbilityId.PERISH_BODY, AbilityId.CURSED_BODY, AbilityId.CUTE_CHARM, AbilityId.OBLIVIOUS,
    AbilityId.AROMA_VEIL, AbilityId.MAGIC_BOUNCE, AbilityId.MAGIC_GUARD, AbilityId.SIMPLE, AbilityId.CONTRARY,
    AbilityId.TRUANT, AbilityId.SLOW_START, AbilityId.STURDY, AbilityId.FLASH_FIRE, AbilityId.GORILLA_TACTICS,
    AbilityId.SUPREME_OVERLORD, AbilityId.MOTOR_DRIVE, AbilityId.SAP_SIPPER, AbilityId.LIGHTNING_ROD, AbilityId.STORM_DRAIN,
  ];
  // TURN_HEAL pools
  const healMoves = usable.filter((m: any) =>
    m.attrs.some((a: any) => ["RecoilAttr", "HitHealAttr", "RechargeAttr"].includes(a.constructor.name)),
  );
  const healAbilityIds = allAbilities
    .filter((a: any) =>
      a?.attrs.some((x: any) =>
        isA(x, ["BlockRecoilDamageAttr", "BlockNonDirectDamageAbAttr", "ReverseDrainAbAttr", "PostTurnStatusHealAbAttr", "TypeImmunityHealAbAttr"]),
      ),
    )
    .map((a: any) => a.id);
  // TURN_WEATHER pools (the moves that set a weather or terrain are left out)
  const fieldSetters = ["WeatherChangeAttr", "ClearWeatherAttr", "TerrainChangeAttr", "ClearTerrainAttr"];
  const fieldReaders = [
    MoveId.EXPANDING_FORCE, MoveId.RISING_VOLTAGE, MoveId.MISTY_EXPLOSION, MoveId.GRASSY_GLIDE, MoveId.PSYBLADE,
    MoveId.HYDRO_STEAM,
  ];
  // primal weather reads the move's type after these change it
  const typeShifters = [
    MoveId.HIDDEN_POWER, MoveId.REVELATION_DANCE, MoveId.TERA_BLAST, MoveId.RAGING_BULL, MoveId.IVY_CUDGEL,
  ];
  const weatherMoves = usable.filter((m: any) => {
    const names = [...m.attrs, ...(m.chargeAttrs ?? [])].map((a: any) => a.constructor.name);
    if (names.some((n: string) => fieldSetters.includes(n))) {
      return false;
    }
    return (
      m.type === PokemonType.WATER
      || m.type === PokemonType.FIRE
      || names.some((n: string) => /Weather|Terrain|Storm|Thunder|Blizzard|SemiInvulnerable/.test(n))
      || names.includes("ConfuseAttr")
      || fieldReaders.includes(m.id)
      || typeShifters.includes(m.id)
    );
  });
  const weatherAbilityIds = allAbilities
    .filter((a: any) =>
      a?.attrs.some((x: any) =>
        isA(x, [
          "SuppressWeatherEffectAbAttr", "PreWeatherDamageAbAttr", "PostWeatherLapseAbAttr", "PostWeatherChangeAbAttr",
          "PostTerrainChangeAbAttr", "TerrainEventTypeChangeAbAttr",
        ]),
      ),
    )
    .map((a: any) => a.id)
    .concat([
      AbilityId.MAGIC_GUARD, AbilityId.LEVITATE, AbilityId.SWIFT_SWIM, AbilityId.CHLOROPHYLL, AbilityId.SAND_RUSH,
      AbilityId.SLUSH_RUSH, AbilityId.SAND_FORCE, AbilityId.HYDRATION, AbilityId.LEAF_GUARD, AbilityId.SURGE_SURFER,
      AbilityId.NORMALIZE, AbilityId.LIQUID_VOICE,
    ]);
  // TURN_TRAPS pools
  const trapTags = [
    BattlerTagType.BIND, BattlerTagType.WRAP, BattlerTagType.FIRE_SPIN, BattlerTagType.WHIRLPOOL, BattlerTagType.CLAMP,
    BattlerTagType.SAND_TOMB, BattlerTagType.MAGMA_STORM, BattlerTagType.SNAP_TRAP, BattlerTagType.THUNDER_CAGE,
    BattlerTagType.INFESTATION,
  ];
  const trapMoves = usable.filter((m: any) =>
    m.attrs.some((a: any) =>
      ["TrapAttr", "RemoveBattlerTagAttr", "RemoveArenaTrapAttr", "RemoveScreensAttr", "RemoveArenaTagsAttr"].includes(
        a.constructor.name,
      ),
    ),
  );
  const trapArenaTags = [
    ArenaTagType.SPIKES, ArenaTagType.TOXIC_SPIKES, ArenaTagType.STEALTH_ROCK, ArenaTagType.STICKY_WEB,
    ArenaTagType.REFLECT, ArenaTagType.LIGHT_SCREEN, ArenaTagType.AURORA_VEIL, ArenaTagType.MIST, ArenaTagType.SAFEGUARD,
  ];
  // TURN_PROTECT pools
  const protectMoves = usable.filter((m: any) =>
    m.attrs.some((a: any) => ["ProtectAttr", "AddSubstituteAttr", "RemoveAllSubstitutesAttr"].includes(a.constructor.name))
    || [MoveId.LEECH_SEED, MoveId.RAPID_SPIN, MoveId.MORTAL_SPIN, MoveId.FEINT].includes(m.id),
  );
  const protectHitters = usable.filter((m: any) =>
    m.attrs.some((a: any) => a.constructor.name === "MultiHitAttr")
    || (m.flags & (MoveFlags.SOUND_BASED | MoveFlags.IGNORE_SUBSTITUTE | MoveFlags.IGNORE_PROTECT)) !== 0
    || (m.category === MoveCategory.STATUS && [MoveTarget.NEAR_OTHER, MoveTarget.NEAR_ENEMY, MoveTarget.ALL_NEAR_ENEMIES].includes(m.moveTarget)),
  );
  const protectAbilityIds = [
    AbilityId.INFILTRATOR, AbilityId.UNSEEN_FIST, AbilityId.LONG_REACH, AbilityId.LIQUID_OOZE, AbilityId.MAGIC_GUARD,
    AbilityId.SKILL_LINK, AbilityId.SAP_SIPPER, AbilityId.ROUGH_SKIN, AbilityId.SOUNDPROOF,
  ];
  // TURN_ARENA pools
  const arenaMoves = usable.filter((m: any) =>
    m.attrs.some((a: any) =>
      [
        "AddArenaTagAttr", "AddArenaTrapTagAttr", "AddArenaTrapTagHitAttr", "SwapArenaTagsAttr", "RemoveArenaTrapAttr",
        "RemoveScreensAttr", "RemoveArenaTagsAttr",
      ].includes(a.constructor.name),
    ),
  );
  const arenaHitters = usable.filter((m: any) =>
    m.priority > 0
    || [MoveTarget.ALL_NEAR_ENEMIES, MoveTarget.ALL_NEAR_OTHERS, MoveTarget.ALL_ENEMIES, MoveTarget.ALL_OTHERS].includes(m.moveTarget)
    || (m.category === MoveCategory.STATUS && [MoveTarget.NEAR_OTHER, MoveTarget.NEAR_ENEMY].includes(m.moveTarget))
    || m.attrs.some((a: any) => a.constructor.name === "ConfuseAttr")
    || (m.flags & MoveFlags.GRAVITY) !== 0
    // what the side-wide protections pass over: status moves aimed at everyone, and moves aimed at
    // the user's own side (in a single battle, the user); status moves that ignore protections
    || (m.category === MoveCategory.STATUS
      && ([MoveTarget.ALL, MoveTarget.USER_AND_ALLIES, MoveTarget.USER_OR_NEAR_ALLY].includes(m.moveTarget)
        || (m.flags & MoveFlags.IGNORE_PROTECT) !== 0)),
  );
  const arenaAbilityIds = [
    AbilityId.INFILTRATOR, AbilityId.PRANKSTER, AbilityId.GALE_WINGS, AbilityId.TRIAGE, AbilityId.WIND_RIDER,
    AbilityId.WIND_POWER, AbilityId.LEVITATE, AbilityId.MAGIC_GUARD, AbilityId.SERENE_GRACE, AbilityId.OWN_TEMPO,
  ];
  // TURN_EFFECTS pools
  const effectAttrNames = [
    "WeatherChangeAttr", "TerrainChangeAttr", "FallDownAttr", "ExposedMoveAttr", "AddTypeAttr", "MoneyAttr",
    "RemoveHeldItemAttr", "StealHeldItemChanceAttr", "StealEatBerryAttr", "StatusIfBoostedAttr",
    "AddBattlerTagIfBoostedAttr", "JawLockAttr", "MissEffectAttr", "WaterShurikenMultiHitTypeAttr", "HealAttr",
    "SacrificialAttr", "SacrificialAttrOnHit", "HalfSacrificialAttr", "ResetStatsAttr", "SwapStatStagesAttr",
    "ChangeTypeAttr", "MessageAttr", "FlameBurstAttr", "HealOnAllyAttr", "CopyTypeAttr",
  ];
  const effectMoves = usable.filter((m: any) => m.attrs.some((a: any) => effectAttrNames.includes(a.constructor.name)));
  const windMoves = attacks.filter((m: any) => (m.flags & MoveFlags.WIND_MOVE) !== 0);
  const effectAbilityIds = [
    AbilityId.SAND_SPIT, AbilityId.SEED_SOWER, AbilityId.TOXIC_DEBRIS, AbilityId.WIND_POWER, AbilityId.ELECTROMORPHOSIS,
    AbilityId.STICKY_HOLD, AbilityId.MAGIC_GUARD, AbilityId.LEVITATE, AbilityId.MEGA_LAUNCHER, AbilityId.DAMP,
    AbilityId.SERENE_GRACE, AbilityId.SHEER_FORCE, AbilityId.OWN_TEMPO, AbilityId.SIMPLE,
  ];
  // TURN_CALLED pools
  const calledMoves = [
    MoveId.METRONOME, MoveId.MIRROR_MOVE, MoveId.COPYCAT, MoveId.SLEEP_TALK, MoveId.ASSIST, MoveId.NATURE_POWER,
    MoveId.INSTRUCT,
  ].map(id => allMoves[id]);
  const biomeIds = Object.values(BiomeId).filter(v => typeof v === "number") as number[];
  // TURN_HEADER pools
  const headerIds = [MoveId.FOCUS_PUNCH, MoveId.SHELL_TRAP, MoveId.BEAK_BLAST];
  const headerMoves = headerIds.map(id => allMoves[id]);
  const headerHitters = usable.filter((m: any) =>
    m.priority > 0
    || m.attrs.some((a: any) => ["MultiHitAttr", "ProtectAttr", "AddSubstituteAttr"].includes(a.constructor.name))
    || (m.category === MoveCategory.STATUS && [MoveTarget.NEAR_OTHER, MoveTarget.NEAR_ENEMY].includes(m.moveTarget)),
  );
  // TURN_DELAYED pools
  const delayedMoves = [MoveId.FUTURE_SIGHT, MoveId.DOOM_DESIRE, MoveId.WISH].map(id => allMoves[id]);
  const protectOnly = usable.filter((m: any) => m.attrs.some((a: any) => a.constructor.name === "ProtectAttr"));
  // TURN_SWITCH pools
  const switchAttrNames = ["ForceSwitchOutAttr", "ChillyReceptionAttr", "PartingShotAttr"];
  const switchMoves = usable.filter((m: any) => m.attrs.some((a: any) => switchAttrNames.includes(a.constructor.name)));
  const switchOthers = [MoveId.PROTECT, MoveId.DETECT, MoveId.SUBSTITUTE, MoveId.MIST].map(id => allMoves[id]);
  const switchAbilityIds = [
    AbilityId.SUCTION_CUPS, AbilityId.GUARD_DOG, AbilityId.WIMP_OUT, AbilityId.EMERGENCY_EXIT, AbilityId.SOUNDPROOF,
    AbilityId.INFILTRATOR, AbilityId.CONTRARY, AbilityId.CLEAR_BODY, AbilityId.WHITE_SMOKE, AbilityId.FULL_METAL_BODY,
    AbilityId.HYPER_CUTTER, AbilityId.ROUGH_SKIN,
  ];
  // TURN_SUMMON: abilities that act when their Pokemon comes in, or as it leaves
  const entryAbilityIds = [
    AbilityId.INTIMIDATE, AbilityId.DRIZZLE, AbilityId.DROUGHT, AbilityId.SAND_STREAM, AbilityId.SNOW_WARNING,
    AbilityId.ELECTRIC_SURGE, AbilityId.GRASSY_SURGE, AbilityId.MISTY_SURGE, AbilityId.PSYCHIC_SURGE,
    AbilityId.PRESSURE, AbilityId.MOLD_BREAKER, AbilityId.UNNERVE, AbilityId.DOWNLOAD, AbilityId.TRACE,
    AbilityId.FRISK, AbilityId.ANTICIPATION, AbilityId.FOREWARN, AbilityId.SCREEN_CLEANER, AbilityId.IMPOSTER,
    AbilityId.AIR_LOCK, AbilityId.CLOUD_NINE, AbilityId.NEUTRALIZING_GAS, AbilityId.INTREPID_SWORD,
    AbilityId.DAUNTLESS_SHIELD, AbilityId.LEVITATE, AbilityId.MAGIC_GUARD, AbilityId.IMMUNITY, AbilityId.OWN_TEMPO,
    AbilityId.OBLIVIOUS, AbilityId.LIMBER, AbilityId.INSOMNIA, AbilityId.CONTRARY, AbilityId.CLEAR_BODY,
    AbilityId.DEFIANT, AbilityId.COMPETITIVE, AbilityId.WIMP_OUT, AbilityId.EMERGENCY_EXIT,
  ];
  const headerAbilityIds = [
    AbilityId.LONG_REACH, AbilityId.WATER_VEIL, AbilityId.WATER_BUBBLE, AbilityId.THERMAL_EXCHANGE, AbilityId.COMATOSE,
    AbilityId.PURIFYING_SALT, AbilityId.MAGIC_GUARD, AbilityId.FLAME_BODY, AbilityId.ROUGH_SKIN, AbilityId.UNSEEN_FIST,
    AbilityId.INFILTRATOR,
  ];
  const arenaTagPool = [
    ArenaTagType.REFLECT, ArenaTagType.LIGHT_SCREEN, ArenaTagType.AURORA_VEIL, ArenaTagType.MIST, ArenaTagType.SAFEGUARD,
    ArenaTagType.SPIKES, ArenaTagType.TOXIC_SPIKES, ArenaTagType.STEALTH_ROCK, ArenaTagType.STICKY_WEB,
    ArenaTagType.TAILWIND, ArenaTagType.TRICK_ROOM, ArenaTagType.GRAVITY, ArenaTagType.QUICK_GUARD,
    ArenaTagType.WIDE_GUARD, ArenaTagType.MAT_BLOCK, ArenaTagType.CRAFTY_SHIELD, ArenaTagType.NO_CRIT,
    ArenaTagType.MUD_SPORT, ArenaTagType.WATER_SPORT, ArenaTagType.FIRE_GRASS_PLEDGE, ArenaTagType.WATER_FIRE_PLEDGE,
    ArenaTagType.GRASS_WATER_PLEDGE, ArenaTagType.HAPPY_HOUR, ArenaTagType.FAIRY_LOCK, ArenaTagType.ION_DELUGE,
  ];
  const healStatusAttrs: [number, any][] = allAbilities
    .filter((a: any) => a)
    .sort((a: any, b: any) => a.id - b.id)
    .flatMap((a: any) =>
      a.attrs.filter((x: any) => x.constructor.name === "PostSummonHealStatusAbAttr").map((x: any) => [a.id, x]),
    );

  // Bench Pokemon so a fainted player Pokemon has someone to switch to (SwitchPhase) and the
  // party lines are real. Made once; species and status are drawn per case.
  const benchPool = (player: boolean) =>
    Array.from({ length: 5 }, () =>
      player
        ? scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5)
        : scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA), 5, TrainerSlot.NONE),
    );
  const playerBench = benchPool(true);

  // The party screen a SwitchPhase opens: the n-th screen of the turn sends in the Pokemon the n-th
  // number of the case's PICK list picks among those it can send in (the bench, not fainted). The
  // numbers come from the case index, not the case stream, so the streams stay as they were.
  let picks: number[] = [];
  let pickAt = 0;
  const uiSetMode = scene.ui.setMode.bind(scene.ui);
  scene.ui.setMode = (mode: UiMode, ...args: any[]) => {
    if (mode !== UiMode.PARTY) {
      return uiSetMode(mode, ...args);
    }
    const party = scene.getPlayerParty();
    const first = battle.getBattlerCount();
    const eligible = party.map((_: any, k: number) => k).filter((k: number) => k >= first && !party[k].isFainted());
    const r = picks[pickAt++ % picks.length];
    args[2](eligible.length ? eligible[r % eligible.length] : -1, PartyOption.SEND_OUT);
    return Promise.resolve();
  };

  // ---- the state dump (the C engine prints the same records; see tests/turntest.c) ----
  const L = (xs: any[]) => (xs.length ? xs.join(",") : "-");
  const num = (v: any) => (v === undefined || v === null ? "u" : String(v));
  const rngState = (st: string) => st.split(",").slice(1).join(",");
  const turnMove = (t: any) => `${t.move}:${L(t.targets).replace(/,/g, "/")}:${t.useMode}:${num(t.result)}`;
  // A Pokemon's records are keyed by its battler index on the field (0, 2), or p<slot> / e<slot>
  // for a bench Pokemon of the player or the enemy
  const monLines = (p: any, b: string) => {
    const td = p.turnData;
    const sd = p.summonData;
    const st = p.status;
    if (st && st.effect === StatusEffect.NONE) {
      throw new Error("Status object with effect NONE");
    }
    return [
      [
        `MON b=${b} id=${p.id} sp=${p.species.speciesId} form=${p.formIndex} lv=${p.level} ivs=${L(p.ivs)} nat=${p.nature}`,
        `ab=${num(sd.ability)} abil=${p.getAbility().id} babil=${p.getAbility(true).id} passive=${p.passive ? 1 : 0} cpass=${p.customPokemonData.passive} gender=${p.gender}`,
        `friend=${p.friendship} hp=${p.hp} stats=${L(p.stats)} st=${st ? st.effect : 0} toxic=${st ? st.toxicTurnCount : 0}`,
        `sleep=${num(st?.sleepTurnsRemaining)} freeze=${num(st?.freezeTurnsRemaining)} stages=${L(sd.statStages)}`,
        `tera=${p.isTerastallized ? 1 : 0} teratype=${p.teraType} stellar=${L([...p.stellarTypesBoosted].sort((x, y) => x - y))}`,
        `boss=${p.isBoss() ? 1 : 0} shiny=${p.isShiny(true) ? 1 : 0} onfield=${p.isOnField() ? 1 : 0} out=${p.switchOutStatus ? 1 : 0}`,
        `shc=${sd.hitCount} types=${L(sd.types)} added=${sd.addedType ?? 0} abilsup=${sd.abilitySuppressed ? 1 : 0}`,
        `tsd=${p.tempSummonData.turnCount} tswd=${p.tempSummonData.waveTurnCount}`,
        `aas=${L([...sd.abilitiesApplied])} aaw=${L([...p.waveData.abilitiesApplied])}`
          + (p.battleData.hasEatenBerry ? " eb=1" : "")
          + (sd.stats.some((v: number) => v) ? ` ss=${L(sd.stats)}` : ""),
      ].join(" "),
      [
        `TURN b=${b} acted=${td.acted ? 1 : 0} order=${num(td.order)} hc=${td.hitCount} hl=${td.hitsLeft}`,
        `dealt=${td.totalDamageDealt} single=${td.singleHitDamageDealt} taken=${td.damageTaken}`,
        `inc=${td.statStagesIncreased ? 1 : 0} dec=${td.statStagesDecreased ? 1 : 0} eff=${num(td.moveEffectiveness)}`,
        `pending=${td.pendingStatus} switched=${td.switchedInThisTurn ? 1 : 0} summoned=${td.summonedThisTurn ? 1 : 0}`,
        `joined=${td.joinedRound ? 1 : 0} pledge=${td.combiningPledge ?? 0}`,
        `recv=${L(td.attacksReceived.map((r: any) => `${r.move}:${r.result}:${r.damage}:${r.critical ? 1 : 0}:${r.sourceId}:${r.sourceBattlerIndex}`))}`,
      ].join(" "),
      `MOVES b=${b} list=${L(p.moveset.map((m: any) => `${m.moveId}:${m.ppUsed}:${m.ppUp}:${num(m.maxPpOverride)}`))}`,
      `HIST b=${b} list=${L(sd.moveHistory.map(turnMove))}`,
      `MQ b=${b} list=${L(sd.moveQueue.map(turnMove))}`,
      ...sd.tags.map(
        (t: any) =>
          `BTAG b=${b} type=${t.tagType} turns=${t.turnCount} smove=${t.sourceMove ?? 0} sid=${t.sourceId ?? -1}`
          + ` stock=${t.stockpiledCount ?? 0} crit=${t.critStages ?? 0} auto=${t.autotomizeCount ?? 0}`
          + (boostTags.includes(t.tagType) ? ` stat=${t.stat} mult=${t.multiplier}` : "")
          + (t.tagType === BattlerTagType.SUBSTITUTE ? ` hp=${t.hp} focus=${t.sourceInFocus ? 1 : 0}` : "")
          + (t.tagType === BattlerTagType.SEEDED ? ` sidx=${t.sourceIndex ?? -1}` : "")
          + (t.tagType === BattlerTagType.SHELL_TRAP && t.activated ? " act=1" : "")
          + (t.tagType === BattlerTagType.ROOSTED && t.isBasePureFlying ? " bpf=1" : "")
          + ([BattlerTagType.DISABLED, BattlerTagType.ENCORE, BattlerTagType.GORILLA_TACTICS].includes(t.tagType)
            ? ` mid=${t.moveId ?? 0}`
            : "")
          + (t.tagType === BattlerTagType.SUPREME_OVERLORD ? ` fc=${t.faintCount ?? 0}` : "")
          + (t.tagType === BattlerTagType.STOCKPILING
            ? ` scd=${t.statChangeCounts[Stat.DEF]} scs=${t.statChangeCounts[Stat.SPDEF]}`
            : ""),
      ),
    ];
  };
  const dumpState = () => {
    const lines: string[] = [];
    const bs = battle.battleSeedState;
    lines.push(`RNG g=${rngState(Phaser.Math.RND.state())} b=${bs ? rngState(bs) : "none"}`);
    const fh = (h: any[]) => L(h.map((e: any) => `${e.pokemon.id}:${e.turn}`));
    lines.push(
      [
        `BATTLE turn=${battle.turn} wave=${battle.waveIndex} bseed=${battle.battleSeed} wseed=${scene.waveSeed}`,
        `lastmove=${battle.lastMove ?? 0} efaints=${battle.enemyFaints} pfaints=${scene.arena.playerFaints}`,
        `pfh=${fh(battle.playerFaintsHistory)} efh=${fh(battle.enemyFaintsHistory)}`,
        `lpi=${num(battle.lastPlayerInvolved)} lei=${num(battle.lastEnemyInvolved)}`,
        `participants=${L([...battle.playerParticipantIds].sort((x: number, y: number) => x - y))}`,
        `loot=${battle.postBattleLoot.length} score=${battle.battleScore} money=${battle.moneyScattered} type=${battle.battleType} double=${battle.double ? 1 : 0}`,
        `mel=${scene.getMaxExpLevel()} bar=${scene.abilityBar.isVisible() ? 1 : 0} ball=${num(battle.lastUsedPokeball)}`,
        `gm=${scene.gameMode.modeId}`,
      ].join(" "),
    );
    const w = scene.arena.weather;
    const t = scene.arena.terrain;
    lines.push(
      `ARENA weather=${w ? w.weatherType : 0} wturns=${w ? w.turnsLeft : 0} wmax=${w ? w.maxDuration : 0}`
        + ` terrain=${t ? t.terrainType : 0} tturns=${t ? t.turnsLeft : 0} tmax=${t ? t.maxDuration : 0}`
        + ` ignab=${scene.arena.ignoreAbilities ? 1 : 0} ignsrc=${num(scene.arena.ignoringEffectSource)}`
        + ` biome=${scene.arena.biomeId}`,
    );
    for (const at of scene.arena.tags) {
      lines.push(`ATAG type=${at.tagType} side=${at.side} turns=${at.turnCount} max=${at.maxDuration} smove=${at.sourceMove ?? 0} sid=${at.sourceId ?? -1}`
        + (at instanceof EntryHazardTag ? ` layers=${at.layers}` : ""));
    }
    for (const pt of scene.arena.positionalTagManager.tags as any[]) {
      lines.push(`POS type=${pt.tagType} turns=${pt.turnCount} target=${pt.targetIndex} sid=${pt.sourceId ?? -1}`
        + ` smove=${pt.sourceMove ?? 0} heal=${pt.healHp ?? 0}`);
    }
    for (const [side, party] of [[0, scene.getPlayerParty()], [1, scene.getEnemyParty()]] as [number, any[]][]) {
      party.forEach((p: any, k: number) => {
        lines.push(...monLines(p, k === 0 ? String(side * 2) : `${side ? "e" : "p"}${k}`));
      });
    }
    for (const [side, list] of [[0, scene.modifiers], [1, scene.enemyModifiers]] as [number, any[]][]) {
      for (const m of list) {
        lines.push(`MOD side=${side} cls=${m.constructor.name} pid=${m.pokemonId ?? 0} stack=${m.stackCount}`);
      }
    }
    for (const [side, party] of [[0, scene.getPlayerParty()], [1, scene.getEnemyParty()]] as [number, any[]][]) {
      lines.push(`PARTY side=${side} list=${L(party.map((m: any) => m.id))}`);
    }
    lines.push(`PICK list=${L(picks)}`);
    for (let k = 0; k < 4; k++) {
      const c = battle.turnCommands[k];
      if (c) {
        lines.push(
          `CMD b=${k} cmd=${c.command} move=${c.move?.move ?? 0} targets=${L(c.move?.targets ?? [])} mode=${c.move?.useMode ?? 0} skip=${c.skip ? 1 : 0}`
            + ` cursor=${num(c.cursor)} baton=${c.args?.[0] === true ? 1 : 0}`,
        );
      }
      const pc = battle.preTurnCommands[k];
      if (pc) {
        lines.push(`PRE b=${k} cmd=${pc.command} skip=${pc.skip ? 1 : 0}`);
      }
    }
    // session state kept on ability attr objects (the last status each cure-on-entry attr healed)
    const healed = healStatusAttrs.filter(([, x]) => x.statusHealed).map(([id, x]) => `${id}:${x.statusHealed}`);
    if (healed.length) {
      lines.push(`SESSION healed=${L(healed)}`);
    }
    // the move Sleep Talk's or Assist's condition picked and apply hasn't used yet (kept on the attr object)
    const sel = [MoveId.SLEEP_TALK, MoveId.ASSIST]
      .map(id => [id, (allMoves[id].getAttrs("RandomMovesetMoveAttr")[0] as any).selectedMove])
      .filter(([, v]) => v);
    if (sel.length) {
      lines.push(`CALLSEL list=${L(sel.map(([id, v]) => `${id}:${v}`))}`);
    }
    return lines;
  };
  // What is still queued: the phase tree's levels (bottom first) and the dynamic queues
  const queueLines = () => {
    const tree = pm.phaseQueue;
    const nm = (ph: any) => (ph.phaseName === "DynamicPhaseMarker" ? `Marker:${ph.phaseType}` : ph.phaseName);
    const levels = tree.levels.map((lv: any[]) => L(lv.map(nm)));
    const dyn: string[] = [];
    for (const [name, q] of pm.dynamicQueueManager.dynamicPhaseMap) {
      if (q.queue.length) {
        dyn.push(`${name}:${q.queue.map((ph: any) => ph.battlerIndex).join("/")}`);
      }
    }
    dyn.sort();
    return [`QUEUE levels=${levels.join("|")} deferred=${tree.deferredActive ? 1 : 0} dyn=${L(dyn)}`];
  };

  // ---- driving upstream's phases ----
  let started = 0;
  pm.startCurrentPhase = () => {
    started++;
  };
  // Where the turn engine hands control back: the turn is over (TurnInitPhase), or the battle
  // moves on to something outside a turn (a switch-in prompt, victory, a wild Pokemon leaving).
  const boundary = new Set([
    "TurnInitPhase", "VictoryPhase", "GameOverPhase", "BattleEndPhase", "SelectModifierPhase",
  ]);
  const phaseTag = (ph: any) => {
    const b = ph.battlerIndex;
    return typeof ph.getPokemon === "function" && typeof b === "number" ? `${ph.phaseName}:${b}` : ph.phaseName;
  };
  // The arguments of the switch prompt a turn hands over to
  const b01 = (v: unknown) => (v ? 1 : 0);
  const stopArgs = (ph: any) => {
    if (ph.phaseName === "SwitchPhase") {
      return ` type=${ph.switchType} field=${ph.fieldIndex} modal=${b01(ph.isModal)} return=${b01(ph.doReturn)}`;
    }
    if (ph.phaseName === "SwitchSummonPhase") {
      return ` type=${ph.switchType} field=${ph.fieldIndex} slot=${ph.slotIndex} return=${b01(ph.doReturn)} player=${b01(ph.player)}`;
    }
    return "";
  };
  const runTurn = async () => {
    const trace: string[] = [];
    pm.pushNew("TurnStartPhase");
    pm.shiftPhase();
    for (let guard = 0; ; guard++) {
      const ph = pm.getCurrentPhase();
      if (boundary.has(ph.phaseName)) {
        return { trace, stop: ph.phaseName + stopArgs(ph) };
      }
      if (guard > 400) {
        throw new Error("turn did not end");
      }
      trace.push(phaseTag(ph));
      const before = started;
      ph.start();
      for (let spins = 0; started === before; spins++) {
        await new Promise(r => setTimeout(r, 0));
        if (spins > 20000) {
          throw new Error("stuck in " + ph.phaseName);
        }
      }
    }
  };

  // ---- random states ----
  const statuses = [StatusEffect.BURN, StatusEffect.PARALYSIS, StatusEffect.POISON, StatusEffect.TOXIC, StatusEffect.SLEEP, StatusEffect.FREEZE];
  const setup = (p: any) => {
    p.resetSummonData();
    p.resetTurnData();
    p.resetBattleAndWaveData();
    p.switchOutStatus = false;
    if (!p.isOnField()) {
      scene.field.add(p);
    }
    const sp = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
    p.species = sp;
    p.formIndex = sp.forms.length ? rnd(sp.forms.length) : 0;
    p.fusionSpecies = null;
    p.level = 1 + rnd(100);
    p.ivs = [0, 0, 0, 0, 0, 0].map(() => rnd(32));
    p.nature = rnd(25);
    p.passive = false;
    p.customPokemonData.passive = AbilityId.BALL_FETCH;
    p.summonData.ability = STAT && rnd(2) === 0
      ? statAbilityIds[rnd(statAbilityIds.length)]
      : STATUS_POOL && rnd(3) !== 0 ? statusAbilityIds[rnd(statusAbilityIds.length)]
      : ONHIT && rnd(3) !== 0 ? onHitAbilityIds[rnd(onHitAbilityIds.length)]
      : CHARGE && rnd(6) === 0 ? AbilityId.NO_GUARD
      : TAGS && rnd(4) === 0 ? tagAbilityIds[rnd(tagAbilityIds.length)]
      : HEAL && rnd(2) === 0 ? healAbilityIds[rnd(healAbilityIds.length)]
      : WEATHER && rnd(2) === 0 ? weatherAbilityIds[rnd(weatherAbilityIds.length)]
      : TRAPS && rnd(4) === 0 ? AbilityId.MAGIC_GUARD
      : PROTECT && rnd(3) === 0 ? protectAbilityIds[rnd(protectAbilityIds.length)]
      : ARENA && rnd(3) === 0 ? arenaAbilityIds[rnd(arenaAbilityIds.length)]
      : EFFECTS && rnd(3) === 0 ? effectAbilityIds[rnd(effectAbilityIds.length)]
      : HEADER && rnd(3) === 0 ? headerAbilityIds[rnd(headerAbilityIds.length)]
      : SWITCH && rnd(3) === 0 ? switchAbilityIds[rnd(switchAbilityIds.length)]
      : SUMMON && rnd(6) === 0 ? [AbilityId.NATURAL_CURE, AbilityId.REGENERATOR][rnd(2)]
      : SUMMON && rnd(5) === 0 ? entryAbilityIds[rnd(entryAbilityIds.length)]
      : RICH && rnd(2) === 0 ? abilityIds[rnd(abilityIds.length)] : AbilityId.BALL_FETCH;
    p.gender = rnd(3) - 1;
    p.friendship = rnd(256);
    p.isTerastallized = false;
    p.teraType = rnd(19);
    p.stellarTypesBoosted = [];
    p.calculateStats();
    p.hp = rnd(3) === 0 ? p.getMaxHp() : 1 + rnd(p.getMaxHp());
    p.status = null;
    if (HEAL && rnd(3) === 0) {
      p.status = new Status([StatusEffect.POISON, StatusEffect.TOXIC][rnd(2)], rnd(4));
    } else if (RICH ? rnd(3) === 0 : STATUS || WEATHER ? rnd(4) === 0 : ONHIT || CHARGE || TAGS || TRAPS || PROTECT || ARENA || EFFECTS ? rnd(6) === 0 : HEADER ? rnd(4) === 0 : SWITCH || SUMMON ? rnd(6) === 0 : STAT && rnd(8) === 0) {
      const s = statuses[rnd(statuses.length)];
      p.status = new Status(s, rnd(4), s === StatusEffect.SLEEP ? 1 + rnd(3) : 0, s === StatusEffect.FREEZE ? 1 + rnd(3) : undefined);
    }
    const n = 1 + rnd(4);
    p.moveset = [];
    for (let k = 0; k < n; k++) {
      const m = STAT && rnd(10) < 7
        ? (rnd(5) === 0 ? statCondMoves[rnd(statCondMoves.length)] : statMoves[rnd(statMoves.length)])
        : STATUS && rnd(10) < 7
        ? (rnd(6) === 0 ? thawMoves[rnd(thawMoves.length)] : statusSetMoves[rnd(statusSetMoves.length)])
        : ONHIT && rnd(10) < 8
        ? (rnd(4) !== 0 ? contactAttacks[rnd(contactAttacks.length)] : attacks[rnd(attacks.length)])
        : HEAL && rnd(10) < 6
        ? healMoves[rnd(healMoves.length)]
        : BOOST && rnd(3) === 0
        ? allMoves[fieldSetMoves[rnd(fieldSetMoves.length)]]
        : WEATHER && rnd(10) < 7
        ? weatherMoves[rnd(weatherMoves.length)]
        : TRAPS && rnd(10) < 7
        ? trapMoves[rnd(trapMoves.length)]
        : PROTECT && rnd(10) < 8
        ? (rnd(2) === 0 ? protectMoves[rnd(protectMoves.length)]
          : rnd(2) === 0 ? protectHitters[rnd(protectHitters.length)] : contactAttacks[rnd(contactAttacks.length)])
        : ARENA && rnd(10) < 8
        ? (rnd(2) === 0 ? arenaMoves[rnd(arenaMoves.length)]
          : rnd(2) === 0 ? arenaHitters[rnd(arenaHitters.length)] : attacks[rnd(attacks.length)])
        : EFFECTS && rnd(10) < 7
        ? (rnd(2) === 0 ? effectMoves[rnd(effectMoves.length)]
          : rnd(4) === 0 ? windMoves[rnd(windMoves.length)] : attacks[rnd(attacks.length)])
        : CALLED && rnd(10) < 6
        ? calledMoves[rnd(calledMoves.length)]
        : HEADER && rnd(10) < 6
        ? headerMoves[rnd(headerMoves.length)]
        : DELAYED && rnd(10) < 6
        ? delayedMoves[rnd(delayedMoves.length)]
        : (SWITCH || SUMMON) && rnd(10) < 6
        ? switchMoves[rnd(switchMoves.length)]
        : SWITCH || SUMMON
        ? (rnd(3) === 0 ? switchOthers[rnd(switchOthers.length)]
          : rnd(2) === 0 ? contactAttacks[rnd(contactAttacks.length)] : attacks[rnd(attacks.length)])
        : DELAYED
        ? (rnd(4) === 0 ? protectOnly[rnd(protectOnly.length)] : attacks[rnd(attacks.length)])
        : HEADER
        ? (rnd(2) === 0 ? contactAttacks[rnd(contactAttacks.length)] : headerHitters[rnd(headerHitters.length)])
        : TAGS && rnd(10) < 8
        ? (rnd(4) !== 0 ? tagMoves[rnd(tagMoves.length)] : tagReaders[rnd(tagReaders.length)])
        : CHARGE && rnd(10) < 7
        ? (rnd(3) !== 0 ? chargeMoves[rnd(chargeMoves.length)] : hitsTagMoves[rnd(hitsTagMoves.length)])
        : COND && rnd(10) < 6
        ? withConditions[rnd(withConditions.length)]
        : rnd(10) < 7 ? attacks[rnd(attacks.length)] : usable[rnd(usable.length)];
      p.moveset.push(new PokemonMove(m.id, rnd(8) === 0 ? rnd(m.pp + 1) : 0));
    }
    if (HEADER) {
      for (const pm of p.moveset) {
        if (headerIds.includes(pm.moveId) && rnd(8) === 0) {
          pm.ppUsed = pm.getMovePp();
        }
      }
    }
    if (CALLED && p.moveset.some((m: any) => m.moveId === MoveId.SLEEP_TALK) && rnd(2) === 0) {
      p.status = new Status(StatusEffect.SLEEP, rnd(4), 1 + rnd(3));
    }
    if (TAGS && rnd(2) === 0) {
      // a history for Disable and Encore to read: its own moves, now and then Struggle or Encore
      // (which Encore can't repeat), a called move (virtual) or a failed one
      const pool = [...p.moveset.map((m: any) => m.moveId), MoveId.STRUGGLE, MoveId.ENCORE];
      for (let k = 1 + rnd(2); k > 0; k--) {
        p.pushMoveHistory({
          move: pool[rnd(pool.length)],
          targets: [p.getBattlerIndex()],
          useMode: rnd(5) === 0 ? MoveUseMode.FOLLOW_UP : MoveUseMode.NORMAL,
          result: rnd(4) === 0 ? MoveResult.FAIL : MoveResult.SUCCESS,
        });
      }
    }
    if (COND) {
      p.tempSummonData.waveTurnCount = 1 + rnd(3);
      if (p.moveset.some((m: any) => m.moveId === MoveId.BELCH) && rnd(2) === 0) {
        p.battleData.hasEatenBerry = true; // Belch's condition
      }
      const pool = [...p.moveset.map((m: any) => m.moveId), MoveId.PROTECT, MoveId.DETECT, MoveId.QUICK_GUARD, MoveId.STRUGGLE];
      for (let k = rnd(4); k > 0; k--) {
        p.pushMoveHistory({
          move: pool[rnd(pool.length)],
          targets: [p.getBattlerIndex()],
          useMode: rnd(5) === 0 ? MoveUseMode.INDIRECT : MoveUseMode.NORMAL,
          result: rnd(3) === 0 ? MoveResult.FAIL : MoveResult.SUCCESS,
        });
      }
    }
  };
  // replace one moveset slot with this move and make it the Pokemon's command
  const commandMove = (p: any, move: MoveId) => {
    const slot = rnd(p.moveset.length);
    p.moveset[slot] = new PokemonMove(move);
    const turnMove = { move, targets: getMoveTargets(p, move).targets, useMode: MoveUseMode.NORMAL };
    battle.turnCommands[p.getBattlerIndex()] = p.isPlayer()
      ? { command: Command.FIGHT, cursor: slot, move: turnMove, args: [MoveUseMode.NORMAL, undefined] }
      : { command: Command.FIGHT, move: turnMove, skip: false };
  };
  const command = (p: any) => {
    // CommandPhase / EnemyCommandPhase: a move with PP left (Struggle when none is usable)
    const slots = p.moveset.map((_: any, k: number) => k).filter((k: number) => p.moveset[k].ppUsed < p.moveset[k].getMovePp());
    const moveId = slots.length ? p.moveset[slots[rnd(slots.length)]].moveId : MoveId.STRUGGLE;
    const targets = getMoveTargets(p, moveId).targets;
    const b = p.getBattlerIndex();
    if (p.isPlayer()) {
      battle.turnCommands[b] = { command: Command.FIGHT, cursor: 0, move: { move: moveId, targets, useMode: MoveUseMode.NORMAL }, args: [MoveUseMode.NORMAL, undefined] };
      battle.preTurnCommands[b] = { command: Command.FIGHT, targets: [b], skip: true };
    } else {
      battle.turnCommands[b] = { command: Command.FIGHT, move: { move: moveId, targets, useMode: MoveUseMode.NORMAL }, skip: false };
    }
  };

  const out: string[] = [];
  const thrown: Record<string, number> = {};
  for (let i = 0; i < N; i++) {
    picks = [0, 1, 2].map(j => (Math.imul(3 * i + j + 1, 2654435761) >>> 0) % 60);
    pickAt = 0;
    pm.clearAllPhases();
    pm.phaseQueue.deferredActive = false; // clear() leaves it set
    pm.dynamicQueueManager.clearLastTurnOrder();
    scene.abilityBar.setVisible(false);
    battle.battleScore = 0;
    // faint bookkeeping accumulates over the cases otherwise
    battle.playerFaintsHistory.length = 0;
    battle.enemyFaintsHistory.length = 0;
    battle.enemyFaints = 0;
    scene.arena.playerFaints = 0;
    battle.playerParticipantIds.clear();
    scene.modifiers.length = 0;
    scene.enemyModifiers.length = 0;
    scene.arena.tags.length = 0;
    scene.arena.positionalTagManager.tags.length = 0; // Future Sight, Wish... from an earlier case
    scene.arena.weather = null;
    scene.arena.terrain = null;
    battle.turn = 1 + rnd(30);
    battle.turnCommands = {};
    battle.preTurnCommands = {};
    setup(A);
    setup(D);
    const party = scene.getPlayerParty();
    party.length = 0;
    party.push(A);
    const benchSize = SWITCH && rnd(5) === 0 ? 0 : 1 + rnd(playerBench.length);
    for (const b of playerBench) {
      // a bench Pokemon sent in during an earlier case: off the field, with fresh summon, turn and wave data
      if (b.isOnField()) {
        scene.field.remove(b);
      }
      b.switchOutStatus = false;
      b.resetSummonData();
      b.resetTurnData();
      b.resetBattleAndWaveData();
      b.customPokemonData.ability = -1;
    }
    for (const b of playerBench.slice(0, benchSize)) {
      b.species = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
      // a form an earlier case's switch-in changed to may not exist for the new species
      b.formIndex = 0;
      if (SWITCH || SUMMON) {
        // a Pokemon that can be dragged in: its own level, ability, moves, HP, status and shininess
        b.level = 1 + rnd(100);
        b.ivs = [0, 0, 0, 0, 0, 0].map(() => rnd(32));
        b.nature = rnd(25);
        b.abilityIndex = rnd(3);
        b.shiny = rnd(8) === 0;
        b.moveset = [];
        for (let k = 1 + rnd(4); k > 0; k--) {
          b.moveset.push(new PokemonMove(usable[rnd(usable.length)].id));
        }
      }
      if (SUMMON && rnd(2) === 0) {
        b.customPokemonData.ability = entryAbilityIds[rnd(entryAbilityIds.length)];
      }
      b.calculateStats();
      b.hp = rnd(SWITCH ? 3 : 4) === 0 ? 0 : (SWITCH || SUMMON) && rnd(2) === 0 ? 1 + rnd(b.getMaxHp()) : b.getMaxHp();
      b.status = b.hp ? null : new Status(StatusEffect.FAINT);
      if ((SWITCH || SUMMON) && b.hp && rnd(4) === 0) {
        const st = statuses[rnd(statuses.length)];
        b.status = new Status(st, rnd(4), st === StatusEffect.SLEEP ? 1 + rnd(3) : 0, st === StatusEffect.FREEZE ? 1 + rnd(3) : undefined);
      }
      party.push(b);
    }
    battle.playerParticipantIds.add(A.id);
    if (CALLED) {
      for (const b of party.slice(1)) {
        b.moveset = [];
        for (let k = 1 + rnd(4); k > 0; k--) {
          const m = rnd(3) === 0 ? calledMoves[rnd(calledMoves.length)] : usable[rnd(usable.length)];
          b.moveset.push(new PokemonMove(m.id));
        }
      }
    }
    command(A);
    command(D);
    if (SUMMON && rnd(3) === 0) {
      // the player switches (CommandPhase.tryLeaveField): a bench Pokemon that hasn't fainted
      const bench = party.map((_: any, k: number) => k).filter((k: number) => k >= 1 && !party[k].isFainted());
      if (bench.length) {
        battle.turnCommands[0] = { command: Command.POKEMON, cursor: bench[rnd(bench.length)], args: [false] };
      }
    }
    if (CALLED) {
      (scene.arena as any).biomeId = biomeIds[rnd(biomeIds.length)];
      if (rnd(3) === 0) {
        scene.arena.terrain = new Terrain(1 + rnd(4), 1 + rnd(5), 5);
      }
      if (rnd(6) === 0) {
        scene.arena.tags.push(getArenaTag(ArenaTagType.GRAVITY, 1 + rnd(5), MoveId.GRAVITY, A.id, ArenaTagSide.BOTH));
      }
      battle.lastMove = rnd(4) === 0 ? MoveId.NONE : usable[rnd(usable.length)].id;
      for (const p of [A, D]) {
        for (let k = rnd(4); k > 0; k--) {
          const move = rnd(3) === 0 ? usable[rnd(usable.length)].id : p.moveset[rnd(p.moveset.length)].moveId;
          p.pushMoveHistory({
            move,
            targets: getMoveTargets(p, move).targets,
            useMode: [MoveUseMode.NORMAL, MoveUseMode.NORMAL, MoveUseMode.NORMAL, MoveUseMode.IGNORE_PP,
              MoveUseMode.FOLLOW_UP, MoveUseMode.INDIRECT][rnd(6)],
            result: rnd(4) === 0 ? MoveResult.FAIL : MoveResult.SUCCESS,
          });
        }
      }
      for (const id of [MoveId.SLEEP_TALK, MoveId.ASSIST]) {
        (allMoves[id].getAttrs("RandomMovesetMoveAttr")[0] as any).selectedMove =
          rnd(8) === 0 ? usable[rnd(usable.length)].id : MoveId.NONE;
      }
    }
    if (STAT) {
      if (rnd(8) === 0) {
        D.summonData.ability = A.summonData.ability; // Mirror Armor against Mirror Armor, Opportunist against Opportunist
      }
      for (const p of [A, D]) {
        p.isTerastallized = rnd(4) === 0;
        if (rnd(3) !== 0) {
          p.summonData.statStages = p.summonData.statStages.map(() => [0, 0, 1, -1, 2, -2, 5, -5, 6, -6][rnd(10)]);
        }
      }
      if (rnd(4) === 0) {
        const side = [ArenaTagSide.PLAYER, ArenaTagSide.ENEMY][rnd(2)];
        scene.arena.tags.push(getArenaTag(ArenaTagType.MIST, 1 + rnd(5), MoveId.MIST, A.id, side));
      }
    }
    if (ONHIT) {
      if (rnd(8) === 0) {
        D.summonData.ability = A.summonData.ability; // Rough Skin against Rough Skin, Mummy against Mummy
      }
      if (rnd(4) === 0) {
        // an ability the on-hit ones check: Magic Guard against Rough Skin, Mirror Armor against Gooey...
        const counters = [AbilityId.MAGIC_GUARD, AbilityId.MIRROR_ARMOR, AbilityId.LONG_REACH, AbilityId.SHIELD_DUST, AbilityId.OVERCOAT];
        [A, D][rnd(2)].summonData.ability = counters[rnd(counters.length)];
      }
      for (const p of [A, D]) {
        p.isTerastallized = rnd(6) === 0; // Color Change
        if (rnd(4) === 0) {
          p.summonData.statStages = p.summonData.statStages.map(() => [0, 0, 1, -1, 2, -2, 5, -5, 6, -6][rnd(10)]); // Anger Point from below 0
        }
      }
    }
    if (CHARGE) {
      // half the Pokemon holding a two-turn move are on its second turn: what MoveChargePhase
      // left (CHARGING, the semi-invulnerable tag, the queued move, the OTHER history entry) and
      // the command the command phases build from the queue
      for (const p of [A, D]) {
        const charged = p.moveset.filter((m: any) => m.getMove().isChargingMove());
        if (!charged.length || rnd(2) !== 0) {
          continue;
        }
        const move = charged[rnd(charged.length)].getMove();
        const other = p === A ? D : A;
        const targets = [other.getBattlerIndex()];
        p.summonData.tags.push(getBattlerTag(BattlerTagType.CHARGING, 1, move.id, p.id));
        const semi = move.chargeAttrs.find((a: any) => a.constructor.name === "SemiInvulnerableAttr");
        if (semi) {
          p.summonData.tags.push(getBattlerTag(semi.tagType, 1, move.id, p.id));
        }
        p.summonData.moveQueue = [{ move: move.id, targets: [...targets], useMode: MoveUseMode.IGNORE_PP }];
        p.summonData.moveHistory.push({ move: move.id, targets: [...targets], result: MoveResult.OTHER, useMode: MoveUseMode.IGNORE_PP });
        const turnMove = { move: move.id, targets: [...targets], useMode: MoveUseMode.IGNORE_PP };
        const b = p.getBattlerIndex();
        battle.turnCommands[b] = p.isPlayer()
          ? { command: Command.FIGHT, cursor: p.moveset.findIndex((m: any) => m.moveId === move.id), move: turnMove, args: [MoveUseMode.IGNORE_PP, turnMove] }
          : { command: Command.FIGHT, move: turnMove, skip: false };
      }
      for (const p of [A, D]) {
        p.isTerastallized = rnd(8) === 0;
      }
      // Smack Down, Thousand Arrows or Gravity against a Pokemon that knows Fly or Bounce, now and
      // then: caught mid-air, it falls. (Its move isn't interrupted: both take the FLYING tag off
      // before adding INTERRUPTED, whose canAdd asks for FLYING.)
      for (const p of [A, D]) {
        const other = p === A ? D : A;
        const flier = p.moveset.some((m: any) => m.moveId === MoveId.FLY || m.moveId === MoveId.BOUNCE);
        if (flier && !other.getTag(BattlerTagType.CHARGING) && rnd(2) === 0) {
          commandMove(other, [MoveId.SMACK_DOWN, MoveId.THOUSAND_ARROWS, MoveId.GRAVITY][rnd(3)]);
        }
      }
    }
    if (TAGS) {
      for (const p of [A, D]) {
        const other = p === A ? D : A;
        p.isTerastallized = rnd(6) === 0; // Tar Shot
        // some tags already there (built directly: addTag would run onAdd)
        if (rnd(3) === 0) {
          for (let k = 1 + rnd(2); k > 0; k--) {
            const type = portedTags[rnd(portedTags.length)];
            if (p.getTag(type) || type === BattlerTagType.FRENZY) {
              continue;
            }
            const self = [BattlerTagType.NO_RETREAT, BattlerTagType.INGRAIN, BattlerTagType.DESTINY_BOND].includes(type);
            const tag = getBattlerTag(type, 1 + rnd(4), rnd(2) === 0 ? MoveId.NONE : MoveId.MEAN_LOOK, self ? p.id : other.id) as any;
            if (type === BattlerTagType.AUTOTOMIZED) {
              tag.autotomizeCount = 1 + rnd(3);
            }
            if (type === BattlerTagType.CRIT_BOOST || type === BattlerTagType.DRAGON_CHEER) {
              tag.critStages = 1 + rnd(2);
            }
            if (type === BattlerTagType.POWER_TRICK) {
              tag.swapStat(p); // what its onAdd did
            }
            if (type === BattlerTagType.GORILLA_TACTICS) {
              tag.moveId = p.moveset[rnd(p.moveset.length)].moveId;
              if (rnd(2) === 0) {
                p.setStat(Stat.ATK, p.getStat(Stat.ATK, false) * 1.5, false); // what its onAdd did
              }
            }
            if (type === BattlerTagType.SUPREME_OVERLORD) {
              tag.faintCount = rnd(6);
            }
            if (type === BattlerTagType.SLOW_START) {
              tag.turnCount = 1 + rnd(5);
            }
            if (type === BattlerTagType.STOCKPILING) {
              // stacks from earlier turns, each raise counted or not (capped stages, Contrary)
              tag.stockpiledCount = 1 + rnd(3);
              tag.statChangeCounts[Stat.DEF] = rnd(tag.stockpiledCount + 1);
              tag.statChangeCounts[Stat.SPDEF] = rnd(tag.stockpiledCount + 1);
            }
            if (type === BattlerTagType.NIGHTMARE && !p.status && rnd(3) !== 0) {
              p.status = new Status(StatusEffect.SLEEP, 0, 1 + rnd(3)); // a nightmare needs sleep
            }
            if (type === BattlerTagType.DISABLED) {
              tag.moveId = rnd(4) === 0 ? MoveId.NONE : p.moveset[rnd(p.moveset.length)].moveId;
            }
            if (type === BattlerTagType.DISABLED || type === BattlerTagType.TAUNT) {
              tag.turnCount = 1 + rnd(4); // their constructors fix 4; some run out this turn
            }
            if (type === BattlerTagType.ENCORE) {
              // an Encore from an earlier turn: that turn's end cleared its private addedThisTurn
              tag.moveId = p.moveset[rnd(p.moveset.length)].moveId;
              tag.lapse(p, BattlerTagLapseType.TURN_END);
              tag.turnCount = 1 + rnd(4);
            }
            p.summonData.tags.push(tag);
            if (type === BattlerTagType.INFATUATED && rnd(2) === 0) {
              // Attract again on a Pokemon that already holds it (the tag's onOverlap)
              p.gender = rnd(2);
              other.gender = 1 - p.gender;
              commandMove(other, MoveId.ATTRACT);
            }
            if (type === BattlerTagType.CURSED && rnd(2) === 0) {
              // a Ghost-type Curse on a Pokemon already cursed (it fails with its message)
              other.isTerastallized = true;
              other.teraType = PokemonType.GHOST;
              commandMove(other, MoveId.CURSE);
            }
            if (type === BattlerTagType.POWDER && rnd(2) === 0) {
              // a Fire move under the powder, now and then in heavy rain (no explosion there)
              commandMove(p, [MoveId.EMBER, MoveId.FLAMETHROWER, MoveId.FLAME_WHEEL][rnd(3)]);
              if (rnd(2) === 0) {
                scene.arena.weather = new Weather(WeatherType.HEAVY_RAIN, 0, 0);
              }
            }
            if (type === BattlerTagType.STOCKPILING && rnd(2) === 0) {
              commandMove(p, rnd(2) === 0 ? MoveId.SPIT_UP : MoveId.SWALLOW); // the stacks go, and the stat stages with them
            }
            // what the hit check reads with the Lock-On tag: the newest Lock-On or Mind Reader in the
            // history and its targets (the other Pokemon, now and then the user itself), sometimes
            // with a later move on top
            if (type === BattlerTagType.IGNORE_ACCURACY && rnd(3) !== 0) {
              const tb = rnd(4) === 0 ? p.getBattlerIndex() : other.getBattlerIndex();
              p.pushMoveHistory({ move: rnd(2) ? MoveId.LOCK_ON : MoveId.MIND_READER, targets: [tb], useMode: MoveUseMode.NORMAL, result: MoveResult.SUCCESS });
              if (rnd(3) === 0) {
                p.pushMoveHistory({ move: p.moveset[0].moveId, targets: [other.getBattlerIndex()], useMode: MoveUseMode.NORMAL, result: MoveResult.SUCCESS });
              }
            }
          }
        }
        // the middle of a rampage: what the last turn's MoveLockTag lapse left
        const rampage = p.moveset.filter((m: any) =>
          m.getMove().attrs.some((a: any) => isA(a, ["AddBattlerTagAttr"]) && a.tagType === BattlerTagType.FRENZY),
        );
        if (!rampage.length || rnd(2) !== 0 || p.getTag(BattlerTagType.FRENZY)) {
          continue;
        }
        const move = rampage[rnd(rampage.length)].getMove();
        const targets = [other.getBattlerIndex()];
        p.summonData.tags.push(getBattlerTag(BattlerTagType.FRENZY, 1 + rnd(2), move.id, p.id));
        p.summonData.moveHistory.push({ move: move.id, targets: [...targets], result: MoveResult.SUCCESS, useMode: rnd(2) ? MoveUseMode.IGNORE_PP : MoveUseMode.NORMAL });
        p.summonData.moveQueue = [{ move: move.id, targets: [...targets], useMode: MoveUseMode.IGNORE_PP }];
        const turnMove = { move: move.id, targets: [...targets], useMode: MoveUseMode.IGNORE_PP };
        const b = p.getBattlerIndex();
        battle.turnCommands[b] = p.isPlayer()
          ? { command: Command.FIGHT, cursor: p.moveset.findIndex((m: any) => m.moveId === move.id), move: turnMove, args: [MoveUseMode.IGNORE_PP, turnMove] }
          : { command: Command.FIGHT, move: turnMove, skip: false };
      }
      // what some tags read: a terrain (Yawn fails in Electric Terrain) or rain (Powder is off in
      // Heavy Rain) now and then
      if (rnd(6) === 0) {
        scene.arena.terrain = new Terrain(1 + rnd(4), 1 + rnd(5), 5);
      }
      if (rnd(8) === 0 && !scene.arena.weather) {
        scene.arena.weather = new Weather(rnd(2) === 0 ? WeatherType.HEAVY_RAIN : WeatherType.RAIN, 1 + rnd(5), 5);
      }
      for (const p of [A, D]) {
        if (p.moveset.some((m: any) => m.moveId === MoveId.YAWN) && rnd(2) === 0) {
          scene.arena.terrain = new Terrain(TerrainType.ELECTRIC, 1 + rnd(5), 5); // Yawn fails on a grounded target
        }
        if (rnd(10) === 0) {
          // Truant with the tag its entry left, and now and then a move last turn to loaf after
          p.summonData.ability = AbilityId.TRUANT;
          if (!p.getTag(BattlerTagType.TRUANT)) {
            p.summonData.tags.push(getBattlerTag(BattlerTagType.TRUANT, 1, MoveId.NONE, p.id));
          }
          if (rnd(3) !== 0) {
            p.pushMoveHistory({ move: p.moveset[0].moveId, targets: [p.getBattlerIndex()], useMode: MoveUseMode.NORMAL, result: MoveResult.SUCCESS });
          }
        }
      }
    }
    if (TRAPS) {
      // a trap already there, as an earlier turn's TrapAttr and TURN_END lapses leave it (1 to 5
      // turns left, its source the other Pokemon), or Mean Look's (built directly: addTag would run onAdd)
      for (const p of [A, D]) {
        const other = p === A ? D : A;
        if (rnd(2) === 0) {
          p.summonData.tags.push(getBattlerTag(trapTags[rnd(trapTags.length)], 1 + rnd(5), MoveId.NONE, other.id));
        } else if (rnd(6) === 0) {
          p.summonData.tags.push(getBattlerTag(BattlerTagType.TRAPPED, 1 + rnd(4), MoveId.MEAN_LOOK, other.id));
        }
      }
      // a terrain on a quarter of the cases and fog on an eighth, for Defog, Ice Spinner and Steel Roller
      scene.arena.terrain = rnd(4) === 0 ? new Terrain(1 + rnd(4), 1 + rnd(5), 5) : null;
      scene.arena.weather = rnd(8) === 0 ? new Weather(6, 1 + rnd(5), 5) : null; // WeatherType.FOG
      if (rnd(3) === 0) {
        for (let k = 1 + rnd(2); k > 0; k--) {
          const type = trapArenaTags[rnd(trapArenaTags.length)];
          const side = rnd(2) === 0 ? ArenaTagSide.PLAYER : ArenaTagSide.ENEMY;
          if (!scene.arena.getTagOnSide(type, side)) {
            scene.arena.tags.push(getArenaTag(type, 1 + rnd(5), MoveId.NONE, A.id, side));
          }
        }
      }
    }
    if (DELAYED) {
      // pending Future Sight, Doom Desire and Wish tags, built by upstream's own loader
      const mgr = scene.arena.positionalTagManager;
      for (let k = rnd(3) === 0 ? 0 : 1 + rnd(2); k > 0; k--) {
        const targetIndex = [A.getBattlerIndex(), D.getBattlerIndex(), A.getBattlerIndex() + 1][rnd(5) === 0 ? 2 : rnd(2)];
        const target = targetIndex === A.getBattlerIndex() ? A : targetIndex === D.getBattlerIndex() ? D : null;
        if (rnd(3) === 0) {
          mgr.addTag({
            tagType: PositionalTagType.WISH,
            turnCount: 1 + rnd(2),
            targetIndex,
            healHp: 1 + rnd(target ? target.getMaxHp() : 100),
            pokemonName: "x",
          });
        } else {
          const bench = party.slice(1);
          const r = rnd(8);
          const sourceId = r < 3 ? A.id : r < 6 ? D.id : r === 6 && bench.length ? bench[rnd(bench.length)].id
            : r === 6 ? A.id : 1 + rnd(1000);
          mgr.addTag({
            tagType: PositionalTagType.DELAYED_ATTACK,
            sourceId,
            targetIndex,
            sourceMove: rnd(2) === 0 ? MoveId.FUTURE_SIGHT : MoveId.DOOM_DESIRE,
            turnCount: rnd(2) === 0 ? 1 : 2 + rnd(2),
          });
        }
      }
      for (const p of [A, D]) {
        if (rnd(3) === 0) {
          p.hp = 1 + rnd(Math.max(1, Math.floor(p.getMaxHp() / 4)));
        }
      }
    }
    if (SUMMON && rnd(2) === 0) {
      // entry hazards on the player's side, laid by the wild Pokemon
      const hazards = [ArenaTagType.SPIKES, ArenaTagType.TOXIC_SPIKES, ArenaTagType.STEALTH_ROCK, ArenaTagType.STICKY_WEB];
      for (let k = 1 + rnd(3); k > 0; k--) {
        const type = hazards[rnd(hazards.length)];
        if (scene.arena.getTagOnSide(type, ArenaTagSide.PLAYER)) {
          continue;
        }
        const tag = getArenaTag(type, 0, MoveId.NONE, D.id, ArenaTagSide.PLAYER) as any;
        tag.layers = 1 + rnd(tag.maxLayers);
        scene.arena.tags.push(tag);
      }
    }
    if (SWITCH || SUMMON) {
      const r = rnd(3);
      battle.waveIndex = r === 0 ? 10 * (1 + rnd(19)) : r === 1 ? [5, 15, 25, 45, 49, 55, 95, 99, 105, 149, 155, 195][rnd(12)]
        : 1 + rnd(199);
      scene.gameMode = getGameMode([GameModes.CLASSIC, GameModes.CLASSIC, GameModes.ENDLESS, GameModes.DAILY][rnd(4)]);
      if (rnd(4) === 0) {
        scene.arena.weather = new Weather(WeatherType.SNOW, 1 + rnd(5), 5);
      }
      for (const side of [ArenaTagSide.PLAYER, ArenaTagSide.ENEMY]) {
        if (rnd(6) === 0) {
          scene.arena.tags.push(getArenaTag(ArenaTagType.MIST, 1 + rnd(5), MoveId.MIST, A.id, side));
        }
      }
      for (const p of [A, D]) {
        if (rnd(6) === 0) {
          const tag = getBattlerTag(BattlerTagType.SUBSTITUTE, 0, MoveId.SUBSTITUTE, p.id) as any;
          tag.hp = 1 + rnd(Math.max(1, Math.floor(p.getMaxHp() / 4)));
          tag.sourceInFocus = false;
          tag.sprite = scene.addFieldSprite(p.x, p.y, `pkmn${p.isPlayer() ? "__back" : ""}__sub`);
          p.summonData.tags.push(tag);
        }
        if (rnd(4) === 0) {
          const edge = rnd(2) === 0 ? -6 : 6;
          p.setStatStage(Stat.ATK, edge - Math.sign(edge) * rnd(2));
          p.setStatStage(Stat.SPATK, edge - Math.sign(edge) * rnd(2));
        }
        if (rnd(3) === 0) {
          const max = p.getMaxHp();
          p.hp = Math.min(max, Math.ceil(max / 2) + rnd(Math.max(1, Math.floor(max / 8))));
        }
        if (SWITCH && p.moveset.some((m: any) => m.moveId === MoveId.BATON_PASS) && rnd(3) === 0) {
          // Power Trick, which Baton Pass hands over with its Attack and Defense swap
          const tag = getBattlerTag(BattlerTagType.POWER_TRICK, 0, MoveId.POWER_TRICK, p.id) as any;
          tag.swapStat(p); // what its onAdd did
          p.summonData.tags.push(tag);
        }
      }
    }
    if (HEADER) {
      for (const p of [A, D]) {
        if (rnd(6) === 0) {
          const tag = getBattlerTag(BattlerTagType.SUBSTITUTE, 0, MoveId.SUBSTITUTE, p.id) as any;
          tag.hp = 1 + rnd(Math.max(1, Math.floor(p.getMaxHp() / 4)));
          tag.sourceInFocus = false;
          tag.sprite = scene.addFieldSprite(p.x, p.y, `pkmn${p.isPlayer() ? "__back" : ""}__sub`);
          p.summonData.tags.push(tag);
        }
      }
    }
    if (PROTECT) {
      for (const p of [A, D]) {
        const other = p === A ? D : A;
        // Leech Seed and a Substitute already there, with what onAdd leaves (built directly: addTag
        // would run onAdd): the seeder's slot; a doll with 1 to a quarter of max HP left, and a
        // stand-in for the sprite its animation made
        if (rnd(3) === 0 && !p.isOfType(PokemonType.GRASS)) {
          const tag = getBattlerTag(BattlerTagType.SEEDED, 1, MoveId.LEECH_SEED, other.id) as any;
          tag.sourceIndex = other.getBattlerIndex();
          p.summonData.tags.push(tag);
        }
        if (rnd(3) === 0) {
          const tag = getBattlerTag(BattlerTagType.SUBSTITUTE, 0, MoveId.SUBSTITUTE, p.id) as any;
          tag.hp = 1 + rnd(Math.max(1, Math.floor(p.getMaxHp() / 4)));
          tag.sourceInFocus = false;
          tag.sprite = scene.addFieldSprite(p.x, p.y, `pkmn${p.isPlayer() ? "__back" : ""}__sub`);
          p.summonData.tags.push(tag);
        }
        // a protection that worked on the turns before, so the next one rolls its chance
        const prot = p.moveset.filter((m: any) => m.getMove().attrs.some((a: any) => a.constructor.name === "ProtectAttr"));
        if (prot.length && rnd(3) === 0) {
          for (let k = 1 + rnd(2); k > 0; k--) {
            p.pushMoveHistory({
              move: prot[rnd(prot.length)].moveId,
              targets: [p.getBattlerIndex()],
              useMode: MoveUseMode.NORMAL,
              result: MoveResult.SUCCESS,
            });
          }
        }
      }
    }
    if (ARENA) {
      // arena tags already up, built directly (addTag would run onAdd): screens, hazards with some
      // layers, field effects, the side-wide protections and the pledge fields, each on a random
      // side unless its constructor makes it field-wide, set by either Pokemon
      for (let k = rnd(4); k > 0; k--) {
        const type = arenaTagPool[rnd(arenaTagPool.length)];
        const side = rnd(2) === 0 ? ArenaTagSide.PLAYER : ArenaTagSide.ENEMY;
        const source = rnd(2) === 0 ? A : D;
        const tag = getArenaTag(type, 1 + rnd(5), MoveId.NONE, source.id, side) as any;
        if (!tag || scene.arena.getTagOnSide(type, tag.side)) {
          continue;
        }
        // half the time part of a fixed duration (the pledge fields' 4 turns...) has passed
        if (tag.turnCount > 1 && rnd(2) === 0) {
          tag.turnCount = 1 + rnd(tag.turnCount);
        }
        if (tag instanceof EntryHazardTag) {
          tag.layers = 1 + rnd(tag.maxLayers);
        }
        scene.arena.tags.push(tag);
      }
      // two of the side-wide protections on one side, so only the first one says so
      if (rnd(4) === 0) {
        const guards = [ArenaTagType.QUICK_GUARD, ArenaTagType.WIDE_GUARD, ArenaTagType.MAT_BLOCK, ArenaTagType.CRAFTY_SHIELD];
        const side = rnd(2) === 0 ? ArenaTagSide.PLAYER : ArenaTagSide.ENEMY;
        for (let k = 0; k < 2; k++) {
          const type = guards[rnd(guards.length)];
          if (!scene.arena.getTagOnSide(type, side)) {
            scene.arena.tags.push(getArenaTag(type, 1, MoveId.NONE, (rnd(2) === 0 ? A : D).id, side) as any);
          }
        }
      }
      for (const p of [A, D]) {
        const other = p === A ? D : A;
        const sideOf = (q: any) => (q.isPlayer() ? ArenaTagSide.PLAYER : ArenaTagSide.ENEMY);
        const layUp = (type: ArenaTagType, side: ArenaTagSide) => {
          const tag = getArenaTag(type, 1 + rnd(5), MoveId.NONE, p.id, side) as any;
          if (tag && !scene.arena.getTagOnSide(type, tag.side)) {
            if (tag instanceof EntryHazardTag) {
              (tag as any).layers = 1 + rnd(tag.maxLayers);
            }
            scene.arena.tags.push(tag);
          }
        };
        // half the time the tag this Pokemon's move lays is already where it would go, so the move
        // stacks a layer, runs the tag's overlap or fails (Stone Axe's Stealth Rock always full)
        for (const m of p.moveset) {
          const lay = m.getMove().attrs.find((a: any) =>
            ["AddArenaTagAttr", "AddArenaTrapTagAttr", "AddArenaTrapTagHitAttr"].includes(a.constructor.name),
          ) as any;
          if (lay && rnd(2) === 0) {
            layUp(lay.tagType, sideOf(lay.selfSideTarget ? p : other));
          }
        }
        // what the side-wide protections and Safeguard look at the ability for: Quick Guard against
        // a raised priority, Safeguard against an Infiltrator's confusion; Stone Axe and Ceaseless
        // Edge with a changed chance
        if ([AbilityId.PRANKSTER, AbilityId.GALE_WINGS, AbilityId.TRIAGE].includes(p.getAbility().id) && rnd(2) === 0) {
          layUp(ArenaTagType.QUICK_GUARD, sideOf(other));
        }
        if (p.moveset.some((m: any) => m.getMove().attrs.some((a: any) => a.constructor.name === "ConfuseAttr")) && rnd(2) === 0) {
          layUp(ArenaTagType.SAFEGUARD, sideOf(other));
          if (rnd(2) === 0) {
            p.summonData.ability = AbilityId.INFILTRATOR;
          }
        }
        if (p.moveset.some((m: any) => [MoveId.STONE_AXE, MoveId.CEASELESS_EDGE].includes(m.moveId)) && rnd(2) === 0) {
          p.summonData.ability = rnd(2) === 0 ? AbilityId.SERENE_GRACE : AbilityId.SHEER_FORCE;
        }
        if (p.moveset.some((m: any) => m.moveId === MoveId.TAILWIND) && rnd(2) === 0) {
          p.summonData.ability = rnd(2) === 0 ? AbilityId.WIND_RIDER : AbilityId.WIND_POWER;
          // half the Wind Power ones already charged
          if (p.summonData.ability === AbilityId.WIND_POWER && rnd(2) === 0) {
            p.summonData.tags.push(getBattlerTag(BattlerTagType.CHARGED, 1, MoveId.NONE, p.id));
          }
        }
      }
    }
    if (EFFECTS) {
      scene.arena.weather = rnd(3) === 0 ? new Weather(1 + rnd(9), rnd(4) === 0 ? 0 : 1 + rnd(5), 5) : null;
      scene.arena.terrain = rnd(3) === 0 ? new Terrain(1 + rnd(4), 1 + rnd(5), 5) : null;
      if (rnd(4) === 0) {
        const tag = getArenaTag(ArenaTagType.TOXIC_SPIKES, 0, MoveId.NONE, (rnd(2) === 0 ? A : D).id,
          rnd(2) === 0 ? ArenaTagSide.PLAYER : ArenaTagSide.ENEMY) as any;
        tag.layers = 1 + rnd(2);
        scene.arena.tags.push(tag);
      }
      const has = (p: any, name: string) =>
        p.moveset.some((m: any) => m.getMove().attrs.some((a: any) => a.constructor.name === name));
      for (const p of [A, D]) {
        const other = p === A ? D : A;
        p.isTerastallized = rnd(6) === 0;
        if (rnd(4) === 0) {
          p.summonData.addedType = [PokemonType.GHOST, PokemonType.GRASS][rnd(2)];
        }
        // built directly (addTag would run onAdd): exposed, grounded, or charged by Wind Power
        const pre = [BattlerTagType.IGNORE_GHOST, BattlerTagType.IGNORE_DARK, BattlerTagType.IGNORE_FLYING, BattlerTagType.CHARGED];
        if (rnd(6) === 0) {
          p.summonData.tags.push(getBattlerTag(pre[rnd(pre.length)], 1, MoveId.NONE, other.id));
        }
        if ((has(p, "StatusIfBoostedAttr") || has(p, "AddBattlerTagIfBoostedAttr")) && rnd(2) === 0) {
          other.turnData.statStagesIncreased = true;
        }
        if (has(p, "FallDownAttr") && rnd(2) === 0) {
          if (rnd(2) === 0) {
            other.summonData.ability = AbilityId.LEVITATE;
          } else {
            other.summonData.types = [PokemonType.FLYING];
          }
        }
        if (has(p, "MissEffectAttr")) {
          const r = rnd(3);
          if (r === 0) {
            other.summonData.statStages[6] = 6; // evasion
          } else if (r === 1) {
            other.summonData.types = [PokemonType.GHOST];
          }
        }
        // Foresight and co. facing the tag they add (the add fails, and so does the message)
        if (has(p, "ExposedMoveAttr") && rnd(2) === 0) {
          const ex = p.moveset.map((m: any) => m.getMove().attrs.find((a: any) => a.constructor.name === "ExposedMoveAttr")).find(Boolean);
          other.summonData.tags.push(getBattlerTag(ex.tagType, 1, MoveId.NONE, p.id));
        }
        // Pay Day's amount grows with the wave set
        if (has(p, "MoneyAttr") && rnd(2) === 0) {
          battle.waveIndex = 1 + rnd(190);
        }
        // Reflect Type at a typeless target, with or without an added type
        if (has(p, "CopyTypeAttr") && rnd(2) === 0) {
          other.summonData.types = [PokemonType.UNKNOWN];
          if (rnd(2) === 0) {
            other.summonData.addedType = PokemonType.GHOST;
          }
        }
        if (has(p, "WaterShurikenMultiHitTypeAttr") && rnd(2) === 0) {
          p.species = speciesDataRegistry.getSpecies(SpeciesId.BATTLE_BOND_GRENINJA);
          p.formIndex = rnd(2);
          p.calculateStats();
          p.hp = Math.min(p.hp, p.getMaxHp());
        }
      }
    }
    if (HEAL) {
      for (const p of [A, D]) {
        // recharging: what the last turn's RECHARGING tag (2 turns, one lapsed at turn end) and the
        // command phases leave: the player keeps the placeholder in its queue and commands it, the
        // enemy's getNextMove() clears the queue and picks a move as usual
        const recharge = p.moveset.filter((m: any) => m.getMove().attrs.some((a: any) => a.constructor.name === "RechargeAttr"));
        if (!recharge.length || rnd(2) !== 0) {
          continue;
        }
        p.summonData.tags.push(getBattlerTag(BattlerTagType.RECHARGING, 1, recharge[0].moveId, p.id));
        (p.summonData.tags.at(-1) as any).turnCount = 1;
        p.summonData.moveHistory.push({ move: recharge[0].moveId, targets: [(p === A ? D : A).getBattlerIndex()], result: MoveResult.SUCCESS, useMode: MoveUseMode.NORMAL });
        if (p.isPlayer()) {
          p.summonData.moveQueue = [{ move: MoveId.NONE, targets: [], useMode: MoveUseMode.NORMAL }];
          const b = p.getBattlerIndex();
          battle.turnCommands[b] = {
            command: Command.FIGHT, cursor: -1,
            move: { move: MoveId.NONE, targets: getMoveTargets(p, MoveId.NONE).targets, useMode: MoveUseMode.NORMAL },
            args: [MoveUseMode.NORMAL, undefined], targets: [b],
          };
        }
      }
    }
    if (WEATHER) {
      const wt = rnd(8) !== 0 ? 1 + rnd(9) : 0; // WeatherType: Sunny ... Strong Winds
      const wturns = rnd(3) === 0 ? 0 : 1 + rnd(5);
      scene.arena.weather = wt ? new Weather(wt, wturns, wturns ? 5 : 0) : null;
      const tturns = rnd(3) === 0 ? 0 : 1 + rnd(5);
      scene.arena.terrain = rnd(2) === 0 ? new Terrain(1 + rnd(4), tturns, tturns ? 5 : 0) : null;
      for (const p of [A, D]) {
        p.isTerastallized = rnd(6) === 0;
        if (rnd(4) === 0) {
          // built directly (addTag would run onAdd): Protosynthesis's or Quark Drive's stat boost
          const tag = getBattlerTag(boostTags[rnd(2)], 1, MoveId.NONE, p.id) as any;
          tag.stat = [Stat.ATK, Stat.DEF, Stat.SPATK, Stat.SPDEF, Stat.SPD][rnd(5)];
          tag.multiplier = tag.stat === Stat.SPD ? 1.5 : 1.3;
          p.summonData.tags.push(tag);
        }
        if (BOOST) {
          if (rnd(4) !== 0) {
            p.summonData.ability = [AbilityId.PROTOSYNTHESIS, AbilityId.QUARK_DRIVE][rnd(2)];
          }
          if (rnd(2) === 0) {
            // Attack to Speed only: the stats the highest-stat pick compares
            p.summonData.statStages = p.summonData.statStages.map((v: number, i: number) =>
              i < 5 && rnd(3) === 0 ? [1, -1, 2, -2, 6, -6][rnd(6)] : v,
            );
          }
        }
      }
    }
    if (STATUS) {
      // which cure-on-entry abilities have cured before (their message names the last cure)
      for (const [, x] of healStatusAttrs) {
        x.statusHealed = rnd(3) === 0 ? statuses[rnd(statuses.length)] : undefined;
      }
      if (rnd(8) === 0) {
        D.summonData.ability = A.summonData.ability; // Synchronize against Synchronize
      }
      for (const p of [A, D]) {
        p.isTerastallized = rnd(5) === 0;
      }
      if (rnd(4) === 0) {
        const side = [ArenaTagSide.PLAYER, ArenaTagSide.ENEMY][rnd(2)];
        scene.arena.tags.push(getArenaTag(ArenaTagType.SAFEGUARD, 1 + rnd(5), MoveId.SAFEGUARD, A.id, side));
      }
    }
    if (PROBE) {
      // same levels half the time (OHKO moves), stat stages near the caps (Belly Drum and co.)
      if (rnd(2) === 0) {
        D.level = A.level;
        D.calculateStats();
        D.hp = Math.min(D.hp, D.getMaxHp());
      }
      for (const p of [A, D]) {
        if (rnd(3) === 0) {
          p.summonData.statStages = p.summonData.statStages.map(() => [0, 5, 6, -5, -6][rnd(5)]);
        }
        p.isTerastallized = rnd(4) === 0;
        p.turnData.acted = rnd(3) === 0;
        if (rnd(8) === 0) {
          p.summonData.ability = [AbilityId.PLUS, AbilityId.MINUS, AbilityId.LEVITATE][rnd(3)];
        }
        // battler tags as the conditions read them (built directly: addTag would run onAdd)
        for (let k = rnd(3); k > 0; k--) {
          const type = probeBattlerTags[rnd(probeBattlerTags.length)];
          if (p.getTag(type)) {
            continue;
          }
          const tag = getBattlerTag(type, 1 + rnd(4), MoveId.NONE, (p === A ? D : A).id) as any;
          if (type === BattlerTagType.STOCKPILING) {
            tag.stockpiledCount = 1 + rnd(3);
          }
          // what onAdd would have set (no extra draws): a doll at full HP, the seeder's slot
          if (type === BattlerTagType.SUBSTITUTE) {
            tag.hp = Math.floor(p.getMaxHp() / 4);
            tag.sourceInFocus = false;
          }
          if (type === BattlerTagType.SEEDED) {
            tag.sourceIndex = (p === A ? D : A).getBattlerIndex();
          }
          p.summonData.tags.push(tag);
        }
      }
      scene.arena.weather = rnd(3) === 0 ? new Weather(1 + rnd(9), 1 + rnd(5)) : null;
      scene.arena.terrain = rnd(3) === 0 ? new Terrain(1 + rnd(4), 1 + rnd(5)) : null;
      for (let k = rnd(3); k > 0; k--) {
        const type = probeArenaTags[rnd(probeArenaTags.length)];
        const side = [ArenaTagSide.BOTH, ArenaTagSide.PLAYER, ArenaTagSide.ENEMY][rnd(3)];
        if (!scene.arena.getTagOnSide(type, side)) {
          scene.arena.tags.push(getArenaTag(type, 1 + rnd(5), MoveId.NONE, A.id, side));
        }
      }
      if (SPROBE) {
        for (const p of [A, D]) {
          p.turnData.pendingStatus = rnd(8) === 0 ? statuses[rnd(statuses.length)] : StatusEffect.NONE;
        }
      }
      const before = dumpState();
      const probes: string[] = [];
      let threw = false;
      if (SPROBE) {
        const aa = (p: any) => `${L([...p.summonData.abilitiesApplied])}/${L([...p.waveData.abilitiesApplied])}`;
        for (const p of [A, D]) {
          for (const src of [p === A ? D : A, p, null]) {
            for (let eff = StatusEffect.POISON; eff <= StatusEffect.FAINT && !threw; eff++) {
              const ovr = rnd(4) === 0;
              const ign = rnd(4) === 0;
              let res: string;
              try {
                res = p.canSetStatus(eff, true, ovr, src, ign) ? "1" : "0";
              } catch (e: any) {
                res = "throw";
                threw = true;
              }
              const bs = battle.battleSeedState;
              probes.push(
                `SPROBE p=${p.getBattlerIndex()} src=${src ? src.getBattlerIndex() : -1} eff=${eff} ovr=${ovr ? 1 : 0}`
                  + ` ign=${ign ? 1 : 0} res=${res} aa=${aa(A)}|${aa(D)}`
                  + ` g=${rngState(Phaser.Math.RND.state())} b=${bs ? rngState(bs) : "none"}`,
              );
            }
          }
        }
        out.push(`CASE ${i}`, ...before, "RUN", ...probes, "END stop=probe");
        continue;
      }
      // first the StatStageChangeAttr conditions of the moves that have one, called directly
      // (seq=0, attr = the attr's index on the move; they draw no random number)
      for (const [u, t] of [[A, D], [D, A]]) {
        for (const m of statCondMoves) {
          m.attrs.forEach((a: any, k: number) => {
            if (threw || !hasStatCond(a)) {
              return;
            }
            let res: string;
            try {
              res = a.options.condition(u, t, m) ? "1" : "0";
            } catch (e: any) {
              res = "throw";
              threw = true;
            }
            const bs = battle.battleSeedState;
            probes.push(
              `PROBE u=${u.getBattlerIndex()} t=${t.getBattlerIndex()} move=${m.id} seq=0 attr=${k} res=${res}`
                + ` g=${rngState(Phaser.Math.RND.state())} b=${bs ? rngState(bs) : "none"}`,
            );
          });
        }
      }
      for (const [u, t] of [[A, D], [D, A]]) {
        for (const pmv of u.moveset) {
          for (const seq of [2, 3, 4]) {
            let res: string;
            try {
              res = pmv.getMove().applyConditions(u, t, seq) ? "1" : "0";
            } catch (e: any) {
              res = "throw";
              threw = true;
            }
            const bs = battle.battleSeedState;
            probes.push(
              `PROBE u=${u.getBattlerIndex()} t=${t.getBattlerIndex()} move=${pmv.moveId} seq=${seq} res=${res}`
                + ` g=${rngState(Phaser.Math.RND.state())} b=${bs ? rngState(bs) : "none"}`,
            );
            if (threw) {
              break;
            }
          }
          if (threw) {
            break;
          }
        }
        if (threw) {
          break;
        }
      }
      out.push(`CASE ${i}`, ...before, "RUN", ...probes, "END stop=probe");
      continue;
    }
    let before: string[];
    let run: { trace: string[]; stop: string };
    try {
      before = dumpState();
      run = await runTurn();
    } catch (e: any) {
      const key = String(e?.message ?? e).slice(0, 80);
      if (!thrown[key] && process.env.TURN_STACKS) {
        console.error("THROWN", e?.stack);
      }
      thrown[key] = (thrown[key] ?? 0) + 1;
      continue;
    }
    out.push(`CASE ${i}`, ...before, "RUN", `TRACE list=${L(run.trace)}`, ...dumpState(), ...queueLines(), `END stop=${run.stop}`);
  }
  if (STAT) {
    out.push(`# stat condition moves: ${statCondMoves.map((m: any) => m.id).join(",")}`);
  }
  out.push(`# upstream threw (case left out): ${JSON.stringify(thrown)}`);
  fs.writeFileSync(OUT, out.join("\n") + "\n");
}, 3_600_000);
