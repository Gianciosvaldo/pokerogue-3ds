/* The Expert Pokemon Breeder oracle (the harness family of mystery-trainer.producer.test.ts). The real encounter is started through the game manager; per case the
 * run and wave seeds, the biome (Space included: its own three Cleffa), the wave (25..175, never a boss wave or one ending in 1) and the party (levels, friendship with
 * ties, fainted members, held items, shiny luck) are changed, onInit runs as EncounterPhase runs it (in executeWithSeedOffset(.., waveIndex)), the option's
 * onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500). initBattleWithEnemyConfig is REALLY run for the fight (the trainer, its three Pokemon with
 * the config's fields, the levels and the held items are the real ones; the assets and the intro visuals complete at once). setEncounterRewards and
 * handleMysteryEncounterBattleFailed are recorded. The battle's SummonPhase (updateModifiers) runs between the option and its end; then the win's rewards callback
 * (doPostEncounterCleanup) or, in half of the cases, encounter.onGameOver() runs, and the party, the friendship and the held items after it are recorded.
 * GC_BRD_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import * as AnimUtils from "#utils/anim-utils";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { EnemyPokemon } from "#field/pokemon";
import { Trainer } from "#field/trainer";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { timedEventManager } from "#app/global-event-manager";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_BRD_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.PIKACHU, SpeciesId.BULBASAUR, SpeciesId.EEVEE, SpeciesId.MAGIKARP],
  [SpeciesId.MEWTWO, SpeciesId.CATERPIE, SpeciesId.SNORLAX, SpeciesId.RATTATA, SpeciesId.ARCEUS],
  [SpeciesId.SLUGMA, SpeciesId.LEAFEON, SpeciesId.CHERUBI, SpeciesId.SNORLAX, SpeciesId.BLISSEY, SpeciesId.DITTO],
  [SpeciesId.LEAFEON, SpeciesId.TROPIUS, SpeciesId.SNORLAX, SpeciesId.PICHU],
  [SpeciesId.GROUDON, SpeciesId.KYOGRE, SpeciesId.RAYQUAZA, SpeciesId.ZACIAN, SpeciesId.ETERNATUS],
];
const BIOMES = [BiomeId.TEMPLE, BiomeId.TALL_GRASS, BiomeId.BEACH, BiomeId.GRASS, BiomeId.ISLAND, BiomeId.RUINS, BiomeId.SPACE, BiomeId.GRAVEYARD, BiomeId.BADLANDS, BiomeId.MEADOW, BiomeId.PLAINS, BiomeId.FOREST, BiomeId.SEA, BiomeId.VOLCANO, BiomeId.SPACE, BiomeId.CAVE];

const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "MINT": return t.nature;
    case "SPECIES_STAT_BOOSTER": case "RARE_SPECIES_STAT_BOOSTER":
      return ["LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_TOOTH", "DEEP_SEA_SCALE"].indexOf(t.key);
  }
  return -1;
};

const lines: string[] = [`V 1 ${N}`];
const cover = { options: [0, 0, 0], space: 0, wins: 0, losses: 0, eggs: [0, 0], ties: 0, items: 0, female: 0, errors: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

/** a Pokemon as one record: species form level boss segments ivs nature abilityIndex passive shiny variant gender friendship id hp max stats n moves */
const dump = (e: any) => `${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`;
/** what the encounter's config adds: the ball, the tera type, the ability, the luck, the slot, the name */
const extra = (e: any) => `${e.pokeball} ${e.teraType} ${e.getAbility().id} ${e.luck} ${e.trainerSlot} ${e.fusionSpecies ? e.fusionSpecies.speciesId : 0} ${e.name.replace(/\s+/g, "_")}`;
const tokenOf = (s: string | undefined) => (s ?? "").replace(/\s+/g, "_").replace(/\n/g, "|");

for (let ci = 0; ci < configs.length; ci++) {

  it(`the expert breeder, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(42).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.PLAINS, [MysteryEncounterType.THE_EXPERT_POKEMON_BREEDER]]]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.THE_EXPERT_POKEMON_BREEDER, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(3);
    scene.time.delayedCall = () => ({}) as any;
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    const failed = vi.spyOn(EncounterPhaseUtils, "handleMysteryEncounterBattleFailed").mockImplementation(() => {});
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((async () => {}) as any);
    vi.spyOn(AnimUtils, "playTween").mockImplementation((async () => {}) as any);
    vi.spyOn(EnemyPokemon.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(Trainer.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(Trainer.prototype, "initSprite").mockImplementation((() => {}) as any);
    vi.spyOn(Trainer.prototype, "getSprites").mockImplementation((() => []) as any);
    vi.spyOn(Trainer.prototype, "getTintSprites").mockImplementation((() => []) as any);
    scene.tweens.add = (cfg: any) => { cfg.onComplete?.(); return { stop: () => {}, remove: () => {}, isPlaying: () => false } as any; };
    const original = [...party];
    lines.push(`P ${party.length} ${party.map((p: any) => `${p.species.speciesId} ${p.formIndex}`).join(" ")}`);
    const share = Math.floor(N / configs.length);
    const mk = (id: string, mon?: any, arg?: number, stack = 1) => () => {
      const type: any = arg === undefined ? (modifierTypes as any)[id]() : (EncounterPhaseUtils.generateModifierType((modifierTypes as any)[id], [arg]) as any);
      if (!type.id) type.id = id;
      const modifier = type.newModifier(mon);
      modifier.stackCount = stack;
      scene.addModifier(modifier, true);
    };
    const list = () => scene.modifiers.map((m: any) => {
      const idx = m.pokemonId == null ? -1 : original.findIndex((p: any) => p.id === m.pokemonId);
      return `${m.type.id} ${argOf(m.type)} ${m.pokemonId == null ? -1 : idx} ${m.stackCount}`;
    });
    const order = () => scene.getPlayerParty().map((p: any) => original.findIndex((q: any) => q === p)).join(" ");
    const pm = scene.phaseManager;
    const ids = (a: number[]) => a.join(" ");
    for (let i = 0; i < share; i++) {
      const k = i % 3, path = Math.floor(i / 3) % 2;
      let wave = 25 + rnd(150);
      while (wave % 10 === 0 || wave % 10 === 1) wave++;
      const runSeed = word(), waveSeed = word(), biome = BIOMES[rnd(BIOMES.length)];
      scene.setSeed(runSeed); scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.enemyParty = [];
      scene.newArena(biome);
      scene.party = [...original];
      encounter.seedOffset = undefined;
      encounter.updateSeedOffset();
      encounter.misc = null;
      encounter.dialogueTokens = {};
      encounter.doEncounterRewards = undefined;
      encounter.enemyPartyConfigs.length = 0;
      encounter.startOfBattleEffects.length = 0;
      scene.modifiers.length = 0;
      scene.enemyModifiers.length = 0;
      party.length = 0; party.push(...original);
      for (let m = 0; m < party.length; m++) {
        const mon = party[m];
        mon.level = 5 + rnd(96);
        mon.nature = rnd(25);
        mon.ivs = [0, 1, 2, 3, 4, 5].map(() => rnd(32));
        mon.shiny = rnd(5) === 0;
        mon.variant = mon.shiny ? rnd(3) : 0;
        mon.luck = mon.shiny ? mon.variant + 1 : 0;
        mon.abilityIndex = rnd(2);
        mon.friendship = rnd(3) === 0 ? 70 : rnd(256);
        mon.customPokemonData.ability = AbilityId.BALL_FETCH;
        mon.status = null;
        mon.calculateStats();
      }
      const adds: Array<() => void> = [];
      for (const mon of party) {
        if (rnd(2) === 0) adds.push(mk("BERRY", mon, rnd(11), 1 + rnd(3)));
        if (rnd(4) === 0) adds.push(mk("LEFTOVERS", mon));
        if (rnd(5) === 0) adds.push(mk("SOOTHE_BELL", mon));
      }
      if (rnd(2) === 0) adds.push(mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      /* about one case in five has a fainted member or two (fewer than three able ones is not tested: the encounter cannot be offered then) */
      for (const mon of party) mon.hp = 1 + rnd(mon.getMaxHp());
      if (rnd(5) === 0) party[rnd(party.length)].hp = 0;
      if (rnd(7) === 0) party[rnd(party.length)].hp = 0;
      while (party.filter((mon: any) => mon.hp > 0).length < 3) party.find((mon: any) => mon.hp <= 0).hp = 1;
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      scene.money = rnd(5000);
      const mons = party.map((mon: any) => `${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.nature} ${mon.friendship} ${mon.ivs.join(" ")} ${mon.hp} ${mon.getMaxHp()} ${mon.stats.join(" ")} ${mon.moveset.length} ${mon.moveset.map((mv: any) => mv.moveId).join(" ")} ${mon.luck} ${mon.getAbility().id}`);
      const before0 = list();
      const meets = encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${k} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId} ${scene.money} ${path}`, `U ${mons.join(" ; ")}`,
        `L ${before0.length} ${before0.join(" ")}`, `A ${meets.join(" ")}`);

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`brd-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      let initMsgs = 0;
      vi.spyOn(pm, "queueMessage").mockImplementation((() => { initMsgs++; }) as any);
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      vi.mocked(pm.queueMessage).mockRestore();
      const rngInit = rng();
      const cfg = encounter.enemyPartyConfigs[0], m = encounter.misc;
      const picks = [m.pokemon1, m.pokemon2, m.pokemon3].map((p: any) => original.indexOf(p));
      const sprite0 = encounter.spriteConfigs[0].species?.speciesId ?? encounter.spriteConfigs[0].species;
      lines.push(`R ${rngBefore}`, `I ${rngInit}`, `W ${initMsgs} ${picks.join(" ")} ${m.pokemon1CommonEggs} ${m.pokemon1RareEggs} ${m.pokemon2CommonEggs} ${m.pokemon2RareEggs} ${m.pokemon3CommonEggs} ${m.pokemon3RareEggs} ${sprite0}`,
        `Y ${cfg.trainerType} ${cfg.female === undefined ? -1 : cfg.female ? 1 : 0} ${cfg.doubleBattle ? 1 : 0} ${(cfg.pokemonConfigs ?? []).length} ${(cfg.pokemonConfigs ?? []).map((c: any) =>
          `${c.species.speciesId} ${tokenOf(c.nickname)} ${c.abilityIndex ?? -1} ${c.shiny ? 1 : 0} ${c.variant ?? -1} ${c.nature ?? -1} ${c.moveSet ? c.moveSet.join(",") : "-"} ${c.ivs ? c.ivs.join(",") : "-"} ${(c.modifierConfigs ?? []).length} ${c.modifierConfigs ? c.modifierConfigs[0].stackCount : 0} ${c.tera ?? -1} ${c.friendship ?? -1} ${c.isBoss ? 1 : 0}`).join(" ; ")}`,
        `K ${Object.keys(encounter.dialogueTokens).filter(t => /^pokemon\dName|Eggs$/.test(t)).sort().join(" ")}`,
        `K2 ${["pokemon1Name", "pokemon2Name", "pokemon3Name"].map(t => tokenOf(encounter.dialogueTokens[t])).join(" ")} ${["pokemon1", "pokemon2", "pokemon3"].map(n => ["Rare", "Common"].map(r => tokenOf(encounter.dialogueTokens[`${n}${r}Eggs`]) || "-").join(" ")).join(" ")}`);
      cover.space += biome === BiomeId.SPACE ? 1 : 0;
      cover.ties += new Set(party.map((p: any) => p.friendship)).size < party.length ? 1 : 0;

      const option = encounter.options[k];
      encounter.selectedOption = option;
      leave.mockClear(); rewards.mockClear(); failed.mockClear();
      const queued: string[] = [];
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...args: any[]) => { queued.push(args.length && args.every(a => typeof a === "number" || typeof a === "boolean") ? `${name}_${args.join("_")}` : name); }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      const generated: string[] = [];
      const origAdd = scene.addEnemyPokemon.bind(scene);
      vi.spyOn(scene, "addEnemyPokemon").mockImplementation(((...a: any[]) => { const r = (origAdd as any)(...a); const ms = r.getMoveset(); generated.push([0, 1, 2, 3].map(i => ms[i]?.moveId ?? 0).join(" ")); return r; }) as any);
      const pre = option.onPreOptionPhase ? 1 : 0;
      const rngPre = rng();
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      vi.mocked(scene.addEnemyPokemon).mockRestore();
      vi.mocked(pm.unshiftNew).mockRestore();
      vi.mocked(pm.pushNew).mockRestore();
      const afterOption = rng();
      const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
      const eggs: any[] = rewards.mock.calls.length ? ((rewards.mock.calls[0][1] as any[]) ?? []) : [];
      const funcs = (given?.guaranteedModifierTypeFuncs ?? []).map((f: any) => Object.entries(modifierTypes as any).filter(([, v]) => v === f).map(([key]) => key)[0]);
      const outro = (encounter.dialogue.outro ?? []).map((o: any) => `${tokenOf(o.speaker)}/${tokenOf(o.text)}`);
      lines.push(`G ${generated.join(" ; ")}`, `Q ${pre} ${rngPre} ${afterOption}`,
        `T ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${funcs.length} ${funcs.join(" ")} ${(given?.guaranteedModifierTiers ?? []).length} ${(given?.guaranteedModifierTypeOptions ?? []).length} ${given?.rerollMultiplier ?? 0} ${leave.mock.calls.length} ${rewards.mock.calls.length && rewards.mock.calls[0][2] ? 1 : 0} ${encounter.onGameOver ? 1 : 0}`,
        `Z2 ${eggs.length} ${eggs.map(e => `${e.tier} ${e.sourceType} ${e.pulled ? 1 : 0} ${tokenOf(e.eggDescriptor)}`).join(" ; ")}`,
        `OU ${outro.length} ${outro.join(" ; ")}`,
        `O ${scene.money} ${tokenOf(encounter.dialogueTokens.chosenPokemon)} ${encounter.misc.chosenPokemon ? original.indexOf(encounter.misc.chosenPokemon) : -1}`,
        `H ${queued.length} ${queued.join(" ")}`);
      cover.options[k]++;
      cover.eggs[0] += eggs.filter(e => e.tier === 0).length;
      cover.eggs[1] += eggs.filter(e => e.tier === 1).length;

      /* the fight: the real trainer and its three Pokemon */
      const battle = scene.currentBattle, enemies = scene.getEnemyParty(), trainer = battle.trainer;
      const enemyItems = scene.enemyModifiers.map((mm: any) => `${enemies.findIndex((p: any) => p.id === mm.pokemonId)} ${mm.type.id} ${argOf(mm.type)} ${mm.stackCount}`);
      lines.push(`D ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${trainer.config.trainerType} ${trainer.variant} ${trainer.partyTemplateIndex} ${trainer.nameKey ?? "-"} ${tokenOf(trainer.name)} ${trainer.partnerNameKey ?? "-"}`,
        `F ${encounter.startOfBattleEffects.length}`, `J ${enemyItems.length} ${enemyItems.join(" ")}`);
      lines.push(`X ${enemies.length} ${enemies.map((e: any) => dump(e)).join(" ; ")}`);
      lines.push(`X3 ${enemies.length} ${enemies.map((e: any) => extra(e)).join(" ; ")}`);
      lines.push(`N ${enemies.map((e: any) => `${e.status?.effect ?? 0} ${e.aiType}`).join(" ; ")} ${[...new Set(trainer.config.trainerAI.instantTeras as number[])].sort().join(",") || "-"} ${trainer.config.trainerAI.teraMode}`);
      cover.items += enemyItems.length > 0 ? 1 : 0;
      cover.female += trainer.variant === 1 ? 1 : 0;

      /* the player's side during the fight: only the chosen Pokemon is left, and the battle's SummonPhase runs updateModifiers */
      const partyDuring = order();
      scene.updateModifiers(true);
      const duringItems = list();
      lines.push(`B ${scene.getPlayerParty().length} ${partyDuring} | ${duringItems.length} ${duringItems.join(" ")}`);

      const chosen = encounter.misc.chosenPokemon, friendBefore = chosen.friendship;
      if (path === 0) {
        /* the win: the rewards callback (doPostEncounterCleanup) */
        const cb = rewards.mock.calls[0][2] as () => void;
        cb();
        const after = list();
        lines.push(`WIN ${scene.getPlayerParty().length} ${order()} | ${friendBefore} ${chosen.friendship} | ${after.length} ${after.join(" ")} | ${encounter.misc.encounterFailed ? 1 : 0}`);
        cover.wins++;
      } else {
        /* the loss: the lone Pokemon fainted, GameOverPhase asks the encounter */
        chosen.hp = 0;
        encounter.doEncounterRewards = () => true;
        let result: any = "error";
        try { result = encounter.onGameOver(); } catch (e: any) { cover.errors++; result = `error:${String(e.message).slice(0, 60).replace(/\s+/g, "_")}`; }
        const after = list();
        const outroAfter = (encounter.dialogue.outro ?? []).map((o: any) => `${tokenOf(o.speaker)}/${tokenOf(o.text)}`);
        lines.push(`LOSS ${result === false ? 0 : result === true ? 1 : result} ${scene.getPlayerParty().length} ${order()} | ${friendBefore} ${chosen.friendship} ${chosen.hp} | ${after.length} ${after.join(" ")} | ${encounter.misc.encounterFailed ? 1 : 0} ${encounter.doEncounterRewards ? 1 : 0} | ${failed.mock.calls.length} ${failed.mock.calls.length ? failed.mock.calls[0].map((a: any) => (a ? 1 : 0)).join("") : "-"} | ${outroAfter.length} ${outroAfter.join(" ; ")}`);
        cover.losses++;
      }
      lines.push("E");
      scene.party = [...original];
      party.length = 0; party.push(...original);
    }
  }, 1800000);
}

it("writes the expert breeder trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-breeder.txt", lines.join("\n") + "\n");
  console.log(`EXPERT BREEDER ${SEED}: options=${cover.options} space=${cover.space} wins=${cover.wins} losses=${cover.losses} eggs(common,rare)=${cover.eggs} friendship ties=${cover.ties} female=${cover.female} fights with items=${cover.items} errors=${cover.errors}`);
  expect(cover.options.every(n => n > 0)).toBe(true);
  expect(cover.errors).toBe(0);
});
