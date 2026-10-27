/* The DNA Splicers oracle (the harness family of mystery-gts.producer.test.ts and reward-core's apply.producer.test.ts). Per case a fresh
 * random party is built through the game manager's scene (species of every kind, forms, levels, IVs, natures, passives, some already
 * fused, a fifth fainted, statuses with their counters), held items of every kind are added in a shuffled order (vitamins and the
 * Mystery Encounter stat items among them: they change the stats), and one pair of members is fused the way the reward screen does it:
 * FusePokemonModifierType's selectFilter on both picks, then the modifier through BattleScene.addModifier, which runs
 * FusePokemonModifier.apply -> PlayerPokemon.fuse for real (the held-item transfer, updateModifiers, removePartyMemberModifiers, the splice
 * of the party). Nothing is mocked but what is only observed: the LearnMovePhases fuse() queues (phaseManager.unshiftNew is recorded, never
 * run) and getRandomStatus, whose unseeded coin flip is handed the value the case chose (Math.random, as it draws it) and counted.
 * Per case: the party before (every field game-core compares, the moveset, the stats), the held items in the list's order, the pair,
 * the coin and whether it was drawn, the filters' answers; then, when the modifier was applied, the party after in order (each by its
 * place in the party before: the fusion half, ability, HP, status and its counters, stats, luck), the items after in order, the
 * LearnMovePhases in the order they were queued, and the Phaser RNG before and after (a fusion draws nothing seeded).
 * GC_FUSION_N=600 GC_FUSION_SEED=777 GC_FUSION_OUT=/abs/fusion.txt, Node22. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { Status } from "#data/status-effect";
import * as statusHelpers from "#data/status-effect";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_FUSION_N ?? 600), SEED = Number(process.env.GC_FUSION_SEED ?? 777);
const OUT = process.env.GC_FUSION_OUT ?? "/tmp/fusion-cases.txt";
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const lines: string[] = [`V 1 ${N}`];
const cover = {
  fused: 0, refused: 0, coin: [0, 0], noCoin: 0, fainted: 0, bothFainted: 0, vitamins: 0, leftBehind: 0, locked: 0, tracker: 0, learn: 0,
  fieldFirst: 0, donorFirst: 0, species: new Set<number>(), shinyDonor: 0, wonderGuard: 0, alreadyFused: 0, passive: 0, items: new Set<string>(),
};
const ssbKeys = ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_SCALE", "DEEP_SEA_TOOTH"];
const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "SPECIES_STAT_BOOSTER": case "RARE_SPECIES_STAT_BOOSTER": return ssbKeys.indexOf(t.key);
    case "FORM_CHANGE_ITEM": case "RARE_FORM_CHANGE_ITEM": return t.formChangeItem;
    case "MYSTERY_ENCOUNTER_SHUCKLE_JUICE": return t.statModifier;
    case "MYSTERY_ENCOUNTER_OLD_GATEAU": return 0;
  }
  return -1;
};
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 40).toBe(true);
});

const CHUNK = 100;
const chunks = Math.ceil(N / CHUNK);
for (let ci = 0; ci < chunks; ci++) {
  it(`dna splicers, cases ${ci * CHUNK}..`, async () => {
    const game = new GameManager(phaserGame);
    game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
    await game.classicMode.startBattle(SpeciesId.BULBASAUR);
    const scene = game.scene as any, party: any[] = scene.getPlayerParty(), pm = scene.phaseManager;
    // the coin flip: getRandomStatus's one Math.random, handed the value the case chose and counted
    let coin = 0, draws = 0;
    const realStatus = statusHelpers.getRandomStatus;
    vi.spyOn(statusHelpers, "getRandomStatus").mockImplementation((a, b) => {
      const realRandom = Math.random;
      try {
        Math.random = () => { draws++; return (coin + 0.5) / 2; };
        return realStatus(a, b);
      } finally { Math.random = realRandom; }
    });
    const queued: Array<[string, any[]]> = [];
    vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...a: any[]) => { queued.push([name, a]); }) as any);
    vi.spyOn(pm, "pushNew").mockImplementation(((name: string, ...a: any[]) => { queued.push([`+${name}`, a]); }) as any);
    const all = speciesDataRegistry.getAllSpecies();
    const legends = all.filter((s: any) => s.legendary || s.subLegendary || s.mythical);
    const regionals = all.filter((s: any) => s.speciesId >= 2000);
    const pickSpecies = (): any => {
      const r = rnd(100);
      if (r < 3) return speciesDataRegistry.getSpecies(SpeciesId.SHEDINJA);
      if (r < 12) return legends[rnd(legends.length)];
      if (r < 22) return regionals[rnd(regionals.length)];
      return all[rnd(all.length)];
    };
    const heldPool: Array<[string, boolean, () => number | undefined]> = [
      ["LEFTOVERS", false, () => undefined], ["SHELL_BELL", false, () => undefined], ["SOUL_DEW", false, () => undefined], ["FOCUS_BAND", false, () => undefined],
      ["QUICK_CLAW", false, () => undefined], ["KINGS_ROCK", false, () => undefined], ["WIDE_LENS", false, () => undefined], ["MULTI_LENS", false, () => undefined],
      ["SCOPE_LENS", false, () => undefined], ["WHITE_HERB", false, () => undefined], ["LUCKY_EGG", false, () => undefined], ["GOLDEN_EGG", false, () => undefined],
      ["GRIP_CLAW", false, () => undefined], ["TOXIC_ORB", false, () => undefined], ["FLAME_ORB", false, () => undefined], ["MINI_BLACK_HOLE", false, () => undefined],
      ["REVIVER_SEED", false, () => undefined], ["EVIOLITE", false, () => undefined], ["BATON", false, () => undefined], ["IV_SCANNER", false, () => undefined],
      ["BERRY", true, () => rnd(11)], ["BASE_STAT_BOOSTER", true, () => rnd(6)], ["BASE_STAT_BOOSTER", true, () => rnd(6)], ["BASE_STAT_BOOSTER", true, () => 0],
      ["ATTACK_TYPE_BOOSTER", true, () => rnd(18)],
      // the two pools have no item in common (the Deep Sea items are the common ones), as in the game: upstream matches a held stack by class
      // and species list, game-core by key and argument
      ["SPECIES_STAT_BOOSTER", true, () => 4 + rnd(2)], ["RARE_SPECIES_STAT_BOOSTER", true, () => rnd(4)],
      ["FORM_CHANGE_ITEM", true, () => 1 + rnd(40)], ["RARE_FORM_CHANGE_ITEM", true, () => 100 + rnd(40)],
      ["EVOLUTION_TRACKER_GIMMIGHOUL", false, () => undefined], ["MYSTERY_ENCOUNTER_SHUCKLE_JUICE", true, () => (rnd(2) ? 10 : -15)],
      ["MYSTERY_ENCOUNTER_OLD_GATEAU", false, () => undefined], ["MYSTERY_ENCOUNTER_MACHO_BRACE", false, () => undefined],
    ];
    const funcs = (modifierTypes as any);
    const mk = (id: string, mon: any, arg: number | undefined, stack: number): boolean => {
      try {
        const gen = funcs[id];
        if (!gen) return false;
        let type: any;
        if (arg === undefined) type = gen();
        else if (id === "SPECIES_STAT_BOOSTER" || id === "RARE_SPECIES_STAT_BOOSTER") type = EncounterPhaseUtils.generateModifierType(gen, [ssbKeys[arg]]) as any;
        else type = EncounterPhaseUtils.generateModifierType(gen, [arg]) as any;
        if (!type) return false;
        if (!type.id) type.id = id;
        const modifier = type.newModifier(mon);
        if (!modifier) return false;
        modifier.stackCount = Math.max(1, Math.min(stack, modifier.getMaxStackCount()));   // never past the maximum, as the game cannot reach it
        scene.addModifier(modifier, true);
        return true;
      } catch {
        return false;
      }
    };
    const statuses = [StatusEffect.POISON, StatusEffect.TOXIC, StatusEffect.PARALYSIS, StatusEffect.SLEEP, StatusEffect.FREEZE, StatusEffect.BURN];
    const monTokens = (p: any): string => [
      p.id, p.species.speciesId, p.formIndex, p.level, p.gender, p.nature, p.abilityIndex, +p.shiny, p.variant, p.luck, p.friendship, ...p.ivs,
      p.hp, p.status?.effect ?? 0, p.status?.toxicTurnCount ?? 0, p.status?.sleepTurnsRemaining ?? -1, p.status?.freezeTurnsRemaining ?? -1,
      +!!p.pauseEvolutions, +!!p.passive,
      p.fusionSpecies ? p.fusionSpecies.speciesId : -1, p.fusionFormIndex ?? 0, p.fusionGender ?? 0, p.fusionAbilityIndex ?? -1, +!!p.fusionShiny, p.fusionVariant ?? 0, p.fusionLuck ?? 0,
      ...p.stats, p.moveset.length, ...p.moveset.map((m: any) => m.moveId),
    ].join(" ");
    const after = (p: any, idx: number): string => [
      idx, p.species.speciesId, p.formIndex, p.fusionSpecies ? p.fusionSpecies.speciesId : -1, p.fusionFormIndex ?? 0, p.fusionGender ?? 0, p.fusionAbilityIndex ?? -1,
      +!!p.fusionShiny, p.fusionVariant ?? 0, p.fusionLuck ?? 0, p.getAbility(true).id, p.getAbility().id,
      p.hp, p.status?.effect ?? 0, p.status?.toxicTurnCount ?? 0, p.status?.sleepTurnsRemaining ?? -1, p.status?.freezeTurnsRemaining ?? -1,
      +!!p.pauseEvolutions, ...p.stats, p.luck, p.getLuck(), p.moveset.length, ...p.moveset.map((m: any) => m.moveId),
    ].join(" ");
    const share = Math.min(CHUNK, N - ci * CHUNK);
    for (let i = 0; i < share; i++) {
      const caseNo = ci * CHUNK + i;
      Phaser.Math.RND.sow([`fusion-build-${SEED}-${caseNo}`]);
      scene.modifiers.length = 0;
      party.length = 0;
      const nParty = [2, 2, 3, 3, 3, 4, 4, 5, 6, 6, 6][rnd(11)];
      for (let j = 0; j < nParty; j++) {
        const sp = j > 0 && rnd(8) === 0 ? party[rnd(j)].species : pickSpecies();
        const level = 1 + rnd(rnd(2) ? 100 : 25);
        const mon = scene.addPlayerPokemon(sp, level);
        if (sp.forms.length > 1 && rnd(10) < 4) mon.formIndex = rnd(sp.forms.length);
        mon.fusionSpecies = null;
        mon.nature = rnd(25);
        mon.passive = rnd(2) === 0;
        mon.pauseEvolutions = rnd(5) === 0;
        mon.shiny = rnd(5) === 0;
        mon.variant = mon.shiny ? rnd(3) : 0;
        mon.luck = mon.shiny ? mon.variant + 1 : 0;
        const nm = 1 + rnd(4);
        const ids: number[] = [];
        while (ids.length < nm) { const id = 1 + rnd(MoveId.PSYCHIC_NOISE); if (!ids.includes(id)) ids.push(id); }
        mon.moveset = ids.map(id => new PokemonMove(id));
        if (rnd(8) === 0) {
          // already a fusion: the filter refuses it
          const half = all[rnd(all.length)];
          mon.fusionSpecies = half;
          mon.fusionFormIndex = half.forms.length > 1 ? rnd(half.forms.length) : 0;
          mon.fusionGender = rnd(3) - 1;
          mon.fusionAbilityIndex = rnd(2);
          mon.fusionShiny = rnd(4) === 0;
          mon.fusionVariant = mon.fusionShiny ? rnd(3) : 0;
          mon.fusionLuck = mon.fusionShiny ? mon.fusionVariant + 1 : 0;
        }
        mon.calculateStats();
        party.push(mon);
      }
      const original = [...party];
      const primary = rnd(party.length);
      const donor = (primary + 1 + rnd(party.length - 1)) % party.length;
      // held items: a few kinds each, stacked now and then, in a shuffled order; the run's own items too
      const adds: Array<() => void> = [];
      for (const mon of party) {
        const n = rnd(3) === 0 ? 0 : 1 + rnd(4);
        for (let a = 0; a < n; a++) {
          const [id, takesArg, argf] = heldPool[rnd(heldPool.length)];
          const arg = takesArg ? argf() : undefined, stack = 1 + rnd(rnd(3) === 0 ? 6 : 2);
          adds.push(() => { mk(id, mon, arg, stack); });
        }
      }
      // one Orb each, now and then: upstream matches the two Orbs as one class (a Pokemon holding either cannot take the other)
      if (rnd(8) === 0) {
        const [x, y] = rnd(2) ? ["TOXIC_ORB", "FLAME_ORB"] : ["FLAME_ORB", "TOXIC_ORB"];
        adds.push(() => { mk(x, party[primary], undefined, 1); }, () => { mk(y, party[donor], undefined, 1); });
      }
      if (rnd(2)) adds.push(() => { mk("AMULET_COIN", undefined, undefined, 1 + rnd(3)); });
      if (rnd(3) === 0) adds.push(() => { mk("EXP_SHARE", undefined, undefined, 1 + rnd(3)); });
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      // HP and status last (a held vitamin moved the maximum): a fifth fainted, a third with a status
      for (const mon of party) {
        const max = mon.getMaxHp();
        const r = rnd(8);
        mon.hp = r < 2 ? max : r === 2 ? 0 : 1 + rnd(max);
        const st = mon.hp === 0 ? (rnd(2) === 0 ? StatusEffect.FAINT : 0) : rnd(3) === 0 ? statuses[rnd(statuses.length)] : 0;
        mon.status = st
          ? new Status(st, st === StatusEffect.TOXIC ? rnd(6) : 0, st === StatusEffect.SLEEP ? 1 + rnd(4) : undefined, st === StatusEffect.FREEZE ? 1 + rnd(3) : undefined)
          : null;
      }
      // a fainted Pokemon has the FAINT status or none, a living one never has FAINT
      const settle = (m: any) => { if (m.hp > 0 && m.status?.effect === StatusEffect.FAINT) m.status = null; };
      // a pair that is mostly sound, with the odd refusal (a fused member) from the fusions above
      // more often than chance, make both living with a status (the coin flip), or the primary fainted with a living donor
      const shape = rnd(9);
      if (shape === 0) { for (const j of [primary, donor]) { const m = party[j]; if (!m.status || m.status.effect === StatusEffect.FAINT) { m.hp = Math.max(1, m.hp); m.status = new Status(statuses[rnd(statuses.length)], 0, undefined, undefined); } } }
      if (shape === 1) { party[primary].hp = 0; party[primary].status = null; if (party[donor].hp === 0) party[donor].hp = 1; }
      if (shape === 2) { party[donor].hp = 0; party[donor].status = null; if (party[primary].hp === 0) party[primary].hp = 1; }
      // one hit point against none: the average lands on a half hit point, where Math.round and floor(x + 0.5) part for some maxima
      if (shape === 3 || shape === 4) {
        const [one, none] = shape === 3 ? [primary, donor] : [donor, primary];
        party[one].hp = 1; party[one].status = null; party[none].hp = 0; party[none].status = null;
        // the fainted primary takes the status of the donor that is down to one hit point
        if (shape === 4 && rnd(2) === 0) party[one].status = new Status(statuses[rnd(statuses.length)], 0, undefined, undefined);
      }
      party.forEach(settle);
      coin = rnd(2);
      draws = 0;
      const mons = party.map(monTokens);
      const items = scene.modifiers.map((m: any) => `${m.type.id} ${argOf(m.type)} ${m.pokemonId == null ? -1 : original.findIndex((p: any) => p.id === m.pokemonId)} ${m.stackCount}`);
      const typeFn = (modifierTypes as any).DNA_SPLICERS;
      const type = typeFn();
      const filterA = type.selectFilter(party[primary]), filterB = type.selectFilter(party[donor]);
      const refused = filterA !== null || filterB !== null;
      lines.push(`C ${caseNo} ${party.length} ${primary} ${donor} ${coin}`, ...mons.map(m => `M ${m}`), `I ${items.length} ${items.join(" ")}`,
        `F ${refused ? 1 : 0}`);
      queued.length = 0;
      const rngBefore = rng();
      let applied = false;
      if (!refused) {
        const modifier = type.newModifier(party[primary], party[donor]);
        const before = scene.modifiers.length;
        const ret = scene.addModifier(modifier);
        applied = ret || scene.modifiers.length !== before || party.length < original.length;
      }
      const rngAfter = rng();
      expect(rngAfter).toBe(rngBefore);   // a fusion draws nothing seeded
      if (!applied) {
        expect(party.length).toBe(original.length);
        lines.push("R 0", "E");
        cover.refused++;
        continue;
      }
      expect(party.length).toBe(original.length - 1);
      expect(draws <= 1).toBe(true);
      const learn = queued.filter(([n]) => n === "LearnMovePhase");
      expect(learn.length).toBe(queued.length);
      lines.push(`R 1 ${draws}`, `S ${party.length} ${party.map((p: any) => after(p, original.indexOf(p))).join(" ; ")}`);
      const itemsAfter = scene.modifiers.map((m: any) => `${m.type.id} ${argOf(m.type)} ${m.pokemonId == null ? -1 : original.findIndex((p: any) => p.id === m.pokemonId)} ${m.stackCount}`);
      lines.push(`J ${itemsAfter.length} ${itemsAfter.join(" ")}`, `L ${learn.length} ${learn.map(([, a]) => `${a[0]} ${a[1]}`).join(" ")}`, "E");
      // coverage
      const p0 = original[primary], p1 = original[donor];
      cover.fused++;
      cover.coin[coin] += draws;
      if (!draws) cover.noCoin++;
      if (original[primary].hp === 0 || original[donor].hp === 0) cover.fainted++;
      cover.learn += learn.length;
      cover.species.add(p0.species.speciesId);
      if (p1.shiny) cover.shinyDonor++;
      if (p0.getAbility(true).id === 25 || p0.fusionSpecies && p0.getAbility().id === 25) cover.wonderGuard++;
      if (p0.passive) cover.passive++;
      for (const it of items) cover.items.add(it.split(" ")[0]);
      if (items.some((s: string) => s.startsWith("BASE_STAT_BOOSTER") && s.split(" ")[2] === String(donor))) cover.vitamins++;
      if (items.some((s: string) => s.startsWith("EVOLUTION_TRACKER"))) cover.tracker++;
      // a held item that stayed behind with the donor: more than what moved
      const donorStacks = items.filter((s: string) => s.split(" ")[2] === String(donor)).reduce((n: number, s: string) => n + Number(s.split(" ")[3]), 0);
      const gained = itemsAfter.filter((s: string) => s.split(" ")[2] === String(primary)).reduce((n: number, s: string) => n + Number(s.split(" ")[3]), 0)
        - items.filter((s: string) => s.split(" ")[2] === String(primary)).reduce((n: number, s: string) => n + Number(s.split(" ")[3]), 0);
      if (gained < donorStacks) cover.leftBehind++;
    }
  }, 1_800_000);
}

it("writes the trace", () => {
  expect(cover.fused).toBeGreaterThan(N * 0.45);
  expect(cover.refused).toBeGreaterThan(0);
  expect(cover.coin[0] + cover.coin[1]).toBeGreaterThan(0);
  lines.push(`# fused ${cover.fused} refused ${cover.refused} coin ${cover.coin.join("/")} nocoin ${cover.noCoin} fainted ${cover.fainted} vitamins ${cover.vitamins} leftBehind ${cover.leftBehind} tracker ${cover.tracker} learn ${cover.learn} wonderGuard ${cover.wonderGuard} species ${cover.species.size} items ${[...cover.items].sort().join(",")}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
  console.log(lines[lines.length - 1]);
});
