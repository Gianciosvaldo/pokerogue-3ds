/* The Fun and Games! oracle (card 82's harness family, see mystery-salesman.producer.test.ts). Unlike the one-action oracles this
 * one plays the REAL encounter through the game manager's real phases: the encounter is started (runToMysteryEncounter), the
 * party and money are changed, the option is picked through the real party screen (runMysteryEncounterToEnd) and the three
 * turns are played with the player's own FIGHT commands (CommandPhase.handleCommand) until the phase queue reaches either the
 * next CommandPhase or the SelectModifierPhase the encounter ends on. Nothing is mocked but the intro visuals and the
 * Pokemon's animations (the game manager's own), and GameManager's pin of randBattleSeedInt, which is put back to its real
 * body because the Wobbuffet's AI move choice and the damage rolls draw on it.
 *
 * Per case: the run and wave seeds, the battle seed, the wave, the money, the party (species, level, IVs, nature, ability,
 * moves; all stats and HP are read back), the dex entry of Wobbuffet; then what upstream did: the options' meetsRequirements(),
 * the run RNG before the option, the Wobbuffet (level, stats, IVs, nature, ability, shininess, moveset in order) with the run
 * RNG after it was made, per turn (an HP override the case applied before the command, the slot used, the Wobbuffet's and the
 * lead's HP afterwards, the run RNG), the money, the encounter's own messages in order, the party after (level, experience,
 * friendship, HP, moves), the dex entry, and the shop SelectModifierPhase would open (read the way the other mystery
 * oracles read it) with the run RNG after it. Cases in which a Pokemon of the party faints are dropped (a forced switch screen
 * is another card's). A share of cases forces the end-of-game HP around the three prize thresholds: the last turn is Splash and the
 * Wobbuffet's HP is set just before it to floor(maxHP * 0.03|0.15|0.33) + -1..2.
 * GC_FG_N=400 GC_MYS_SEED=777 GC_MYS_OUT=/abs/mystery.txt, Node22. */
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { BattleScene } from "#app/battle-scene";
import { getLevelTotalExp } from "#data/exp";
import { modifierTypes } from "#data/data-lists";
import { BiomeId } from "#enums/biome-id";
import { Command } from "#enums/command";
import { MoveId } from "#enums/move-id";
import { MoveUseMode } from "#enums/move-use-mode";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { SpeciesId } from "#enums/species-id";
import { UiMode } from "#enums/ui-mode";
import { PokemonMove } from "#moves/pokemon-move";
import * as EncounterPhaseUtils from "#mystery-encounters/encounter-phase-utils";
import * as MysteryEncounters from "#mystery-encounters/mystery-encounter-biomes";
import { HUMAN_TRANSITABLE_BIOMES } from "#mystery-encounters/mystery-encounter-biomes";
import { regenerateModifierPoolThresholds, ModifierPoolType } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import { runMysteryEncounterToEnd } from "#test/utils/encounter-test-utils";
import Phaser from "phaser";
import i18next from "i18next";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_FG_N ?? 400), SEED = Number(process.env.GC_MYS_SEED ?? 777);
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const word = (n = 24) => Array.from({ length: n }, () => alnum[rnd(alnum.length)]).join("");
const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").join(" ");
const dbg = (m: string) => { if (process.env.GC_FG_DEBUG) fs.appendFileSync(process.env.GC_FG_DEBUG, `${m}\n`); };

const configs: SpeciesId[][] = [
  [SpeciesId.LAPRAS, SpeciesId.GENGAR, SpeciesId.ABRA],
  [SpeciesId.PIKACHU],
  [SpeciesId.BULBASAUR, SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE, SpeciesId.RATTATA, SpeciesId.MAGIKARP, SpeciesId.EEVEE],
  [SpeciesId.MEW, SpeciesId.DRAGONITE, SpeciesId.SNORLAX],
];
const BIOMES = [BiomeId.PLAINS, BiomeId.TOWN, BiomeId.FOREST, BiomeId.SEA, BiomeId.CAVE, BiomeId.VOLCANO, BiomeId.SPACE, BiomeId.LABORATORY];
// moves with no self-KO, no charge or recharge turn, no switch, nothing the engine is known to model differently: damage, a
// priority move, fixed damage, multi-hit, stat changes and healing
const POOL = [MoveId.TACKLE, MoveId.SCRATCH, MoveId.EMBER, MoveId.WATER_GUN, MoveId.QUICK_ATTACK, MoveId.BODY_SLAM, MoveId.FLAMETHROWER, MoveId.SURF,
  MoveId.THUNDERBOLT, MoveId.EARTHQUAKE, MoveId.PSYCHIC, MoveId.SHADOW_BALL, MoveId.EXTREME_SPEED, MoveId.SEISMIC_TOSS, MoveId.DRAGON_RAGE,
  MoveId.SWORDS_DANCE, MoveId.CALM_MIND, MoveId.GROWL, MoveId.SPLASH, MoveId.RECOVER, MoveId.ICE_BEAM, MoveId.HYPER_VOICE, MoveId.BULLET_SEED,
  MoveId.FURY_SWIPES, MoveId.POUND, MoveId.SUCKER_PUNCH];
const THRESHOLDS = [0.03, 0.15, 0.33];
const MESSAGE_KEY = /^(mysteryEncounters\/funAndGames:(ko|endGame|bestResult|greatResult|goodResult|badResult|turnRemaining[123]|chargingContinue)|mysteryEncounterMessages:paidMoney)$/;

const lines: string[] = [];
const cover = { options: [0, 0], played: 0, dropped: 0, disabled: 0, ko: 0, forced: 0, exact: 0, prizes: [0, 0, 0, 0], boundary: 0, hit: [0, 0, 0], levelups: 0, overrides: 0, shops: new Set<string>(), species: new Set<number>(), moves: new Set<number>(), turns: [0, 0, 0, 0] };

const argOf = (t: any): number => {
  switch (t.id) {
    case "BERRY": return t.berryType;
    case "BASE_STAT_BOOSTER": case "TEMP_STAT_STAGE_BOOSTER": return t.stat;
    case "ATTACK_TYPE_BOOSTER": return t.moveType;
    case "TM_COMMON": case "TM_GREAT": case "TM_ULTRA": return t.moveId;
    case "EVOLUTION_ITEM": case "RARE_EVOLUTION_ITEM": return t.evolutionItem;
    case "FORM_CHANGE_ITEM": case "RARE_FORM_CHANGE_ITEM": return t.formChangeItem;
    case "MINT": return t.nature;
    case "TERA_SHARD": return t.teraType;
    default: return -1;
  }
};
const movesOf = (p: any): string => {
  const ids: number[] = p.getMoveset().map((m: any) => m.moveId);
  return `${ids.length} ${ids.join(" ")}`;
};
const dexTokens = (gameData: any, id: number): string => {
  const d = gameData.dexData[id];
  return `${id} ${d.seenAttr} ${d.seenCount} ${d.caughtAttr} ${d.caughtCount}`;
};

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 20).toBe(true);
});

for (let ci = 0; ci < configs.length; ci++) {
  const share = Math.floor(N / configs.length);
  for (let i = 0; i < share; i++) {
    const only = process.env.GC_FG_ONLY;
    (only && Number(only) !== ci * share + i ? it.skip : it)(`fun and games, party ${ci} case ${i}`, async () => {
      seed = (Math.imul(SEED, 7919) + Math.imul(ci * share + i + 1, 104729)) >>> 0;
      const game = new GameManager(phaserGame);
      BattleScene.prototype.randBattleSeedInt = function (range: number, min = 0) { return (this as any).currentBattle?.randSeedInt(range, min); };
      game.override.mysteryEncounterChance(100).startingWave(45).startingBiome(BiomeId.CAVE).disableTrainerWaves().startingLevel(20);
      const biomes = new Map<BiomeId, MysteryEncounterType[]>([[BiomeId.VOLCANO, [MysteryEncounterType.FIGHT_OR_FLIGHT]]]);
      HUMAN_TRANSITABLE_BIOMES.forEach(b => biomes.set(b, [MysteryEncounterType.FUN_AND_GAMES]));
      vi.spyOn(MysteryEncounters, "mysteryEncountersByBiome", "get").mockReturnValue(biomes);
      await game.runToMysteryEncounter(MysteryEncounterType.FUN_AND_GAMES, configs[ci]);
      const scene = game.scene as any, encounter = scene.currentBattle.mysteryEncounter, party: any[] = scene.getPlayerParty(), gameData = scene.gameData;
      expect(encounter.options.length).toBe(2);
      const keys: string[] = [];
      const realT = i18next.t.bind(i18next);
      vi.spyOn(i18next, "t").mockImplementation(((k: any, o?: any) => {
        if (typeof k === "string" && MESSAGE_KEY.test(k)) keys.push(k);
        return realT(k, o);
      }) as any);

      const wave = 10 + rnd(171), runSeed = word(), waveSeed = word(), battleSeed = word(16);
      scene.seed = runSeed; scene.waveSeed = waveSeed;
      scene.currentBattle.waveIndex = wave;
      scene.currentBattle.battleSeed = battleSeed;
      scene.currentBattle.battleSeedState = null;
      scene.arena.biomeId = BIOMES[rnd(BIOMES.length)];
      scene.modifiers.length = 0;
      const mr = rnd(10), mode = mr < 4 ? 0 : mr < 7 ? 1 : 2;   // 1 = the prize thresholds, 2 = the Wobbuffet is knocked out at a turn
      const koTurn = 1 + rnd(3);
      const k = rnd(8) === 0 ? 1 : 0;
      const pick = rnd(party.length);

      // the party: levels (the Wobbuffet is the picked one's), IVs, nature, ability, 1..3 moves
      const setup: string[] = [];
      const origIds: number[] = [];
      for (const [j, p] of party.entries()) {
        p.level = mode === 2 ? 45 + rnd(56) : 1 + rnd(rnd(3) ? 100 : 30);
        p.exp = getLevelTotalExp(p.level, p.species.growthRate);
        // a knock-out gives experience: some Pokemon are a few points short of the next level, and have room for what it teaches
        if (mode === 2 && p.level < 100 && rnd(2)) p.exp = getLevelTotalExp(p.level + 1, p.species.growthRate) - 1 - rnd(300);
        p.ivs = Array.from({ length: 6 }, () => rnd(32));
        p.nature = rnd(25);
        const abilities = [p.species.ability1, p.species.ability2, p.species.abilityHidden].filter((a: any) => a != null && a !== 0).length;
        p.abilityIndex = Math.min(rnd(3), Math.max(0, abilities - 1));
        p.pauseEvolutions = true;
        p.passive = false;
        const n = mode === 2 ? 1 + rnd(2) : 1 + rnd(3), ms: number[] = [];
        while (ms.length < n) { const m = POOL[rnd(POOL.length)]; if (!ms.includes(m)) ms.push(m); }
        if (mode === 1 && j === pick && !ms.includes(MoveId.SPLASH)) { if (ms.length === 3) ms.pop(); ms.push(MoveId.SPLASH); }
        p.moveset = ms.map(m => new PokemonMove(m));
        p.calculateStats();
        p.hp = p.getMaxHp();
        origIds.push(p.id);
        ms.forEach(m => cover.moves.add(m));
        setup.push(`${p.species.speciesId} ${p.formIndex} ${p.level} ${p.ivs.join(" ")} ${p.nature} ${p.abilityIndex} ${p.exp} ${p.friendship} ${p.stats.join(" ")} ${p.hp} ${movesOf(p)}`);
        cover.species.add(p.species.speciesId);
      }
      // the wallet around the price
      const meetsBefore = () => encounter.options.map((o: any) => (o.meetsRequirements() ? 1 : 0));
      scene.money = 1e9;
      encounter.options[0].meetsRequirements();
      const cost = (encounter.options[0].requirements[0] as any).requiredMoney;
      const moneys = [cost - 1, cost, cost, cost + 1, 7 * cost, 12345, 0];
      scene.money = Math.max(0, moneys[rnd(moneys.length)]);
      const meets = meetsBefore();
      const runs = meets[k] === 1;
      const wob = speciesDataRegistry.getSpecies(SpeciesId.WOBBUFFET);
      void wob;
      const dex = gameData.dexData[SpeciesId.WOBBUFFET];
      dex.seenAttr = rnd(3) === 0 ? 0n : rnd(2) ? 157n : 3n;
      dex.seenCount = rnd(6);
      dex.caughtAttr = rnd(2) ? 0n : 157n;
      dex.caughtCount = rnd(4);
      lines.push(`C ${ci * share + i} ${runSeed} ${waveSeed} ${battleSeed} ${wave} ${scene.money} ${k} ${pick} ${party.length} ${runs ? 1 : 0} ${mode} ${scene.arena.biomeId}`,
        `U ${setup.join(" ; ")}`, `A ${meets.join(" ")} ${cost}`, `D ${dexTokens(gameData, SpeciesId.WOBBUFFET)}`);
      if (!runs) { cover.disabled++; lines.push("E"); vi.mocked(i18next.t).mockRestore(); return; }

      // the option's real flow; the run RNG is read where the option phase starts (before its seed offset)
      Phaser.Math.RND.sow([`fg-${SEED}-${ci}-${i}`]);
      let rngBefore = "";
      const exec = scene.executeWithSeedOffset.bind(scene);
      let optionStarted = false;
      const option = encounter.options[k];
      const realOnOption = option.onOptionPhase;
      option.onOptionPhase = function (...a: any[]) { optionStarted = true; return realOnOption.apply(this, a); };
      vi.spyOn(scene, "executeWithSeedOffset").mockImplementation(((fn: any, offset: number, ...rest: any[]) => {
        if (!optionStarted && !rngBefore) rngBefore = rng();
        return exec(fn, offset, ...rest);
      }) as any);
      const leave = vi.spyOn(EncounterPhaseUtils, "leaveEncounterWithoutBattle");
      void leave;
      game.onNextPrompt("MessagePhase", UiMode.MESSAGE, () => { game.endPhase(); });
      dbg(`case ${ci}/${i} option ${k} pick ${pick} mode ${mode} wave ${wave}`);
      const pm = scene.phaseManager;
      const pi = game.phaseInterceptor as any;
      const stepTo = async (...names: string[]): Promise<string> => {
        pi.target = "LoginPhase";
        for (let guard = 0; guard < 400; guard++) {
          const cur = pm.getCurrentPhase();
          if (names.includes(cur.phaseName)) return cur.phaseName;
          await pi.run(cur);
        }
        throw new Error(`never reached ${names}`);
      };
      let dropped = false;
      const turnLines: string[] = [];
      let wobLine = "";
      // runs on until the player has a FIGHT menu again ("cmd", started and waiting) or the encounter reached its shop; a
      // CommandPhase that ends by itself (a move the player is forced to repeat: Encore, a charge) is run through
      const nextInput = async (): Promise<string> => {
        for (let guard = 0; guard < 20; guard++) {
          const where = await stepTo("CommandPhase", "SelectModifierPhase");
          if (where !== "CommandPhase") return "shop";
          const ph = pm.getCurrentPhase();
          pi.target = "CommandPhase";
          await pi.run(ph);
          if (pm.getCurrentPhase().phaseName === "CommandPhase") return "cmd";
          dbg(`a ${ph.phaseName} ended by itself: now ${pm.getCurrentPhase().phaseName} state ${pi.state}`);
          cover.forced++;
        }
        throw new Error("no input point");
      };
      if (k === 0) {
        await runMysteryEncounterToEnd(game, 1, { pokemonNo: pick + 1 }, true);
        // the first CommandPhase: the Wobbuffet exists
        const w = game.field.getEnemyPokemon();
        wobLine = `W ${w.level} ${w.hp} ${w.stats.join(" ")} ${w.ivs.join(" ")} ${w.nature} ${w.abilityIndex} ${+w.shiny} ${movesOf(w)} ${scene.money} ${rng()}`;
        const maxHp = w.getMaxHp();
        let at = "cmd";
        for (let t = 1; t <= 3 && at === "cmd" && !dropped; t++) {
          const cur = pm.getCurrentPhase();
          const lead = party[0];
          if (lead.hp <= 0 || w.hp <= 0) { dropped = true; break; }
          let ov = -1, ovMax = -1;
          if (mode === 1 && t === 3) {
            const thr = THRESHOLDS[rnd(3)];
            let max = maxHp;
            // half of them get a maximum HP at which the ratio can land exactly on the threshold (0.15 of a multiple of 20, 0.03 and
            // 0.33 of a multiple of 100), where a < written as <= is the only difference
            if (rnd(2)) { max = thr === 0.15 ? 20 * (1 + rnd(25)) : 100 * (1 + rnd(5)); ovMax = max; w.stats[0] = max; cover.exact++; }
            ov = Math.max(1, (ovMax > 0 ? Math.round(max * thr) + rnd(3) - 1 : Math.floor(max * thr) + rnd(4) - 1));
            w.hp = ov;
            cover.overrides++;
          } else if (mode === 2 && t === koTurn) {
            ov = 1 + rnd(40);
            w.hp = Math.min(ov, w.hp);
            ov = w.hp;
            cover.overrides++;
          }
          const slots = lead.getMoveset().map((_m: any, s: number) => s).filter((s: number) => lead.trySelectMove(s)[0]);
          let slot = slots.length ? slots[rnd(slots.length)] : 0;
          if (mode === 1 && t === 3) {
            const splash = lead.getMoveset().findIndex((m: any) => m.moveId === MoveId.SPLASH);
            if (splash >= 0 && slots.includes(splash)) slot = splash;
          }
          const ok = (cur as any).handleCommand(Command.FIGHT, slot, MoveUseMode.NORMAL);
          expect(ok).toBe(true);
          await vi.waitUntil(() => pm.getCurrentPhase() !== cur, { interval: 0, timeout: 5000 });   // end() hands over a tick later
          at = await nextInput();
          if (party.some(p => p.hp <= 0)) dropped = true;
          turnLines.push(`T ${ovMax} ${ov} ${slot} ${w.hp} ${party[0].hp} ${rng()}`);
        }
        if (!dropped && at === "cmd") dropped = true;   // still a FIGHT menu after three turns: not an ending
        if (!dropped && party.some(p => p.hp <= 0)) dropped = true;
      } else {
        await runMysteryEncounterToEnd(game, 2, undefined, false);
        await stepTo("SelectModifierPhase");
      }
      vi.mocked(scene.executeWithSeedOffset).mockRestore();
      vi.mocked(i18next.t).mockRestore();
      if (dropped) {
        // dropped cases are not in the trace: the case record above was written, take it and its three records back
        lines.length -= 4;
        cover.dropped++;
        return;
      }

      // the shop SelectModifierPhase is about to open, read the way the other mystery oracles read it
      const phase: any = pm.getCurrentPhase();
      const rngAtShop = rng();
      phase.updateSeed();
      regenerateModifierPoolThresholds(scene.getPlayerParty(), ModifierPoolType.PLAYER, 0);
      const count = phase.getModifierCount();
      const opts = phase.getModifierTypeOptions(count);
      const shop = `${count} ${opts.length} ${opts.map((o: any) => `${o.type.id} ${argOf(o.type)} ${o.type.tier ?? -1} ${o.upgradeCount}`).join(" ")}`;
      const afterShop = rng();
      const order = party.map(p => origIds.indexOf(p.id));
      const final = party.map(p => `${p.species.speciesId} ${p.level} ${p.exp} ${p.friendship} ${p.hp} ${movesOf(p)}`);
      lines.push(`R ${rngBefore}`);
      if (k === 0) lines.push(wobLine, `L ${turnLines.length}`, ...turnLines);
      lines.push(`Q ${scene.money} ${rngAtShop} ${keys.length} ${keys.join(" ")}`, `S ${shop}`, `G ${afterShop}`,
        `P ${order.join(" ")} ; ${final.join(" ; ")}`, `X ${dexTokens(gameData, SpeciesId.WOBBUFFET)}`, "E");
      cover.options[k]++;
      if (k === 0) {
        cover.played++;
        cover.turns[turnLines.length]++;
        const ko = keys.includes("mysteryEncounters/funAndGames:ko");
        cover.ko += ko ? 1 : 0;
        const prize = keys.includes("mysteryEncounters/funAndGames:bestResult") ? 0 : keys.includes("mysteryEncounters/funAndGames:greatResult") ? 1 : keys.includes("mysteryEncounters/funAndGames:goodResult") ? 2 : keys.includes("mysteryEncounters/funAndGames:badResult") ? 3 : -1;
        if (prize >= 0) cover.prizes[prize]++;
        if (mode === 1 && prize >= 0) cover.boundary++;
        if (final.some((f, j) => Number(f.split(" ")[1]) > Number(setup[order[j]]?.split(" ")[2]))) cover.levelups++;
      }
      cover.shops.add(opts.map((o: any) => o.type.id).join(","));
    }, Number(process.env.GC_FG_TIMEOUT ?? 600000));
  }
}

it("writes the fun and games trace", () => {
  lines.unshift(`V 1 ${N}`);
  lines.push(`Z ${cover.options[0] + cover.options[1] + cover.disabled}`);
  fs.writeFileSync((process.env.GC_MYS_OUT ?? "/tmp/mystery.txt").replace(/\.txt$/, "") + "-fungames.txt", lines.join("\n") + "\n");
  console.log(`FUNGAMES ${SEED}: options=${cover.options} played=${cover.played} disabled=${cover.disabled} dropped=${cover.dropped} ko=${cover.ko} prizes(best/great/good/bad)=${cover.prizes} boundary=${cover.boundary} overrides=${cover.overrides} turns(0..3)=${cover.turns} levelups=${cover.levelups} forced=${cover.forced} exact=${cover.exact} shops=${cover.shops.size} species=${cover.species.size} moves=${cover.moves.size}`);
  expect(cover.options.every(n => n > 0) && cover.played > 0).toBe(true);
});
void modifierTypes;
