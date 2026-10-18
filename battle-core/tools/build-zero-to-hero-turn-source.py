#!/usr/bin/env python3
"""Additive Zero to Hero turns over the unchanged established turn producer.
Run it with TURN_TRAINER=1 TURN_SUMMON=1 and replay with the unchanged build/turntest.
Usage: build-zero-to-hero-turn-source.py <tests/turntest.producer.test.ts> <out.ts>"""
from pathlib import Path
import sys

s = Path(sys.argv[1]).read_text()

def replace(a, b):
    global s
    if s.count(a) != 1:
        raise SystemExit('Zero to Hero turn source anchor changed: ' + a[:80])
    s = s.replace(a, b, 1)

replace('''    if (SUMMON && rnd(3) === 0) {
      // the player switches (CommandPhase.tryLeaveField): a bench Pokemon that hasn't fainted''',
'''    // Zero to Hero overlay: Palafin on the field and in reserves, both forms, both sides; natural,
    // passive or overridden ability; switch-outs by command, U-turn and Baton Pass, and knockouts
    // the reused battle keeps BattleType.CLEAR after a final-wave victory case, which the port's
    // trainer checks (Roar, Whirlwind) read differently from currentBattle.trainer; restore it per case
    battle.battleType = TRAINER ? BattleType.TRAINER : BattleType.WILD;
    const z2hPalafin = speciesDataRegistry.getSpecies(SpeciesId.PALAFIN);
    const z2hSet = (p: any) => {
      const ratio = p.hp / Math.max(1, p.getMaxHp());
      p.species = z2hPalafin;
      p.formIndex = rnd(2);
      p.fusionSpecies = null;
      const k = rnd(8);
      p.summonData.ability = k === 2 ? AbilityId.BALL_FETCH : undefined;  // a summon override leaveField clears
      p.customPokemonData.ability = k === 0 || k === 1 ? AbilityId.BALL_FETCH : -1;
      p.passive = k === 1;
      p.customPokemonData.passive = k === 1 ? AbilityId.ZERO_TO_HERO : AbilityId.NONE;
      p.calculateStats();
      p.generateName();
      p.hp = p.hp > 0 ? Math.max(1, Math.min(p.getMaxHp(), Math.round(ratio * p.getMaxHp()))) : 0;
    };
    const z2hMove = (p: any, move: MoveId) => {
      p.moveset[0] = new PokemonMove(move);
      const targets = getMoveTargets(p, move).targets;
      const b = p.getBattlerIndex();
      if (p.isPlayer()) {
        battle.turnCommands[b] = { command: Command.FIGHT, cursor: 0, move: { move, targets, useMode: MoveUseMode.NORMAL }, args: [MoveUseMode.NORMAL, undefined] };
        battle.preTurnCommands[b] = { command: Command.FIGHT, targets: [b], skip: true };
      } else {
        battle.turnCommands[b] = { command: Command.FIGHT, move: { move, targets, useMode: MoveUseMode.NORMAL }, skip: false };
      }
    };
    if (rnd(4) !== 0) z2hSet(A);
    if (rnd(4) !== 0) z2hSet(D);
    for (const b of party.slice(1)) if (rnd(2) === 0) z2hSet(b);
    if (TRAINER) for (const b of scene.getEnemyParty().slice(1)) if (rnd(2) === 0) z2hSet(b);
    {
      const r = rnd(12), q = rnd(12);
      if (r < 2) z2hMove(A, MoveId.U_TURN);
      else if (r === 2) z2hMove(A, MoveId.BATON_PASS);
      else if (r === 3) z2hMove(A, MoveId.TACKLE);
      if (TRAINER && q < 2) z2hMove(D, MoveId.U_TURN);
      else if (q === 2) z2hMove(D, MoveId.TACKLE);
      if (A.species === z2hPalafin && rnd(2) === 0) {
        const bench = party.map((_: any, k: number) => k).filter((k: number) => k >= 1 && !party[k].isFainted());
        if (bench.length) battle.turnCommands[0] = { command: Command.POKEMON, cursor: bench[rnd(bench.length)], args: [false] };
      }
      if (TRAINER && D.species === z2hPalafin && rnd(2) === 0) {
        const eparty = scene.getEnemyParty();
        const reserves = eparty.map((_: any, k: number) => k).filter((k: number) => k >= 1 && !eparty[k].isFainted());
        if (reserves.length) battle.turnCommands[2] = { command: Command.POKEMON, cursor: reserves[rnd(reserves.length)], args: [false], skip: false };
      }
    }
    if (SUMMON && rnd(3) === 0) {
      // the player switches (CommandPhase.tryLeaveField): a bench Pokemon that hasn't fainted''')

Path(sys.argv[2]).write_text(s)
