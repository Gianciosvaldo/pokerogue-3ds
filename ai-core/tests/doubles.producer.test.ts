/*
 * Task 74: upstream half of ai-core's doubles suite. Additive: the established singles producer
 * (ai.producer.test.ts) and its comparator are unchanged. This file is that producer turned into a
 * double battle; the lines and blocks that differ are marked "DOUBLES". Copy into an upstream
 * checkout as test/tests/zz-dump/doubles.producer.test.ts and run with
 *   AIDBL_OUT=/abs/ai-doubles.txt AIDBL_N=2000 AIDBL_SEED=777 npx vitest run test/tests/zz-dump/doubles.producer.test.ts
 * (tools/doublesdiff.sh does both halves on Node 22).
 *
 * Each case builds a random state inside a real upstream double battle: two player Pokemon and two
 * enemies on the field. Wild half the time, a trainer's the other half (a boss trainer a third of
 * those, a double trainer whose Pokemon alternate between its two trainer slots three times in
 * four, instant Tera now and then) with 0 to 4 Pokemon on the enemy bench, some fainted. The four
 * field Pokemon get random species, levels, HP, statuses, stat stages and movesets, the singles
 * producer's mix plus spread moves, moves aimed at an ally and moves whose target is drawn at
 * random; a third an ability the AI reads, or one that reads the ally or the user's side (Friend
 * Guard, Plus, Minus, Queenly Majesty, the veils, Power Spot...), or any ability. Each enemy gets
 * the singles producer's restricting tags, traps, queued moves and Substitutes independently. One
 * case in ten has a fainted Pokemon in a field slot (as FaintPhase leaves it: off the field, no
 * replacement) and one in ten a Tatsugiri commanding from a Dondozo (Commander's COMMANDED tag) on
 * either side.
 *
 * It then runs upstream's own EnemyCommandPhase for each enemy that TurnInitPhase gives one to (an
 * active one), field index 0 then 1, as the turn does, and after each writes the enemy's command
 * (CMD, PRE), the switch counter, both RNG states, the enemy's move queue and the move scores
 * getNextMove logged. After both: the attr state Sleep Talk and Assist keep, the phases left
 * queued, the abilities applied and revealed on all four, and in a trainer battle, for each of the
 * two trainer slots, the switch-in scores and the party slot trainer.getNextSummonIndex picks.
 * The file starts with "# AI_DOUBLES_CONTRACT 1 <N>" and ends with "# AI_DOUBLES_COMPLETE <cases
 * written> <cases upstream threw on>", which build/doublestest checks before comparing anything.
 */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allAbilities, allMoves } from "#data/data-lists";
import { CommandedTag, getBattlerTag } from "#data/battler-tags";
import { EntryHazardTag, getArenaTag } from "#data/arena-tag";
import { Status } from "#data/status-effect";
import { Terrain } from "#data/terrain";
import { TrainerAI } from "#data/trainers/trainer-config";
import { Weather } from "#data/weather";
import { AbilityId } from "#enums/ability-id";
import { AiType } from "#enums/ai-type";
import { ArenaTagSide } from "#enums/arena-tag-side";
import { ArenaTagType } from "#enums/arena-tag-type";
import { BattleType } from "#enums/battle-type";
import { BattlerTagLapseType } from "#enums/battler-tag-lapse-type";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveCategory } from "#enums/move-category";
import { MoveId } from "#enums/move-id";
import { MoveResult } from "#enums/move-result";
import { MoveTarget } from "#enums/move-target";
import { MoveUseMode } from "#enums/move-use-mode";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { TeraAIMode } from "#enums/tera-ai-mode";
import { TrainerSlot } from "#enums/trainer-slot";
import { TrainerVariant } from "#enums/trainer-variant";
import { WeatherType } from "#enums/weather-type";
import { Trainer } from "#field/trainer";
import { getMoveTargets } from "#moves/move-utils";
import { PokemonMove } from "#moves/pokemon-move";
import { EnemyCommandPhase } from "#phases/enemy-command-phase";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("ai doubles producer", async () => {
  const N = Number(process.env.AIDBL_N ?? 2000);
  const OUT = process.env.AIDBL_OUT ?? "/tmp/pr-ai-doubles.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override
    .battleStyle("double") // DOUBLES
    .ability(AbilityId.BALL_FETCH)
    .enemyAbility(AbilityId.BALL_FETCH)
    .battleType(BattleType.WILD)
    .startingWave(3)
    .enemySpecies(SpeciesId.MAGIKARP);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR, SpeciesId.CHARMANDER); // DOUBLES

  // GameManager pins battle rolls to their maximum; put upstream's real body back.
  (BattleScene.prototype as any).randBattleSeedInt = function (this: any, range: number, min = 0) {
    return this.currentBattle?.randSeedInt(range, min);
  };
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  const pm = scene.phaseManager as any;
  // DOUBLES: the four Pokemon on the field
  if (!battle.double || battle.getBattlerCount() !== 2) {
    throw new Error("not a double battle");
  }
  const [A0, A1] = scene.getPlayerField() as any[];
  const [D0, D1] = scene.getEnemyField() as any[];
  if (!A0 || !A1 || !D0 || !D1) {
    throw new Error("the field does not have four Pokemon");
  }
  const fieldMons = [A0, A1, D0, D1];

  let seed = Number(process.env.AIDBL_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const ids = Object.keys(speciesDataRegistry.data).map(Number);
  const usable = allMoves.filter((m: any) => m && m.id !== MoveId.NONE && !m.isUnimplemented);
  const attacks = usable.filter((m: any) => m.category !== MoveCategory.STATUS);
  // DOUBLES: the moves whose targets doubles changes: spread moves, moves aimed at an ally (or that
  // default to it), and moves whose one target is drawn at random among the opponents
  const spreadTargets = [MoveTarget.ALL_NEAR_ENEMIES, MoveTarget.ALL_NEAR_OTHERS, MoveTarget.ALL_ENEMIES, MoveTarget.ALL_OTHERS];
  const spreadMoves = usable.filter((m: any) => spreadTargets.includes(m.moveTarget));
  const allyTargets = [MoveTarget.NEAR_ALLY, MoveTarget.ALLY, MoveTarget.USER_OR_NEAR_ALLY, MoveTarget.USER_AND_ALLIES];
  const allyMoves = usable.filter((m: any) => allyTargets.includes(m.moveTarget) || m.allyTargetDefault);
  const randomTargetMoves = usable.filter((m: any) => m.moveTarget === MoveTarget.RANDOM_NEAR_ENEMY);
  const abilityIds = allAbilities.filter((a: any) => a && a.id !== AbilityId.NONE).map((a: any) => a.id);
  // abilities the AI reads: the trappers and what escapes them, immunities and damage changers the
  // KO check and the scores go through, what the status and stat scores ask about
  const aiAbilityIds = [
    "ARENA_TRAP", "SHADOW_TAG", "MAGNET_PULL", "RUN_AWAY", "LEVITATE", "MAGIC_GUARD", "PRANKSTER",
    "SERENE_GRACE", "SHEER_FORCE", "SHIELD_DUST", "WONDER_GUARD", "STURDY", "MULTISCALE", "VOLT_ABSORB",
    "WATER_ABSORB", "FLASH_FIRE", "LIGHTNING_ROD", "STORM_DRAIN", "SAP_SIPPER", "MOTOR_DRIVE", "DRY_SKIN",
    "SCRAPPY", "NORMALIZE", "PIXILATE", "REFRIGERATE", "AERILATE", "GALVANIZE", "TINTED_LENS", "FILTER",
    "SOLID_ROCK", "THICK_FAT", "HEATPROOF", "LIMBER", "IMMUNITY", "INSOMNIA", "VITAL_SPIRIT", "OWN_TEMPO",
    "OBLIVIOUS", "INNER_FOCUS", "UNAWARE", "CONTRARY", "SIMPLE", "CLEAR_BODY", "HUGE_POWER", "GUTS",
    "TECHNICIAN", "SKILL_LINK", "ADAPTABILITY", "SOUNDPROOF", "BULLETPROOF", "OVERCOAT", "ROCK_HEAD",
    "RECKLESS", "PURIFYING_SALT", "COMATOSE", "SUCTION_CUPS", "GUARD_DOG", "EARTH_EATER", "WELL_BAKED_BODY",
    "TERA_SHELL", "PARENTAL_BOND", "MOLD_BREAKER", "TERAVOLT", "TURBOBLAZE", "LIQUID_OOZE", "POISON_HEAL",
    "SWIFT_SWIM", "CHLOROPHYLL", "UNBURDEN", "MISTY_SURGE", "PSYCHIC_SURGE", "AIR_LOCK", "CLOUD_NINE",
    "DESOLATE_LAND", "PRIMORDIAL_SEA", "SWEET_VEIL", "FLOWER_VEIL", "LEAF_GUARD", "SHED_SKIN", "HYDRATION",
    "TRIAGE", "GALE_WINGS", "ILLUSION",
  ]
    .map(n => (AbilityId as any)[n])
    .filter((v: any) => typeof v === "number");
  // DOUBLES: abilities that read the ally or the user's side (the KO check's Friend Guard, Plus and
  // Minus with ignoreAllyAbility, the side's Queenly Majesty in the effectiveness, the side's power
  // boosts, the veils in the status scores), as battle-core's doubles producer picks them, and
  // Commander
  const isA = (o: any, names: string[]) => {
    for (let c = o?.constructor; c && c !== Object; c = Object.getPrototypeOf(c)) {
      if (names.includes(c.name)) {
        return true;
      }
    }
    return false;
  };
  const allyAbilityIds = allAbilities
    .filter((a: any) =>
      a?.attrs.some((x: any) =>
        isA(x, [
          "AllyStatMultiplierAbAttr",
          "AlliedFieldDamageReductionAbAttr",
          "AllyMoveCategoryPowerBoostAbAttr",
          "UserFieldMoveTypePowerBoostAbAttr",
          "FieldPriorityMoveImmunityAbAttr",
          "RedirectMoveAbAttr",
          "ConditionalUserFieldBattlerTagImmunityAbAttr",
          "ConditionalUserFieldProtectStatAbAttr",
          "ConditionalUserFieldStatusEffectImmunityAbAttr",
          "UserFieldBattlerTagImmunityAbAttr",
          "UserFieldStatusEffectImmunityAbAttr",
          "PostTurnResetStatusAbAttr",
          "CopyFaintedAllyAbilityAbAttr",
          "CommanderAbAttr",
          "PostSummonAllyHealAbAttr",
        ]),
      ),
    )
    .map((a: any) => a.id)
    .concat([AbilityId.PLUS, AbilityId.MINUS, AbilityId.TELEPATHY, AbilityId.TRACE]);
  const statuses = [StatusEffect.BURN, StatusEffect.PARALYSIS, StatusEffect.POISON, StatusEffect.TOXIC, StatusEffect.SLEEP, StatusEffect.FREEZE];
  const restrictTags = [
    BattlerTagType.ENCORE, BattlerTagType.DISABLED, BattlerTagType.TAUNT, BattlerTagType.TORMENT,
    BattlerTagType.THROAT_CHOPPED, BattlerTagType.HEAL_BLOCK, BattlerTagType.GORILLA_TACTICS,
    BattlerTagType.IMPRISON, BattlerTagType.TRAPPED, BattlerTagType.ALWAYS_CRIT, BattlerTagType.FRENZY,
  ];
  // the moves whose attr objects hold session-wide state the scores read
  const multiStatusAttrs: [number, any][] = usable.flatMap((m: any) =>
    m.attrs.filter((a: any) => a.constructor.name === "MultiStatusEffectAttr").map((a: any) => [m.id, a]),
  );
  const variableHealAttrs: [number, any][] = usable.flatMap((m: any) =>
    m.attrs.filter((a: any) => a.constructor.name === "VariableHealAttr").map((a: any) => [m.id, a]),
  );
  const initialHeal = variableHealAttrs.map(([, a]) => a.healRatio);

  const playerBench = Array.from({ length: 3 }, () =>
    scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5),
  );
  // DOUBLES: up to 4 on the enemy bench (a party holds 6)
  const enemyBench = Array.from({ length: 4 }, () =>
    scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA), 5, TrainerSlot.TRAINER),
  );

  // ---- the state dump: battle-core's turntest records (see battle-core/tests/turntest.producer.test.ts) ----
  const L = (xs: any[]) => (xs.length ? xs.join(",") : "-");
  const num = (v: any) => (v === undefined || v === null ? "u" : String(v));
  const rngState = (st: string) => st.split(",").slice(1).join(",");
  const turnMove = (t: any) => `${t.move}:${L(t.targets).replace(/,/g, "/")}:${t.useMode}:${num(t.result)}`;
  const monLines = (p: any, b: string) => {
    const td = p.turnData;
    const sd = p.summonData;
    const st = p.status;
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
          + (t.tagType === BattlerTagType.SUBSTITUTE ? ` hp=${t.hp} focus=${t.sourceInFocus ? 1 : 0}` : "")
          + ([BattlerTagType.DISABLED, BattlerTagType.ENCORE, BattlerTagType.GORILLA_TACTICS].includes(t.tagType)
            ? ` mid=${t.moveId ?? 0}`
            : ""),
      ),
      // what the AI reads that the battle records don't hold
      `AIMON b=${b} type=${p.isPlayer() ? -1 : p.aiType} revealed=${p.waveData.abilityRevealed ? 1 : 0}`
        + ` iti=${p.isPlayer() ? -1 : p.initialTeamIndex} slot=${p.isPlayer() ? 0 : p.trainerSlot}`,
    ];
  };
  const commandLine = (k: number) => {
    const lines: string[] = [];
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
    return lines;
  };
  const commandLines = () => [0, 1, 2, 3].flatMap(commandLine);
  // the move Sleep Talk's or Assist's condition picked and no use has taken yet (kept on the attr
  // object across battles: a pick an earlier case's scores made is still there)
  const callSelLine = () => {
    const sel = [MoveId.SLEEP_TALK, MoveId.ASSIST]
      .map(id => [id, (allMoves[id].getAttrs("RandomMovesetMoveAttr")[0] as any).selectedMove])
      .filter(([, v]) => v);
    return `CALLSEL list=${L(sel.map(([id, v]) => `${id}:${v}`))}`;
  };
  const rngLine = () => {
    const bs = battle.battleSeedState;
    return `RNG g=${rngState(Phaser.Math.RND.state())} b=${bs ? rngState(bs) : "none"}`;
  };
  const dumpState = () => {
    const lines: string[] = [rngLine()];
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
    lines.push(...commandLines());
    const tr = battle.trainer;
    if (tr) {
      lines.push(`TRAINER boss=${tr.config.isBoss ? 1 : 0} double=${tr.isDouble() ? 1 : 0}`
        + ` tera=${tr.config.trainerAI.teraMode} instant=${L(tr.config.trainerAI.instantTeras)}`);
    }
    lines.push(`AI counter=${battle.enemySwitchCounter} skip=${battle.mysteryEncounter?.skipEnemyBattleTurns ? 1 : 0} seed=${scene.seed}`);
    const multi = multiStatusAttrs.filter(([, a]) => a.effect !== a.effects[0]).map(([id, a]) => `${id}:${a.effect}`);
    const heal = variableHealAttrs.filter(([, a]) => a.healRatio !== 1).map(([id, a]) => `${id}:${a.healRatio}`);
    lines.push(`SESS multi=${L(multi)} heal=${L(heal)}`);
    lines.push(callSelLine());
    return lines;
  };

  // ---- random states ----
  // moves whose scores read something rare (sun, a semi-invulnerable target, sleep, the first turn),
  // one pick in ten, so a random moveset meets those states often enough
  const focus = [
    "GROWTH", "GUST", "TWISTER", "THUNDER", "HURRICANE", "SKY_UPPERCUT", "EARTHQUAKE", "MAGNITUDE", "SURF",
    "WHIRLPOOL", "SMACK_DOWN", "FAKE_OUT", "FIRST_IMPRESSION", "SNORE", "SLEEP_TALK", "SUPER_FANG", "BRINE",
    "BATON_PASS", "REVIVAL_BLESSING", "SOLAR_BEAM", "WEATHER_BALL", "MOONLIGHT", "TRI_ATTACK", "SUCKER_PUNCH",
    // DOUBLES: scores and targets doubles changes (Flame Burst's ally score, the targets Expanding
    // Force and Tera Starstorm vary, healing an ally)
    "FLAME_BURST", "EXPANDING_FORCE", "TERA_STARSTORM", "HEAL_PULSE", "POLLEN_PUFF", "HELPING_HAND",
    "BEAT_UP", "FLORAL_HEALING", "DECORATE", "INSTRUCT",
  ].map(n => usable.find((m: any) => m.id === (MoveId as any)[n])).filter(m => m);
  // DOUBLES: two picks in ten a spread move, one aimed at an ally, one whose target is random
  const randomMoveset = (p: any) => {
    p.moveset = [];
    for (let k = 1 + rnd(4); k > 0; k--) {
      const r = rnd(10);
      const m = r === 0 ? focus[rnd(focus.length)]
        : r <= 2 ? spreadMoves[rnd(spreadMoves.length)]
        : r === 3 ? allyMoves[rnd(allyMoves.length)]
        : r === 4 ? randomTargetMoves[rnd(randomTargetMoves.length)]
        : rnd(2) === 0 ? attacks[rnd(attacks.length)] : usable[rnd(usable.length)];
      p.moveset.push(new PokemonMove(m.id, rnd(8) === 0 ? rnd(m.pp + 1) : 0, rnd(6) === 0 ? 1 + rnd(3) : 0));
    }
  };
  // DOUBLES: a third of the abilities from the ally pool
  const randomAbility = () =>
    rnd(3) === 0
      ? (rnd(2) === 0 ? aiAbilityIds[rnd(aiAbilityIds.length)] : allyAbilityIds[rnd(allyAbilityIds.length)])
      : rnd(3) === 0 ? abilityIds[rnd(abilityIds.length)] : AbilityId.BALL_FETCH;
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
    p.summonData.ability = randomAbility();
    p.gender = rnd(3) - 1;
    p.friendship = rnd(256);
    p.isTerastallized = rnd(8) === 0;
    p.teraType = rnd(19);
    p.stellarTypesBoosted = [];
    p.calculateStats();
    p.generateName();
    const r = rnd(4);
    p.hp = r === 0 ? p.getMaxHp() : r === 1 ? 1 + rnd(Math.max(1, Math.floor(p.getMaxHp() / 5))) : 1 + rnd(p.getMaxHp());
    p.status = null;
    if (rnd(5) === 0) {
      const s = statuses[rnd(statuses.length)];
      p.status = new Status(s, rnd(4), s === StatusEffect.SLEEP ? 1 + rnd(3) : 0, s === StatusEffect.FREEZE ? 1 + rnd(3) : undefined);
    }
    if (rnd(4) === 0) {
      p.summonData.statStages = p.summonData.statStages.map(() => [0, 0, 1, -1, 2, -2, 6, -6][rnd(8)]);
    }
    randomMoveset(p);
    p.tempSummonData.waveTurnCount = 1 + rnd(3); // Fake Out and First Impression read it
    if (rnd(4) === 0) {
      const pool = [...p.moveset.map((m: any) => m.moveId), MoveId.STRUGGLE];
      for (let k = 1 + rnd(3); k > 0; k--) {
        p.pushMoveHistory({
          move: pool[rnd(pool.length)],
          targets: [p.getBattlerIndex()],
          useMode: rnd(5) === 0 ? MoveUseMode.INDIRECT : MoveUseMode.NORMAL,
          result: rnd(4) === 0 ? MoveResult.FAIL : MoveResult.SUCCESS,
        });
      }
    }
    p.waveData.abilityRevealed = rnd(2) === 0;
  };
  // a bench Pokemon: off the field, fresh summon, turn and wave data, its own species, moves and HP
  const setupBench = (b: any) => {
    if (b.isOnField()) {
      scene.field.remove(b);
    }
    b.switchOutStatus = false;
    b.resetSummonData();
    b.resetTurnData();
    b.resetBattleAndWaveData();
    b.species = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
    b.formIndex = 0;
    b.fusionSpecies = null;
    b.level = 1 + rnd(100);
    b.ivs = [0, 0, 0, 0, 0, 0].map(() => rnd(32));
    b.nature = rnd(25);
    b.abilityIndex = rnd(3);
    b.passive = false;
    b.customPokemonData.passive = AbilityId.BALL_FETCH;
    // (the test's ability override beats customPokemonData.ability; summonData.ability beats both)
    b.summonData.ability = randomAbility();
    b.isTerastallized = false;
    b.calculateStats();
    b.generateName();
    b.hp = rnd(4) === 0 ? 0 : rnd(2) === 0 ? 1 + rnd(b.getMaxHp()) : b.getMaxHp();
    b.status = b.hp ? null : new Status(StatusEffect.FAINT);
    if (b.hp && rnd(5) === 0) {
      const s = statuses[rnd(statuses.length)];
      b.status = new Status(s, rnd(4), s === StatusEffect.SLEEP ? 1 + rnd(3) : 0, s === StatusEffect.FREEZE ? 1 + rnd(3) : undefined);
    }
    randomMoveset(b);
    b.waveData.abilityRevealed = rnd(4) === 0;
  };
  // DOUBLES: the tags the singles producer gives its enemy, for one enemy
  const enemyTags = (D: any, foe: any) => {
    // tags on the enemy the AI reads: move restrictions, traps, Laser Focus, a rampage
    if (rnd(3) === 0) {
      for (let k = 1 + rnd(2); k > 0; k--) {
        const type = restrictTags[rnd(restrictTags.length)];
        if (D.getTag(type)) {
          continue;
        }
        const tag = getBattlerTag(type, 1 + rnd(4), MoveId.NONE, foe.id) as any;
        if (type === BattlerTagType.DISABLED) {
          tag.moveId = D.moveset[rnd(D.moveset.length)].moveId;
        }
        if (type === BattlerTagType.GORILLA_TACTICS) {
          tag.moveId = D.moveset[rnd(D.moveset.length)].moveId;
        }
        if (type === BattlerTagType.ENCORE) {
          tag.moveId = D.moveset[rnd(D.moveset.length)].moveId;
          tag.lapse(D, BattlerTagLapseType.TURN_END);
          tag.turnCount = 1 + rnd(4);
        }
        if (type === BattlerTagType.IMPRISON && rnd(2) === 0) {
          // the imprisoning Pokemon shares a move with it
          foe.moveset[0] = new PokemonMove(D.moveset[rnd(D.moveset.length)].moveId);
        }
        if (type === BattlerTagType.TORMENT && rnd(2) === 0) {
          const m = D.moveset[rnd(D.moveset.length)].moveId;
          D.pushMoveHistory({ move: m, targets: [foe.getBattlerIndex()], useMode: MoveUseMode.NORMAL, result: MoveResult.SUCCESS });
        }
        D.summonData.tags.push(tag);
      }
    }
    // a queued move (a charging move's second turn, a rampage, a called move)
    if (rnd(10) === 0) {
      for (let k = 1 + rnd(2); k > 0; k--) {
        const move = rnd(2) === 0 ? D.moveset[rnd(D.moveset.length)].moveId : usable[rnd(usable.length)].id;
        D.summonData.moveQueue.push({
          move,
          targets: getMoveTargets(D, move).targets,
          useMode: [MoveUseMode.NORMAL, MoveUseMode.IGNORE_PP, MoveUseMode.INDIRECT, MoveUseMode.FOLLOW_UP][rnd(4)],
        });
      }
    }
  };

  const out: string[] = [`# AI_DOUBLES_CONTRACT 1 ${N}`]; // DOUBLES
  const thrown: Record<string, number> = {};
  let written = 0;
  let threwCount = 0;
  let logged: number[] | null = null;
  const realLog = console.log;
  for (let i = 0; i < N; i++) {
    pm.clearAllPhases();
    pm.phaseQueue.deferredActive = false;
    scene.abilityBar.setVisible(false);
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
    battle.mysteryEncounter = undefined;
    for (const p of fieldMons) {
      setup(p);
    }
    const [A, D] = [A0, D0];

    // the player's party: the two on the field and a bench of 0 to 3 (Revival Blessing and the
    // switching moves count them)
    const pparty = scene.getPlayerParty();
    pparty.length = 0;
    pparty.push(A0, A1); // DOUBLES
    for (const b of playerBench.slice(0, rnd(playerBench.length + 1))) {
      setupBench(b);
      pparty.push(b);
    }
    battle.playerParticipantIds.add(A0.id);
    battle.playerParticipantIds.add(A1.id); // DOUBLES

    // a wild battle or a trainer's
    const trainerCase = rnd(2) === 0;
    const eparty: any[] = [D0, D1]; // DOUBLES
    if (trainerCase) {
      const ai = new TrainerAI(rnd(6) === 0 ? TeraAIMode.SMART_TERA : TeraAIMode.NO_TERA);
      if (rnd(4) === 0) {
        for (let k = 1 + rnd(2); k > 0; k--) {
          ai.setInstantTera(rnd(6));
        }
      }
      const trainer = Object.create(Trainer.prototype);
      trainer.config = { isBoss: rnd(3) === 0, doubleOnly: false, trainerAI: ai };
      // DOUBLES: a double trainer three times in four (isDouble(): its Pokemon alternate between
      // the two trainer slots, as Trainer.genPartyMember gives them), else one trainer
      trainer.variant = rnd(4) === 0 ? TrainerVariant.DEFAULT : TrainerVariant.DOUBLE;
      battle.trainer = trainer;
      battle.battleType = BattleType.TRAINER;
      for (const b of enemyBench.slice(0, rnd(enemyBench.length + 1))) {
        setupBench(b);
        b.aiType = AiType.SMART;
        eparty.push(b);
      }
      if (eparty.length >= 4 && rnd(4) === 0) {
        // two bench Pokemon alike, so their switch-in scores tie and the session seed picks one
        const [x, y] = [eparty[2], eparty[3]]; // DOUBLES: the bench starts at 2
        y.species = x.species;
        y.formIndex = x.formIndex;
        y.level = x.level;
        y.ivs = [...x.ivs];
        y.nature = x.nature;
        y.abilityIndex = x.abilityIndex;
        y.summonData.ability = x.summonData.ability;
        y.moveset = x.moveset.map((m: any) => new PokemonMove(m.moveId, m.ppUsed, m.ppUp));
        y.calculateStats();
        y.hp = x.hp;
        y.status = x.status ? new Status(x.status.effect) : null;
      }
      eparty.forEach((p: any, k: number) => {
        p.trainerSlot = !trainer.isDouble() || !(k % 2) ? TrainerSlot.TRAINER : TrainerSlot.TRAINER_PARTNER; // DOUBLES
        p.initialTeamIndex = rnd(3) === 0 ? rnd(6) : k;
      });
      if (rnd(8) === 0) {
        // a Pokemon of this trainer's fainted this battle (a fainted Pokemon doesn't instantly Tera)
        const f = eparty[rnd(eparty.length)];
        battle.enemyFaintsHistory.push({ pokemon: f, turn: 1 });
      }
      battle.enemySwitchCounter = rnd(2) === 0 ? 0 : rnd(5);
    } else {
      battle.trainer = null;
      battle.battleType = BattleType.WILD;
      for (const p of eparty) {
        p.trainerSlot = TrainerSlot.NONE;
        p.initialTeamIndex = 0;
      }
      battle.enemySwitchCounter = 0;
    }
    battle.enemyParty.length = 0;
    battle.enemyParty.push(...eparty);
    // its constructor's AI type, now and then another
    for (const E of [D0, D1]) {
      E.aiType = rnd(6) === 0 ? rnd(3) : trainerCase ? AiType.SMART : AiType.SMART_RANDOM;
    }
    if (rnd(16) === 0) {
      battle.mysteryEncounter = { skipEnemyBattleTurns: true };
    }

    // DOUBLES: each enemy gets the singles producer's tags and queued moves, against a random foe
    enemyTags(D0, [A0, A1][rnd(2)]);
    enemyTags(D1, [A0, A1][rnd(2)]);
    // the player mid-Fly/Dig/Dive/Shadow Force: what Gust, Earthquake or Surf score against it
    for (const P of [A0, A1]) {
      if (rnd(16) === 0) {
        const semi = [BattlerTagType.FLYING, BattlerTagType.UNDERGROUND, BattlerTagType.UNDERWATER, BattlerTagType.HIDDEN];
        P.summonData.tags.push(getBattlerTag(semi[rnd(semi.length)], 1, MoveId.NONE, P.id));
      }
    }
    for (const p of fieldMons) {
      if (rnd(8) === 0) {
        const tag = getBattlerTag(BattlerTagType.SUBSTITUTE, 0, MoveId.SUBSTITUTE, p.id) as any;
        tag.hp = 1 + rnd(Math.max(1, Math.floor(p.getMaxHp() / 4)));
        tag.sourceInFocus = false;
        tag.sprite = scene.addFieldSprite(p.x, p.y, `pkmn${p.isPlayer() ? "__back" : ""}__sub`);
        p.summonData.tags.push(tag);
      }
    }
    // DOUBLES: a Tatsugiri commanding from inside its ally (CommanderAbAttr gives the ally a
    // CommandedTag with the Tatsugiri as its source), on either side
    if (rnd(10) === 0) {
      const side = rnd(2) === 0 ? [A0, A1] : [D0, D1];
      const k = rnd(2);
      const [tatsu, dozo] = [side[k], side[1 - k]];
      tatsu.species = speciesDataRegistry.getSpecies(SpeciesId.TATSUGIRI);
      tatsu.formIndex = rnd(3);
      tatsu.summonData.ability = AbilityId.COMMANDER;
      tatsu.calculateStats();
      tatsu.generateName();
      tatsu.hp = 1 + rnd(tatsu.getMaxHp());
      if (rnd(4) !== 0) {
        dozo.species = speciesDataRegistry.getSpecies(SpeciesId.DONDOZO);
        dozo.formIndex = 0;
        dozo.calculateStats();
        dozo.generateName();
        dozo.hp = 1 + rnd(dozo.getMaxHp());
      }
      dozo.summonData.tags.push(new CommandedTag(tatsu.id));
    }
    // DOUBLES: a Pokemon fainted in a field slot, as FaintPhase leaves it when nothing replaces it
    // (hp 0, FAINT, summon data reset, switched out and off the field)
    if (rnd(10) === 0) {
      const f = fieldMons[rnd(4)];
      f.hp = 0;
      f.status = new Status(StatusEffect.FAINT);
      f.resetSummonData();
      f.switchOutStatus = true;
      scene.field.remove(f);
    }
    // the arena: entry hazards (mostly on the enemy's side, where a switch-in meets them), Gravity,
    // Fairy Lock, weather and terrain
    if (rnd(3) === 0) {
      const hazards = [ArenaTagType.SPIKES, ArenaTagType.TOXIC_SPIKES, ArenaTagType.STEALTH_ROCK, ArenaTagType.STICKY_WEB];
      for (let k = 1 + rnd(3); k > 0; k--) {
        const type = hazards[rnd(hazards.length)];
        const side = rnd(4) === 0 ? ArenaTagSide.PLAYER : ArenaTagSide.ENEMY;
        if (scene.arena.getTagOnSide(type, side)) {
          continue;
        }
        const tag = getArenaTag(type, 0, MoveId.NONE, side === ArenaTagSide.ENEMY ? A.id : D.id, side) as any;
        tag.layers = 1 + rnd(tag.maxLayers);
        scene.arena.tags.push(tag);
      }
    }
    if (rnd(8) === 0) {
      // a screen on the player's side (the KO check's damage goes through it), or on the enemy's
      const type = [ArenaTagType.REFLECT, ArenaTagType.LIGHT_SCREEN, ArenaTagType.AURORA_VEIL][rnd(3)];
      const side = rnd(4) === 0 ? ArenaTagSide.ENEMY : ArenaTagSide.PLAYER;
      scene.arena.tags.push(getArenaTag(type, 1 + rnd(5), MoveId.NONE, side === ArenaTagSide.PLAYER ? A.id : D.id, side));
    }
    if (rnd(10) === 0) {
      const side = rnd(2) === 0 ? ArenaTagSide.ENEMY : ArenaTagSide.PLAYER;
      scene.arena.tags.push(getArenaTag(ArenaTagType.TAILWIND, 1 + rnd(4), MoveId.TAILWIND, side === ArenaTagSide.PLAYER ? A.id : D.id, side));
    }
    if (rnd(10) === 0) {
      scene.arena.tags.push(getArenaTag(ArenaTagType.GRAVITY, 1 + rnd(5), MoveId.GRAVITY, A.id, ArenaTagSide.BOTH));
    }
    if (rnd(16) === 0) {
      scene.arena.tags.push(getArenaTag(ArenaTagType.FAIRY_LOCK, 1 + rnd(2), MoveId.FAIRY_LOCK, A.id, ArenaTagSide.BOTH));
    }
    if (rnd(4) === 0) {
      const w = [WeatherType.SUNNY, WeatherType.RAIN, WeatherType.SANDSTORM, WeatherType.HAIL, WeatherType.SNOW,
        WeatherType.FOG, WeatherType.HEAVY_RAIN, WeatherType.HARSH_SUN, WeatherType.STRONG_WINDS][rnd(9)];
      scene.arena.weather = new Weather(w, 1 + rnd(5), 5);
    }
    if (rnd(4) === 0) {
      scene.arena.terrain = new Terrain(1 + rnd(4), 1 + rnd(5), 5);
    }
    // session-wide state on attr objects: the status Tri Attack's last use rolled, the ratio Moonlight's healed
    for (const [, a] of multiStatusAttrs) {
      a.effect = rnd(4) === 0 ? a.effects[rnd(a.effects.length)] : a.effects[0];
    }
    variableHealAttrs.forEach(([, a], k) => {
      a.healRatio = rnd(4) === 0 ? [0.25, 0.5, 2 / 3, 1][rnd(4)] : initialHeal[k];
    });

    // DOUBLES: the KO check's ally abilities, with the defender's HP put between the damage with and
    // without them. Friend Guard on the defender's ally counts once the defender's own ability is
    // revealed. ignoreAllyAbility (set from whether the defender's ally's ability is revealed) reaches
    // getAttackDamage's attacker Attack, so it decides whether Flower Gift in sun on the attacker's
    // ally counts. Everything here happens before the state is written, so the replay starts from it.
    if (rnd(6) === 0) {
      const k = rnd(2);
      const j = rnd(2);
      const [X, Y] = [[A0, A1][k], [A0, A1][1 - k]];
      const [E, F] = [[D0, D1][j], [D0, D1][1 - j]];
      const fg = rnd(2) === 0;
      // (Flower Gift raises the ally's Attack, so only physical moves see it)
      const pool = E.moveset.map((pm: any) => pm.getMove())
        .filter((mv: any) => (fg ? mv.category !== MoveCategory.STATUS : mv.category === MoveCategory.PHYSICAL));
      const m = pool.length ? pool[rnd(pool.length)] : null;
      const holder = fg ? Y : F;
      if (m && X.hp > 0 && holder.hp > 0 && E.hp > 0
        && X.summonData.ability !== AbilityId.COMMANDER && holder.summonData.ability !== AbilityId.COMMANDER) {
        const dmg = (ignoreAllyAbility: boolean) =>
          X.getAttackDamage({ source: E, move: m, ignoreAbility: !X.waveData.abilityRevealed, ignoreSourceAbility: false,
            ignoreAllyAbility, ignoreSourceAllyAbility: false, isCritical: false, simulated: true }).damage;
        let lo: number;
        let hi: number;
        if (fg && rnd(3) === 0) {
          // a fainted ally's Friend Guard doesn't count (ally?.isActive(true), and canApplyAbility's
          // hp > 0 too). The fainted Pokemon keeps Friend Guard as summonData.ability: the test's
          // ability override hides the natural ability a Pokemon like Clefairy would have here.
          X.waveData.abilityRevealed = true;
          Y.hp = 0;
          Y.status = new Status(StatusEffect.FAINT);
          Y.resetSummonData();
          Y.switchOutStatus = true;
          scene.field.remove(Y);
          Y.summonData.ability = AbilityId.FRIEND_GUARD;
          hi = dmg(true);
          lo = Math.floor(hi * 0.75);
        } else if (fg) {
          X.waveData.abilityRevealed = true;
          Y.summonData.ability = AbilityId.BALL_FETCH;
          hi = dmg(true);
          Y.summonData.ability = AbilityId.FRIEND_GUARD;
          lo = dmg(true);
        } else {
          F.summonData.ability = AbilityId.FLOWER_GIFT;
          Y.waveData.abilityRevealed = rnd(3) === 0;
          scene.arena.weather = new Weather(WeatherType.SUNNY, 1 + rnd(5), 5);
          lo = dmg(true);
          hi = dmg(false);
        }
        if (hi > lo && lo < X.getMaxHp()) {
          X.hp = Math.min(X.getMaxHp(), lo + 1 + rnd(hi - lo));
        }
      }
    }
    const lines: string[] = [`CASE ${i}`, ...dumpState(), "RUN"];
    try {
      // DOUBLES: TurnInitPhase gives an EnemyCommandPhase to each active enemy, field index 0 then 1
      for (const f of [0, 1]) {
        const E = [D0, D1][f];
        if (!E.isActive()) {
          continue;
        }
        logged = null;
        console.log = (...args: any[]) => {
          if (args[0] === "Move Scores:") {
            logged = [...args[1]];
          }
        };
        try {
          const ph = new EnemyCommandPhase(f) as any;
          ph.end = () => {};
          ph.start();
        } finally {
          console.log = realLog;
        }
        lines.push(`STEP f=${f}`);
        lines.push(...commandLine(2 + f));
        lines.push(`AFTER counter=${battle.enemySwitchCounter}`);
        lines.push(rngLine());
        lines.push(`MQ b=${2 + f} list=${L(E.summonData.moveQueue.map(turnMove))}`);
        if (logged) {
          lines.push(`SCORES list=${L(logged as number[])}`);
        }
      }
      // what the choices left behind: phases a condition queued (an ability shown, an immunity
      // message), abilities applied for real, abilities revealed
      const nm = (ph: any) => (ph.phaseName === "DynamicPhaseMarker" ? `Marker:${ph.phaseType}` : ph.phaseName);
      lines.push(callSelLine());
      lines.push(`QUEUE levels=${pm.phaseQueue.levels.map((lv: any[]) => L(lv.map(nm))).join("|")}`);
      fieldMons.forEach((p: any, b: number) => {
        lines.push(`APPLIED b=${b} aas=${L([...p.summonData.abilitiesApplied])} aaw=${L([...p.waveData.abilitiesApplied])}`
          + ` revealed=${p.waveData.abilityRevealed ? 1 : 0}`);
      });
      if (battle.trainer) {
        // DOUBLES: for each of the two trainer slots, the switch-in scores EnemyCommandPhase weighs
        // (entry hazards included), compared as numbers, and the Pokemon the trainer would send in
        // after a faint (SwitchSummonPhase with slot -1)
        for (const ts of [TrainerSlot.TRAINER, TrainerSlot.TRAINER_PARTNER]) {
          const sw = battle.trainer.getPartyMemberMatchupScores(ts, true);
          lines.push(`MATCHUP ts=${ts} list=${L(sw.map(([k, s]: [number, number]) => `${k}:${s}`))}`);
          const scores = battle.trainer.getPartyMemberMatchupScores(ts);
          lines.push(`NEXT ts=${ts} slot=${scores.length ? battle.trainer.getNextSummonIndex(ts) : -1}`);
        }
      }
    } catch (e: any) {
      const key = String(e?.message ?? e).slice(0, 80);
      thrown[key] = (thrown[key] ?? 0) + 1;
      threwCount++;
      continue;
    }
    lines.push(`END ${i}`);
    out.push(...lines);
    written++;
  }
  out.push(`# upstream threw (case left out): ${JSON.stringify(thrown)}`);
  out.push(`# AI_DOUBLES_COMPLETE ${written} ${threwCount}`);
  fs.writeFileSync(OUT, out.join("\n") + "\n");
  console.log(`AI DOUBLES cases=${written} threw=${threwCount} requested=${N}`);
}, 3_600_000);
