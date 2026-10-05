/* Card 94: catching in the run, against upstream at 8555c08. Additive: no existing producer is changed. Writes
 * CATCH_OUT (default /tmp/gc-catch.txt); CATCH_N flow cases (default 64), CATCH_RULES ball-rule states (default
 * 3000), CATCH_SEED (default 777).
 *
 * Two parts, both on the real code with only the presentation (text, sound, tweens) as the harness has it:
 *  - RULES: on a live CommandPhase, random game modes, waves, biomes, battle types, bosses and dex states are put in
 *    place and the real CommandPhase.handleBallCommand (checkCanUseBall and the boss rule) is asked for each ball. The
 *    answer is "allowed" or the locale key of the text it queues.
 *  - FLOW: a seeded wild battle (party of 1..6, a wild Pokemon with its own hp, status, IVs, items, maybe a boss), a
 *    ball thrown through the real command UI; a full party is answered through the real option menu and party
 *    screen (release one, or No). The party, its held items, the ball counts, the dex entries and starter candy of
 *    the species' chain, the messages and both RNG states are written before and after, for the C side to replay.
 *
 * What the harness does to a throw: GameManager makes BattleScene.randBattleSeedInt return its maximum and its tweens
 * run no onRepeat callback, so the shake checks of AttemptCapturePhase never run and every throw "catches". The roll
 * is therefore not compared here (battle-core's capture fixtures compare it); each flow is a throw the formula itself
 * guarantees (a Master Ball, or a modified catch rate of 255 or more), so that upstream's and the C port's answer is
 * the same, and what the catch does to the party, the dex, the items and the messages is compared.
 * Wave numbers that are a multiple of 10 are left out: the harness stops before the boss wave's reward phases and the
 * run's next wave, the C run does not (card 93 covers what a boss wave's victory gives). The state is read as the reward phase is about
 * to start, which is where the C run waits. */
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { getPokeballCatchMultiplier } from "#data/pokeball";
import { getStatusEffectCatchRateMultiplier, Status } from "#data/status-effect";
import { AbilityId } from "#enums/ability-id";
import { BattleType } from "#enums/battle-type";
import { BerryType } from "#enums/berry-type";
import { BiomeId } from "#enums/biome-id";
import { Challenges } from "#enums/challenges";
import { Button } from "#enums/buttons";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { PokeballType } from "#enums/pokeball";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { UiMode } from "#enums/ui-mode";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import i18next from "i18next";
import { afterAll, beforeAll, beforeEach, describe, expect, it, vi } from "vitest";

const N = Number(process.env.CATCH_N ?? 64);
const RULES = Number(process.env.CATCH_RULES ?? 3000);
const SEED = Number(process.env.CATCH_SEED ?? 777);
const OUT = process.env.CATCH_OUT ?? "/tmp/gc-catch.txt";

let seed = SEED;
const rnd = (n: number) => {
  seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
  return (seed >>> 8) % n;
};
const pick = <T>(a: readonly T[]): T => a[rnd(a.length)];

const lines: string[] = [`V 1 ${SEED}`];
const counts: Record<string, number> = {};
const count = (k: string) => {
  counts[k] = (counts[k] ?? 0) + 1;
};

const rngTuple = () => Phaser.Math.RND.state().split(",").slice(1).join(",");

const PARTY_POOL = [
  SpeciesId.BULBASAUR,
  SpeciesId.CHARMANDER,
  SpeciesId.SQUIRTLE,
  SpeciesId.PIKACHU,
  SpeciesId.PIDGEY,
  SpeciesId.RATTATA,
  SpeciesId.GEODUDE,
  SpeciesId.ABRA,
  SpeciesId.GASTLY,
  SpeciesId.EEVEE,
  SpeciesId.MAGIKARP,
  SpeciesId.ZUBAT,
];
// the chain of a species with a starter at its foot, a lone starter, a species that has none, a form-rich one, a
// legendary, Shedinja (Wonder Guard)
const ENEMY_POOL = [
  SpeciesId.CATERPIE,
  SpeciesId.METAPOD,
  SpeciesId.BUTTERFREE,
  SpeciesId.PIKACHU,
  SpeciesId.MAGIKARP,
  SpeciesId.GYARADOS,
  SpeciesId.EEVEE,
  SpeciesId.GENGAR,
  SpeciesId.MEWTWO,
  SpeciesId.ARTICUNO,
  SpeciesId.DRAGONITE,
  SpeciesId.SHEDINJA,
  SpeciesId.ZUBAT,
  SpeciesId.SCATTERBUG,
  SpeciesId.VIVILLON,
];
const BIOMES = [BiomeId.TOWN, BiomeId.PLAINS, BiomeId.GRASS, BiomeId.METROPOLIS, BiomeId.SLUM, BiomeId.FOREST, BiomeId.DOJO, BiomeId.FACTORY];
const STATUSES = [StatusEffect.NONE, StatusEffect.NONE, StatusEffect.POISON, StatusEffect.TOXIC, StatusEffect.PARALYSIS, StatusEffect.SLEEP, StatusEffect.FREEZE, StatusEffect.BURN];
// held items that do nothing to a turn, for the throws that can fail; the others only where a Master Ball ends the turn
const INERT_ITEMS = ["LUCKY_EGG", "GRIP_CLAW", "SCOPE_LENS", "WIDE_LENS", "KINGS_ROCK", "LEEK"];
const ACTIVE_ITEMS = ["LEFTOVERS", "SHELL_BELL", "FOCUS_BAND", "QUICK_CLAW", "BERRY"];
const BERRIES = [BerryType.SITRUS, BerryType.LUM, BerryType.ORAN, BerryType.LEPPA];

// stacks of two only for items whose stack can take it (a full stack is turned into Poke Balls by addModifier)
const qtyOf = (name: string) => (name === "LUCKY_EGG" || name === "SCOPE_LENS" || name === "WIDE_LENS" ? 1 + rnd(2) : 1);
// (Soothe Bell is left out: the friendship it adds on a victory is a known gap of the run's victory phase, not of a catch)
/** `k` different items of a pool */
function pickItems(pool: readonly string[], k: number): string[] {
  const left = [...pool];
  const out: string[] = [];
  while (k-- > 0 && left.length) out.push(left.splice(rnd(left.length), 1)[0]);
  return out;
}

/** a PokemonHeldItemModifier as the overrides give one (modifier.ts overrideHeldItems) */
function giveItem(scene: any, pokemon: any, name: string, qty: number, berry?: BerryType) {
  const fn = (modifierTypes as any)[name];
  let type = fn();
  if (type.is("ModifierTypeGenerator")) {
    type = type.generateType([], berry != null ? [berry] : undefined);
  }
  const mod = type.withIdFromFunc(fn).newModifier(pokemon);
  mod.pokemonId = pokemon.id;
  mod.stackCount = qty;
  scene.addModifier(mod, true, false, false, true);
}

function itemLines(scene: any, tag: string, owners: any[]) {
  const out: string[] = [];
  const mods = (scene.modifiers as any[]).filter(m => m.is?.("PokemonHeldItemModifier"));
  for (const m of mods) {
    const idx = owners.findIndex(p => p.id === m.pokemonId);
    out.push(`${tag} idx=${idx} key=${m.type.id} arg=${m.berryType ?? -1} stack=${m.stackCount}`);
  }
  return out.sort();
}

describe("catch differential", () => {
  let phaserGame: Phaser.Game;
  let game: GameManager;

  beforeAll(() => {
    phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
  });
  beforeEach(() => {
    game = new GameManager(phaserGame);
  });
  afterAll(() => {
    lines.push(`Z ${N}`);
    fs.writeFileSync(OUT, lines.join("\n") + "\n");
    console.log(`CATCH seed=${SEED} ${Object.entries(counts).map(([k, v]) => `${k}=${v}`).join(" ")}`);
  });

  it("ball rules", async () => {
    game.override
      .battleStyle("single")
      .enemySpecies(SpeciesId.MAGIKARP)
      .enemyMoveset(MoveId.SPLASH)
      .moveset(MoveId.SPLASH)
      .startingLevel(50)
      .enemyLevel(20)
      .disableTrainerWaves();
    await game.classicMode.startBattle(SpeciesId.BULBASAUR);
    const scene = game.scene as any;
    const phase = scene.phaseManager.getCurrentPhase();
    expect(phase.phaseName).toBe("CommandPhase");
    const enemy = scene.getEnemyPokemon();
    const gameData = scene.gameData;
    const starterIds: number[] = speciesDataRegistry.getAllStarters().sort((a, b) => a - b);
    let queued: string | null = null;
    vi.spyOn(phase, "queueShowText").mockImplementation((key: string) => {
      queued = key;
    });
    const modes = [GameModes.CLASSIC, GameModes.CLASSIC, GameModes.CHALLENGE, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.DAILY];
    const waves = [1, 10, 50, 195, 199, 200, 201, 250, 495, 500, 5195, 5250, 5000];
    const enemySpecies = [SpeciesId.MAGIKARP, SpeciesId.GREAT_TUSK, SpeciesId.ETERNATUS, SpeciesId.SHEDINJA, SpeciesId.CATERPIE];
    const saved = { ...scene.currentBattle };
    for (let i = 0; i < RULES; i++) {
      const mode = pick(modes);
      const gm = getGameMode(mode);
      // Challenge Mode lists every challenge (value 0 or not), the other modes none; Fresh Start at its value 1 is the
      // one the ball rules read
      const chal = mode === GameModes.CHALLENGE ? pick([0, 0, 1]) : 0;
      if (chal) gm.challenges.find((c: any) => c.id === Challenges.FRESH_START).value = 1;
      scene.gameMode = gm;
      const wave = rnd(4) === 0 ? 1 + rnd(260) : pick(waves);
      scene.currentBattle.waveIndex = wave;
      const biome = rnd(3) === 0 ? BiomeId.TOWN : BiomeId.END;
      (scene.arena as any).biomeId = biome;
      const btype = pick([BattleType.WILD, BattleType.WILD, BattleType.WILD, BattleType.TRAINER, BattleType.MYSTERY_ENCOUNTER]);
      const catchAllowed = rnd(2) === 0;
      scene.currentBattle.battleType = btype;
      scene.currentBattle.mysteryEncounter = btype === BattleType.MYSTERY_ENCOUNTER ? { catchAllowed } : undefined;
      const species = pick(enemySpecies);
      enemy.species = speciesDataRegistry.getSpecies(species);
      enemy.hp = rnd(8) === 0 ? 0 : 1 + rnd(30);
      const segs = pick([0, 0, 2, 3, 4]);
      enemy.bossSegments = segs;
      enemy.bossSegmentIndex = segs ? rnd(segs) : 0;
      // the starters caught: all, all but one, all but two, none (the lowest ids missing)
      const missing = pick([0, 0, 1, 2, 5, 600]);
      starterIds.forEach((id, k) => {
        gameData.dexData[id].caughtAttr = k < missing ? 0n : 157n;
      });
      const enemyCaught = rnd(2) === 0;
      gameData.dexData[species].caughtAttr = enemyCaught ? 157n : 0n;
      const ball = rnd(6); // 0..4 are the balls the command takes; 5 is not one it takes
      queued = null;
      const hadWG = enemy.hasAbility(AbilityId.WONDER_GUARD, false, true);
      const ok = phase.handleBallCommand(ball);
      scene.currentBattle.turnCommands[0] = null;
      expect(ok || queued != null || ball >= 5).toBe(true);
      const sys = starterIds.filter(id => gameData.dexData[id].caughtAttr).length;
      lines.push(
        `RULE mode=${mode} chal=${chal} wave=${wave} biome=${biome} btype=${btype} catch=${+catchAllowed} species=${species} hp=${enemy.hp} segs=${segs} segidx=${enemy.bossSegmentIndex} wg=${+hadWG} missing=${missing} caught=${+enemyCaught} ball=${ball} starters=${sys} -> ${ok ? "-" : (queued ?? "silent")}`,
      );
      count(ok ? "rule-allowed" : `rule-${queued ?? "silent"}`);
    }
    Object.assign(scene.currentBattle, saved);
    scene.gameMode = getGameMode(GameModes.CLASSIC);
  }, 600000);

  for (let i = 0; i < N; i++) {
    it(`flow ${i}`, async () => {
      const nParty = rnd(3) === 0 ? 6 : 1 + rnd(6);
      const party = [...PARTY_POOL].sort(() => rnd(3) - 1).slice(0, nParty);
      // distinct species, in an order the draw above doesn't bias
      const species = pick(ENEMY_POOL);
      const eLevel = 5 + rnd(60);
      const biome = pick(BIOMES);
      let wave = 1 + rnd(30);
      if (wave % 10 === 0) wave++;
      const segs = rnd(5) === 0 ? 2 + rnd(3) : 0;
      // balls: the Master Ball in half the cases (it ends the turn), a boss with a shield only takes that one; any
      // other ball only for a species whose catch rate can reach 255 with it (see the header)
      const catchRate = speciesDataRegistry.getAllSpecies().find(s => s.speciesId === species)!.catchRate;
      let ball = rnd(2) === 0 || segs ? PokeballType.MASTER_BALL : pick([PokeballType.POKEBALL, PokeballType.GREAT_BALL, PokeballType.ULTRA_BALL, PokeballType.ROGUE_BALL]);
      if (ball !== PokeballType.MASTER_BALL && catchRate * getPokeballCatchMultiplier(ball) * 2.5 < 300) ball = PokeballType.MASTER_BALL;
      const sure = ball === PokeballType.MASTER_BALL;
      const itemPool = sure ? [...INERT_ITEMS, ...ACTIVE_ITEMS] : INERT_ITEMS;
      // the harness normalizes IVs, natures and shininess; addPlayerPokemon would then give the caught Pokemon the
      // overrides' values, which no run has
      game.override.disableShinies = false;
      game.override.normalizeIVs = false;
      game.override.normalizeNatures = false;
      game.override
        .battleStyle("single")
        .enemySpecies(species)
        .enemyLevel(eLevel)
        .enemyMoveset(MoveId.SPLASH)
        .moveset(MoveId.SPLASH)
        .ability(AbilityId.NONE)
        .passiveAbility(AbilityId.NONE)
        .startingLevel(100)
        .startingWave(wave)
        .startingBiome(biome)
        .disableTrainerWaves()
        .mysteryEncounterChance(0);
      if (segs) game.override.enemyHealthSegments(segs);
      const enemyItems: { name: string; count: number; type?: BerryType }[] = [];
      for (const name of pickItems(itemPool, rnd(3))) {
        enemyItems.push({ name, count: qtyOf(name), ...(name === "BERRY" ? { type: pick(BERRIES) } : {}) });
      }
      if (enemyItems.length) game.override.enemyHeldItems(enemyItems as any);
      await game.classicMode.startBattle(...party);
      const scene = game.scene as any;
      const enemy = scene.getEnemyPokemon();
      const players: any[] = scene.getPlayerParty();
      const why = scene.currentBattle.battleType !== BattleType.WILD ? "not-wild" : scene.arena.weather?.weatherType ? "weather" : !enemy ? "no-enemy" : enemy.isBoss() !== !!segs ? "boss-flag" : null;
      if (why) {
        lines.push(`SKIP i=${i} ${why}`);
        count(`skipped-${why}`);
        return;
      }

      // the party as it is when the ball is thrown: hp and status of each, the held items
      for (const p of players) {
        p.hp = rnd(5) === 0 ? 0 : 1 + rnd(p.getMaxHp());
        p.status = p.hp ? null : new Status(StatusEffect.FAINT);
      }
      players[0].hp = Math.max(players[0].hp, 1);
      players[0].status = null;
      for (const p of players) {
        for (const name of pickItems(itemPool, rnd(3))) {
          giveItem(scene, p, name, qtyOf(name), name === "BERRY" ? pick(BERRIES) : undefined);
        }
      }
      // the wild Pokemon: IVs, nature (so its stats), hp, status, shininess
      enemy.ivs = Array.from({ length: 6 }, () => rnd(32));
      enemy.nature = rnd(25);
      enemy.calculateStats();
      enemy.hp = 1 + rnd(enemy.getMaxHp());
      const setStatus = (st: StatusEffect) => {
        enemy.status = st === StatusEffect.NONE ? null : new Status(st, 0, st === StatusEffect.SLEEP ? 3 : undefined, st === StatusEffect.FREEZE ? 3 : undefined);
      };
      setStatus(pick(STATUSES));
      if (rnd(4) === 0) {
        enemy.shiny = true;
        enemy.variant = rnd(3);
      }
      if (ball !== PokeballType.MASTER_BALL) {
        // AttemptCapturePhase's modified catch rate; a throw that cannot fail is one at 255 or more. Part of the draws
        // are put at 1 HP, asleep, to get there (a shiny catches no easier here: no event multiplier is in play).
        const modified = () => {
          const _3m = 3 * enemy.getMaxHp();
          const status = enemy.status ? getStatusEffectCatchRateMultiplier(enemy.status.effect) : 1;
          return Math.round((((_3m - 2 * enemy.hp) * enemy.species.catchRate * getPokeballCatchMultiplier(ball)) / _3m) * status);
        };
        if (modified() < 256) enemy.hp = 1;
        if (modified() < 256) setStatus(StatusEffect.SLEEP);
        if (modified() < 256) {
          lines.push(`SKIP i=${i} unsure`);
          count("skipped-unsure");
          return;
        }
      }
      // the dex: the chain's entries part-filled, candy in the root
      const gameData = scene.gameData;
      const chain: number[] = [];
      for (let id: number | null = species; id != null; id = speciesDataRegistry.getPrevolution(id)) chain.push(id);
      const startRoot = enemy.species.getRootSpeciesId(true);
      if (!chain.includes(startRoot)) chain.push(startRoot);
      for (const id of chain) {
        const d = gameData.dexData[id];
        const r = rnd(4);
        d.caughtAttr = r === 0 ? 0n : r === 1 ? 157n : r === 2 ? 3n : d.caughtAttr;
        d.natureAttr = rnd(3) === 0 ? 0 : 1 << (1 + rnd(24));
        d.caughtCount = rnd(5);
        d.ivs = Array.from({ length: 6 }, () => rnd(32));
        const sd = gameData.starterData[id];
        if (sd) {
          sd.candyCount = rnd(40);
          sd.abilityAttr = rnd(8);
        }
      }
      for (let b = 0; b < 5; b++) scene.pokeballCounts[b] = rnd(4);
      scene.pokeballCounts[ball] = 3;

      // Release choices come from the party screen: slots 0..n-1 release that one, -1 answers No
      const full = players.length === 6;
      const release = full ? (rnd(4) === 0 ? -1 : rnd(6)) : -2;

      const battle = scene.currentBattle;
      Phaser.Math.RND.sow([`catch-${SEED}-${i}`]);
      const bs = battle.battleSeedState;
      const dexLine = (tag: string, id: number) => {
        const d = gameData.dexData[id];
        const sd = gameData.starterData[id];
        return `${tag} id=${id} caught=${d.caughtAttr} nat=${d.natureAttr} cnt=${d.caughtCount} ivs=${d.ivs.join(",")} candy=${sd?.candyCount ?? -1} abil=${sd?.abilityAttr ?? -1}`;
      };
      const mon = (tag: string, p: any, idx: number) =>
        `${tag} idx=${idx} sp=${p.species.speciesId} form=${p.formIndex} lv=${p.level} hp=${p.hp} st=${p.status?.effect ?? 0} stats=${p.stats.join(",")} ball=${p.pokeball} mw=${p.metWave} mlv=${p.metLevel} mb=${p.metBiome} msp=${p.metSpecies} ivs=${p.ivs.join(",")} nat=${p.nature} ab=${p.abilityIndex} gen=${p.gender} shiny=${+p.shiny} var=${p.variant} fr=${p.friendship} moves=${p.moveset.map((m: any) => m.moveId).join(",")}`;

      lines.push(
        `CASE i=${i} wave=${battle.waveIndex} biome=${scene.arena.biomeId} ball=${ball} n=${players.length} rel=${release} bseed=${battle.battleSeed} wseed=${scene.waveSeed} g=${rngTuple()} b=${bs ? bs.split(",").slice(1).join(",") : "none"} turn=${battle.turn} segs=${enemy.bossSegments} segidx=${enemy.bossSegmentIndex}`,
        `BALLS ${[0, 1, 2, 3, 4].map(b => scene.pokeballCounts[b]).join(",")}`,
        ...players.map((p, k) => mon("PM", p, k)),
        mon("EN", enemy, -1),
        `ENX sleep=${enemy.status?.sleepTurnsRemaining ?? -1} id=${enemy.id}`,
        ...itemLines(scene, "PI", players),
      );
      const enemyMods = ((scene as any).enemyModifiers as any[]).filter(m => m.is?.("PokemonHeldItemModifier"));
      for (const m of enemyMods) lines.push(`EI key=${m.type.id} arg=${m.berryType ?? -1} stack=${m.stackCount}`);
      for (const id of chain) lines.push(dexLine("DP", id));

      // the throw, then the answers the real UI is asked for
      const t = vi.spyOn(i18next, "t");
      const refused = (phase => {
        let key: string | null = null;
        const q = vi.spyOn(phase, "queueShowText").mockImplementation((k: string) => {
          key = k;
        });
        const ok = (phase as any).handleBallCommand(ball);
        q.mockRestore();
        scene.currentBattle.turnCommands[0] = null;
        return ok ? null : key;
      })(scene.phaseManager.getCurrentPhase());
      expect(refused).toBeNull();

      game.doThrowPokeball(ball);
      if (full) {
        game.onNextPrompt("AttemptCapturePhase", UiMode.OPTION_SELECT, () => {
          const h = scene.ui.getHandler();
          h.setCursor(release === -1 ? 3 : 2); // Yes / No
          h.processInput(Button.ACTION);
        });
        if (release >= 0) {
          game.onNextPrompt("AttemptCapturePhase", UiMode.PARTY, () => {
            const h = scene.ui.getHandler();
            h.setCursor(release);
            h.processInput(Button.ACTION); // the slot
            h.processInput(Button.ACTION); // Release, the first of its options
          });
          // the farewell text waits for a key
          game.onNextPrompt("AttemptCapturePhase", UiMode.PARTY, () => scene.ui.getHandler().processInput(Button.ACTION), undefined, true);
        }
      }
      await game.toEndOfTurn();
      // the turn's end queues its heals (Leftovers) as phases of their own; the C run has made them by the time it
      // shows the reward screen, so upstream is read there too
      await game.phaseInterceptor.to("SelectModifierPhase", false);

      const after: any[] = scene.getPlayerParty();
      const keys = t.mock.calls.map(c => String(c[0]));
      const caught = keys.includes("battle:pokemonCaught"); // the roll passed (a full party may still answer No)
      const farewell = keys.filter(k => /^partyUiHandler:(goodbye|byebye|farewell|soLong|thisIsWhereWePart|illMissYou|illNeverForgetYou|untilWeMeetAgain|sayonara|smellYaLater)$/.test(k)).length;
      const started = keys.filter(k => k === "battle:addedAsAStarter").length;
      t.mockRestore();
      const ab = battle.battleSeedState;
      lines.push(
        `RES caught=${+caught} n=${after.length} farewell=${farewell} started=${started} caughtMsg=${keys.includes("battle:pokemonCaught") ? 1 : 0} partyFull=${keys.includes("battle:partyFull") ? 1 : 0} last=${battle.lastUsedPokeball ?? -1}`,
        `RBALLS ${[0, 1, 2, 3, 4].map(b => scene.pokeballCounts[b]).join(",")}`,
        ...after.map((p, k) => mon("RM", p, k)),
        `REN hp=${enemy.hp} st=${enemy.status?.effect ?? 0}`,
        ...itemLines(scene, "RI", after),
      );
      for (const id of chain) lines.push(dexLine("DA", id));
      lines.push(`RRNG g=${rngTuple()} b=${ab ? ab.split(",").slice(1).join(",") : "none"}`, "E");
      count(caught ? (full ? (release >= 0 ? "caught-release" : "caught-no") : "caught") : "failed");
      if (segs) count("boss");
      if (enemyItems.length) count("enemy-items");
    }, 120000);
  }
});
