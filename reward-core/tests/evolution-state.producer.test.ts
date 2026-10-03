/* Additive Task 89 application-stage producer. No EvolutionPhase is executed.
 * Copies into external upstream test/tests/zz-dump/. The queue boundary is
 * captured, while selection, modifier construction and addModifier stay real.
 * EVO_N=2000 EVO_SEED=777 EVO_OUT=/tmp/evo.txt node ...vitest... run this file.
 * Any unexpected queue call or exception fails; no caught/skipped cases. */
import { EvolutionItem } from "#balance/pokemon-evolutions";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { Status } from "#data/status-effect";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { PokemonMove } from "#moves/pokemon-move";
import { EvolutionItemModifierType } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

it("evolution application full-state producer", async () => {
  const N = Number(process.env.EVO_N ?? 2000);
  const SEED = Number(process.env.EVO_SEED ?? 777);
  let seed = SEED;
  const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE)
    .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const party = scene.getPlayerParty();
  const pool = Array.from({ length: 6 }, () => scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR), 5));
  const stones = [EvolutionItem.FIRE_STONE, EvolutionItem.WATER_STONE, EvolutionItem.THUNDER_STONE, EvolutionItem.DAWN_STONE];
  const species = [SpeciesId.VULPIX, SpeciesId.SHELLDER, SpeciesId.PIKACHU, SpeciesId.SNORUNT];
  const key = (s: string | null | undefined) => s == null ? "-" : s === "" ? "_" : s;
  // 63 numeric fields, matching evolutionstatetest.c; no pointer/padding serialization.
  const mon = (p: any) => [p.id, p.species.speciesId, p.formIndex, p.fusionSpecies?.speciesId ?? -1,
    p.fusionFormIndex ?? 0, p.gender, p.fusionGender ?? 0, p.level, p.exp, p.hp, p.getMaxHp(),
    p.status?.effect ?? 0, p.getNature(), p.teraType, p.friendship, p.getAbility(true).id,
    p.getAbility().id, p.hasPassive() ? p.getPassiveAbility().id : 0, +p.pauseEvolutions,
    +p.isAllowedInChallenge(), p.abilityIndex, p.fusionAbilityIndex ?? -1, +p.shiny, +p.fusionShiny,
    p.variant, p.fusionVariant ?? 0, p.luck, p.fusionLuck ?? 0, +p.passive, p.getMoveset().length,
    ...Array.from({ length: 4 }, (_, j) => { const m = p.getMoveset()[j]; return m ? [m.moveId, m.ppUsed, m.ppUp, m.getMovePp()] : [0, 0, 0, 0]; }).flat(),
    p.moveset.length, ...Array.from({ length: 4 }, (_, j) => p.moveset[j]?.moveId ?? 0), ...p.ivs, ...p.stats];
  const items = () => scene.modifiers.map((m: any) => [m.type.id, m.stackCount,
    m.pokemonId == null ? -1 : party.findIndex((p: any) => p.id === m.pokemonId), 0, 0, 1].join(" "));
  const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").map(Number).join(" ");
  const lines: string[] = [`V 1 ${N}`];
  const accepted = [0, 0, 0, 0], rejected = [0, 0, 0, 0];
  let fusionRequests = 0, pausedAccepted = 0;
  for (let i = 0; i < N; i++) {
    party.length = 0;
    scene.modifiers.length = 0;
    const count = 1 + rnd(6), target = rnd(count), family = i % 4;
    const valid = rnd(2) === 0, fusion = rnd(3) === 0;
    for (let j = 0; j < count; j++) {
      const p = pool[j];
      p.resetSummonData(); p.resetBattleAndWaveData();
      p.species = speciesDataRegistry.getSpecies(j === target && valid && !fusion ? species[family] : SpeciesId.BULBASAUR);
      p.formIndex = 0;
      p.fusionSpecies = j === target && valid && fusion ? speciesDataRegistry.getSpecies(species[family]) : null;
      p.fusionFormIndex = 0; p.abilityIndex = rnd(2); p.fusionAbilityIndex = p.fusionSpecies ? rnd(2) : 0;
      p.shiny = rnd(2) === 0; p.fusionShiny = rnd(2) === 0;
      p.variant = 0; p.fusionVariant = 0; p.fusionGender = rnd(3) - 1;
      p.luck = rnd(4); p.fusionLuck = rnd(4); p.gender = rnd(3) - 1;
      p.passive = rnd(2) === 0; p.level = 1 + rnd(100); p.exp = rnd(100000);
      p.ivs = Array.from({ length: 6 }, () => rnd(32)); p.nature = rnd(25);
      p.teraType = rnd(19); p.friendship = rnd(256); p.pauseEvolutions = rnd(2) === 0;
      p.moveset = [new PokemonMove(MoveId.TACKLE, rnd(4), rnd(4)), new PokemonMove(MoveId.GROWL, rnd(4), rnd(4))];
      p.calculateStats(); p.hp = rnd(p.getMaxHp() + 1);
      p.status = rnd(3) === 0 ? new Status(StatusEffect.POISON) : null;
      party.push(p);
    }
    const add = (name: string, owner: number) => {
      const f = (modifierTypes as any)[name]; const t = f().withIdFromFunc(f);
      const m = owner < 0 ? t.newModifier() : t.newModifier(party[owner]);
      m.stackCount = 1; scene.modifiers.push(m);
    };
    add("HEALING_CHARM", -1); add("LEFTOVERS", rnd(count));
    Phaser.Math.RND.sow([`evo-${SEED}-${i}`]);
    const beforeBattleRng = scene.currentBattle.rngState;
    lines.push(`C ${i} ${count} ${target} ${stones[family]}`, `R ${rng()}`);
    party.forEach((p: any) => {
      const state = mon(p); expect(state).toHaveLength(63);
      state.forEach(v => expect(Number.isFinite(v)).toBe(true));
      lines.push(`P ${state.join(" ")}`);
    });
    lines.push(`I ${items().length}`, ...items().map((s: string) => `H ${s}`));
    const queued: any[][] = [];
    const original = scene.phaseManager.unshiftNew;
    scene.phaseManager.unshiftNew = (name: string, ...args: any[]) => {
      expect(name).toBe("EvolutionPhase");
      queued.push(args);
    };
    let result: boolean;
    try {
      const t = new EvolutionItemModifierType(stones[family]);
      const filter = t.selectFilter(party[target]);
      result = filter == null && scene.addModifier(t.newModifier(party[target]), false, false);
    } finally { scene.phaseManager.unshiftNew = original; }
    lines.push(`A ${+result} ${queued.length}`);
    for (const [p, e, lastLevel] of queued) {
      const isFusion = e.primarySpeciesId != null;
      lines.push(["Q", p.id, isFusion ? e.primarySpeciesId : p.species.speciesId, e.speciesId,
        e.item, key(e.preFormKey), key(e.evoFormKey), lastLevel, e.level, +isFusion].join(" "));
      fusionRequests += +isFusion;
    }
    party.forEach((p: any) => lines.push(`T ${mon(p).join(" ")}`));
    lines.push(`J ${items().length}`, ...items().map((s: string) => `K ${s}`), `S ${rng()}`, "E");
    expect(scene.currentBattle.rngState).toBe(beforeBattleRng);
    (result ? accepted : rejected)[family]++;
    pausedAccepted += +(result && party[target].pauseEvolutions);
  }
  accepted.forEach(n => expect(n).toBeGreaterThan(0));
  rejected.forEach(n => expect(n).toBeGreaterThan(0));
  expect(fusionRequests).toBeGreaterThan(0); expect(pausedAccepted).toBeGreaterThan(0);
  lines.push(`Z ${N}`);
  fs.writeFileSync(process.env.EVO_OUT ?? "/tmp/evo-state.txt", lines.join("\n") + "\n");
  console.log(`EVOSTATE seed=${SEED} cases=${N} accepted=${accepted.join("/")} rejected=${rejected.join("/")} fusionRequests=${fusionRequests} pausedAccepted=${pausedAccepted} thrown=0 skipped=0`);
}, 120000);
