/*
 * Card 94: the dex, candy and IV side of a catch, for game-core differential testing. Additive: it leaves
 * dump-dexupdates.test.ts and its comparison as they are, and is read by the same comparator (dexupdatetest).
 * Each OP_CATCH is what AttemptCapturePhase.catch() does with the caught Pokemon, in its order:
 *   gameData.updateSpeciesDexIvs(pokemon.species.getRootSpeciesId(true), pokemon.ivs)
 *   gameData.setPokemonCaught(pokemon)           // incrementCount, no egg, showMessage
 * in a Classic or a Daily Run, the Pokemon a boss or not. The "added as a starter" messages the catch shows are
 * recorded after each op (MSG), by species. Writes /tmp/gc_dex_catch.txt (or DEX_CATCH_OUT); DEX_CATCH_N ops
 * (default 1500).
 */
import { globalScene } from "#app/global-scene";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { GameModes } from "#enums/game-modes";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("dump dex catch", async () => {
  new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  const outFile = process.env.DEX_CATCH_OUT ?? "/tmp/gc_dex_catch.txt";
  const count = Number(process.env.DEX_CATCH_N ?? 1500);

  const rnd = new Phaser.Math.RandomDataGenerator(["dex_catch_94"]);
  const speciesList = speciesDataRegistry.getAllSpecies();
  const speciesIds = speciesList.map(s => s.speciesId).sort((a, b) => a - b);
  const starterIds = speciesDataRegistry.getAllStarters().sort((a, b) => a - b);
  const gameData = globalScene.gameData;
  const classic = getGameMode(GameModes.CLASSIC);
  const daily = getGameMode(GameModes.DAILY);

  // setPokemonSpeciesCaught(pokemon, species, ...) runs once for the caught species and once for each prevolution; the
  // "added as a starter" text is shown by the call that is on its species, so the species is noted on entry
  let current = 0;
  const original = (gameData as any).setPokemonSpeciesCaught.bind(gameData);
  vi.spyOn(gameData as any, "setPokemonSpeciesCaught").mockImplementation((...args: any[]) => {
    current = args[1].speciesId;
    return original(...args);
  });
  const shown: number[] = [];
  vi.spyOn(globalScene.ui, "showText").mockImplementation((text: any, _delay: any, callback: any) => {
    if (/ has been\s+added as a starter!$/.test(String(text))) shown.push(current);
    callback?.();
  });

  // species whose chain has a starter at its foot or a starter in the middle, so the root rule is exercised,
  // and plain starters, which the random picks alone reach less often
  const starterPool = speciesList.filter(s => speciesDataRegistry.isStarter(s.speciesId));
  const evolvedPool = speciesList.filter(s => speciesDataRegistry.getPrevolution(s.speciesId) != null);

  const opLines: string[] = [];
  let withMessages = 0;
  let dailyOps = 0;
  let bosses = 0;
  for (let i = 0; i < count; i++) {
    const pool = rnd.frac() < 0.4 ? starterPool : rnd.frac() < 0.5 ? evolvedPool : speciesList;
    const speciesObj = rnd.pick(pool);
    const speciesId = speciesObj.speciesId;
    const formIndex = rnd.between(0, Math.max(0, speciesObj.forms.length - 1));
    const gender = rnd.pick([-1, 0, 1]);
    const shiny = rnd.frac() < 0.25;
    const variant = shiny ? rnd.between(0, 2) : 0;
    const nature = rnd.between(0, 24);
    const abilityIndex = rnd.between(0, 2);
    const isDaily = rnd.frac() < 0.3;
    const boss = rnd.frac() < 0.2;
    const ivs = [0, 0, 0, 0, 0, 0].map(() => rnd.between(0, 31));

    const mon = globalScene.addPlayerPokemon(speciesObj, rnd.between(5, 50), abilityIndex, formIndex, gender, shiny, variant, ivs, nature);
    // an EnemyPokemon is a boss while it has shield segments; the dex only asks isBoss()
    (mon as any).isBoss = () => boss;

    // some roots already owned (all that a catch could unlock, or a part of it), so the early-return and
    // "has new attribute" branches are reached, not only the first catch of every species
    const owned = rnd.frac();
    if (owned < 0.45) {
      const entry = gameData.dexData[speciesId];
      entry.caughtAttr |= owned < 0.2 ? mon.getDexAttr() & speciesObj.getFullUnlocksData() : 157n;
      opLines.push(`OP_UNLOCK_SPECIES ${speciesId} ${entry.caughtAttr}`);
    }

    globalScene.gameMode = isDaily ? daily : classic;
    shown.length = 0;
    opLines.push(
      `OP_CATCH ${speciesId} ${formIndex} ${mon.gender} ${shiny ? 1 : 0} ${variant} ${abilityIndex} ${nature} ${isDaily ? 1 : 0} ${boss ? 1 : 0} ${ivs.join(" ")}`,
    );
    gameData.updateSpeciesDexIvs(mon.species.getRootSpeciesId(true), mon.ivs);
    await gameData.setPokemonCaught(mon);

    const started = [...shown];
    if (started.length > 0) withMessages++;
    if (isDaily) dailyOps++;
    if (boss) bosses++;
    opLines.push(`OP_MSG ${started.length}${started.map(s => " " + s).join("")}`);
  }
  globalScene.gameMode = classic;

  const dumpLines: string[] = [...opLines];
  for (const id of speciesIds) {
    const entry = gameData.dexData[id];
    if (!entry) continue;
    const ivs = entry.ivs || [0, 0, 0, 0, 0, 0];
    dumpLines.push(
      `DEX ${id} ${entry.seenAttr} ${entry.caughtAttr} ${entry.natureAttr} ${entry.seenCount} ${entry.caughtCount} ${entry.hatchedCount} ${ivs.join(" ")}`,
    );
  }
  for (const id of starterIds) {
    const entry = gameData.starterData[id];
    if (!entry) continue;
    let moves = [0, 0, 0, 0];
    if (Array.isArray(entry.moveset)) {
      moves = [entry.moveset[0] ?? 0, entry.moveset[1] ?? 0, entry.moveset[2] ?? 0, entry.moveset[3] ?? 0];
    }
    dumpLines.push(
      `STARTER ${id} ${entry.abilityAttr} ${entry.passiveAttr} ${entry.valueReduction} ${entry.candyCount} ${entry.friendship} ${entry.eggMoves} ${moves.join(" ")} ${entry.classicWinCount}`,
    );
  }
  fs.writeFileSync(outFile, dumpLines.join("\n") + "\n");
  console.log(`DEXCATCH ops=${count} daily=${dailyOps} boss=${bosses} with-starter-message=${withMessages}`);
}, 120000);
