/* Game-core Mint adapter oracle at pinned 8555c08, Node22. Actual item
 * selection/addModifier/custom nature/stat calculation/dex unlocking run.
 * Transports all 54 modeled party fields, complete fixture inventory, exact
 * species/prevolution dex entries and RNG. No catches/skips/unexpected queues.
 * GC_MINT_N=2000 GC_MINT_SEED=777 GC_MINT_OUT=<trace>.
 */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { Status } from "#data/status-effect";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { PokemonMove } from "#moves/pokemon-move";
import { PokemonNatureChangeModifierType } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

it("bounded Mint application producer", async () => {
  const N = Number(process.env.GC_MINT_N ?? 2000), SEED = Number(process.env.GC_MINT_SEED ?? 777);
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 100).toBe(true);
  let seed = SEED;
  const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE)
    .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any, party = scene.getPlayerParty();
  const pool = Array.from({ length: 6 }, () => scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR), 5));
  const species = [SpeciesId.BULBASAUR, SpeciesId.IVYSAUR, SpeciesId.PIKACHU, SpeciesId.SHEDINJA, SpeciesId.DEOXYS, SpeciesId.CHARIZARD];
  const mon = (p: any) => [p.id, p.species.speciesId, p.formIndex, p.fusionSpecies?.speciesId ?? -1,
    p.fusionFormIndex ?? 0, p.level, p.exp, p.hp, p.status?.effect ?? 0, p.getNature(),
    p.nature, p.customPokemonData.nature, p.getAbility(true).id, p.getAbility().id,
    p.hasPassive() ? p.getPassiveAbility().id : 0, p.friendship, p.gender, p.abilityIndex,
    p.fusionAbilityIndex ?? -1, +p.shiny, +p.passive, p.variant, p.luck, +p.pauseEvolutions,
    p.teraType, p.getMoveset().length,
    ...Array.from({length:4}, (_,j) => {const m=p.getMoveset()[j];return m?[m.moveId,m.ppUsed,m.ppUp,m.getMovePp()]:[0,0,0,0];}).flat(),
    ...p.ivs, ...p.stats];
  const ancestors = (id: number) => { const a: number[]=[]; do { a.push(id); id=speciesDataRegistry.getPrevolution(id)!; } while(id!=null); return a; };
  const dex = (ids: number[]) => ids.map(id => {const d=scene.gameData.dexData[id];return [id,Number(d.seenAttr),Number(d.caughtAttr),d.natureAttr,d.seenCount,d.caughtCount,d.hatchedCount,...d.ivs].join(' ');});
  const items = () => scene.modifiers.map((m: any) => [m.type.id, m.stackCount,
    m.pokemonId == null ? -1 : party.findIndex((p: any) => p.id === m.pokemonId), 0, 0, 1].join(" "));
  const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").map(Number).join(" ");
  const lines: string[] = [`V 2 ${N}`], accepted = Array(25).fill(0), rejected = Array(25).fill(0);
  let fused = 0, fainted = 0, custom = 0, forms = 0, wonder = 0;
  for (let i = 0; i < N; i++) {
    party.length = 0; scene.modifiers.length = 0;
    const count = 1 + rnd(6), target = rnd(count), nature = i % 25;
    for (let j = 0; j < count; j++) {
      const p = pool[j]; p.resetSummonData(); p.resetBattleAndWaveData();
      p.species = speciesDataRegistry.getSpecies(species[rnd(species.length)]);
      p.formIndex = rnd(Math.max(p.species.forms.length, 1));
      p.fusionSpecies = rnd(3) === 0 ? speciesDataRegistry.getSpecies(species[rnd(species.length)]) : null;
      p.fusionFormIndex = p.fusionSpecies ? rnd(Math.max(p.fusionSpecies.forms.length, 1)) : 0;
      p.abilityIndex = 0; p.fusionAbilityIndex = 0; p.passive = rnd(2) === 0;
      p.shiny = rnd(2) === 0; p.fusionShiny = rnd(2) === 0; p.variant = 0; p.fusionVariant = 0;
      p.luck = rnd(4); p.fusionLuck = rnd(4); p.gender = rnd(3) - 1; p.fusionGender = rnd(3) - 1;
      p.level = i % 7 === 0 ? 255 : 1 + rnd(100); p.exp = rnd(100000); p.ivs = Array.from({ length: 6 }, () => rnd(32));
      p.nature = rnd(25); p.customPokemonData.nature = rnd(2) === 0 ? rnd(25) : -1;
      if (j === target) {
        const before = i % 4 === 0 ? nature : (nature + 1 + rnd(24)) % 25;
        if (p.customPokemonData.nature < 0) p.nature = before; else p.customPokemonData.nature = before;
      }
      p.teraType = rnd(19); p.friendship = rnd(256); p.pauseEvolutions = rnd(2) === 0;
      p.moveset = [new PokemonMove(MoveId.TACKLE, rnd(4), rnd(4)), new PokemonMove(MoveId.GROWL, rnd(4), rnd(4))];
      p.calculateStats(); p.hp = rnd(p.getMaxHp() + 1);
      p.status = rnd(3) === 0 ? new Status(StatusEffect.POISON) : null;
      party.push(p);
    }
    const add = (name: string, owner: number) => {
      const f = (modifierTypes as any)[name], t = f().withIdFromFunc(f);
      const m = owner < 0 ? t.newModifier() : t.newModifier(party[owner]); m.stackCount = 1; scene.modifiers.push(m);
    };
    add("HEALING_CHARM", -1); add("LEFTOVERS", rnd(count));
    const ids = [...new Set(party.flatMap((p: any) => ancestors(p.species.speciesId)))].sort((a,b) => Number(a)-Number(b)) as number[];
    for (const id of ids) Object.assign(scene.gameData.dexData[id], {seenAttr:3n,caughtAttr:0n,natureAttr:1 << (rnd(25)+1),seenCount:11,caughtCount:7,hatchedCount:4,ivs:[1,2,3,4,5,6]});
    const chain = ancestors(party[target].species.speciesId);
    scene.gameData.dexData[chain.at(-1)!].caughtAttr = i % 3 === 0 ? 0n : 1n;
    Phaser.Math.RND.sow([`mint-${SEED}-${i}`]);
    const battleRng = scene.currentBattle.rngState;
    lines.push(`C ${i} ${count} ${target} ${nature}`, `R ${rng()}`);
    party.forEach((p: any) => {
      const values = mon(p); expect(values).toHaveLength(54);
      values.forEach(v => expect(Number.isFinite(v)).toBe(true)); lines.push(`P ${values.join(" ")}`);
    });
    lines.push(`I ${items().length}`, ...items().map((s: string) => `H ${s}`));
    lines.push(`D ${ids.length}`, ...dex(ids).map(s=>`B ${s}`));
    const requests: number[][] = [];
    const originalUnlock = scene.gameData.unlockSpeciesNature, originalQueue = scene.phaseManager.unshiftNew;
    scene.gameData.unlockSpeciesNature = (s: any, n: number) => {
      requests.push([s.speciesId, n]); return originalUnlock.call(scene.gameData, s, n);
    };
    scene.phaseManager.unshiftNew = (...args: any[]) => { throw new Error(`unexpected queue ${args[0]}`); };
    const p = party[target];
    fused += +(p.fusionSpecies != null); fainted += +(p.hp === 0); custom += +(p.customPokemonData.nature >= 0);
    forms += +(p.formIndex > 0 || p.fusionFormIndex > 0);
    wonder += +(p.getAbility(true).id === AbilityId.WONDER_GUARD || (p.hasPassive() && p.getPassiveAbility().id === AbilityId.WONDER_GUARD));
    let result: boolean;
    try {
      const t = new PokemonNatureChangeModifierType(nature);
      result = t.selectFilter(p) == null && scene.addModifier(t.newModifier(p), false, false);
    } finally { scene.gameData.unlockSpeciesNature = originalUnlock; scene.phaseManager.unshiftNew = originalQueue; }
    lines.push(`A ${+result} ${requests.length}`, ...requests.map(r => `Q ${r.join(" ")}`));
    party.forEach((p: any) => lines.push(`T ${mon(p).join(" ")}`));
    lines.push(`J ${items().length}`, ...items().map((s: string) => `K ${s}`), `S ${rng()}`, "E");
    lines.push(`L ${ids.length}`, ...dex(ids).map(s=>`U ${s}`));
    expect(scene.currentBattle.rngState).toBe(battleRng);
    (result ? accepted : rejected)[nature]++;
  }
  for (const counts of [accepted, rejected]) counts.forEach(n => expect(n).toBeGreaterThan(0));
  for (const n of [fused, fainted, custom, forms, wonder]) expect(n).toBeGreaterThan(0);
  lines.push(`Z ${N}`);
  fs.writeFileSync(process.env.GC_MINT_OUT ?? "/tmp/mint-state.txt", lines.join("\n") + "\n");
  console.log(`GC_MINT seed=${SEED} cases=${N} accepted=${accepted.join("/")} rejected=${rejected.join("/")} fusion=${fused} fainted=${fainted} custom=${custom} forms=${forms} wonder=${wonder} thrown=0 skipped=0`);
}, 120000);
