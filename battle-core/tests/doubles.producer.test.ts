/*
 * Task 73: upstream half of the doubles turn suite. Additive: the established singles producer
 * (turntest.producer.test.ts) and its comparator are unchanged. Copy into an upstream checkout as
 * test/tests/zz-dump/doubles.producer.test.ts and run with
 *   DOUBLES_OUT=/abs/doubles.txt DOUBLES_N=2000 DOUBLES_SEED=777 npx vitest run test/tests/zz-dump/doubles.producer.test.ts
 * (tools/doublesdiff.sh does both halves on Node 22).
 *
 * Each case builds a random wild double battle inside a real upstream battle: two player Pokemon
 * and two wild Pokemon on the field (random species, level, IVs, nature, HP, 1 to 4 moves; Ball
 * Fetch as the ability unless DOUBLES_RICH=1, no held items, no status, as the singles suite's
 * plain mode) and 0 to 4
 * player bench Pokemon, a quarter of them fainted. Moves are drawn so that spread moves (the
 * ALL_NEAR_ENEMIES, ALL_NEAR_OTHERS, ALL_ENEMIES and ALL_OTHERS targets) and moves aimed at an
 * ally (NEAR_ALLY, ALLY, USER_OR_NEAR_ALLY, USER_AND_ALLIES, and those that default to the ally)
 * come up often. All four battlers get a FIGHT command the way the command phases build one: a
 * move with PP left (Struggle when none), its targets from getMoveTargets: every one of them for a
 * move that hits several, else one of them picked at random (the target select screen for the
 * player, the AI's pick for the enemy), which is where a single-target move aimed at the ally
 * comes from. Then upstream's own phases run from TurnStartPhase until the turn is over (the next
 * TurnInitPhase) or a phase the turn engine stops at (GameOverPhase, BattleEndPhase,
 * SelectModifierPhase), exactly as the singles producer drives them: a VictoryPhase runs without
 * the experience, a SwitchPhase's party screen takes the n-th number of the case's PICK list
 * modulo the bench Pokemon it can send in.
 *
 * The records are the singles suite's (tests/turntest.c reads and compares them): a Pokemon's
 * records are keyed by its party slot, 0 / 2 for the first one of each side and p<slot> /
 * e<slot> for the others, so the second Pokemon on the field (battler 1 or 3) is p1 / e1. The
 * file starts with "# DOUBLES_CONTRACT 1 <N>" and ends with "# DOUBLES_COMPLETE <cases written>
 * <cases upstream threw on>", which build/doublestest checks before it compares anything.
 */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allAbilities, allMoves } from "#data/data-lists";
import { EntryHazardTag } from "#data/arena-tag";
import { Status } from "#data/status-effect";
import { AbilityId } from "#enums/ability-id";
import { BattleType } from "#enums/battle-type";
import { Command } from "#enums/command";
import { MoveCategory } from "#enums/move-category";
import { MoveId } from "#enums/move-id";
import { MoveTarget } from "#enums/move-target";
import { MoveUseMode } from "#enums/move-use-mode";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { BattlerTagType } from "#enums/battler-tag-type";
import { Stat } from "#enums/stat";
import { UiMode } from "#enums/ui-mode";
import { getMoveTargets } from "#moves/move-utils";
import { Pokemon } from "#field/pokemon";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import { PartyOption } from "#ui/party-ui-handler";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("doubles turn producer", async () => {
  for (const [key, value] of Object.entries(process.env)) {
    if (key.startsWith("TURN_") && value === "1") {
      throw new Error(`the singles producer's mode ${key} does not apply to the doubles suite`);
    }
  }
  const N = Number(process.env.DOUBLES_N ?? 2000);
  const OUT = process.env.DOUBLES_OUT ?? "/tmp/pr-doubles.txt";
  (Pokemon.prototype as any).hideInfo = async function() {};
  (Phaser.GameObjects.Sprite.prototype as any).setPosition = function() { return this; };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override
    .battleStyle("double")
    .ability(AbilityId.BALL_FETCH)
    .enemyAbility(AbilityId.BALL_FETCH)
    .battleType(BattleType.WILD)
    .startingWave(3)
    .enemySpecies(SpeciesId.MAGIKARP);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR, SpeciesId.CHARMANDER);

  // GameManager pins battle rolls to their maximum; put upstream's real body back.
  (BattleScene.prototype as any).randBattleSeedInt = function (this: any, range: number, min = 0) {
    return this.currentBattle?.randSeedInt(range, min);
  };
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;
  const pm = scene.phaseManager as any;
  if (!battle.double || battle.getBattlerCount() !== 2) {
    throw new Error("not a double battle");
  }
  const [A0, A1] = scene.getPlayerField() as any[];
  const [D0, D1] = scene.getEnemyField() as any[];
  if (!A0 || !A1 || !D0 || !D1) {
    throw new Error("the field does not have four Pokemon");
  }

  let seed = Number(process.env.DOUBLES_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const ids = Object.keys(speciesDataRegistry.data).map(Number);
  const usable = allMoves.filter((m: any) => m && m.id !== MoveId.NONE && !m.isUnimplemented);
  const attacks = usable.filter((m: any) => m.category !== MoveCategory.STATUS);
  const spreadTargets = [MoveTarget.ALL_NEAR_ENEMIES, MoveTarget.ALL_NEAR_OTHERS, MoveTarget.ALL_ENEMIES, MoveTarget.ALL_OTHERS];
  const spreadMoves = usable.filter((m: any) => spreadTargets.includes(m.moveTarget));
  const allyTargets = [MoveTarget.NEAR_ALLY, MoveTarget.ALLY, MoveTarget.USER_OR_NEAR_ALLY, MoveTarget.USER_AND_ALLIES];
  const allyMoves = usable.filter((m: any) => allyTargets.includes(m.moveTarget) || m.allyTargetDefault);
  const boostTags = [BattlerTagType.PROTOSYNTHESIS, BattlerTagType.QUARK_DRIVE];
  // DOUBLES_RICH=1 (opt-in; the default suite draws nothing for it): half of the field Pokemon get
  // a random ability, half of those one from the pool below, the abilities that read the ally or the
  // user's side of the field. Off, every field Pokemon keeps Ball Fetch.
  const RICH = process.env.DOUBLES_RICH === "1";
  const isA = (o: any, names: string[]) => {
    for (let c = o?.constructor; c && c !== Object; c = Object.getPrototypeOf(c)) {
      if (names.includes(c.name)) {
        return true;
      }
    }
    return false;
  };
  const abilityIds = allAbilities.filter((a: any) => a && a.id !== AbilityId.NONE).map((a: any) => a.id);
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

  // Bench Pokemon so a fainted player Pokemon has someone to switch to (SwitchPhase). Made once;
  // species and HP are drawn per case.
  const playerBench = Array.from({ length: 4 }, () =>
    scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5),
  );

  // The party screen a SwitchPhase opens: the n-th screen of the turn sends in the Pokemon the n-th
  // number of the case's PICK list picks among those it can send in (the bench, not fainted), as
  // the singles producer does.
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

  // ---- the state dump: the singles producer's records (tests/turntest.c reads them) ----
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
  const dumpState = (before = false) => {
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
        + ` smove=${pt.sourceMove ?? 0} heal=${pt.healHp ?? 0}`
        + (before && pt.wisherId !== undefined ? ` wisher=${pt.wisherId}` : ""));
    }
    for (const [side, party] of [[0, scene.getPlayerParty()], [1, scene.getEnemyParty()]] as [number, any[]][]) {
      party.forEach((p: any, k: number) => {
        lines.push(...monLines(p, k === 0 ? String(side * 2) : `${side ? "e" : "p"}${k}`));
      });
    }
    for (const [side, list] of [[0, scene.modifiers], [1, scene.enemyModifiers]] as [number, any[]][]) {
      for (const m of list) {
        lines.push(
          `MOD side=${side} cls=${m.constructor.name} pid=${m.pokemonId ?? 0} stack=${m.stackCount}`
            + (m.berryType !== undefined ? ` btype=${m.berryType}` : "")
            + (m.constructor.name === "EnemyAttackStatusEffectChanceModifier" ? ` effect=${m.effect}` : ""),
        );
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
    return lines;
  };
  // What is still queued: the phase tree's levels (bottom first) and the dynamic queues. What
  // VictoryPhase queues after the BattleEndPhase is the game's to run, and left out.
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

  // ---- driving upstream's phases (as the singles producer) ----
  let started = 0;
  pm.startCurrentPhase = () => {
    started++;
  };
  const boundary = new Set(["TurnInitPhase", "GameOverPhase", "BattleEndPhase", "SelectModifierPhase"]);
  scene.applyPartyExp = () => {};
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
  const runTurn = async () => {
    const trace: string[] = [];
    shown = [];
    pm.pushNew("TurnStartPhase");
    pm.shiftPhase();
    for (let guard = 0; ; guard++) {
      const ph = pm.getCurrentPhase();
      if (boundary.has(ph.phaseName)) {
        return { trace, stop: ph.phaseName + stopArgs(ph) };
      }
      if (guard > 600) {
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
    p.summonData.ability = RICH && rnd(2) === 0
      ? (rnd(2) === 0 ? allyAbilityIds[rnd(allyAbilityIds.length)] : abilityIds[rnd(abilityIds.length)])
      : AbilityId.BALL_FETCH;
    p.gender = rnd(3) - 1;
    p.friendship = rnd(256);
    p.isTerastallized = false;
    p.teraType = rnd(19);
    p.stellarTypesBoosted = [];
    p.calculateStats();
    // the name messages use is fixed when the Pokemon is made; the species changed since
    p.generateName();
    p.hp = rnd(3) === 0 ? p.getMaxHp() : 1 + rnd(p.getMaxHp());
    p.status = null;
    const n = 1 + rnd(4);
    p.moveset = [];
    for (let k = 0; k < n; k++) {
      const r = rnd(10);
      const m = r < 4 ? spreadMoves[rnd(spreadMoves.length)]
        : r < 7 ? attacks[rnd(attacks.length)]
        : r < 8 ? allyMoves[rnd(allyMoves.length)]
        : usable[rnd(usable.length)];
      p.moveset.push(new PokemonMove(m.id, rnd(8) === 0 ? rnd(m.pp + 1) : 0));
    }
  };
  // CommandPhase / EnemyCommandPhase: a move with PP left (Struggle when none is usable); a move
  // that hits several targets gets them all, the others one of them (the target screen's or the
  // AI's pick)
  const command = (p: any) => {
    const slots = p.moveset.map((_: any, k: number) => k).filter((k: number) => p.moveset[k].ppUsed < p.moveset[k].getMovePp());
    const moveId = slots.length ? p.moveset[slots[rnd(slots.length)]].moveId : MoveId.STRUGGLE;
    const set = getMoveTargets(p, moveId);
    const targets = set.multiple || set.targets.length <= 1 ? set.targets : [set.targets[rnd(set.targets.length)]];
    const b = p.getBattlerIndex();
    if (p.isPlayer()) {
      battle.turnCommands[b] = { command: Command.FIGHT, cursor: 0, move: { move: moveId, targets, useMode: MoveUseMode.NORMAL }, args: [MoveUseMode.NORMAL, undefined] };
      battle.preTurnCommands[b] = { command: Command.FIGHT, targets: [b], skip: true };
    } else {
      battle.turnCommands[b] = { command: Command.FIGHT, move: { move: moveId, targets, useMode: MoveUseMode.NORMAL }, skip: false };
    }
  };

  const out: string[] = [`# DOUBLES_CONTRACT 1 ${N}`];
  const thrown: Record<string, number> = {};
  let written = 0;
  let threwCount = 0;
  for (let i = 0; i < N; i++) {
    picks = [0, 1, 2].map(j => (Math.imul(3 * i + j + 1, 2654435761) >>> 0) % 60);
    pickAt = 0;
    pm.clearAllPhases();
    pm.phaseQueue.deferredActive = false; // clear() leaves it set
    pm.dynamicQueueManager.clearLastTurnOrder();
    scene.abilityBar.setVisible(false);
    scene.tweens.add = (config: any) => { config.onComplete?.(); return {} as any; };
    if ((scene as any).ui?.messageHandler) (scene as any).ui.messageHandler.prompt = null;
    battle.battleScore = 0;
    // faint bookkeeping accumulates over the cases otherwise
    battle.playerFaintsHistory.length = 0;
    battle.enemyFaintsHistory.length = 0;
    battle.enemyFaints = 0;
    scene.arena.playerFaints = 0;
    battle.playerParticipantIds.clear();
    battle.postBattleLoot.length = 0;
    battle.lastMove = MoveId.NONE;
    battle.lastPlayerInvolved = undefined;
    battle.lastEnemyInvolved = undefined;
    scene.arena.ignoreAbilities = false;
    scene.arena.ignoringEffectSource = null;
    scene.modifiers.length = 0;
    scene.enemyModifiers.length = 0;
    scene.arena.tags.length = 0;
    scene.arena.positionalTagManager.tags.length = 0;
    scene.arena.weather = null;
    scene.arena.terrain = null;
    battle.turn = 1 + rnd(30);
    battle.turnCommands = {};
    battle.preTurnCommands = {};
    for (const p of [A0, A1, D0, D1]) {
      setup(p);
    }
    // the parties in field order: the two Pokemon on each side first (getPlayerField() and
    // getEnemyField() are their first two members), then the player's bench
    const party = scene.getPlayerParty();
    party.length = 0;
    party.push(A0, A1);
    const eparty = scene.getEnemyParty();
    eparty.length = 0;
    eparty.push(D0, D1);
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
    for (const b of playerBench.slice(0, rnd(playerBench.length + 1))) {
      b.species = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
      // a form an earlier case's switch-in changed to may not exist for the new species
      b.formIndex = 0;
      b.calculateStats();
      b.generateName();
      b.hp = rnd(4) === 0 ? 0 : b.getMaxHp();
      b.status = b.hp ? null : new Status(StatusEffect.FAINT);
      party.push(b);
    }
    battle.playerParticipantIds.add(A0.id);
    battle.playerParticipantIds.add(A1.id);
    for (const p of [A0, A1, D0, D1]) {
      command(p);
    }
    let before: string[];
    let run: { trace: string[]; stop: string };
    let texts: string[];
    try {
      before = dumpState(true);
      run = await runTurn();
      texts = shown;
    } catch (e: any) {
      const key = String(e?.message ?? e).slice(0, 80);
      thrown[key] = (thrown[key] ?? 0) + 1;
      threwCount++;
      continue;
    }
    written++;
    out.push(`CASE ${i}`, ...before, "RUN", `TRACE list=${L(run.trace)}`, `TEXTS ${JSON.stringify(texts)}`, ...dumpState(), ...queueLines(), `END stop=${run.stop}`);
  }
  out.push(`# upstream threw (case left out): ${JSON.stringify(thrown)}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);
  fs.writeFileSync(OUT, out.join("\n") + "\n");
  console.log(`DOUBLES cases=${written} threw=${threwCount} requested=${N}`);
}, 3_600_000);
