import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { SpeciesFormChangeItemTrigger } from "#data/form-change-triggers";
import { AbilityId } from "#enums/ability-id";
import { FormChangeItem } from "#enums/form-change-item";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { ModifierData } from "#system/modifier-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect, it } from "vitest";

it("pinned held form constructors and complete item-trigger rule matrix", async () => {
  const native = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("double").moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH)
    .enemySpecies(SpeciesId.MEW);
  await game.classicMode.startBattle(SpeciesId.MEW, SpeciesId.MEW);
  BattleScene.prototype.randBattleSeedInt = native;
  const scene: any = game.scene, pm: any = scene.phaseManager;
  const actors: any[] = [...scene.getPlayerField(), ...scene.getEnemyField()];
  expect(actors.length).toBe(4);
  const enums = Object.values(FormChangeItem).filter((v): v is number => typeof v === "number")
    .map(id => ({ id, key: FormChangeItem[id] }));
  expect(enums.length).toBe(159);
  const typeCache = new Map<string, any>();
  const make = (key: string, id: number, p: any) => {
    const cacheKey = `${key}:${id}`;
    if (!typeCache.has(cacheKey)) {
      const factory = (modifierTypes as any)[key], generator = factory().withIdFromFunc(factory);
      const type = generator.generateType([p], [id]);
      expect(type).not.toBeNull(); type.id = key; typeCache.set(cacheKey, type);
    }
    return typeCache.get(cacheKey).newModifier(p);
  };
  const constructors: any[] = [];
  for (const { id } of enums) for (const key of ["FORM_CHANGE_ITEM", "RARE_FORM_CHANGE_ITEM"])
    for (let owner = 0; owner < 4; owner++) for (const active of [false, true]) {
      const m: any = make(key, id, actors[owner]); m.active = active;
      const clone = m.clone(), data = new ModifierData(m, owner < 2);
      const restored: any = new ModifierData(JSON.parse(JSON.stringify(data)), owner < 2).toModifier(m.constructor);
      expect(restored).not.toBeNull();
      constructors.push({ key, item: id, owner, active, args: m.getArgs(),
        clone: { item: clone.formChangeItem, owner: clone.pokemonId, active: clone.active, stack: clone.stackCount,
          virtual: clone.virtualStackCount, locked: !clone.isTransferable },
        restored: { item: restored.formChangeItem, owner: restored.pokemonId, active: restored.active,
          stack: restored.stackCount, virtual: restored.virtualStackCount, locked: !restored.isTransferable },
        matchesClone: m.matchType(clone), max: m.getMaxStackCount() });
    }
  expect(constructors.length).toBe(2544);
  const rules: any[] = [], references: any[] = [];
  for (const species of speciesDataRegistry.getAllSpecies()) {
    if (!speciesDataRegistry.hasFormChanges(species.speciesId)) continue;
    for (const [index, fc] of speciesDataRegistry.getFormChanges(species.speciesId).entries()) {
      const trigger: any = fc.findTrigger(SpeciesFormChangeItemTrigger);
      if (!trigger) continue;
      const parts: any[] = (fc.trigger as any).triggers ?? [fc.trigger];
      expect(parts.every(t => ["SpeciesFormChangeItemTrigger", "SpeciesDefaultFormMatchTrigger"].includes(t.constructor.name))).toBe(true);
      const formIndex = (key: string) => species.forms.findIndex(f => f.formKey === key);
      const defaultTrigger = parts.find(t => t.constructor.name === "SpeciesDefaultFormMatchTrigger");
      const conditions: any[] = [];
      for (const c of fc.conditions) {
        expect(c.enforceFunc == null).toBe(true);
        const source = String(c.predicate).replace(/\s+/g, " ").replace(/__vite_ssr_import_\d+__\./g, "");
        const ability = /^\(p\) => p\.hasAbility\((\d+)\)$/.exec(source);
        if (ability) conditions.push({ ability: Number(ability[1]) });
        else if (source === "(_p) => !!globalScene.gameData.dexData[species].caughtAttr") {
          const real = scene.gameData.dexData, reads: number[] = [];
          scene.gameData.dexData = new Proxy({}, { get: (_t, key) => { reads.push(Number(key)); return { caughtAttr: 1n }; } });
          try { expect(c.predicate(actors[0])).toBe(true); } finally { scene.gameData.dexData = real; }
          expect(reads.length).toBe(1); conditions.push({ caught: reads[0] });
        } else throw new Error(`Unclassified pinned item-form condition: ${source}`);
      }
      expect(conditions.length).toBeLessThanOrEqual(1);
      const row = { species: species.speciesId, index, from: formIndex(fc.preFormKey), to: formIndex(fc.formKey),
        preKey: fc.preFormKey, toKey: fc.formKey, item: trigger.item, active: trigger.active, quiet: fc.quiet,
        defaultForm: defaultTrigger ? formIndex(defaultTrigger.formKey) : -1, conditions };
      expect(row.from).toBeGreaterThanOrEqual(0); expect(row.to).toBeGreaterThanOrEqual(0);
      rules.push(row); references.push(fc);
    }
  }
  expect(rules.length).toBe(416); expect(new Set(rules.map(r => r.species)).size).toBe(137);
  const caught = [...new Set(rules.flatMap(r => r.conditions.flatMap((c: any) => c.caught ? [c.caught] : [])))].sort((a, b) => a - b);
  expect(caught.length).toBe(6);
  const referenceIndex = new Map(references.map((fc, i) => [fc, i])), cases: any[] = [], seen = new Set<number>();
  let focus = -1, eligible: boolean | null = null, duringActive: boolean | null = null, main: any;
  const eligibleSeen = new Set<number>(), routeSeen = new Set<string>();
  references.forEach((fc, index) => {
    const nativeCanChange = fc.canChange.bind(fc);
    fc.canChange = (pokemon: any) => {
      const result = nativeCanChange(pokemon);
      if (index === focus) { eligible = result; duringActive = main.active; if (result) eligibleSeen.add(index); }
      return result;
    };
  });
  const observe = (rule: number, shape: number, mask: number, override: boolean, gender: number, nature: number, group: number) => {
    const row = rules[rule], actor = (shape & 8) ? 2 : 0, p = actors[actor];
    for (const q of actors) {
      q.species = speciesDataRegistry.getSpecies(SpeciesId.MEW); q.formIndex = 0; q.resetSummonData();
      q.summonData.ability = AbilityId.LEVITATE; q.fusionSpecies = null; q.passive = false; q.hp = 1;
    }
    if (group === 4) scene.field.remove(p);
    else if (scene.field.getIndex(p) < 0) scene.field.add(p);
    expect(p.isOnField()).toBe(group !== 4);
    p.species = speciesDataRegistry.getSpecies(row.species); p.formIndex = row.from;
    p.gender = gender; p.nature = nature; p.customPokemonData.nature = -1;
    expect(p.getNature()).toBe(nature);
    p.abilityIndex = 0; p.summonData.ability = override ? AbilityId.LEVITATE : AbilityId.NONE;
    p.summonData.abilitySuppressed = false;
    for (let j = 0; j < caught.length; j++) scene.gameData.dexData[caught[j]].caughtAttr = mask & (1 << j) ? 1n : 0n;
    scene.modifiers = []; scene.enemyModifiers = [];
    const m: any = make("FORM_CHANGE_ITEM", row.item, p); m.active = !!(shape & 1);
    m.pokemonId = shape & 4 ? p.id : (p.id === 1 ? 2 : 1);
    const side = shape & 16 ? scene.enemyModifiers : scene.modifiers;
    side.push(m);
    let companion = -1;
    if (row.species === SpeciesId.NECROZMA && row.preKey === "ultra") {
      companion = row.toKey === "dawn-wings" ? FormChangeItem.N_LUNARIZER : FormChangeItem.N_SOLARIZER;
      const companions = group === 5
        ? [FormChangeItem.N_LUNARIZER, FormChangeItem.N_SOLARIZER].filter((_v, i) => mask & (1 << i))
        : [companion];
      for (const item of companions) {
        const extra: any = make("FORM_CHANGE_ITEM", item, p); extra.pokemonId = group === 3 ? p.id : m.pokemonId; extra.active = true; side.push(extra);
      }
    }
    if (group === 3) {
      const matchingCopy: any = make("FORM_CHANGE_ITEM", row.item, p); matchingCopy.active = row.active;
      scene.modifiers.push(matchingCopy);
    }
    pm.phaseQueue.levels = [[]]; pm.phaseQueue.currentLevel = 0; pm.phaseQueue.deferredActive = false;
    const before = m.active, globalRng = Phaser.Math.RND.state(), battleRng = scene.currentBattle.battleSeedState;
    const baseAbility = p.getAbility(true).id, ability = p.getAbility().id;
    focus = rule; eligible = null; duringActive = null; main = m;
    const shouldApply = m.shouldApply(p), result = m.apply(p, !!(shape & 2));
    expect(eligible).not.toBeNull();
    const queue = pm.phaseQueue.levels.flat(); expect(queue.length).toBeLessThanOrEqual(1);
    const phase: any = queue[0], selected = phase ? referenceIndex.get(phase.formChange) : -1;
    if (phase) { expect(selected).not.toBeUndefined(); seen.add(selected!); }
    expect(m.active).toBe(before); expect(Phaser.Math.RND.state()).toBe(globalRng);
    expect(scene.currentBattle.battleSeedState).toBe(battleRng);
    cases.push({ rule, group, shape, mask, override, gender, nature, baseAbility, ability, companion,
      mods: [scene.modifiers.map((q: any) => [q.pokemonId, q.formChangeItem, q.active]),
        scene.enemyModifiers.map((q: any) => [q.pokemonId, q.formChangeItem, q.active])],
      onField: p.isOnField(), shouldApply, eligible, duringActive, result, selected, phase: phase?.phaseName ?? "", active: m.active });
    const snapshot = cases.at(-1), routeKey = `${rule}:${result}`;
    snapshot.routes = [];
    if (!routeSeen.has(routeKey)) {
      routeSeen.add(routeKey);
      for (const delayed of [false, true]) {
        pm.phaseQueue.levels = [[]]; pm.phaseQueue.currentLevel = 0; pm.phaseQueue.deferredActive = false;
        const sentinel = pm.create("MessagePhase", "held form routing sentinel");
        pm.pushPhase(sentinel);
        m.active = snapshot.duringActive;
        const routed = scene.triggerPokemonFormChange(p, SpeciesFormChangeItemTrigger, delayed);
        const levels = pm.phaseQueue.levels.map((level: any[]) => level.map(q => q === sentinel ? -2 : referenceIndex.get(q.formChange)));
        expect(levels.flat().every((q: any) => q !== undefined)).toBe(true);
        snapshot.routes.push({ delayed, result: routed, levels });
        m.active = before;
        expect(Phaser.Math.RND.state()).toBe(globalRng);
        expect(scene.currentBattle.battleSeedState).toBe(battleRng);
      }
    }
  };
  for (let rule = 0; rule < rules.length; rule++) {
    const row = rules[rule], gender = row.toKey === "female" ? 1 : 0, nature = row.toKey === "lowkey" ? 1 : 0;
    for (let shape = 0; shape < 32; shape++) for (let gate = 0; gate < 4; gate++)
      observe(rule, shape, gate & 2 ? 63 : 0, !!(gate & 1), gender, nature, 0);
    for (let shape = 0; shape < 32; shape++) for (let gate = 0; gate < 4; gate++)
      observe(rule, shape, gate & 2 ? 63 : 0, !!(gate & 1), gender, nature, 4);
    if (row.species === SpeciesId.NECROZMA && row.preKey === "ultra")
      for (let shape = 0; shape < 32; shape++) for (let companions = 0; companions < 4; companions++)
        observe(rule, shape, companions, false, gender, nature, 5);
    if (row.conditions.some((c: any) => c.caught))
      for (let shape = 0; shape < 32; shape++) for (const mask of [0, 1, 2, 4, 8, 16, 32, 63])
        observe(rule, shape, mask, false, gender, nature, 1);
    for (const shape of [1, 3, 9, 11, 17, 19, 25, 27])
      observe(rule, shape, 63, false, gender, nature, 3);
    if (row.defaultForm >= 0)
      for (let shape = 0; shape < 32; shape++) for (const gender of [-1, 0, 1, 2]) for (let nature = 0; nature < 25; nature++)
        observe(rule, shape, 63, false, gender, nature, 2);
  }
  fs.writeFileSync(process.env.HELD_FORM_OUT! + ".preliminary.json", JSON.stringify({
    eligible: eligibleSeen.size, selected: seen.size, missingSelected: rules.filter((_r, i) => !seen.has(i)),
    missingEligible: rules.filter((_r, i) => !eligibleSeen.has(i)), rules, cases
  }));
  // These simple reverse predicates are identical to an earlier rule's;
  // source first-match selection makes them eligible but never selected.
  const shadowed: number[] = [];
  for (let i = 0; i < rules.length; i++) {
    const r = rules[i];
    if (r.species === SpeciesId.NECROZMA || r.conditions.length || r.defaultForm >= 0) continue;
    if (rules.slice(0, i).some(q => q.species === r.species && q.from === r.from && q.item === r.item
      && q.active === r.active && !q.conditions.length && q.defaultForm < 0)) shadowed.push(i);
  }
  expect(shadowed.map(i => [rules[i].species, rules[i].index])).toEqual([
    [25, 3], [133, 3], [718, 7], ...Array.from({ length: 8 }, (_v, i) => [869, 10 + i]),
  ]);
  expect(cases.length).toBe(124416); expect(eligibleSeen.size).toBe(416);
  expect(rules.map((_r, i) => i).filter(i => !seen.has(i))).toEqual(shadowed);
  expect(seen.size).toBe(405); expect(routeSeen.size).toBe(832);
  fs.writeFileSync(process.env.HELD_FORM_OUT!, JSON.stringify({ version: 1,
    pin: "8555c08c823b856cbec4eb99ca84ea52a955836d", ids: actors.map(p => p.id), enums, caught, rules, shadowed, constructors, cases }) + "\n");
}, 240000);
