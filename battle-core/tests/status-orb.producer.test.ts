/* Card 104: additive Toxic/Flame Orb full-turn producer.
 * Uses the established turn driver/serialization, real pinned TurnEndPhase,
 * actual item instances and status/ability dispatch. Presentation is intercepted
 * as in the established producer; item behavior is never mocked.
 * Node 22; STATUSORB_N=2000 STATUSORB_SEED=777 STATUSORB_OUT=<trace>.
 */
import { BattleScene } from "#app/battle-scene";
import { getPokemonNameWithAffix } from "#app/messages";
import { getGameMode } from "#app/game-mode";
import { settings } from "#app/global-settings-manager";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allAbilities, allMoves, modifierTypes } from "#data/data-lists";
import { EntryHazardTag, getArenaTag } from "#data/arena-tag";
import { getBattlerTag, TrappedTag } from "#data/battler-tags";
import { Status } from "#data/status-effect";
import { Terrain, TerrainType } from "#data/terrain";
import { Weather } from "#data/weather";
import { AbilityId } from "#enums/ability-id";
import { BattleStyle } from "#enums/battle-style";
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
import { TrainerType } from "#enums/trainer-type";
import { UiMode } from "#enums/ui-mode";
import { WeatherType } from "#enums/weather-type";
import { BerryType } from "#enums/berry-type";
import { BerryModifierType } from "#modifiers/modifier-type";
import { getMoveTargets } from "#moves/move-utils";
import { healBlockedMoves } from "#moves/invalid-moves";
import { Pokemon } from "#field/pokemon";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import { PartyOption } from "#ui/party-ui-handler";
import i18next from "i18next";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";
it("bounded status orb turn producer", async () => {
    for (const [key, value] of Object.entries(process.env))
        if (key.startsWith("TURN_") && value === "1")
            throw new Error(`unapproved mode ${key}`);
    if (!process.versions.node.startsWith("22.")) throw new Error("Node 22 required");
    const N = Number(process.env.STATUSORB_N ?? 2000);
    if (!Number.isSafeInteger(N) || N < 20) throw new Error("STATUSORB_N must be an integer >=20");
    const FUSION = false;
    const FORMS = false;
    const OUT = process.env.STATUSORB_OUT ?? "/tmp/pr-status-orb.txt";
    (Pokemon.prototype as any).hideInfo = async function () { };
    (Phaser.GameObjects.Sprite.prototype as any).setPosition = function () { return this; };
    const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
    game.override
        .battleStyle("single")
        .ability(AbilityId.BALL_FETCH)
        .enemyAbility(AbilityId.BALL_FETCH)
        .battleType(BattleType.WILD)
        .startingWave(3)
        .enemySpecies(SpeciesId.MAGIKARP);
    await game.classicMode.startBattle(SpeciesId.BULBASAUR);
    (BattleScene.prototype as any).randBattleSeedInt = function (this: any, range: number, min = 0) {
        return this.currentBattle?.randSeedInt(range, min);
    };
    const scene = game.scene as any;
    const battle = scene.currentBattle as any;
    const pm = scene.phaseManager as any;
    const A = game.field.getPlayerPokemon() as any;
    const D = game.field.getEnemyPokemon() as any;
    let seed = Number(process.env.STATUSORB_SEED ?? 12345);
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
    const withConditions = usable.filter((m: any) => m.conditions.length || m.conditionsSeq2.length || m.conditionsSeq3.length);
    const abilityIds = allAbilities.filter((a: any) => a && a.id !== AbilityId.NONE).map((a: any) => a.id);
    const boostTags = [BattlerTagType.PROTOSYNTHESIS, BattlerTagType.QUARK_DRIVE];
    const fieldSetMoves = [
        MoveId.SUNNY_DAY, MoveId.SUNNY_DAY, MoveId.ELECTRIC_TERRAIN, MoveId.ELECTRIC_TERRAIN, MoveId.RAIN_DANCE,
        MoveId.SANDSTORM, MoveId.SNOWSCAPE, MoveId.GRASSY_TERRAIN, MoveId.MISTY_TERRAIN, MoveId.PSYCHIC_TERRAIN,
    ];
    const isA = (o: any, names: string[]) => {
        for (let c = o?.constructor; c && c !== Object; c = Object.getPrototypeOf(c)) {
            if (names.includes(c.name)) {
                return true;
            }
        }
        return false;
    };
    const statMoves = usable.filter((m: any) => m.attrs.some((a: any) => a.constructor.name === "StatStageChangeAttr"));
    const hasStatCond = (a: any) => a.constructor.name === "StatStageChangeAttr" && a.options.condition.toString() !== "() => true";
    const statCondMoves = usable.filter((m: any) => m.attrs.some(hasStatCond));
    const statAbilityIds = allAbilities
        .filter((a: any) => a?.attrs.some((x: any) => isA(x, [
        "StatStageChangeMultiplierAbAttr",
        "ProtectStatAbAttr",
        "ConditionalUserFieldProtectStatAbAttr",
        "ReflectStatStageChangeAbAttr",
        "StatStageChangeCopyAbAttr",
        "PostStatStageChangeAbAttr",
    ])))
        .map((a: any) => a.id)
        .concat([AbilityId.INFILTRATOR, AbilityId.PLUS, AbilityId.MINUS, AbilityId.LEVITATE]);
    const statusSetMoves = usable.filter((m: any) => m.attrs.some((a: any) => ["StatusEffectAttr", "MultiStatusEffectAttr"].includes(a.constructor.name)));
    const thawMoves = usable.filter((m: any) => m.attrs.some((a: any) => a.selfTarget && a.constructor.name === "HealStatusEffectAttr" && a.isOfEffect(StatusEffect.FREEZE)));
    const statusAbilityIds = allAbilities
        .filter((a: any) => a?.attrs.some((x: any) => isA(x, [
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
    ])))
        .map((a: any) => a.id)
        .concat([AbilityId.LEVITATE]);
    const contactAttacks = attacks.filter((m: any) => m.flags & 1);
    const onHitAbilityIds = allAbilities
        .filter((a: any) => a?.attrs.some((x: any) => isA(x, ["PostDefendAbAttr", "PostAttackAbAttr", "PostReceiveCritStatStageChangeAbAttr", "BlockNonDirectDamageAbAttr"])))
        .map((a: any) => a.id)
        .concat([AbilityId.OVERCOAT, AbilityId.SHIELD_DUST, AbilityId.LONG_REACH]);
    const chargeMoves = usable.filter((m: any) => m.isChargingMove());
    const hitsTagMoves = usable.filter((m: any) => m.attrs.some((a: any) => ["HitsTagAttr", "HitsTagForDoubleDamageAttr"].includes(a.constructor.name)));
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
    const tagMoves = usable.filter((m: any) => m.attrs.some((a: any) => isA(a, ["AddBattlerTagAttr"]) && portedTags.includes(a.tagType))
        || m.id === MoveId.CURSE
        || m.id === MoveId.DESTINY_BOND
        || m.id === MoveId.SPIT_UP
        || m.id === MoveId.SWALLOW);
    const tagReaders = attacks.filter((m: any) => m.type === PokemonType.ELECTRIC
        || m.type === PokemonType.FIRE
        || m.type === PokemonType.GROUND
        || m.attrs.some((a: any) => ["HitsTagForDoubleDamageAttr", "WeightPowerAttr", "CompareWeightPowerAttr"].includes(a.constructor.name))).concat(usable.filter((m: any) => m.hasFlag(MoveFlags.SOUND_BASED)
        || healBlockedMoves.has(m.id)
        || (m.category !== MoveCategory.STATUS && m.accuracy > 0 && m.accuracy < 80)));
    const tagAbilityIds = [
        AbilityId.STEADFAST, AbilityId.INNER_FOCUS, AbilityId.OWN_TEMPO, AbilityId.SHIELD_DUST, AbilityId.SERENE_GRACE,
        AbilityId.NO_GUARD, AbilityId.PERISH_BODY, AbilityId.CURSED_BODY, AbilityId.CUTE_CHARM, AbilityId.OBLIVIOUS,
        AbilityId.AROMA_VEIL, AbilityId.MAGIC_BOUNCE, AbilityId.MAGIC_GUARD, AbilityId.SIMPLE, AbilityId.CONTRARY,
        AbilityId.TRUANT, AbilityId.SLOW_START, AbilityId.STURDY, AbilityId.FLASH_FIRE, AbilityId.GORILLA_TACTICS,
        AbilityId.SUPREME_OVERLORD, AbilityId.MOTOR_DRIVE, AbilityId.SAP_SIPPER, AbilityId.LIGHTNING_ROD, AbilityId.STORM_DRAIN,
    ];
    const healMoves = usable.filter((m: any) => m.attrs.some((a: any) => ["RecoilAttr", "HitHealAttr", "RechargeAttr"].includes(a.constructor.name)));
    const healAbilityIds = allAbilities
        .filter((a: any) => a?.attrs.some((x: any) => isA(x, ["BlockRecoilDamageAttr", "BlockNonDirectDamageAbAttr", "ReverseDrainAbAttr", "PostTurnStatusHealAbAttr", "TypeImmunityHealAbAttr"])))
        .map((a: any) => a.id);
    const fieldSetters = ["WeatherChangeAttr", "ClearWeatherAttr", "TerrainChangeAttr", "ClearTerrainAttr"];
    const fieldReaders = [
        MoveId.EXPANDING_FORCE, MoveId.RISING_VOLTAGE, MoveId.MISTY_EXPLOSION, MoveId.GRASSY_GLIDE, MoveId.PSYBLADE,
        MoveId.HYDRO_STEAM,
    ];
    const typeShifters = [
        MoveId.HIDDEN_POWER, MoveId.REVELATION_DANCE, MoveId.TERA_BLAST, MoveId.RAGING_BULL, MoveId.IVY_CUDGEL,
    ];
    const weatherMoves = usable.filter((m: any) => {
        const names = [...m.attrs, ...(m.chargeAttrs ?? [])].map((a: any) => a.constructor.name);
        if (names.some((n: string) => fieldSetters.includes(n))) {
            return false;
        }
        return (m.type === PokemonType.WATER
            || m.type === PokemonType.FIRE
            || names.some((n: string) => /Weather|Terrain|Storm|Thunder|Blizzard|SemiInvulnerable/.test(n))
            || names.includes("ConfuseAttr")
            || fieldReaders.includes(m.id)
            || typeShifters.includes(m.id));
    });
    const weatherAbilityIds = allAbilities
        .filter((a: any) => a?.attrs.some((x: any) => isA(x, [
        "SuppressWeatherEffectAbAttr", "PreWeatherDamageAbAttr", "PostWeatherLapseAbAttr", "PostWeatherChangeAbAttr",
        "PostTerrainChangeAbAttr", "TerrainEventTypeChangeAbAttr",
    ])))
        .map((a: any) => a.id)
        .concat([
        AbilityId.MAGIC_GUARD, AbilityId.LEVITATE, AbilityId.SWIFT_SWIM, AbilityId.CHLOROPHYLL, AbilityId.SAND_RUSH,
        AbilityId.SLUSH_RUSH, AbilityId.SAND_FORCE, AbilityId.HYDRATION, AbilityId.LEAF_GUARD, AbilityId.SURGE_SURFER,
        AbilityId.NORMALIZE, AbilityId.LIQUID_VOICE,
    ]);
    const trapTags = [
        BattlerTagType.BIND, BattlerTagType.WRAP, BattlerTagType.FIRE_SPIN, BattlerTagType.WHIRLPOOL, BattlerTagType.CLAMP,
        BattlerTagType.SAND_TOMB, BattlerTagType.MAGMA_STORM, BattlerTagType.SNAP_TRAP, BattlerTagType.THUNDER_CAGE,
        BattlerTagType.INFESTATION,
    ];
    const trapMoves = usable.filter((m: any) => m.attrs.some((a: any) => ["TrapAttr", "RemoveBattlerTagAttr", "RemoveArenaTrapAttr", "RemoveScreensAttr", "RemoveArenaTagsAttr"].includes(a.constructor.name)));
    const trapArenaTags = [
        ArenaTagType.SPIKES, ArenaTagType.TOXIC_SPIKES, ArenaTagType.STEALTH_ROCK, ArenaTagType.STICKY_WEB,
        ArenaTagType.REFLECT, ArenaTagType.LIGHT_SCREEN, ArenaTagType.AURORA_VEIL, ArenaTagType.MIST, ArenaTagType.SAFEGUARD,
    ];
    const protectMoves = usable.filter((m: any) => m.attrs.some((a: any) => ["ProtectAttr", "AddSubstituteAttr", "RemoveAllSubstitutesAttr"].includes(a.constructor.name))
        || [MoveId.LEECH_SEED, MoveId.RAPID_SPIN, MoveId.MORTAL_SPIN, MoveId.FEINT].includes(m.id));
    const protectHitters = usable.filter((m: any) => m.attrs.some((a: any) => a.constructor.name === "MultiHitAttr")
        || (m.flags & (MoveFlags.SOUND_BASED | MoveFlags.IGNORE_SUBSTITUTE | MoveFlags.IGNORE_PROTECT)) !== 0
        || (m.category === MoveCategory.STATUS && [MoveTarget.NEAR_OTHER, MoveTarget.NEAR_ENEMY, MoveTarget.ALL_NEAR_ENEMIES].includes(m.moveTarget)));
    const protectAbilityIds = [
        AbilityId.INFILTRATOR, AbilityId.UNSEEN_FIST, AbilityId.LONG_REACH, AbilityId.LIQUID_OOZE, AbilityId.MAGIC_GUARD,
        AbilityId.SKILL_LINK, AbilityId.SAP_SIPPER, AbilityId.ROUGH_SKIN, AbilityId.SOUNDPROOF,
    ];
    const arenaMoves = usable.filter((m: any) => m.attrs.some((a: any) => [
        "AddArenaTagAttr", "AddArenaTrapTagAttr", "AddArenaTrapTagHitAttr", "SwapArenaTagsAttr", "RemoveArenaTrapAttr",
        "RemoveScreensAttr", "RemoveArenaTagsAttr",
    ].includes(a.constructor.name)));
    const arenaHitters = usable.filter((m: any) => m.priority > 0
        || [MoveTarget.ALL_NEAR_ENEMIES, MoveTarget.ALL_NEAR_OTHERS, MoveTarget.ALL_ENEMIES, MoveTarget.ALL_OTHERS].includes(m.moveTarget)
        || (m.category === MoveCategory.STATUS && [MoveTarget.NEAR_OTHER, MoveTarget.NEAR_ENEMY].includes(m.moveTarget))
        || m.attrs.some((a: any) => a.constructor.name === "ConfuseAttr")
        || (m.flags & MoveFlags.GRAVITY) !== 0
        || (m.category === MoveCategory.STATUS
            && ([MoveTarget.ALL, MoveTarget.USER_AND_ALLIES, MoveTarget.USER_OR_NEAR_ALLY].includes(m.moveTarget)
                || (m.flags & MoveFlags.IGNORE_PROTECT) !== 0)));
    const arenaAbilityIds = [
        AbilityId.INFILTRATOR, AbilityId.PRANKSTER, AbilityId.GALE_WINGS, AbilityId.TRIAGE, AbilityId.WIND_RIDER,
        AbilityId.WIND_POWER, AbilityId.LEVITATE, AbilityId.MAGIC_GUARD, AbilityId.SERENE_GRACE, AbilityId.OWN_TEMPO,
    ];
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
    const calledMoves = [
        MoveId.METRONOME, MoveId.MIRROR_MOVE, MoveId.COPYCAT, MoveId.SLEEP_TALK, MoveId.ASSIST, MoveId.NATURE_POWER,
        MoveId.INSTRUCT,
    ].map(id => allMoves[id]);
    const biomeIds = Object.values(BiomeId).filter(v => typeof v === "number") as number[];
    const headerIds = [MoveId.FOCUS_PUNCH, MoveId.SHELL_TRAP, MoveId.BEAK_BLAST];
    const headerMoves = headerIds.map(id => allMoves[id]);
    const headerHitters = usable.filter((m: any) => m.priority > 0
        || m.attrs.some((a: any) => ["MultiHitAttr", "ProtectAttr", "AddSubstituteAttr"].includes(a.constructor.name))
        || (m.category === MoveCategory.STATUS && [MoveTarget.NEAR_OTHER, MoveTarget.NEAR_ENEMY].includes(m.moveTarget)));
    const delayedMoves = [MoveId.FUTURE_SIGHT, MoveId.DOOM_DESIRE, MoveId.WISH].map(id => allMoves[id]);
    const protectOnly = usable.filter((m: any) => m.attrs.some((a: any) => a.constructor.name === "ProtectAttr"));
    const switchAttrNames = ["ForceSwitchOutAttr", "ChillyReceptionAttr", "PartingShotAttr"];
    const switchMoves = usable.filter((m: any) => m.attrs.some((a: any) => switchAttrNames.includes(a.constructor.name)));
    const switchOthers = [MoveId.PROTECT, MoveId.DETECT, MoveId.SUBSTITUTE, MoveId.MIST].map(id => allMoves[id]);
    const switchAbilityIds = [
        AbilityId.SUCTION_CUPS, AbilityId.GUARD_DOG, AbilityId.WIMP_OUT, AbilityId.EMERGENCY_EXIT, AbilityId.SOUNDPROOF,
        AbilityId.INFILTRATOR, AbilityId.CONTRARY, AbilityId.CLEAR_BODY, AbilityId.WHITE_SMOKE, AbilityId.FULL_METAL_BODY,
        AbilityId.HYPER_CUTTER, AbilityId.ROUGH_SKIN,
    ];
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
    const healStatusAttrs: [
        number,
        any
    ][] = allAbilities
        .filter((a: any) => a)
        .sort((a: any, b: any) => a.id - b.id)
        .flatMap((a: any) => a.attrs.filter((x: any) => x.constructor.name === "PostSummonHealStatusAbAttr").map((x: any) => [a.id, x]));
    const benchPool = (player: boolean) => Array.from({ length: 5 }, () => player
        ? scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5)
        : scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA), 5, TrainerSlot.NONE));
    const playerBench = benchPool(true);
    const enemyBench = [];
    let epicks: string[] = [];
    let picks: number[] = [];
    let pickAt = 0;
    const uiSetMode = scene.ui.setMode.bind(scene.ui);
    let checkYes = false;
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
    const L = (xs: any[]) => (xs.length ? xs.join(",") : "-");
    const num = (v: any) => (v === undefined || v === null ? "u" : String(v));
    const rngState = (st: string) => st.split(",").slice(1).join(",");
    const turnMove = (t: any) => `${t.move}:${L(t.targets).replace(/,/g, "/")}:${t.useMode}:${num(t.result)}`;
    const monLines = (p: any, b: string) => {
        const td = p.turnData;
        const sd = p.summonData;
        const st = p.status;
        if (st && st.effect === StatusEffect.NONE) {
            throw new Error("Status object with effect NONE");
        }
        return [
            [
                `MON b=${b} id=${p.id} sp=${p.species.speciesId} form=${p.formIndex}${p.fusionSpecies ? ` fsp=${p.fusionSpecies.speciesId} fform=${p.fusionFormIndex} fabil=${p.fusionAbilityIndex}` : ""} lv=${p.level} ivs=${L(p.ivs)} nat=${p.nature}`,
                `ab=${num(sd.ability)} abil=${p.getAbility().id} babil=${p.getAbility(true).id} passive=${p.passive ? 1 : 0} cpass=${p.customPokemonData.passive} gender=${p.gender}`,
                `friend=${p.friendship} hp=${p.hp} stats=${L(p.stats)} st=${st ? st.effect : 0} toxic=${st ? st.toxicTurnCount : 0}`,
                `sleep=${num(st?.sleepTurnsRemaining)} freeze=${num(st?.freezeTurnsRemaining)} stages=${L(sd.statStages)}`,
                `tera=${p.isTerastallized ? 1 : 0} teratype=${p.teraType} stellar=${L([...p.stellarTypesBoosted].sort((x, y) => x - y))}`,
                `boss=${p.isBoss() ? 1 : 0} shiny=${p.isShiny(true) ? 1 : 0} onfield=${p.isOnField() ? 1 : 0} out=${p.switchOutStatus ? 1 : 0}`,
                `shc=${sd.hitCount} types=${L(sd.types)} added=${sd.addedType ?? 0} abilsup=${sd.abilitySuppressed ? 1 : 0}`,
                `tsd=${p.tempSummonData.turnCount} tswd=${p.tempSummonData.waveTurnCount}`,
                `aas=${L([...sd.abilitiesApplied])} aaw=${L([...p.waveData.abilitiesApplied])}`
                    + (p.battleData.hasEatenBerry ? " eb=1" : "")
                    + (p.trainerSlot ? ` tslot=${p.trainerSlot}` : "")
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
            ...sd.tags.map((t: any) => `BTAG b=${b} type=${t.tagType} turns=${t.turnCount} smove=${t.sourceMove ?? 0} sid=${t.sourceId ?? -1}`
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
                    : "")),
        ];
    };
    const dumpState = (before = false) => {
        const lines: string[] = [];
        const bs = battle.battleSeedState;
        lines.push(`RNG g=${rngState(Phaser.Math.RND.state())} b=${bs ? rngState(bs) : "none"}`);
        const fh = (h: any[]) => L(h.map((e: any) => `${e.pokemon.id}:${e.turn}`));
        lines.push([
            `BATTLE turn=${battle.turn} wave=${battle.waveIndex} bseed=${battle.battleSeed} wseed=${scene.waveSeed}`,
            `lastmove=${battle.lastMove ?? 0} efaints=${battle.enemyFaints} pfaints=${scene.arena.playerFaints}`,
            `pfh=${fh(battle.playerFaintsHistory)} efh=${fh(battle.enemyFaintsHistory)}`,
            `lpi=${num(battle.lastPlayerInvolved)} lei=${num(battle.lastEnemyInvolved)}`,
            `participants=${L([...battle.playerParticipantIds].sort((x: number, y: number) => x - y))}`,
            `loot=${battle.postBattleLoot.length} score=${battle.battleScore} money=${battle.moneyScattered} type=${battle.battleType} double=${battle.double ? 1 : 0}`,
            `mel=${scene.getMaxExpLevel()} bar=${scene.abilityBar.isVisible() ? 1 : 0} ball=${num(battle.lastUsedPokeball)}`,
            `gm=${scene.gameMode.modeId}`,
        ].join(" "));
        const w = scene.arena.weather;
        const t = scene.arena.terrain;
        lines.push(`ARENA weather=${w ? w.weatherType : 0} wturns=${w ? w.turnsLeft : 0} wmax=${w ? w.maxDuration : 0}`
            + ` terrain=${t ? t.terrainType : 0} tturns=${t ? t.turnsLeft : 0} tmax=${t ? t.maxDuration : 0}`
            + ` ignab=${scene.arena.ignoreAbilities ? 1 : 0} ignsrc=${num(scene.arena.ignoringEffectSource)}`
            + ` biome=${scene.arena.biomeId}`);
        for (const at of scene.arena.tags) {
            lines.push(`ATAG type=${at.tagType} side=${at.side} turns=${at.turnCount} max=${at.maxDuration} smove=${at.sourceMove ?? 0} sid=${at.sourceId ?? -1}`
                + (at instanceof EntryHazardTag ? ` layers=${at.layers}` : ""));
        }
        for (const pt of scene.arena.positionalTagManager.tags as any[]) {
            lines.push(`POS type=${pt.tagType} turns=${pt.turnCount} target=${pt.targetIndex} sid=${pt.sourceId ?? -1}`
                + ` smove=${pt.sourceMove ?? 0} heal=${pt.healHp ?? 0}`
                + (before && pt.wisherId !== undefined ? ` wisher=${pt.wisherId}` : ""));
        }
        for (const [side, party] of [[0, scene.getPlayerParty()], [1, scene.getEnemyParty()]] as [
            number,
            any[]
        ][]) {
            party.forEach((p: any, k: number) => {
                lines.push(...monLines(p, k === 0 ? String(side * 2) : `${side ? "e" : "p"}${k}`));
            });
        }
        for (const [side, list] of [[0, scene.modifiers], [1, scene.enemyModifiers]] as [
            number,
            any[]
        ][]) {
            for (const m of list) {
                lines.push(`MOD side=${side} cls=${m.constructor.name} pid=${m.pokemonId ?? 0} stack=${m.stackCount}`
                    + (m.berryType !== undefined ? ` btype=${m.berryType}` : "")
                    + (m.constructor.name === "EnemyAttackStatusEffectChanceModifier" ? ` effect=${m.effect}` : "")
                    + (m.constructor.name === "TurnStatusEffectModifier" ? ` effect=${m.getStatusEffect()}` : ""));
            }
        }
        for (const [side, party] of [[0, scene.getPlayerParty()], [1, scene.getEnemyParty()]] as [
            number,
            any[]
        ][]) {
            lines.push(`PARTY side=${side} list=${L(party.map((m: any) => m.id))}`);
        }
        lines.push(`PICK list=${L(picks)}`);
        if (battle.trainer) {
            const tr = battle.trainer;
            lines.push(`TRAINER partner=${tr.isPartner() ? 1 : 0} names=${JSON.stringify([tr.getName(TrainerSlot.TRAINER), tr.getName(TrainerSlot.TRAINER_PARTNER)])}`);
        }
        for (let k = 0; k < 4; k++) {
            const c = battle.turnCommands[k];
            if (c) {
                lines.push(`CMD b=${k} cmd=${c.command} move=${c.move?.move ?? 0} targets=${L(c.move?.targets ?? [])} mode=${c.move?.useMode ?? 0} skip=${c.skip ? 1 : 0}`
                    + ` cursor=${num(c.cursor)} baton=${c.args?.[0] === true ? 1 : 0}`);
            }
            const pc = battle.preTurnCommands[k];
            if (pc) {
                lines.push(`PRE b=${k} cmd=${pc.command} skip=${pc.skip ? 1 : 0}`);
            }
        }
        const healed = healStatusAttrs.filter(([, x]) => x.statusHealed).map(([id, x]) => `${id}:${x.statusHealed}`);
        if (healed.length) {
            lines.push(`SESSION healed=${L(healed)}`);
        }
        const sel = [MoveId.SLEEP_TALK, MoveId.ASSIST]
            .map(id => [id, (allMoves[id].getAttrs("RandomMovesetMoveAttr")[0] as any).selectedMove])
            .filter(([, v]) => v);
        if (sel.length) {
            lines.push(`CALLSEL list=${L(sel.map(([id, v]) => `${id}:${v}`))}`);
        }
        return lines;
    };
    const afterBattle = new Set([
        "TrainerVictoryPhase", "GameOverPhase", "EggLapsePhase", "ModifierRewardPhase", "SelectModifierPhase",
        "AddEnemyBuffModifierPhase", "SelectBiomePhase", "NewBattlePhase",
    ]);
    const queueLines = () => {
        const tree = pm.phaseQueue;
        const nm = (ph: any) => (ph.phaseName === "DynamicPhaseMarker" ? `Marker:${ph.phaseType}` : ph.phaseName);
        const levels = tree.levels.map((lv: any[]) => L(lv.filter((ph: any) => !afterBattle.has(ph.phaseName)).map(nm)));
        const dyn: string[] = [];
        for (const [name, q] of pm.dynamicQueueManager.dynamicPhaseMap) {
            if (q.queue.length) {
                dyn.push(`${name}:${q.queue.map((ph: any) => ph.battlerIndex).join("/")}`);
            }
        }
        dyn.sort();
        return [`QUEUE levels=${levels.join("|")} deferred=${tree.deferredActive ? 1 : 0} dyn=${L(dyn)}`];
    };
    let started = 0;
    pm.startCurrentPhase = () => {
        started++;
    };
    const boundary = new Set([
        "TurnInitPhase", "GameOverPhase", "BattleEndPhase", "SelectModifierPhase",
    ]);
    scene.applyPartyExp = () => { };
    const phaseTag = (ph: any) => {
        const b = ph.battlerIndex;
        return typeof ph.getPokemon === "function" && typeof b === "number" ? `${ph.phaseName}:${b}` : ph.phaseName;
    };
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
    let shown: string[] = [];
    const ui = scene.ui as any;
    const uiShowText = ui.showText.bind(ui);
    ui.showText = (text: string, ...rest: any[]) => {
        const prompt = rest[3];
        if (!(prompt && String(text).indexOf("$") > -1)) {
            shown.push(String(text));
        }
        return uiShowText(text, ...rest);
    };
    const uiShowDialogue = ui.showDialogue.bind(ui);
    ui.showDialogue = (text: string, name: string, ...rest: any[]) => {
        shown.push(`${name}: ${text}`);
        return uiShowDialogue(text, name, ...rest);
    };
    const queueStart = () => {
        if (battle.battleType === BattleType.TRAINER) {
            pm.unshiftNew("SummonPhase", 0, false);
        }
        scene.getEnemyField().forEach((p: any, e: number) => {
            if (p.isShiny(true)) {
                pm.unshiftNew("ShinySparklePhase", 2 + e);
            }
        });
        const allowed = scene.getPokemonAllowedInBattle();
        if (!allowed[0].isOnField()) {
            pm.pushNew("SummonPhase", 0);
        }
        if (allowed.length > 1 && allowed[1].isOnField()) {
            pm.pushNew("ReturnPhase", 1);
        }
        pm.pushNew("ToggleDoublePositionPhase", false);
        if (battle.battleType !== BattleType.TRAINER && (battle.waveIndex > 1 || !scene.gameMode.isDaily) && allowed.length > 1) {
            pm.pushNew("CheckSwitchPhase", 0, battle.double);
        }
        pm.pushNew("InitEncounterPhase");
    };
    const runTurn = async () => {
        const trace: string[] = [];
        shown = [];
        {
            pm.pushNew("TurnStartPhase");
        }
        pm.shiftPhase();
        for (let guard = 0;; guard++) {
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
    const statuses = [StatusEffect.BURN, StatusEffect.PARALYSIS, StatusEffect.POISON, StatusEffect.TOXIC, StatusEffect.SLEEP, StatusEffect.FREEZE];
    const setup = (p: any) => {
        p.resetSummonData();
        p.resetTurnData();
        p.resetBattleAndWaveData();
        p.switchOutStatus = false;
        if (!p.isOnField()) {
            scene.field.add(p);
        }
        const formSpeciesIds = [
            SpeciesId.AEGISLASH,
            SpeciesId.DARMANITAN,
            SpeciesId.GALAR_DARMANITAN,
            SpeciesId.MINIOR,
            SpeciesId.MORPEKO,
        ];
        const sp = speciesDataRegistry.getSpecies(FORMS ? formSpeciesIds[rnd(formSpeciesIds.length)] : ids[rnd(ids.length)]);
        p.species = sp;
        p.formIndex = sp.forms.length ? rnd(sp.forms.length) : 0;
        if (FUSION && rnd(2) === 0) {
            const fsp = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
            p.fusionSpecies = fsp;
            p.fusionFormIndex = fsp.forms.length ? rnd(fsp.forms.length) : 0;
            p.fusionAbilityIndex = rnd(3);
        }
        else {
            p.fusionSpecies = null;
        }
        p.level = 1 + rnd(100);
        p.ivs = [0, 0, 0, 0, 0, 0].map(() => rnd(32));
        p.nature = rnd(25);
        p.passive = false;
        p.customPokemonData.passive = AbilityId.BALL_FETCH;
        p.summonData.ability = FORMS
            ? p.getAbility(true).id
            :
                (AbilityId.BALL_FETCH);
        p.gender = rnd(3) - 1;
        p.friendship = rnd(256);
        p.isTerastallized = false;
        p.teraType = rnd(19);
        p.stellarTypesBoosted = [];
        p.calculateStats();
        p.generateName();
        p.hp = FORMS
            ? (rnd(2) === 0 ? p.getMaxHp() : 1 + rnd(Math.floor(p.getMaxHp() * 0.5)))
            : (rnd(3) === 0 ? p.getMaxHp() : 1 + rnd(p.getMaxHp()));
        p.status = null;
        const n = 1 + rnd(4);
        p.moveset = [];
        for (let k = 0; k < n; k++) {
            const m = rnd(10) < 7 ? attacks[rnd(attacks.length)] : usable[rnd(usable.length)];
            p.moveset.push(new PokemonMove(m.id, rnd(8) === 0 ? rnd(m.pp + 1) : 0));
        }
    };
    const commandMove = (p: any, move: MoveId) => {
        const slot = rnd(p.moveset.length);
        p.moveset[slot] = new PokemonMove(move);
        const turnMove = { move, targets: getMoveTargets(p, move).targets, useMode: MoveUseMode.NORMAL };
        battle.turnCommands[p.getBattlerIndex()] = p.isPlayer()
            ? { command: Command.FIGHT, cursor: slot, move: turnMove, args: [MoveUseMode.NORMAL, undefined] }
            : { command: Command.FIGHT, move: turnMove, skip: false };
    };
    const command = (p: any) => {
        const slots = p.moveset.map((_: any, k: number) => k).filter((k: number) => p.moveset[k].ppUsed < p.moveset[k].getMovePp());
        const moveId = slots.length ? p.moveset[slots[rnd(slots.length)]].moveId : MoveId.STRUGGLE;
        const targets = getMoveTargets(p, moveId).targets;
        const b = p.getBattlerIndex();
        if (p.isPlayer()) {
            battle.turnCommands[b] = { command: Command.FIGHT, cursor: 0, move: { move: moveId, targets, useMode: MoveUseMode.NORMAL }, args: [MoveUseMode.NORMAL, undefined] };
            battle.preTurnCommands[b] = { command: Command.FIGHT, targets: [b], skip: true };
        }
        else {
            battle.turnCommands[b] = { command: Command.FIGHT, move: { move: moveId, targets, useMode: MoveUseMode.NORMAL }, skip: false };
        }
    };
    const out: string[] = [];
    const applied = [0, 0], rejected = [0, 0];
    const stateCounts = Array(10).fill(0);
    const thrown: Record<string, number> = {};
    for (let i = 0; i < N; i++) {
        picks = [0, 1, 2].map(j => (Math.imul(3 * i + j + 1, 2654435761) >>> 0) % 60);
        pickAt = 0;
        pm.clearAllPhases();
        pm.phaseQueue.deferredActive = false;
        pm.dynamicQueueManager.clearLastTurnOrder();
        scene.abilityBar.setVisible(false);
        scene.tweens.add = (config: any) => { config.onComplete?.(); return {} as any; };
        if ((scene as any).ui?.messageHandler)
            (scene as any).ui.messageHandler.prompt = null;
        battle.battleScore = 0;
        battle.playerFaintsHistory.length = 0;
        battle.enemyFaintsHistory.length = 0;
        battle.enemyFaints = 0;
        scene.arena.playerFaints = 0;
        battle.playerParticipantIds.clear();
        scene.modifiers.length = 0;
        scene.enemyModifiers.length = 0;
        scene.arena.tags.length = 0;
        scene.arena.positionalTagManager.tags.length = 0;
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
        const benchSize = 1 + rnd(playerBench.length);
        for (const b of playerBench) {
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
            b.formIndex = 0;
            b.calculateStats();
            b.generateName();
            b.hp = rnd(4) === 0 ? 0 : b.getMaxHp();
            b.status = b.hp ? null : new Status(StatusEffect.FAINT);
            party.push(b);
        }
        battle.playerParticipantIds.add(A.id);
        const orb = i % 2;
        const state = Math.floor(i / 2) % 10;
        for (const p of [...party, ...scene.getEnemyParty()]) {
            p.species = speciesDataRegistry.getSpecies(SpeciesId.RATTATA);
            p.formIndex = 0;
            p.fusionSpecies = null;
            p.generateName();
            p.summonData.ability = AbilityId.NONE;
            p.customPokemonData.passive = AbilityId.NONE;
            p.passive = false;
            p.status = null;
            p.isTerastallized = false;
            p.summonData.abilitySuppressed = false;
            p.summonData.statStages.fill(0);
            p.moveset = [new PokemonMove(MoveId.SPLASH)];
            p.calculateStats();
            p.hp = p.getMaxHp();
        }
        for (const p of [A, D]) {
            if (state === 1)
                p.status = new Status(StatusEffect.PARALYSIS);
            if (state === 2)
                p.summonData.ability = orb ? AbilityId.WATER_VEIL : AbilityId.IMMUNITY;
            if (state === 3) {
                p.species = speciesDataRegistry.getSpecies(orb ? SpeciesId.CHARMANDER : SpeciesId.MAGNEMITE);
                p.generateName();
            }
            if (state === 4)
                p.summonData.ability = AbilityId.COMATOSE;
            if (state === 5) {
                p.summonData.ability = orb ? AbilityId.WATER_VEIL : AbilityId.IMMUNITY;
                p.summonData.abilitySuppressed = true;
            }
            if (state === 6)
                p.summonData.ability = AbilityId.SYNCHRONIZE;
            if (state === 7)
                p.summonData.ability = AbilityId.POISON_HEAL;
            p.calculateStats();
            p.hp = p.getMaxHp();
            const itemFunc = orb ? modifierTypes.FLAME_ORB : modifierTypes.TOXIC_ORB;
            const mod = itemFunc().withIdFromFunc(itemFunc).newModifier(p);
            if (state === 8)
                (mod as any).pokemonId = p.isPlayer() ? party[1].id : A.id;
            (p.isPlayer() ? scene.modifiers : scene.enemyModifiers).push(mod as any);
        }
        if (state === 9)
            scene.arena.terrain = new Terrain(TerrainType.MISTY, 5, 5);
        command(A);
        command(D);
        let before: string[];
        let run: {
            trace: string[];
            stop: string;
        };
        let texts: string[];
        try {
            before = dumpState(true);
            epicks = [];
            run = await runTurn();
            texts = shown;
        }
        catch (e: any) {
            const key = String(e?.message ?? e).slice(0, 80);
            if (!thrown[key] && process.env.TURN_STACKS) {
                console.error("THROWN", e?.stack);
            }
            thrown[key] = (thrown[key] ?? 0) + 1;
            continue;
        }
        stateCounts[state]++;
        const hits = run.trace.filter((p: string) => p.startsWith("ObtainStatusEffectPhase")).length;
        if (hits)
            applied[orb] += hits;
        else
            rejected[orb] += 2;
        out.push(`CASE ${i}`, ...before, "RUN", `TRACE list=${L(run.trace)}`, `TEXTS ${JSON.stringify(texts)}`, ...dumpState(), ...queueLines(), `END stop=${run.stop}`);
    }
    if (Object.keys(thrown).length)
        throw new Error(`upstream exceptions: ${JSON.stringify(thrown)}`);
    if (stateCounts.reduce((a, b) => a + b, 0) !== N)
        throw new Error("incomplete trace");
    out.unshift(`# STATUSORB_CONTRACT 1 ${N}`);
    out.push(`# STATUSORB_COMPLETE ${N}`);
    console.log(`STATUSORB cases=${N} applied=${applied.join("/")} rejected=${rejected.join("/")} states=${stateCounts.join("/")}`);
    if ([...applied, ...rejected].some(n => n === 0))
        throw new Error("missing orb applied/rejected coverage");
    fs.writeFileSync(OUT, out.join("\n") + "\n");
}, 3600000);
