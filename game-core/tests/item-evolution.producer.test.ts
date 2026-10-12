/* Additive evolution-item oracle (EvolutionItemModifierType.selectFilter, addModifier, EvolutionItemModifier.apply and
 * Pokemon.evolve) across every species with an item evolution. GC_IEVO_N (default 2000), GC_IEVO_SEED, GC_IEVO_OUT.
 * Environment (biome, time of day, caught species, held Deep Sea items, gender, moves, level, form) is randomised
 * around each evolution's own conditions so both outcomes are exercised. Node22. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { EvolutionItem } from "#balance/pokemon-evolutions";
import { allMoves, modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { LearnMoveSituation } from "#enums/learn-move-situation";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import { EvolutionItemModifierType } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

it("evolution item producer", async () => {
  const N = Number(process.env.GC_IEVO_N ?? 2000), SEED = Number(process.env.GC_IEVO_SEED ?? 777);
  expect(process.versions.node.split(".")[0]).toBe("22");
  let seed = SEED;
  const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.GROWLITHE);
  const scene = game.scene as any, p = scene.getPlayerParty()[0];
  vi.spyOn(p, "loadAssets").mockResolvedValue(undefined);
  vi.spyOn(p, "updateInfo").mockResolvedValue(undefined);
  vi.spyOn(scene, "updateModifiers").mockImplementation(() => {});
  const queued: any[][] = [];
  vi.spyOn(scene.phaseManager, "unshiftNew").mockImplementation((name: any, ...args: any[]) => { queued.push([name, ...args]); });
  vi.spyOn(scene.phaseManager, "unshiftPhase").mockImplementation(() => { throw new Error("unexpected phase"); });
  const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");
  const state = () => [p.species.speciesId, p.formIndex, p.level, p.abilityIndex, p.getAbility(true).id, +p.pauseEvolutions,
    p.hp, ...p.stats, p.moveset.length,
    ...Array.from({ length: 4 }, (_, j) => { const m = p.moveset[j]; return m ? [m.moveId, m.ppUsed, m.ppUp] : [0, 0, 0]; }).flat()];
  const ssbKeys = ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_SCALE", "DEEP_SEA_TOOTH"];
  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  const itemEvos: { species: number; evo: any }[] = [];
  const levelEvos: { species: number; evo: any }[] = [];
  for (const id of speciesIds)
    if (speciesDataRegistry.hasEvolutions(id))
      for (const e of speciesDataRegistry.getEvolutions(id)) (e.item ? itemEvos : levelEvos).push({ species: id, evo: e });
  const allItems = Object.values(EvolutionItem).filter((v: any) => typeof v === "number" && v > 0) as number[];
  const itemSpecies = [...new Set(itemEvos.map(x => x.species))];
  const moveIds = allMoves.map((m: any) => m.id).filter((id: number) => id > 0 && id !== MoveId.NONE);
  const biomes = Object.values(BiomeId).filter((v: any) => typeof v === "number") as number[];
  const lines = [`V 1 ${N}`];
  const cover = { accepted: 0, rejected: 0, wrongItem: 0, levelEvo: 0, TIME: [0, 0], BIOME: [0, 0], GENDER: [0, 0], SPECIES_CAUGHT: [0, 0], HELD_ITEM: [0, 0], MOVE_TYPE: [0, 0] } as any;   // [rejected, accepted]
  for (let i = 0; i < N; i++) {
    scene.modifiers.length = 0; queued.length = 0;
    p.resetSummonData(); p.resetBattleAndWaveData();
    const mode = i % 5;   // 0,1,2: a real item evolution; 3: right species, other item; 4: a level evolution, any item
    let base: { species: number; evo: any } | null = null;
    let speciesId: number, item: number;
    if (mode <= 2) { base = itemEvos[rnd(itemEvos.length)]; speciesId = base.species; item = base.evo.item; }
    else if (mode === 3) { speciesId = itemSpecies[rnd(itemSpecies.length)]; item = allItems[rnd(allItems.length)]; }
    else { base = levelEvos[rnd(levelEvos.length)]; speciesId = base.species; item = allItems[rnd(allItems.length)]; }
    const sp = speciesDataRegistry.getSpecies(speciesId);
    p.species = sp; p.fusionSpecies = null; p.fusionFormIndex = 0;
    // form: the evolution's preFormKey half of the time
    let form = rnd(Math.max(sp.forms.length, 1));
    if (base?.evo.preFormKey != null && rnd(3) !== 0) {
      const f = sp.forms.findIndex((x: any) => x.formKey === base!.evo.preFormKey);
      if (f >= 0) form = f;
    }
    p.formIndex = form;
    const evoLevel = base ? base.evo.level : 1;   // (set below: p.friendship and the nature are overridden by the conditions)
    p.level = rnd(4) === 0 ? 1 + rnd(100) : Math.min(100, Math.max(1, evoLevel + rnd(5) - 2));
    p.gender = [-1, 0, 1][rnd(3)]; p.nature = rnd(25); p.abilityIndex = rnd(3); p.friendship = rnd(256);
    p.shiny = rnd(2) === 0; p.variant = 0; p.passive = false; p.teraType = -1;
    p.pauseEvolutions = rnd(2) === 0;
    p.ivs = Array.from({ length: 6 }, () => rnd(32));
    // conditions of the chosen evolution drive the environment
    const conds: any[] = base?.evo.condition?.data ?? [];
    let moves: number[] = [];
    const caught: number[] = [];
    const held: number[] = [];
    let biome = biomes[rnd(biomes.length)];
    let want: number | null = null;
    for (const c of conds) {
      const yes = rnd(10) < 6;
      switch (c.key) {
        case 3: if (yes) moves.push(c.move); break;
        case 4: if (yes) { const m = allMoves.filter((x: any) => x.id > 0 && x.type === c.pkmnType).map((x: any) => x.id as number); if (m.length) moves.push(m[rnd(m.length)]); } break;
        case 7: if (yes) biome = c.biome[rnd(c.biome.length)]; break;
        case 2: if (yes) { const w = c.time[rnd(c.time.length)]; if (w >= 0) want = w; } break;
        case 12: if (yes) caught.push(c.speciesCaught); break;
        case 13: if (yes) p.gender = c.gender; break;
        case 1: p.friendship = yes ? Math.min(255, c.value + rnd(5)) : rnd(c.value); break;
        case 14: if (yes) p.nature = c.nature[rnd(c.nature.length)]; break;
        case 8: if (yes) moves.push(c.move); break;
        case 15: if (yes) held.push(ssbKeys.indexOf(c.itemKey)); break;
      }
    }
    for (let k = rnd(3); k > 0; k--) moves.push(moveIds[rnd(moveIds.length)]);
    moves = [...new Set(moves)].slice(0, 4);
    p.moveset = moves.map(id => new PokemonMove(id, rnd(4), rnd(4)));
    const wave = 1 + rnd(200);
    let offset = rnd(40);
    if (want !== null) {   // waveCycle < 15 DAY, < 20 DUSK, < 35 NIGHT, else DAWN; TimeOfDay DAWN=0 DAY=1 DUSK=2 NIGHT=3
      const cycle = [35 + rnd(5), rnd(15), 15 + rnd(5), 20 + rnd(15)][want];
      offset = (cycle - (wave % 40) + 40) % 40;
    }
    scene.arena.biomeId = biome; scene.currentBattle.waveIndex = wave; scene.waveCycleOffset = offset;
    p.calculateStats(); p.hp = rnd(p.getMaxHp() + 1);
    const root = sp.getRootSpeciesId(), rootCaught = rnd(4) !== 0;
    const interesting = new Set<number>([speciesId, root, ...itemEvos.filter(x => x.species === speciesId).map(x => x.evo.speciesId)]);
    const condCaught = new Set<number>(itemEvos.flatMap(x => (x.evo.condition?.data ?? []).filter((c: any) => c.key === 12).map((c: any) => c.speciesCaught)));
    for (const id of [...interesting, ...condCaught]) {
      const d = scene.gameData.dexData[id];
      d.seenAttr = d.caughtAttr = 0n; d.natureAttr = d.seenCount = d.caughtCount = d.hatchedCount = 0; d.ivs = [0, 0, 0, 0, 0, 0];
    }
    scene.gameData.dexData[speciesId].caughtAttr = 1n;
    if (rootCaught) scene.gameData.dexData[root].caughtAttr = 1n;
    for (const id of caught) scene.gameData.dexData[id].caughtAttr = 1n;
    const heldRare: number[] = [];
    for (const k of held) {
      let made = false;
      for (const name of ["SPECIES_STAT_BOOSTER", "RARE_SPECIES_STAT_BOOSTER"]) {
        const f = (modifierTypes as any)[name], g = f().generateType([], [ssbKeys[k]]);
        if (!g) continue;
        expect(g.key).toBe(ssbKeys[k]);
        scene.modifiers.push(g.withIdFromFunc(f).newModifier(p)); heldRare.push(+(name[0] === "R")); made = true; break;
      }
      expect(made).toBe(true);
    }
    Phaser.Math.RND.sow([`ie-${SEED}-${i}`]);
    const battleRng = scene.currentBattle.rngState;
    lines.push(`C ${i} ${speciesId} ${form} ${p.level} ${p.gender} ${p.friendship} ${p.nature} ${+p.pauseEvolutions} ${item} ${biome} ${wave} ${offset} ${+p.shiny} ${p.abilityIndex} ${p.ivs.join(" ")}`,
      `R ${rng()}`, `I ${state().join(" ")}`,
      `G ${root} ${+rootCaught} ${caught.length} ${caught.join(" ")}`, `H ${held.length} ${held.map((k, j) => `${heldRare[j]} ${k}`).join(" ")}`);
    // getEvolution() (the level-up check) on the same Pokemon: the evolved species, 0 = none
    const levelEvo = p.getEvolution();
    lines.push(`Q ${scene.seed} ${p.id} ${levelEvo ? levelEvo.speciesId : 0}`);
    const t = new EvolutionItemModifierType(item);
    t.id = item > 50 ? "RARE_EVOLUTION_ITEM" : "EVOLUTION_ITEM";
    t.tier = 2;
    let accepted = false;
    if (t.selectFilter(p) == null) scene.addModifier(t.newModifier(p), false, false);
    accepted = queued.length > 0;
    if (accepted) {
      expect(queued.length).toBe(1);
      expect(queued[0][0]).toBe("EvolutionPhase");
      const e = queued[0][2], before = p.species;
      await p.evolve(e, before);
      const lv = p.getLevelMoves({ startingLevel: p.level, includeEvolutionMoves: true, learnSituation: LearnMoveSituation.EVOLUTION }).filter((m: any[]) => m[0] === 0);
      for (const [, move] of lv) if (!p.moveset.some((m: any) => m.moveId === move) && p.moveset.length < 4) p.setMove(p.moveset.length, move);
    }
    lines.push(`A ${+accepted} ${queued.length}`, `T ${state().join(" ")}`, `D ${interesting.size}`);
    for (const id of interesting) {
      const d = scene.gameData.dexData[id];
      lines.push(`X ${id} ${d.seenAttr} ${d.caughtAttr} ${d.natureAttr} ${d.ivs.join(" ")}`);
    }
    lines.push(`S ${rng()}`, "E");
    expect(scene.currentBattle.rngState).toBe(battleRng);
    cover.levelEvo += +(levelEvo != null);
    cover.accepted += +accepted; cover.rejected += +!accepted; cover.wrongItem += +(mode >= 3);
    if (base) {
      for (const c of conds) {
        const name = ({ 2: "TIME", 7: "BIOME", 13: "GENDER", 12: "SPECIES_CAUGHT", 15: "HELD_ITEM", 4: "MOVE_TYPE" } as any)[c.key];
        if (name) cover[name][+accepted]++;
      }
    }
  }
  console.log(`IEVO ${SEED}: ${N} cases coverage ${JSON.stringify(cover)}`);
  expect(cover.levelEvo).toBeGreaterThan(N / 20);
  expect(cover.accepted).toBeGreaterThan(N / 10);
  expect(cover.rejected).toBeGreaterThan(N / 10);
  // per condition kind: [rejected, accepted]. Alcremie's nine biome evolutions together cover nearly every biome, so BIOME is
  // practically never rejected for a Milcery that is otherwise eligible
  expect(cover.BIOME[1]).toBeGreaterThan(0);
  for (const k of ["TIME", "GENDER", "SPECIES_CAUGHT", "HELD_ITEM", "MOVE_TYPE"]) { expect(cover[k][0]).toBeGreaterThan(0); expect(cover[k][1]).toBeGreaterThan(0); }
  lines.push(`Z ${N}`);
  fs.writeFileSync(process.env.GC_IEVO_OUT ?? "/tmp/game-item-evolution.txt", lines.join("\n") + "\n");
}, 600000);
