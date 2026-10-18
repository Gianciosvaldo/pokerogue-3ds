#!/usr/bin/env python3
"""Add an explicit Acupressure matrix to a copy of the unchanged doubles driver."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
s = (root/'tests/doubles.producer.test.ts').read_text()
needle = '    // the parties in field order:'
assert s.count(needle) == 1
s = s.replace(needle, '''    // Dedicated bounded fixture: ordinary non-fused actors, no held items/status.
    // All 128 capped-stat masks and native Simple/Contrary execution are included.
    const actors = [A0, A1, D0, D1];
    for (let k = 0; k < 4; k++) {
      const p = actors[k];
      p.species = speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR, SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE, SpeciesId.MAGIKARP][k]);
      p.formIndex = 0; p.fusionSpecies = null; p.level = 50; p.nature = 0; p.ivs = Array(6).fill(15);
      p.summonData.ability = [AbilityId.BALL_FETCH, AbilityId.SIMPLE, AbilityId.CONTRARY][(Math.floor(i / 128) + k) % 3];
      p.calculateStats(); p.generateName(); p.hp = p.getMaxHp();
      for (let j = 0; j < 7; j++) p.summonData.statStages[j] = (i % 128) & (1 << j) ? 6 : [-6, 0, 4, 5][(Math.floor(i / 384) + j) % 4];
      p.moveset = [new PokemonMove(MoveId.ACUPRESSURE)];
    }
    // The established doubles comparator also requires spread hits and multi-faints.
    if (i % 16 === 15) {
      A0.moveset = [new PokemonMove(MoveId.EXPLOSION)];
      A1.hp = D0.hp = D1.hp = 1;
    } else if (i % 16 === 14) {
      A0.moveset = [new PokemonMove(MoveId.SURF)];
    }
    // the parties in field order:''')
s = s.replace('it("doubles turn producer"', 'it("dedicated Acupressure complete doubles turns"')
# Append explicit two-opponent targeting controls, retaining every original
# matrix case (including the random Meloetta reserve at case 5535).
needle = '      command(p);'
assert s.count(needle) == 1
s = s.replace(needle, '''      if (i >= 8192 && p === A0) p.moveset = [new PokemonMove(MoveId.HYPER_VOICE)];
      command(p);''')
Path(sys.argv[1]).write_text(s)
