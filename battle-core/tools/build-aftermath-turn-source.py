#!/usr/bin/env python3
"""Additive Aftermath / Innards Out turns over the unchanged established turn producer.
Run it with TURN_TRAINER=1 TURN_SUMMON=1 and replay with the unchanged build/turntest.
Usage: build-aftermath-turn-source.py <tests/turntest.producer.test.ts> <out.ts>"""
from pathlib import Path
import sys

s = Path(sys.argv[1]).read_text()

def replace(a, b):
    global s
    if s.count(a) != 1:
        raise SystemExit('Aftermath turn source anchor changed: ' + a[:80])
    s = s.replace(a, b, 1)

replace('''    if (SUMMON && rnd(3) === 0) {
      // the player switches (CommandPhase.tryLeaveField): a bench Pokemon that hasn't fainted''',
'''    // Aftermath / Innards Out overlay: holders on both sides (ability or passive), most at low HP so
    // the turn knocks them out; attackers use contact and non-contact moves, some with Magic Guard,
    // Long Reach or Damp, some at low HP so the recoil knocks them out too
    // the reused battle keeps BattleType.CLEAR after a final-wave victory case; restore it per case
    battle.battleType = TRAINER ? BattleType.TRAINER : BattleType.WILD;
    const amHolder = (p: any) => {
      const ab = rnd(3) === 0 ? AbilityId.INNARDS_OUT : AbilityId.AFTERMATH;
      if (rnd(5) === 0) {
        p.passive = true;
        p.customPokemonData.passive = ab;
      } else {
        p.summonData.ability = undefined;
        p.customPokemonData.ability = ab;
      }
      // a Shedinja that loses Wonder Guard gets its HP stat back
      const ratio = p.hp / Math.max(1, p.getMaxHp());
      p.calculateStats();
      p.hp = p.hp > 0 ? Math.max(1, Math.min(p.getMaxHp(), Math.round(ratio * p.getMaxHp()))) : 0;
      if (p.hp > 0 && rnd(4) !== 0) p.hp = Math.max(1, Math.floor((p.getMaxHp() * rnd(25)) / 100));
    };
    const amMove = (p: any, move: MoveId) => {
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
    const amAttacker = (p: any) => {
      const r = rnd(10);
      amMove(p, r < 4 ? MoveId.TACKLE : r === 4 ? MoveId.QUICK_ATTACK : r === 5 ? MoveId.U_TURN
        : r === 6 ? MoveId.WATER_GUN : r === 7 ? MoveId.SEISMIC_TOSS : MoveId.BODY_SLAM);
      const k = rnd(8);
      if (k < 3) {
        p.summonData.ability = undefined;
        p.customPokemonData.ability = [AbilityId.MAGIC_GUARD, AbilityId.LONG_REACH, AbilityId.DAMP][k];
        const ratio = p.hp / Math.max(1, p.getMaxHp());
        p.calculateStats();
        p.hp = p.hp > 0 ? Math.max(1, Math.min(p.getMaxHp(), Math.round(ratio * p.getMaxHp()))) : 0;
      }
      if (p.hp > 0 && rnd(3) === 0) p.hp = Math.max(1, Math.floor((p.getMaxHp() * (1 + rnd(30))) / 100));
    };
    {
      const a = rnd(4), d = rnd(4);
      if (d !== 0) amHolder(D);
      if (a === 0) amHolder(A);
      if (d !== 0 || rnd(2) === 0) amAttacker(A);
      if (a === 0 || rnd(3) === 0) amAttacker(D);
      for (const b of party.slice(1)) if (rnd(3) === 0) amHolder(b);
      if (TRAINER) for (const b of scene.getEnemyParty().slice(1)) if (rnd(3) === 0) amHolder(b);
    }
    if (SUMMON && rnd(3) === 0) {
      // the player switches (CommandPhase.tryLeaveField): a bench Pokemon that hasn't fainted''')

Path(sys.argv[2]).write_text(s)
