/*
 * Upstream half of the Game Stats differential (tests/gamestats_diff_test.c): the counters GameData.setPokemonSeen and setPokemonSpeciesCaught keep in
 * gameStats, over random Pokemon: seen (a wild one or a trainer's, counted or not), caught and hatched (an egg), each with the species' rarity (sub-legendary,
 * legendary, mythical), shiny or not (a fusion's half counts), with and without the "added as a starter" message (which decides whether a prevolution's
 * stage is counted too). After every op a GS line holds the 15 counters those two functions write:
 *   pokemonSeen pokemonCaught pokemonHatched
 *   subLegendary{Seen,Caught,Hatched} legendary{...} mythical{...} shiny{...}
 * Copy into an upstream checkout as test/tests/zz-dump/gc-gamestats.producer.test.ts. GC_GS_N ops (default 2000), GC_GS_SEED, GC_GS_OUT.
 */
import { globalScene } from "#app/global-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("dump game stats", async () => {
  new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  const outFile = process.env.GC_GS_OUT ?? "/tmp/gc_gamestats.txt";
  const count = Number(process.env.GC_GS_N ?? 2000);
  const seed = process.env.GC_GS_SEED ?? "777";
  const rnd = new Phaser.Math.RandomDataGenerator(["gamestats", seed]);
  const gameData = globalScene.gameData;
  const speciesList = speciesDataRegistry.getAllSpecies();
  // the rarity flags are rare: favour the species that have one, and the evolved ones (their prevolution stages are counted by a catch)
  const rare = speciesList.filter(s => s.subLegendary || s.legendary || s.mythical);
  const evolved = speciesList.filter(s => speciesDataRegistry.getPrevolution(s.speciesId) != null);
  const starters = speciesList.filter(s => speciesDataRegistry.isStarter(s.speciesId));

  vi.spyOn(globalScene.ui, "showText").mockImplementation((_text: any, _delay: any, callback: any) => {
    callback?.();
  });

  const gs = () => gameData.gameStats;
  const line = () =>
    "GS "
    + [
      gs().pokemonSeen, gs().pokemonCaught, gs().pokemonHatched,
      gs().subLegendaryPokemonSeen, gs().subLegendaryPokemonCaught, gs().subLegendaryPokemonHatched,
      gs().legendaryPokemonSeen, gs().legendaryPokemonCaught, gs().legendaryPokemonHatched,
      gs().mythicalPokemonSeen, gs().mythicalPokemonCaught, gs().mythicalPokemonHatched,
      gs().shinyPokemonSeen, gs().shinyPokemonCaught, gs().shinyPokemonHatched,
    ].join(" ");

  const out: string[] = [];
  let seen = 0;
  let caught = 0;
  let hatched = 0;
  let fused = 0;
  for (let i = 0; i < count; i++) {
    const r = rnd.frac();
    const pool = r < 0.3 ? rare : r < 0.55 ? evolved : r < 0.7 ? starters : speciesList;
    const speciesObj = rnd.pick(pool);
    const formIndex = rnd.between(0, Math.max(0, speciesObj.forms.length - 1));
    const gender = rnd.pick([-1, 0, 1]);
    const shiny = rnd.frac() < 0.3;
    const variant = shiny ? rnd.between(0, 2) : 0;
    const nature = rnd.between(0, 24);
    const abilityIndex = rnd.between(0, 2);
    const ivs = [0, 0, 0, 0, 0, 0].map(() => rnd.between(0, 31));
    const mon = globalScene.addPlayerPokemon(speciesObj, rnd.between(5, 50), abilityIndex, formIndex, gender, shiny, variant, ivs, nature);
    // a fusion: isShiny() counts the fusion half's shine too
    const fusionSpecies = rnd.frac() < 0.25 ? rnd.pick(speciesList) : null;
    const fusionShiny = fusionSpecies != null && rnd.frac() < 0.5;
    if (fusionSpecies) {
      mon.fusionSpecies = fusionSpecies;
      mon.fusionShiny = fusionShiny;
      fused++;
    }
    const kind = rnd.pick(["SEEN", "SEEN", "CAUGHT", "CAUGHT", "HATCH"]);
    const head = `${speciesObj.speciesId} ${formIndex} ${mon.gender} ${shiny ? 1 : 0} ${variant} ${abilityIndex} ${nature} ${fusionSpecies ? fusionSpecies.speciesId : 0} ${fusionShiny ? 1 : 0}`;
    if (kind === "SEEN") {
      const increment = rnd.frac() < 0.85;
      const trainer = rnd.frac() < 0.3;
      out.push(`OP_SEEN ${head} ${increment ? 1 : 0} ${trainer ? 1 : 0}`);
      gameData.setPokemonSeen(mon, increment, trainer);
      seen++;
    } else {
      const fromEgg = kind === "HATCH";
      const showMessage = rnd.frac() < 0.6;
      // some roots already owned, so a catch is a new starter or not
      if (rnd.frac() < 0.4) {
        const entry = gameData.dexData[speciesObj.speciesId];
        entry.caughtAttr |= 157n;
        out.push(`OP_UNLOCK_SPECIES ${speciesObj.speciesId} ${entry.caughtAttr}`);
      }
      out.push(`OP_CATCH ${head} ${fromEgg ? 1 : 0} ${showMessage ? 1 : 0}`);
      await gameData.setPokemonCaught(mon, true, fromEgg, showMessage);
      if (fromEgg) hatched++;
      else caught++;
    }
    out.push(line());
  }
  fs.writeFileSync(outFile, out.join("\n") + "\n");
  console.log(`GAMESTATS ops=${count} seen=${seen} caught=${caught} hatched=${hatched} fused=${fused}`);
}, 300000);
