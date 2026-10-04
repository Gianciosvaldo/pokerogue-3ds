/* Focused producer for Task 71: BerryModifier triggers and consumption */
import { BattleScene } from "#app/battle-scene";
import { globalScene } from "#app/global-scene";
import { BerryModifier } from "#modifiers/modifier";
import { BerryModifierType } from "#modifiers/modifier-type";
import { BerryType } from "#enums/berry-type";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { StatusEffect } from "#enums/status-effect";
import { HitResult } from "#enums/hit-result";
import { BattlerTagType } from "#enums/battler-tag-type";
import { Stat } from "#enums/stat";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import { it, expect, vi } from "vitest";
import * as fs from "node:fs";

it("focused berry producer", async () => {
  const n = Number(process.env.BERRY_N ?? 2000), initial = Number(process.env.BERRY_SEED ?? 777);
  let seed = initial >>> 0;
  const rnd = (k: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % k; };

  const realBattleRoll = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
    .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH).startingWave(5);
  await game.classicMode.startBattle(SpeciesId.MAGIKARP);
  BattleScene.prototype.randBattleSeedInt = realBattleRoll;

  game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
  const p: any = game.field.getPlayerPokemon(), enemy: any = game.field.getEnemyPokemon();

  let messages: string[] = [];
  vi.spyOn(globalScene.phaseManager, "queueMessage").mockImplementation((s: any) => { messages.push(s); });

  const allBerryTypes = [
    BerryType.SITRUS, BerryType.LUM, BerryType.ENIGMA,
    BerryType.LIECHI, BerryType.GANLON, BerryType.PETAYA, BerryType.APICOT, BerryType.SALAC,
    BerryType.LANSAT, BerryType.STARF, BerryType.LEPPA,
  ];

  const lines = [`V 1 ${n}`];
  const consumedCounts: Record<number, number> = {};
  const rejectedCounts: Record<number, number> = {};
  for (const b of allBerryTypes) { consumedCounts[b] = 0; rejectedCounts[b] = 0; }

  for (let i = 0; i < n; i++) {
    const isPlayer = rnd(2);
    const target = isPlayer ? p : enemy;
    const user = isPlayer ? enemy : p;

    for (const q of [p, enemy]) {
      q.resetSummonData();
      q.resetTurnData();
      q.hp = q.getMaxHp();
      q.status = null;
      q.passive = false;
      q.customPokemonData.passive = -1;
      q.summonData.ability = AbilityId.NONE;
      q.summonData.moveset = null;
      q.moveset = [new PokemonMove(MoveId.TACKLE)];
      for (const m of q.getMoveset()) m.ppUsed = 0;
      for (const st of [Stat.ATK, Stat.DEF, Stat.SPATK, Stat.SPDEF, Stat.SPD, Stat.ACC, Stat.EVA]) {
        q.setStatStage(st, 0);
      }
    }

    const bIndex = rnd(allBerryTypes.length);
    const berryType = allBerryTypes[bIndex];

    // Configure Pokemon state randomly to cover both trigger (apply) and non-trigger (reject)
    if (berryType === BerryType.ENIGMA) {
      if (rnd(2) === 0) {
        target.turnData.attacksReceived.push({
          move: MoveId.TACKLE,
          result: HitResult.SUPER_EFFECTIVE,
          damage: 20,
          critical: false,
          sourceId: user.id,
          sourceBattlerIndex: user.getBattlerIndex(),
        });
      }
    } else if (berryType === BerryType.LUM) {
      if (rnd(2) === 0) {
        target.status = { effect: StatusEffect.POISON };
      }
    } else if (berryType === BerryType.LEPPA) {
      if (rnd(2) === 0) {
        for (const m of target.getMoveset()) m.ppUsed = m.getMovePp();
      }
    } else if (berryType === BerryType.SITRUS) {
      if (rnd(2) === 0) {
        target.hp = Math.floor(target.getMaxHp() * 0.4);
      }
    } else {
      // Stat / Lansat / Starf pinch berries trigger < 25% HP
      if (rnd(2) === 0) {
        target.hp = Math.floor(target.getMaxHp() * 0.1);
      }
    }

    // Ability setup (e.g. Gluttony or Ripen or unburden)
    const ability = [AbilityId.NONE, AbilityId.GLUTTONY, AbilityId.RIPEN, AbilityId.UNBURDEN][rnd(4)];
    target.summonData.ability = ability;

    globalScene.modifiers = [];
    globalScene.enemyModifiers = [];

    const berryModType = new BerryModifierType(berryType);
    const mod: BerryModifier = berryModType.newModifier(target) as BerryModifier;
    mod.stackCount = 1;
    (isPlayer ? globalScene.modifiers : globalScene.enemyModifiers).push(mod);

    Phaser.Math.RND.sow([`berry-global-${initial}-${i}`]);
    const globalBefore = Phaser.Math.RND.state();

    messages = [];

    const attacksRecResult = target.turnData.attacksReceived.length > 0 ? target.turnData.attacksReceived[0].result : 0;
    const hasCritTag = target.getTag(BattlerTagType.CRIT_BOOST) ? 1 : 0;
    const hasConfusedTag = target.getTag(BattlerTagType.CONFUSED) ? 1 : 0;

    const statTypes = [Stat.ATK, Stat.DEF, Stat.SPATK, Stat.SPDEF, Stat.SPD, Stat.ACC, Stat.EVA];
    const stagesBefore = statTypes.map(st => target.getStatStage(st));
    const targetMoveBefore = target.getMoveset()[0];

    const line1 = [
      isPlayer, berryType, target.hp, target.getMaxHp(),
      target.status ? target.status.effect : 0,
      hasConfusedTag, hasCritTag, attacksRecResult, ability,
      targetMoveBefore.ppUsed, targetMoveBefore.getMovePp(),
      ...stagesBefore,
    ].join(" ");

    const shouldApply = mod.shouldApply(target);
    let applied = false;
    if (shouldApply) {
      applied = mod.apply(target);
      consumedCounts[berryType]++;
    } else {
      rejectedCounts[berryType]++;
    }

    const globalAfter = Phaser.Math.RND.state();

    const stagesAfter = statTypes.map(st => target.getStatStage(st));
    const targetMoveAfter = target.getMoveset()[0];

    lines.push(line1);
    lines.push(globalBefore);
    lines.push(globalAfter);

    lines.push([
      Number(shouldApply), Number(applied), target.hp,
      target.status ? target.status.effect : 0,
      target.getTag(BattlerTagType.CRIT_BOOST) ? 1 : 0,
      Number(!!target.getTag(BattlerTagType.UNBURDEN)),
      targetMoveAfter.ppUsed,
      ...stagesAfter,
      messages.length,
    ].join(" "));

    for (const message of messages) {
      const b = [...Buffer.from(message, "utf8")];
      lines.push([b.length, ...b].join(" "));
    }
  }

  console.log(`Berry ${n} cases; consumed ${JSON.stringify(consumedCounts)}; rejected ${JSON.stringify(rejectedCounts)}`);

  for (const b of allBerryTypes) {
    expect(consumedCounts[b]).toBeGreaterThan(0);
    expect(rejectedCounts[b]).toBeGreaterThan(0);
  }

  lines.push(`Z ${n}`);
  fs.writeFileSync(process.env.BERRY_OUT ?? "/tmp/berry.txt", lines.join("\n") + "\n");
}, 120000);
