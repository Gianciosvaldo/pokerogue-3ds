/* The Winstrate Challenge's oracle (the harness family of mystery-superfan-fight.producer.test.ts). The real encounter is started through the game manager; per case the
 * run and wave seeds, the biome, the wave (100..180, never a boss wave or one ending in 1) and the party (levels, held items, shiny luck) are changed, onInit runs as
 * EncounterPhase runs it (in executeWithSeedOffset(.., waveIndex)). Option 1's onOptionPhase is REALLY run (no seed offset around the rest of it: it awaits first), then
 * doContinueEncounter (endTrainerBattleAndShowDialogue) four times and a fifth: the five trainers, their fixed teams, the levels and the held items are the real ones
 * (initBattleWithEnemyConfig, the assets and the intro visuals complete at once); the phases queued between the fights, the dialogue and the messages said, the voucher and
 * the reward are recorded. Option 2 (the refusal) is run the same way. GC_WS_N=200 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import * as AnimUtils from "#utils/anim-utils";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { EnemyPokemon } from "#field/pokemon";
import { Trainer } from "#field/trainer";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as EncounterDialogueUtils from "#mystery-encounters/encounter-dialogue-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { timedEventManager } from "#app/global-event-manager";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_WS_N ?? 200), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");


const configs: SpeciesId[][] = [
  [SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.MAGIKARP],
  [SpeciesId.MEWTWO, SpeciesId.CATERPIE, SpeciesId.SNORLAX, SpeciesId.RATTATA, SpeciesId.ARCEUS],
  [SpeciesId.SCYTHER, SpeciesId.LEAFEON, SpeciesId.CHERUBI, SpeciesId.SNORLAX, SpeciesId.BLISSEY, SpeciesId.DITTO],
  [SpeciesId.EISCUE, SpeciesId.TROPIUS, SpeciesId.PICHU, SpeciesId.GARDEVOIR],
];
const BIOMES = [BiomeId.TEMPLE, BiomeId.TALL_GRASS, BiomeId.BEACH, BiomeId.GRASS, BiomeId.ISLAND, BiomeId.RUINS, BiomeId.SPACE, BiomeId.GRAVEYARD, BiomeId.BADLANDS, BiomeId.MEADOW, BiomeId.PLAINS, BiomeId.FOREST, BiomeId.SEA, BiomeId.VOLCANO, BiomeId.CAVE];

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
const cover = { waves: 0, fights: 0, forms: 0, items: 0, females: 0, errors: 0, refusals: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 20).toBe(true);
});

/** a Pokemon as one record: species form level boss segments ivs nature abilityIndex passive shiny variant gender friendship id hp max stats n moves */
const dump = (e: any) => `${e.species.speciesId} ${e.formIndex} ${e.level} ${e.isBoss() ? 1 : 0} ${e.bossSegments ?? 0} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.passive ? 1 : 0} ${e.shiny ? 1 : 0} ${e.variant} ${e.gender} ${e.friendship} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")}`;
/** what the encounter's config adds: the ball, the tera type, the ability, the luck, the slot, the name */
const extra = (e: any) => `${e.pokeball} ${e.teraType} ${e.getAbility().id} ${e.luck} ${e.trainerSlot} ${e.fusionSpecies ? e.fusionSpecies.speciesId : 0} ${e.name.replace(/\s+/g, "_")}`;
const tokenOf = (s: string | undefined) => (s ?? "").replace(/\s+/g, "_").replace(/\n/g, "|");

for (let ci = 0; ci < configs.length; ci++) {

  it(`the winstrate challenge, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(104).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.PLAINS, [MysteryEncounterType.THE_WINSTRATE_CHALLENGE]]]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.THE_WINSTRATE_CHALLENGE, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(2);
    scene.time.delayedCall = () => ({}) as any;
    const rewards = vi.spyOn(EncounterPhaseUtils, "setEncounterRewards").mockImplementation(() => {});
    const leaves: string[] = [];
    vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(((addHeal?: boolean, mode?: any) => { leaves.push(`${addHeal ? 1 : 0} ${mode ?? "-"}`); }) as any);
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((async () => {}) as any);
    const said: string[] = [];
    vi.spyOn(EncounterDialogueUtils, "showEncounterDialogue").mockImplementation((async (text: string, speaker: string) => { said.push(`D:${text}@${speaker}`); }) as any);
    vi.spyOn(EncounterDialogueUtils, "showEncounterText").mockImplementation((async (text: string) => { said.push(`T:${text}`); }) as any);
    vi.spyOn(AnimUtils, "playTween").mockImplementation((async () => {}) as any);
    vi.spyOn(EnemyPokemon.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(Trainer.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(Trainer.prototype, "initSprite").mockImplementation((() => {}) as any);
    vi.spyOn(Trainer.prototype, "getSprites").mockImplementation((() => []) as any);
    vi.spyOn(Trainer.prototype, "getTintSprites").mockImplementation((() => []) as any);
    scene.tweens.add = (cfg: any) => { cfg.onComplete?.(); return { stop: () => {}, remove: () => {}, isPlaying: () => false } as any; };
    scene.field.remove = () => {};
    const pm = scene.phaseManager;
    const phases: string[] = [];
    vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...args: any[]) => { phases.push(`u:${name}${args.length ? ":" + args.map(a => String(a)).join(",") : ""}`); }) as any);
    vi.spyOn(pm, "pushNew").mockImplementation(((name: string, ...args: any[]) => { phases.push(`p:${name}${args.length ? ":" + args.map(a => String(a)).join(",") : ""}`); }) as any);
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
    for (let i = 0; i < share; i++) {
      let wave = 100 + rnd(80);
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
      encounter.doContinueEncounter = undefined;
      encounter.enemyPartyConfigs.length = 0;
      encounter.startOfBattleEffects.length = 0;
      scene.modifiers.length = 0;
      scene.enemyModifiers.length = 0;
      scene.currentBattle.trainer = null;
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
      }
      if (rnd(2) === 0) adds.push(mk("AMULET_COIN", undefined, undefined, 1 + rnd(5)));
      for (let a = adds.length - 1; a > 0; a--) { const b2 = rnd(a + 1); [adds[a], adds[b2]] = [adds[b2], adds[a]]; }
      adds.forEach(f => f());
      party.forEach((mon: any) => mon.calculateStats());
      for (const mon of party) mon.hp = 1 + rnd(mon.getMaxHp());
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      scene.money = rnd(5000);
      const voucherBefore = [...(scene.gameData.voucherCounts ? Object.values(scene.gameData.voucherCounts) : [])] as number[];

      const mons = party.map((mon: any) => `${mon.species.speciesId} ${mon.formIndex} ${mon.level} ${mon.luck}`);
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId} ${scene.money}`, `U ${mons.join(" ; ")}`);

      /* onInit, as EncounterPhase runs it */
      Phaser.Math.RND.sow([`ws-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      const rngInit = rng();
      lines.push(`R ${rngBefore}`, `I ${rngInit}`, `K ${encounter.enemyPartyConfigs.length} ${encounter.enemyPartyConfigs.map((c: any) => `${c.trainerType}:${c.pokemonConfigs.length}`).join(",")}`);

      const stepDump = (label: string) => {
        const battle = scene.currentBattle, enemies = scene.getEnemyParty(), trainer = battle.trainer;
        const enemyItems = scene.enemyModifiers.map((mm: any) => `${enemies.findIndex((p: any) => p.id === mm.pokemonId)} ${mm.type.id} ${argOf(mm.type)} ${mm.stackCount} ${mm.isTransferable ? 1 : 0}`);
        lines.push(`${label} ${rng()}`, `Ph ${phases.length} ${phases.join(" ")}`, `Sd ${said.length} ${said.map(tokenOf).join(" ")}`);
        if (trainer && enemies.length) {
          lines.push(`D ${battle.mysteryEncounter.encounterMode} ${battle.double ? 1 : 0} ${battle.enemyLevels.length} ${battle.enemyLevels.join(" ")} ${trainer.config.trainerType} ${trainer.variant} ${trainer.partyTemplateIndex} ${trainer.nameKey ?? "-"} ${tokenOf(trainer.name) || "-"} ${trainer.partnerNameKey ?? "-"}`,
            `F ${encounter.startOfBattleEffects.length}`, `J ${enemyItems.length} ${enemyItems.join(" ")}`,
            `X ${enemies.length} ${enemies.map((e: any) => dump(e)).join(" ; ")}`,
            `X3 ${enemies.length} ${enemies.map((e: any) => extra(e)).join(" ; ")}`,
            `N ${enemies.map((e: any) => `${e.status?.effect ?? 0} ${e.aiType} ${e.passive ? 1 : 0}`).join(" ; ")}`);
          cover.fights++;
          cover.forms += enemies.filter((e: any) => e.formIndex > 0).length;
          cover.items += enemyItems.length > 0 ? 1 : 0;
          cover.females += trainer.variant === 1 ? 1 : 0;
        } else {
          lines.push("D none");
        }
        phases.length = 0; said.length = 0;
      };

      const refuse = rnd(6) === 0;
      rewards.mockClear(); leaves.length = 0; phases.length = 0; said.length = 0;
      const option = encounter.options[refuse ? 1 : 0];
      encounter.selectedOption = option;
      lines.push(`O ${refuse ? 1 : 0}`);
      let done: Promise<boolean> | undefined;
      scene.executeWithSeedOffset(() => { done = option.onOptionPhase(); }, encounter.getSeedOffset() * 500);
      await done;
      stepDump("Q");
      if (refuse) {
        cover.refusals++;
        const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
        const funcs = (given?.guaranteedModifierTypeFuncs ?? []).map((f: any) => Object.entries(modifierTypes as any).filter(([, v]) => v === f).map(([key]) => key)[0]);
        lines.push(`T ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${funcs.length} ${funcs.join(" ")} ${(given?.guaranteedModifierTiers ?? []).length} ${(given?.guaranteedModifierTypeOptions ?? []).length} ${leaves.length} ${leaves.join(",")}`, "E");
        scene.enemyModifiers.length = 0; scene.currentBattle.enemyParty = []; scene.currentBattle.trainer = null;
        continue;
      }
      expect(typeof encounter.doContinueEncounter).toBe("function");
      for (let step = 1; step <= 5; step++) {
        try { await encounter.doContinueEncounter?.(); } catch (e) { cover.errors++; lines.push(`ERR ${String(e).slice(0, 80)}`); break; }
        stepDump(`Q${step}`);
        if (step === 5) {
          const given = rewards.mock.calls.length ? (rewards.mock.calls[0][0] as any) : undefined;
          const opts = (given?.guaranteedModifierTypeOptions ?? []).map((o: any) => `${o.type.id} ${o.type.tier} ${o.upgradeCount ?? 0} ${o.cost ?? 0}`);
          const voucherAfter = [...(scene.gameData.voucherCounts ? Object.values(scene.gameData.voucherCounts) : [])] as number[];
          const mods = scene.modifiers.map((m: any) => `${m.type.id}:${m.stackCount}`);
          lines.push(`T ${rewards.mock.calls.length} ${given?.fillRemaining ? 1 : 0} ${(given?.guaranteedModifierTypeFuncs ?? []).length} ${(given?.guaranteedModifierTiers ?? []).length} ${opts.length} ${opts.join(" ")} ${leaves.length} ${leaves.join(",")} ${encounter.doContinueEncounter ? 1 : 0}`,
            `B ${voucherBefore.join(",")} ${voucherAfter.join(",")} ${mods.length} ${mods.join(",")}`);
        }
      }
      lines.push("E");
      cover.waves++;
      scene.enemyModifiers.length = 0;
      scene.currentBattle.enemyParty = [];
      scene.currentBattle.trainer = null;
      scene.party = [...original];
      party.length = 0; party.push(...original);
    }
  }, 3600000);
}

it("writes the winstrate trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-winstrate.txt", lines.join("\n") + "\n");
  console.log(`WINSTRATE ${SEED}: runs=${cover.waves} refusals=${cover.refusals} fights=${cover.fights} forms=${cover.forms} fights with items=${cover.items} female trainers=${cover.females} errors=${cover.errors}`);
  expect(cover.errors).toBe(0);
});
