#!/usr/bin/env python3
"""Additive Tera Shift turns over the unchanged established turn producer.
Run it with TURN_TRAINER=1 TURN_SUMMON=1 and replay with the unchanged build/turntest.
Usage: build-tera-shift-turn-source.py <tests/turntest.producer.test.ts> <out.ts>"""
from pathlib import Path
import sys

s = Path(sys.argv[1]).read_text()

def replace(a, b):
    global s
    if s.count(a) != 1:
        raise SystemExit('Tera Shift turn source anchor changed: ' + a[:80])
    s = s.replace(a, b, 1)

replace('''    if (SUMMON && rnd(3) === 0) {
      // the player switches (CommandPhase.tryLeaveField): a bench Pokemon that hasn't fainted''',
'''    // Tera Shift overlay: Terapagos on the field and in reserves, all three forms, both sides; natural,
    // passive or overridden ability; some terastallized; switch-ins by command, U-turn and Baton Pass,
    // and knockouts
    // the reused battle keeps BattleType.CLEAR after a final-wave victory case; restore it per case
    battle.battleType = TRAINER ? BattleType.TRAINER : BattleType.WILD;
    const tsTerapagos = speciesDataRegistry.getSpecies(SpeciesId.TERAPAGOS);
    const tsSet = (p: any) => {
      const ratio = p.hp / Math.max(1, p.getMaxHp());
      p.species = tsTerapagos;
      p.formIndex = [0, 0, 1, 2][rnd(4)];
      p.fusionSpecies = null;
      const k = rnd(8);
      // the producer's enemy ability override (Ball Fetch) outranks customPokemonData, so an enemy gets
      // Tera Shift as a summon override (which leaveField clears) or as its passive
      p.summonData.ability = k === 2 && !p.isPlayer() ? AbilityId.TERA_SHIFT : undefined;
      p.customPokemonData.ability = k === 0 || k === 1 ? AbilityId.BALL_FETCH : k === 2 && p.isPlayer() ? AbilityId.TERA_SHIFT : -1;
      p.passive = k === 1;
      p.customPokemonData.passive = k === 1 ? AbilityId.TERA_SHIFT : AbilityId.NONE;
      p.isTerastallized = p.formIndex === 2 ? rnd(4) !== 0 : rnd(4) === 0;
      p.calculateStats();
      p.generateName();
      p.hp = p.hp > 0 ? Math.max(1, Math.min(p.getMaxHp(), Math.round(ratio * p.getMaxHp()))) : 0;
      // a Stellar one is often near fainting, so its Tera lapse (FaintPhase.resetTera) gets reached
      if (p.hp > 0 && p.formIndex === 2 && rnd(2) === 0) p.hp = Math.max(1, Math.floor(p.getMaxHp() / 20));
    };
    const tsMove = (p: any, move: MoveId) => {
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
    if (rnd(4) === 0) tsSet(A);
    if (rnd(4) === 0) tsSet(D);
    for (const b of party.slice(1)) if (rnd(2) === 0) tsSet(b);
    if (TRAINER) for (const b of scene.getEnemyParty().slice(1)) if (rnd(2) === 0) tsSet(b);
    {
      const r = rnd(12), q = rnd(12);
      if (r < 2) tsMove(A, MoveId.U_TURN);
      else if (r === 2) tsMove(A, MoveId.BATON_PASS);
      else if (r < 5) tsMove(A, MoveId.TACKLE);
      if (TRAINER && q < 2) tsMove(D, MoveId.U_TURN);
      else if (q < 4) tsMove(D, MoveId.TACKLE);
      if (rnd(2) === 0) {
        const bench = party.map((_: any, k: number) => k).filter((k: number) => k >= 1 && !party[k].isFainted() && party[k].species === tsTerapagos);
        if (bench.length) battle.turnCommands[0] = { command: Command.POKEMON, cursor: bench[rnd(bench.length)], args: [false] };
      }
      if (TRAINER && rnd(2) === 0) {
        const eparty = scene.getEnemyParty();
        const reserves = eparty.map((_: any, k: number) => k).filter((k: number) => k >= 1 && !eparty[k].isFainted() && eparty[k].species === tsTerapagos);
        if (reserves.length) battle.turnCommands[2] = { command: Command.POKEMON, cursor: reserves[rnd(reserves.length)], args: [false], skip: false };
      }
    }
    if (SUMMON && rnd(3) === 0) {
      // the player switches (CommandPhase.tryLeaveField): a bench Pokemon that hasn't fainted''')

# The player's Terapagos records (the producer's player ability override is NONE under TURN_SUMMON) also carry abilityIndex and customPokemonData.ability, so the replay can look its
# ability up again when the form changes (the turn replay checks the lookup against abil/babil)
replace('''          + (sd.stats.some((v: number) => v) ? ` ss=${L(sd.stats)}` : ""),''',
'''          + (sd.stats.some((v: number) => v) ? ` ss=${L(sd.stats)}` : "")
          + (p.isPlayer() && p.species.speciesId === SpeciesId.TERAPAGOS && !p.fusionSpecies ? ` aidx=${p.abilityIndex} cab=${p.customPokemonData.ability}` : ""),''')

Path(sys.argv[2]).write_text(s)
