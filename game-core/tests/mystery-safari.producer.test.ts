/* The Safari Zone oracle (the harness family of mystery-breeder.producer.test.ts). The real encounter is started through the game manager; per case the run and wave seeds,
 * the biome, the wave (10..180, never a boss wave or one ending in 1), the money, the party (levels, a Shiny Charm and an Ability Charm in some cases) are changed, onInit runs
 * as EncounterPhase runs it (in executeWithSeedOffset(.., waveIndex)). Option 1 (enter) runs onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500) as
 * MysteryEncounterOptionSelectedPhase does, and so does each of the minigame's four options the oracle then picks (the encounter hands them to initSubsequentOptionSelect, which is
 * recorded): throw a ball, throw bait, throw mud, flee, until the safari ends. The animations complete at once, except the ball: trainerThrowPokeball is replaced by its roll
 * sequence (up to three randSeedInt(65536) < the twitch rate, one frame after the throw, i.e. outside the option's seed offset) and catchPokemon is recorded, not run (RP_CATCH
 * has its own differential). Every step records the RNG state, the Pokemon summoned, the catch and flee stages, the money and the texts.
 * GC_SAF_N=300 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { SpeciesId } from "#enums/species-id";
import { EnemyPokemon } from "#field/pokemon";
import * as AnimUtils from "#utils/anim-utils";
import * as EncounterDialogueUtils from "#mystery-encounters/encounter-dialogue-utils";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as EncounterPokemonUtils from "#mystery-encounters/encounter-pokemon-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { timedEventManager } from "#app/global-event-manager";
import * as Common from "#utils/common";
import { randSeedInt } from "#utils/common";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_SAF_N ?? 300), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = () => Array.from({ length: 24 }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");

const configs: SpeciesId[][] = [
  [SpeciesId.PIKACHU, SpeciesId.BULBASAUR, SpeciesId.EEVEE],
  [SpeciesId.MEWTWO, SpeciesId.CATERPIE, SpeciesId.SNORLAX, SpeciesId.RATTATA, SpeciesId.GENGAR],
  [SpeciesId.CHARIZARD],
];
const BIOMES = [BiomeId.FOREST, BiomeId.SWAMP, BiomeId.JUNGLE, BiomeId.PLAINS, BiomeId.TALL_GRASS];
const tokenOf = (s: any) => String(s ?? "").replace(/\s+/g, "_").replace(/\n/g, "|");

const lines: string[] = [`V 1 ${N}`];
const cover = { enters: 0, steps: [0, 0, 0, 0], catches: 0, fails: 0, flees: 0, shiny: 0, hidden: 0, full: 0, errors: 0 };

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 30).toBe(true);
});

/** a Pokemon as one record: species form level shiny variant ivs nature abilityIndex gender id hp max stats n moves, then luck and the ball-relevant catch rate */
const dump = (e: any) => `${e.species.speciesId} ${e.formIndex} ${e.level} ${e.shiny ? 1 : 0} ${e.variant} ${e.ivs.join(" ")} ${e.nature} ${e.abilityIndex} ${e.gender} ${e.id} ${e.hp} ${e.getMaxHp()} ${e.stats.join(" ")} ${e.getMoveset().length} ${e.getMoveset().map((m: any) => m.moveId).join(" ")} ${e.luck} ${e.species.catchRate} ${e.teraType}`;

for (let ci = 0; ci < configs.length; ci++) {
  it(`the safari zone, party ${ci}`, async () => {
    const game = new GameManager(phaserGame);
    game.override.mysteryEncounterChance(100).startingWave(42).startingBiome(BiomeId.PLAINS).disableTrainerWaves().startingLevel(5);
    const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.PLAINS, [MysteryEncounterType.SAFARI_ZONE]]]);
    vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
    vi.spyOn(timedEventManager, "getEventEncounters").mockReturnValue([]);
    vi.spyOn(timedEventManager, "activeEvent").mockReturnValue(undefined as any);
    await game.runToMysteryEncounter(MysteryEncounterType.SAFARI_ZONE, configs[ci]);
    const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty();
    expect(encounter.options.length).toBe(2);
    const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle").mockImplementation(() => {});
    vi.spyOn(EncounterPhaseUtils, "transitionMysteryEncounterIntroVisuals").mockImplementation((async () => {}) as any);
    vi.spyOn(AnimUtils, "playTween").mockImplementation((async () => {}) as any);
    vi.spyOn(EnemyPokemon.prototype, "loadAssets").mockImplementation((async () => {}) as any);
    vi.spyOn(EnemyPokemon.prototype, "init").mockImplementation((() => {}) as any);
    scene.loadSe = () => scene; scene.loadAtlas = () => scene;
    const stub: any = new Proxy(function () {}, { get: (_t, k) => (k === "then" ? undefined : k === Symbol.toPrimitive ? () => 0 : stub), apply: () => stub });
    const saved = { addFieldSprite: scene.addFieldSprite, add: scene.field.add, trainer: scene.trainer, delayedCall: scene.time.delayedCall, tweens: scene.tweens.add, loadSe: scene.loadSe, loadAtlas: scene.loadAtlas };
    const stubs = () => { scene.addFieldSprite = () => stub; scene.field.add = () => {}; scene.trainer = stub; };
    const unstub = () => { scene.addFieldSprite = saved.addFieldSprite; scene.field.add = saved.add; scene.trainer = saved.trainer; };
    vi.spyOn(EnemyPokemon.prototype, "getFieldPositionOffset").mockImplementation((() => [0, 0]) as any);
    const texts: string[] = [];
    vi.spyOn(EncounterDialogueUtils, "showEncounterText").mockImplementation((async (t: string) => { texts.push(`T:${tokenOf(t)}`); }) as any);
    /* the texts are recorded as their keys (getEncounterText returns its argument); the ones the animation helpers say are recorded under their keys by hand */
    vi.spyOn(EncounterDialogueUtils, "getEncounterText").mockImplementation(((k: string) => k) as any);
    vi.spyOn(EncounterPokemonUtils, "doPokemonFlee").mockImplementation((async () => { texts.push("T:battle:pokemonFled"); }) as any);
    vi.spyOn(EncounterPokemonUtils, "doPlayerFlee").mockImplementation((async () => { texts.push("T:battle:playerFled"); }) as any);
    /* the ball: the roll sequence of trainerThrowPokeball, one frame after the throw (outside the option's seed offset), then the catch is recorded */
    let twitch = -1;
    vi.spyOn(EncounterPokemonUtils, "trainerThrowPokeball").mockImplementation((async (_p: any, _t: any, rate: number) => {
      twitch = rate;
      await new Promise(r => setTimeout(r, 0));
      for (let s = 0; s < 3; s++) if (!(randSeedInt(65536) < rate)) { rolls.push(`f${s}`); texts.push("T:battle:pokemonBrokeFree"); return false; }
      rolls.push("ok");
      /* catchPokemon's VictoryPhase is exp only, and the wave has had no turn: nobody participated, so applyPartyExp gives nothing */
      expect(scene.currentBattle.playerParticipantIds.size).toBe(0);
      return true;
    }) as any);
    const rolls: string[] = [];
    /* every randSeedInt outside an executeWithSeedOffset (the run's RNG): "range:value" */
    const trace: string[] = [];
    let inSeed = 0;
    const origRsi = Common.randSeedInt;
    vi.spyOn(Common, "randSeedInt").mockImplementation(((range: number, min = 0) => { const v = origRsi(range, min); if (!inSeed) trace.push(`${range}:${v}`); return v; }) as any);
    const origSeedOffset = scene.executeWithSeedOffset;
    scene.executeWithSeedOffset = function (...a: any[]) { inSeed++; try { return origSeedOffset.apply(this, a); } finally { inSeed--; } };
    scene.time.delayedCall = (_ms: number, cb: () => void) => { cb?.(); return {} as any; };
    scene.tweens.add = (cfg: any) => { cfg.onStart?.(); cfg.onComplete?.(); return { stop: () => {}, remove: () => {}, isPlaying: () => false } as any; };
    const pm = scene.phaseManager;
    const original = [...party];
    lines.push(`P ${party.length} ${party.map((p: any) => `${p.species.speciesId} ${p.formIndex}`).join(" ")}`);
    const share = Math.floor(N / configs.length);
    for (let i = 0; i < share; i++) {
      let wave = 10 + rnd(171);
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
      encounter.continuousEncounter = false;
      encounter.dialogueTokens = {};
      scene.modifiers.length = 0;
      scene.enemyModifiers.length = 0;
      party.length = 0; party.push(...original);
      for (const mon of party) { mon.level = 5 + rnd(96); mon.status = null; mon.calculateStats(); mon.hp = mon.getMaxHp(); }
      const charms: string[] = [];
      if (rnd(3) === 0) { const t: any = (modifierTypes as any).SHINY_CHARM(); const m = t.newModifier(); m.stackCount = 1 + rnd(3); scene.addModifier(m, true); charms.push(`SHINY_CHARM ${m.stackCount}`); }
      if (rnd(3) === 0) { const t: any = (modifierTypes as any).ABILITY_CHARM(); const m = t.newModifier(); m.stackCount = 1 + rnd(3); scene.addModifier(m, true); charms.push(`ABILITY_CHARM ${m.stackCount}`); }
      scene.gameData.trainerId = rnd(65536);
      scene.gameData.secretId = rnd(65536);
      const cost = scene.getWaveMoneyAmount(2);
      scene.money = rnd(4) === 0 ? Math.max(0, cost - 1 - rnd(50)) : cost + rnd(5000);
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${wave} ${scene.arena.biomeId} ${scene.gameData.trainerId} ${scene.gameData.secretId} ${scene.money}`,
        `U ${party.map((mon: any) => `${mon.level}`).join(" ")}`, `L ${charms.length} ${charms.join(" ; ")}`,
        `A ${encounter.meetsRequirements() ? 1 : 0} ${encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0)).join(" ")}`);

      Phaser.Math.RND.sow([`saf-${SEED}-${ci}-${i}`]);
      const rngBefore = rng();
      scene.executeWithSeedOffset(() => { encounter.onInit(); encounter.populateDialogueTokensFromRequirements(); }, wave);
      lines.push(`R ${rngBefore}`, `K ${tokenOf(encounter.dialogueTokens.numEncounters)} ${tokenOf(encounter.dialogueTokens.money)}`);
      const option = encounter.options[0];
      if (!option.meetsRequirements()) { lines.push("E"); continue; }
      encounter.selectedOption = option;
      cover.enters++;

      let handed: any[] = [], cursor = -1;
      vi.spyOn(EncounterPhaseUtils, "initSubsequentOptionSelect").mockImplementation(((s: any) => { handed = s.overrideOptions; cursor = s.startingCursorIndex ?? 0; }) as any);
      const queued: string[] = [];
      vi.spyOn(pm, "unshiftNew").mockImplementation(((name: string, ...args: any[]) => { queued.push(`${name}_${args.join("_")}`); }) as any);
      vi.spyOn(pm, "pushNew").mockImplementation(((name: string) => { queued.push(`+${name}`); }) as any);
      vi.spyOn(pm, "queueMessage").mockImplementation(((t: string) => { queued.push(`msg:${/^You paid/.test(t) ? "mysteryEncounterMessages:paidMoney" : tokenOf(t)}`); }) as any);
      const run = async (opt: any, label: string) => {
        leave.mockClear(); texts.length = 0; queued.length = 0; rolls.length = 0; trace.length = 0; handed = []; cursor = -1; twitch = -1;
        const before = scene.money;
        let done: Promise<boolean> | undefined;
        stubs();
        scene.executeWithSeedOffset(() => { done = opt.onOptionPhase(); }, encounter.getSeedOffset() * 500);
        try { await done; } catch (e: any) { cover.errors++; lines.push(`X ${label} error ${String(e.message).slice(0, 80).replace(/\s+/g, "_")}`); return; } finally { unstub(); }
        const m = encounter.misc;
        const mon = m?.pokemon;
        lines.push(`S ${label} ${rng()}`,
          `M ${m?.safariPokemonRemaining ?? -1} ${m?.catchStage ?? 0} ${m?.fleeStage ?? 0} ${scene.money} ${before - scene.money} ${encounter.continuousEncounter ? 1 : 0}`,
          `N ${cursor} ${handed.length} ${leave.mock.calls.length} ${leave.mock.calls.length ? (leave.mock.calls[0][0] ? 1 : 0) : -1} ${twitch} ${rolls.join(",") || "-"} ${scene.getEnemyParty().length}`,
          `W ${tokenOf(encounter.dialogueTokens.remainingCount)} ${tokenOf(encounter.dialogueTokens.pokemonName)} ${texts.length} ${texts.join(" ")} ${queued.length} ${queued.join(" ")}`,
          `Y ${mon ? dump(mon) : "-"}`,
          `Q ${trace.length} ${trace.join(" ")}`);
      };
      await run(option, "enter");
      let steps = 0, moodId: any = undefined, mood = 0;
      while (handed.length === 4 && steps < 60) {
        steps++;
        /* the oracle's player: mostly bait and balls, some mud and fleeing */
        /* a different mood for each Pokemon, so that the stages go deep: mixed, bait-heavy or mud-heavy */
        const id = encounter.misc?.pokemon?.id;
        if (id !== moodId) { moodId = id; mood = rnd(3); }
        const r = rnd(20);
        const a = mood === 0 ? (r < 8 ? 0 : r < 12 ? 1 : r < 16 ? 2 : 3)
          : mood === 1 ? (r < 4 ? 0 : r < 17 ? 1 : r < 19 ? 2 : 3)
          : (r < 4 ? 0 : r < 6 ? 1 : r < 18 ? 2 : 3);
        cover.steps[a]++;
        const opts = handed;
        await run(opts[a], `a${a}`);
        if (a === 0 && rolls.length && rolls[0] === "ok") cover.catches++;
        if (a === 0 && rolls.length && rolls[0] !== "ok") cover.fails++;
        if (a === 3) cover.flees++;
        if (encounter.misc?.pokemon?.shiny) cover.shiny++;
        if (encounter.misc?.pokemon?.abilityIndex === 2) cover.hidden++;
      }
      vi.mocked(pm.unshiftNew).mockRestore(); vi.mocked(pm.pushNew).mockRestore(); vi.mocked(pm.queueMessage).mockRestore();
      vi.mocked(EncounterPhaseUtils.initSubsequentOptionSelect).mockRestore();
      lines.push("E");
      scene.party = [...original];
      party.length = 0; party.push(...original);
    }
    scene.executeWithSeedOffset = origSeedOffset; vi.mocked(Common.randSeedInt).mockRestore();
    scene.time.delayedCall = saved.delayedCall; scene.tweens.add = saved.tweens; scene.loadSe = saved.loadSe; scene.loadAtlas = saved.loadAtlas;
  }, 1800000);
}

it("writes the safari zone trace", () => {
  lines.push(`Z ${Math.floor(N / configs.length) * configs.length}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-safari.txt", lines.join("\n") + "\n");
  console.log(`SAFARI ZONE ${SEED}: enters=${cover.enters} steps(ball,bait,mud,flee)=${cover.steps} catches=${cover.catches} failed=${cover.fails} flees=${cover.flees} shiny=${cover.shiny} hidden=${cover.hidden} errors=${cover.errors}`);
  expect(cover.enters > 0).toBe(true);
  expect(cover.errors).toBe(0);
});
