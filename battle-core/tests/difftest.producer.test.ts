/*
 * Upstream half of the differential test. Copy into an upstream checkout as
 * test/tests/zz-dump/difftest.producer.test.ts and run with
 *   DIFF_OUT=/abs/cases.txt DIFF_N=5000 npx vitest run test/tests/zz-dump/difftest.producer.test.ts
 * It builds random attacker/defender pairs inside a real upstream battle, asks
 * upstream for stats, crit rolls and damage, and writes cases + expected answers.
 * build/difftest replays each case through the C port and compares.
 */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allAbilities, allMoves } from "#data/data-lists";
import { Status } from "#data/status-effect";
import { Terrain } from "#data/terrain";
import { Weather } from "#data/weather";
import { AbilityId } from "#enums/ability-id";
import { Command } from "#enums/command";
import { MoveCategory } from "#enums/move-category";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("difftest producer", async () => {
  const N = Number(process.env.DIFF_N ?? 2000);
  const ABILITIES = process.env.DIFF_ABILITIES !== "0";
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override
    .battleStyle("single")
    .ability(AbilityId.BALL_FETCH)
    .enemyAbility(AbilityId.BALL_FETCH)
    .hasPassiveAbility(false)
    .enemyHasPassiveAbility(false)
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

  const setup = (p: any) => {
    // Non-simulated calls can leave per-turn state behind (e.g. Tera Shell stores
    // turnData.moveEffectiveness), so every case starts from fresh transient data.
    p.resetSummonData();
    p.resetTurnData();
    p.resetBattleAndWaveData();
    const sid = ids[rnd(ids.length)];
    const sp = speciesDataRegistry.getSpecies(sid);
    const form = sp.forms.length ? rnd(sp.forms.length) : 0;
    p.species = sp;
    p.formIndex = form;
    p.fusionSpecies = null;
    p.level = 1 + rnd(100);
    p.ivs = [0, 0, 0, 0, 0, 0].map(() => rnd(32));
    p.nature = rnd(25);
    p.summonData.types = [];
    p.calculateStats();
    p.hp = rnd(3) === 0 ? p.getMaxHp() : 1 + rnd(p.getMaxHp());
    // Active ability via summonData (wins over the Ball Fetch overrides, which still
    // govern the permanent ability that calculateStats looks at). DIFF_ABILITIES=0 keeps Ball Fetch.
    const ability = ABILITIES && rnd(2) === 0 ? 1 + rnd(allAbilities.length - 1) : AbilityId.BALL_FETCH;
    p.summonData.ability = ability;
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
    p.teraType = rnd(19);
    p.gender = rnd(3) - 1; // GENDERLESS -1, MALE 0, FEMALE 1
    p.stellarTypesBoosted = rnd(2) === 0 ? [] : [rnd(19), rnd(19)];
    p.friendship = rnd(256);
    if (p === D) {
      p.bossSegments = rnd(5) === 0 ? 2 : 0;
    }
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
    ];
  };

  for (let i = 0; i < N; i++) {
    battle.turn = 1 + rnd(30);
    const a = setup(A);
    const d = setup(D);
    const weather = [0, 0, 0, 1, 2, 3, 5, 9][rnd(8)];
    scene.arena.weather = weather ? new Weather(weather, 5) : null;
    const terrain = [0, 0, 0, 1, 2, 3, 4][rnd(7)];
    scene.arena.terrain = terrain ? new Terrain(terrain, 5) : null;
    scene.arena.playerFaints = rnd(3) === 0 ? rnd(5) : 0;
    battle.enemyFaints = rnd(3) === 0 ? rnd(5) : 0;
    battle.playerFaintsHistory = rnd(3) === 0 ? [{ pokemon: A, turn: battle.turn - 1 - rnd(2) }] : [];
    battle.enemyFaintsHistory = rnd(3) === 0 ? [{ pokemon: D, turn: battle.turn - 1 - rnd(2) }] : [];
    battle.turnCommands = {};
    for (const p of [A, D]) {
      battle.turnCommands[p.getBattlerIndex()] = { command: rnd(3) === 0 ? Command.BALL : Command.FIGHT };
    }
    const move = attacks[rnd(attacks.length)];
    const crit = rnd(4) === 0;
    const swap = rnd(4) === 0; // the enemy attacks the player
    const src = swap ? D : A;
    const def = swap ? A : D;
    src.moveset = rnd(3) === 0 ? [new PokemonMove(move.id, rnd(move.pp + 1))] : [];
    def.moveset = [];
    const faintTurn = (h: any[]) => (h.length ? h.at(-1).turn : -1);
    const rl = rngLine();
    const worldLine =
      `W ${battle.turn} ${scene.waveSeed} ${terrain} ${scene.arena.playerFaints} ${battle.enemyFaints}`
        + ` ${faintTurn(battle.playerFaintsHistory)} ${faintTurn(battle.enemyFaintsHistory)}`
        + ` ${scene.phaseManager.dynamicQueueManager.getLastTurnOrder().length} ${scene.arena.tags.length}`
        + ` ${scene.modifiers.length} ${scene.enemyModifiers.length} ${scene.getMaxExpLevel()}`
        + ` ${[0, 1, 2, 3].map(k => battle.turnCommands[k]?.command ?? -1).join(" ")}`;
    const caseLine =
      `C ${a.join(" ")} ${late(A).join(" ")} ${d.join(" ")} ${late(D).join(" ")} ${weather} ${move.id} ${crit ? 1 : 0} ${swap ? 1 : 0}`;
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
    }
    lines.push(rl, worldLine, caseLine);
    lines.push(
      `E ${A.stats.join(" ")} ${D.stats.join(" ")} ${sim.damage} ${sim.result} ${critRoll ? 1 : 0} ${real.damage} ${real.result}`,
    );
  }
  lines.push(`# upstream threw (case left out): ${JSON.stringify(thrown)}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
