/*
 * Differential producer for Task 34 Challenge Mode validation.
 * It probes the pinned upstream rules for Single Generation, Single Type and Fresh Start.
 */
import { defaultStarterSpecies } from "#app/constants";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { PokemonType } from "#enums/pokemon-type";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("challenge validation producer", () => {
  new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  const N = Number(process.env.CHALLENGE_N ?? 10000);
  let seed = Number(process.env.CHALLENGE_SEED ?? 12345) >>> 0;
  const OUT = process.env.CHALLENGE_OUT ?? "/tmp/pr-challenges.txt";
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };

  const ids = Object.keys(speciesDataRegistry.data).map(Number);
  const defaultLines = new Set(defaultStarterSpecies.flatMap(id => [id, ...speciesDataRegistry.getEvolutionChain(id)]));

  const picked = () => {
    const species = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
    const form = species.forms.length ? rnd(species.forms.length) : 0;
    const fused = rnd(3) === 0;
    const fusion = fused ? speciesDataRegistry.getSpecies(ids[rnd(ids.length)]) : null;
    const fusionForm = fusion?.forms.length ? rnd(fusion.forms.length) : 0;
    return { species, form, fusion, fusionForm };
  };

  const formOf = (species: any, form: number) => species.forms.length ? species.forms[form] : species;
  const battleTypes = (m: ReturnType<typeof picked>) => {
    const base = formOf(m.species, m.form);
    const first = base.type1;
    let second = base.type2;
    if (m.fusion) {
      const f = formOf(m.fusion, m.fusionForm);
      if (f.type2 !== null && f.type2 !== first) second = f.type2;
      else if (f.type1 !== first) second = f.type1;
    }
    const types = new Set([first, second ?? PokemonType.UNKNOWN]);
    if (types.size > 1) types.delete(PokemonType.UNKNOWN);
    return [...types];
  };

  const lines: string[] = [];
  for (let i = 0; i < N; i++) {
    const a = picked();
    const b = picked();
    let flags = 0;
    if (rnd(2)) flags |= 1 << 0;
    if (rnd(2)) flags |= 1 << 1;
    if (rnd(2)) flags |= 1 << 4;
    if (!flags) flags = [1 << 0, 1 << 1, 1 << 4][rnd(3)];

    const gen = 1 + rnd(9);
    const typeValue = 1 + rnd(18);
    const fresh = 1 + rnd(2);
    const wantedType = typeValue - 1;

    const monAllowed = (m: ReturnType<typeof picked>) => {
      if ((flags & (1 << 0))
          && (m.species.generation !== gen || (m.fusion && m.fusion.generation !== gen))) return false;
      if (flags & (1 << 1)) {
        const override = wantedType === PokemonType.NORMAL && m.species.speciesId === SpeciesId.CASTFORM;
        if (!override && !battleTypes(m).includes(wantedType)) return false;
      }
      return true;
    };

    let starter = true;
    if ((flags & (1 << 0)) && a.species.generation !== gen) starter = false;
    if ((flags & (1 << 1))) {
      const f = formOf(a.species, a.form);
      if (![f.type1, f.type2].includes(wantedType)) starter = false;
    }
    if ((flags & (1 << 4)) && fresh === 1 && !defaultLines.has(a.species.speciesId)) starter = false;

    const allowedA = monAllowed(a);
    const allowedB = monAllowed(b);
    const capture = allowedA; // task admission rule: caught mon must be legal for the active formation
    const party = allowedA && allowedB;

    lines.push([
      flags, gen, typeValue, fresh,
      a.species.speciesId, a.form, a.fusion?.speciesId ?? 0, a.fusionForm,
      b.species.speciesId, b.form, b.fusion?.speciesId ?? 0, b.fusionForm,
      starter ? 1 : 0, allowedA ? 1 : 0, capture ? 1 : 0, party ? 1 : 0,
    ].join(" "));
  }
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
});
