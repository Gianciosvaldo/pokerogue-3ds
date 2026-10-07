/* Additive pinned vitamin oracle: real selectFilter, PersistentModifier.add,
 * BattleScene.addModifier/updateModifiers and SelectModifierPhase.applyModifier.
 * Only UI error/phase-end boundaries are captured; stats/items/purchase logic are real.
 * VIT_N=2000 VIT_SEED=777 VIT_OUT=/abs/vitamin.txt, Node22. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { Status } from "#data/status-effect";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { PokemonMove } from "#moves/pokemon-move";
import { BaseStatBoosterModifierType } from "#modifiers/modifier-type";
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

it("bounded vitamin application producer", async () => {
  const N = Number(process.env.VIT_N ?? 2000), SEED = Number(process.env.VIT_SEED ?? 777);
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
    p.fusionFormIndex ?? 0, p.gender, p.fusionGender ?? 0, p.level, p.exp, p.hp, p.getMaxHp(),
    p.status?.effect ?? 0, p.getNature(), p.teraType, p.friendship, p.getAbility(true).id,
    p.getAbility().id, p.hasPassive() ? p.getPassiveAbility().id : 0, +p.pauseEvolutions,
    +p.isAllowedInChallenge(), p.abilityIndex, p.fusionAbilityIndex ?? -1, +p.shiny, +p.fusionShiny,
    p.variant, p.fusionVariant ?? 0, p.luck, p.fusionLuck ?? 0, +p.passive, p.getMoveset().length,
    ...Array.from({ length: 4 }, (_, j) => { const m = p.getMoveset()[j]; return m ? [m.moveId, m.ppUsed, m.ppUp, m.getMovePp()] : [0, 0, 0, 0]; }).flat(),
    p.moveset.length, ...Array.from({ length: 4 }, (_, j) => p.moveset[j]?.moveId ?? 0), ...p.ivs, ...p.stats, p.nature, p.customPokemonData.nature];
  const items = () => scene.modifiers.map((m: any) => [m.type.id, m.stackCount,
    m.pokemonId == null ? -1 : party.findIndex((p: any) => p.id === m.pokemonId),
    m.type.id === "BASE_STAT_BOOSTER" ? m.getArgs()[1] : 0, 0, 1].join(" "));
  const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").map(Number).join(" ");
  const lines: string[] = [`V 1 ${N}`], accepted = Array(6).fill(0), inoperable = Array(6).fill(0), full = Array(6).fill(0);
  let fused = 0, fainted = 0, custom = 0, forms = 0, wonder = 0, purchased = 0, rewardEnded = 0;
  const purchasedByStat = Array(6).fill(0), endedByStat = Array(6).fill(0);
  for (let i = 0; i < N; i++) {
    party.length = 0; scene.modifiers.length = 0;
    const count = 1 + rnd(6), target = rnd(count), stat = i % 6, mode = i % 5;
    const cost = Math.floor(i / 6) % 3 === 0 ? -1 : 50 + rnd(500), money = 1000 + rnd(10000);
    for (let j = 0; j < count; j++) {
      const p = pool[j]; p.resetSummonData(); p.resetBattleAndWaveData();
      p.species = speciesDataRegistry.getSpecies(species[rnd(species.length)]);
      p.formIndex = rnd(Math.max(p.species.forms.length, 1));
      p.fusionSpecies = rnd(3) === 0 ? speciesDataRegistry.getSpecies(species[rnd(species.length)]) : null;
      p.fusionFormIndex = p.fusionSpecies ? rnd(Math.max(p.fusionSpecies.forms.length, 1)) : 0;
      p.abilityIndex = 0; p.fusionAbilityIndex = 0; p.passive = rnd(2) === 0;
      p.shiny = rnd(2) === 0; p.fusionShiny = rnd(2) === 0; p.variant = 0; p.fusionVariant = 0;
      p.luck = rnd(4); p.fusionLuck = rnd(4); p.gender = rnd(3) - 1; p.fusionGender = rnd(3) - 1;
      p.level = 1 + rnd(100); p.exp = rnd(100000); p.ivs = Array.from({ length: 6 }, () => rnd(32));
      p.nature = rnd(25); p.customPokemonData.nature = rnd(2) === 0 ? rnd(25) : -1;
      if (j === target) p.ivs[stat] = mode === 0 ? 0 : 2 + rnd(30);
      p.teraType = rnd(19); p.friendship = rnd(256); p.pauseEvolutions = rnd(2) === 0;
      p.moveset = [new PokemonMove(MoveId.TACKLE, rnd(4), rnd(4)), new PokemonMove(MoveId.GROWL, rnd(4), rnd(4))];
      party.push(p);
    }
    const add = (name: string, owner: number, arg = 0, stack = 1) => {
      const f = (modifierTypes as any)[name];
      const t = name === "BASE_STAT_BOOSTER" ? new BaseStatBoosterModifierType(arg) : f().withIdFromFunc(f);
      t.id = name;
      const m = owner < 0 ? t.newModifier() : t.newModifier(party[owner]); m.stackCount = stack; scene.modifiers.push(m);
    };
    add("HEALING_CHARM", -1); add("LEFTOVERS", rnd(count));
    const p = party[target];
    if (mode === 1) add("BASE_STAT_BOOSTER", target, stat, p.ivs[stat]);
    else if (mode === 2 || mode === 4) add("BASE_STAT_BOOSTER", target, stat, 1 + rnd(p.ivs[stat] - 1));
    // Other stats/holders coexist; no duplicate holder/stat and no over-cap inputs.
    for (let j = 0; j < count; j++) {
      const other = (stat + 1 + rnd(5)) % 6;
      if (party[j].ivs[other]) add("BASE_STAT_BOOSTER", j, other, 1 + rnd(party[j].ivs[other]));
    }
    party.forEach((q: any, j: number) => {
      q.calculateStats(); q.hp = j === target && mode === 4 ? 0 : rnd(q.getMaxHp() + 1);
      q.status = rnd(3) === 0 ? new Status(StatusEffect.POISON) : null;
    });
    scene.money = money; Phaser.Math.RND.sow([`vit-${SEED}-${i}`]);
    const battleRng = scene.currentBattle.rngState;
    lines.push(`C ${i} ${count} ${target} ${stat} ${cost} ${money}`, `R ${rng()}`);
    party.forEach((q: any) => { const v = mon(q); expect(v).toHaveLength(65);
      v.forEach(n => expect(Number.isFinite(n)).toBe(true)); lines.push(`P ${v.join(" ")}`); });
    lines.push(`I ${items().length}`, ...items().map((v: string) => `H ${v}`));
    let sceneResult = false, added = false, error = 0, ended = 0;
    const originalAdd = scene.addModifier, originalError = scene.ui.playError;
    const originalShift = scene.phaseManager.shiftPhase, originalQueue = scene.phaseManager.unshiftNew;
    scene.addModifier = function (...args: any[]) { sceneResult = originalAdd.apply(this, args); return sceneResult; };
    scene.ui.playError = () => { error++; };
    scene.phaseManager.shiftPhase = () => { ended++; };
    scene.phaseManager.unshiftNew = (name: string) => { throw new Error(`unexpected vitamin queue ${name}`); };
    fused += +(p.fusionSpecies != null); fainted += +(p.hp === 0); custom += +(p.customPokemonData.nature >= 0);
    forms += +(p.formIndex > 0 || p.fusionFormIndex > 0);
    wonder += +(p.getAbility(true).id === AbilityId.WONDER_GUARD || (p.hasPassive() && p.getPassiveAbility().id === AbilityId.WONDER_GUARD));
    let status = 0;
    try {
      const t = new BaseStatBoosterModifierType(stat); t.id = "BASE_STAT_BOOSTER";
      const filter = t.selectFilter!(p);
      if (filter != null) {
        status = t.newModifier(p).getMaxStackCount() === 0 ? 1 : 2;
      } else {
        status = 3;
        const m = t.newModifier(p), original = m.add;
        m.add = function (...args: any[]) { added = original.apply(this, args as any); return added; };
        (new SelectModifierPhase() as any).applyModifier(m, cost, false);
      }
    } finally {
      scene.addModifier = originalAdd; scene.ui.playError = originalError;
      scene.phaseManager.shiftPhase = originalShift; scene.phaseManager.unshiftNew = originalQueue;
    }
    lines.push(`A ${status} ${+added} ${+sceneResult} ${error} ${ended} ${scene.money}`);
    party.forEach((q: any) => lines.push(`T ${mon(q).join(" ")}`));
    lines.push(`J ${items().length}`, ...items().map((v: string) => `K ${v}`), `S ${rng()}`, "E");
    expect(scene.currentBattle.rngState).toBe(battleRng);
    expect(scene.money).toBe(money); // persistent addModifier returns false, even when added
    expect(sceneResult).toBe(false);
    if (status === 3) {
      accepted[stat]++; purchased += +(cost >= 0); rewardEnded += ended;
      purchasedByStat[stat] += +(cost >= 0); endedByStat[stat] += ended;
    }
    else (status === 1 ? inoperable : full)[stat]++;
  }
  for (const counts of [accepted, inoperable, full, purchasedByStat, endedByStat]) counts.forEach(n => expect(n).toBeGreaterThan(0));
  for (const n of [fused, fainted, custom, forms, wonder, purchased, rewardEnded]) expect(n).toBeGreaterThan(0);
  lines.push(`Z ${N}`);
  fs.writeFileSync(process.env.VIT_OUT ?? "/tmp/vitamin-state.txt", lines.join("\n") + "\n");
  console.log(`VITAMIN seed=${SEED} cases=${N} applied=${accepted.join("/")} inoperable=${inoperable.join("/")} full=${full.join("/")} fusion=${fused} fainted=${fainted} custom=${custom} forms=${forms} wonder=${wonder} purchasedWithError=${purchased} rewardEnded=${rewardEnded} thrown=0 skipped=0`);
}, 120000);
