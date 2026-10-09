/* Card 108 additive source oracle. Canonical trainer singles and AttackTypeBooster stock;
 * runs actual EnemyCommandPhase, benefit scores, damage, trainer scores and faint picks.
 * Existing ai.producer.test.ts is untouched. */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves } from "#data/data-lists";
import { EntryHazardTag } from "#data/arena-tag";
import { Status } from "#data/status-effect";
import { TrainerAI } from "#data/trainers/trainer-config";
import { AbilityId } from "#enums/ability-id";
import { AiType } from "#enums/ai-type";
import { BattleType } from "#enums/battle-type";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { TeraAIMode } from "#enums/tera-ai-mode";
import { TrainerSlot } from "#enums/trainer-slot";
import { TrainerVariant } from "#enums/trainer-variant";
import { Trainer } from "#field/trainer";
import { PokemonMove } from "#moves/pokemon-move";
import { AttackTypeBoosterModifier } from "#modifiers/modifier";
import { AttackTypeBoosterModifierType } from "#modifiers/modifier-type";
import { PokemonType } from "#enums/pokemon-type";
import { EnemyCommandPhase } from "#phases/enemy-command-phase";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("Card 108 bounded trainer and held AI producer", async () => {
  const N = Number(process.env.AI108_N ?? 2000);
  const OUT = process.env.AI108_OUT ?? "/tmp/pr-ai.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override
    .battleStyle("single")
    .ability(AbilityId.BALL_FETCH)
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

  let seed = Number(process.env.AI108_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const usable = allMoves.filter((m: any) => m && m.id !== MoveId.NONE && !m.isUnimplemented);
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
  const enemyBench = Array.from({ length: 5 }, () =>
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
  const commandLines = () => {
    const lines: string[] = [];
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
        lines.push(`XMOD side=${side} cls=${m.constructor.name} pid=${m.pokemonId} stack=${m.stackCount} virtual=${m.virtualStackCount} type=${m.moveType} mult=0.2`);
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

  if (!Number.isInteger(N) || N <= 0 || N > 100000) throw Error("invalid request count");
  const out: string[] = [`# AI108_CONTRACT 1 ${N}`];
  let logged: number[] | null = null;
  const realLog = console.log;
  const moves = [MoveId.U_TURN, MoveId.BATON_PASS, MoveId.PARTING_SHOT, MoveId.ROAR,
    MoveId.THIEF, MoveId.KNOCK_OFF, MoveId.TACKLE, MoveId.QUICK_ATTACK];
  const clean = (p: any, active: boolean, level: number) => {
    if (p.isOnField()) scene.field.remove(p);
    p.resetSummonData(); p.resetTurnData(); p.resetBattleAndWaveData();
    p.switchOutStatus = false;
    p.species = speciesDataRegistry.getSpecies(SpeciesId.RATTATA);
    p.formIndex = 0; p.fusionSpecies = null; p.level = level;
    p.ivs = Array.from({length: 6}, () => rnd(32)); p.nature = rnd(25);
    p.passive = false; p.customPokemonData.passive = AbilityId.BALL_FETCH;
    p.summonData.ability = AbilityId.BALL_FETCH;
    p.isTerastallized = false; p.stellarTypesBoosted = []; p.teraType = PokemonType.NORMAL;
    p.status = null; p.moveset = [new PokemonMove(MoveId.TACKLE)];
    p.calculateStats(); p.hp = p.getMaxHp(); p.generateName();
    p.waveData.abilityRevealed = true; p.tempSummonData.waveTurnCount = 1;
    if (active) scene.field.add(p);
  };
  for (let i = 0; i < N; i++) {
    const profile = i % 8, state = Math.floor(i / 8) % 8;
    pm.clearAllPhases(); pm.phaseQueue.deferredActive = false;
    scene.abilityBar.setVisible(false);
    battle.playerFaintsHistory.length = 0; battle.enemyFaintsHistory.length = 0;
    battle.enemyFaints = 0; scene.arena.playerFaints = 0; battle.playerParticipantIds.clear();
    scene.modifiers.length = 0; scene.enemyModifiers.length = 0;
    scene.arena.tags.length = 0; scene.arena.positionalTagManager.tags.length = 0;
    scene.arena.weather = null; scene.arena.terrain = null;
    battle.turn = 2; battle.turnCommands = {}; battle.preTurnCommands = {};
    battle.mysteryEncounter = undefined; battle.battleType = BattleType.TRAINER;
    clean(A, true, 50); clean(D, true, 50);
    playerBench.forEach(p => clean(p, false, 50));
    enemyBench.forEach(p => clean(p, false, state === 6 ? 100 : 45));
    const pparty = scene.getPlayerParty(); pparty.length = 0; pparty.push(A);
    if (state !== 3) pparty.push(playerBench[0]);
    battle.enemyParty.length = 0; battle.enemyParty.push(D);
    if (state !== 1) battle.enemyParty.push(...enemyBench.slice(0, 2));
    const trainer = Object.create(Trainer.prototype);
    trainer.config = {isBoss: false, doubleOnly: false, trainerAI: new TrainerAI(TeraAIMode.NO_TERA)};
    trainer.variant = TrainerVariant.DEFAULT; battle.trainer = trainer;
    battle.enemyParty.forEach((p: any, k: number) => {
      p.trainerSlot = state === 2 && k ? TrainerSlot.TRAINER_PARTNER : TrainerSlot.TRAINER;
      p.initialTeamIndex = k; p.aiType = AiType.SMART;
      if (state === 4 && k) { p.hp = 0; p.status = new Status(StatusEffect.FAINT); }
    });
    D.hp = state === 6 ? 1 : state === 5 ? Math.max(1, Math.floor(D.getMaxHp()/5)) : D.getMaxHp();
    D.summonData.statStages = [0, 0, 0, 0, 0, 0, 0].map(() => state === 7 ? -2 : state === 5 ? 2 : 0);
    D.moveset = [new PokemonMove(moves[profile]), new PokemonMove(MoveId.TACKLE)];
    if (profile === 6) D.moveset = [new PokemonMove(MoveId.TACKLE), new PokemonMove(MoveId.EMBER)];
    battle.enemySwitchCounter = state === 6 ? 0 : 100;
    battle.playerParticipantIds.add(A.id);
    if (profile >= 4 && state !== 0) {
      const owner = profile === 6 ? D : state === 2 ? playerBench[0] : A;
      const m = new AttackTypeBoosterModifier(new AttackTypeBoosterModifierType(PokemonType.NORMAL,20), owner.id, PokemonType.NORMAL,20,1 + state % 3);
      (profile === 6 ? scene.enemyModifiers : scene.modifiers).push(m);
    }
    if (profile === 6) {
      const dmg = A.getAttackDamage({source:D,move:allMoves[MoveId.TACKLE],ignoreAbility:false,ignoreAllyAbility:true,isCritical:false,simulated:true}).damage;
      A.hp = Math.min(A.getMaxHp(), Math.max(1, dmg + (state % 3) - 1));
    }
    multiStatusAttrs.forEach(([,a]) => { a.effect = a.effects[0]; });
    variableHealAttrs.forEach(([,a],k) => { a.healRatio = initialHeal[k]; });
    out.push(`CASE ${i}`);
    out.push(`PROFILE kind=${profile} state=${state}`);
    out.push(...dumpState());
    out.push("RUN");
    logged = null;
    console.log = (...args: any[]) => {
      if (args[0] === "Move Scores:") {
        logged = [...args[1]];
      }
    };
    try {
      const ph = new EnemyCommandPhase(0) as any;
      ph.end = () => {};
      ph.start();
    } finally {
      console.log = realLog;
    }
    const move = allMoves[moves[profile]];
    const damage = A.getAttackDamage({source:D,move:allMoves[MoveId.TACKLE],ignoreAbility:false,ignoreAllyAbility:true,isCritical:false,simulated:true}).damage;
    out.push(`PROBE user=${move.getUserBenefitScore(D,A,move)} target=${move.getTargetBenefitScore(D,A,move)} damage=${damage}`);
    out.push(...commandLines());
    out.push(`AFTER counter=${battle.enemySwitchCounter}`);
    out.push(rngLine());
    out.push(`MQ b=2 list=${L(D.summonData.moveQueue.map(turnMove))}`);
    if (logged) {
      out.push(`SCORES list=${L(logged as number[])}`);
    }
    // what the choice left behind: phases a condition queued (an ability shown, an immunity
    // message), abilities applied for real, abilities revealed
    const nm = (ph: any) => (ph.phaseName === "DynamicPhaseMarker" ? `Marker:${ph.phaseType}` : ph.phaseName);
    out.push(callSelLine());
    out.push(`QUEUE levels=${pm.phaseQueue.levels.map((lv: any[]) => L(lv.map(nm))).join("|")}`);
    for (const [p, b] of [[A, "0"], [D, "2"]] as [any, string][]) {
      out.push(`APPLIED b=${b} aas=${L([...p.summonData.abilitiesApplied])} aaw=${L([...p.waveData.abilitiesApplied])}`
        + ` revealed=${p.waveData.abilityRevealed ? 1 : 0}`);
    }
    if (battle.trainer) {
      // the Pokemon the trainer would send in after a faint (SwitchSummonPhase with slot -1)
      // the switch-in scores EnemyCommandPhase weighs (entry hazards included), compared as numbers
      const sw = battle.trainer.getPartyMemberMatchupScores(TrainerSlot.TRAINER, true);
      out.push(`MATCHUP list=${L(sw.map(([k, s]: [number, number]) => `${k}:${s}`))}`);
      const scores = battle.trainer.getPartyMemberMatchupScores(TrainerSlot.TRAINER);
      out.push(`NEXT slot=${scores.length ? battle.trainer.getNextSummonIndex(TrainerSlot.TRAINER) : -1}`);
    }
    out.push(`END ${i}`);
  }
  out.push(`# AI108_COMPLETE ${N} 0`);
  fs.writeFileSync(OUT, out.join("\n") + "\n");
}, 3_600_000);
