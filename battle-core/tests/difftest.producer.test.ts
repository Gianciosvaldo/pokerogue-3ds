/*
 * Upstream half of the differential test. Copy into an upstream checkout as
 * test/tests/zz-dump/difftest.producer.test.ts and run with
 *   DIFF_OUT=/abs/cases.txt DIFF_N=5000 npx vitest run test/tests/zz-dump/difftest.producer.test.ts
 * It builds random attacker/defender pairs inside a real upstream battle, asks
 * upstream for stats, crit rolls and damage, and writes cases + expected answers.
 * build/difftest replays each case through the C port and compares.
 */
import { BattleScene } from "#app/battle-scene";
import { allAbilities, allMoves } from "#data/data-lists";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { Status } from "#data/status-effect";
import { Weather } from "#data/weather";
import { AbilityId } from "#enums/ability-id";
import { MoveCategory } from "#enums/move-category";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
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
  const battle = game.scene.currentBattle as any;
  const A = game.field.getPlayerPokemon() as any;
  const D = game.field.getEnemyPokemon() as any;

  // Deterministic case generator (independent of Phaser's RNG).
  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const ids = Object.keys(speciesDataRegistry.data).map(Number);
  const attacks = allMoves.filter(
    (m: any) => m && m.category !== MoveCategory.STATUS && m.power > 0 && !m.isUnimplemented,
  );

  const lines: string[] = [];
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
    let sid = ids[rnd(ids.length)];
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
    const s = [StatusEffect.NONE, StatusEffect.NONE, StatusEffect.BURN, StatusEffect.PARALYSIS, StatusEffect.POISON, StatusEffect.TOXIC][rnd(6)];
    p.status = s ? new Status(s) : null;
    p.isTerastallized = rnd(6) === 0;
    p.teraType = rnd(19);
    p.gender = rnd(3) - 1; // GENDERLESS -1, MALE 0, FEMALE 1
    return [sid, form, p.level, ...p.ivs, p.nature, ...stages, s, p.isTerastallized ? 1 : 0, p.teraType, p.hp, ability, p.gender];
  };

  for (let i = 0; i < N; i++) {
    const a = setup(A);
    const d = setup(D);
    const weather = [0, 0, 0, 1, 2, 3, 5, 9][rnd(8)];
    game.scene.arena.weather = weather ? new Weather(weather, 5) : null;
    const move = attacks[rnd(attacks.length)];
    const crit = rnd(4) === 0;
    lines.push(rngLine());
    lines.push(`C ${a.join(" ")} ${d.join(" ")} ${weather} ${move.id} ${crit ? 1 : 0}`);
    const sim = D.getAttackDamage({ source: A, move, isCritical: crit, simulated: true });
    const critRoll = D.getCriticalHitResult(A, move);
    const real = D.getAttackDamage({ source: A, move, isCritical: crit, simulated: false });
    lines.push(`E ${A.stats.join(" ")} ${D.stats.join(" ")} ${sim.damage} ${sim.result} ${critRoll ? 1 : 0} ${real.damage} ${real.result}`);
  }
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
