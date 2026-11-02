/*
 * Dumps Dex and starter updates operations and resulting GameData state for game-core differential testing.
 * Writes /tmp/gc_dex_updates.txt (or DEX_UPDATES_OUT).
 */
import { globalScene } from "#app/global-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("dump dex updates", async () => {
  new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  const outFile = process.env.DEX_UPDATES_OUT ?? "/tmp/gc_dex_updates.txt";

  const rnd = new Phaser.Math.RandomDataGenerator(["dex_updates_777"]);
  const speciesList = speciesDataRegistry.getAllSpecies();
  const speciesIds = speciesList.map(s => s.speciesId).sort((a, b) => a - b);
  const starterIds = speciesDataRegistry.getAllStarters().sort((a, b) => a - b);

  const gameData = globalScene.gameData;
  const opLines: string[] = [];

  const count = 1000;
  for (let i = 0; i < count; i++) {
    const speciesObj = rnd.pick(speciesList);
    const speciesId = speciesObj.speciesId;
    const formIndex = rnd.between(0, Math.max(0, speciesObj.forms.length - 1));
    const gender = rnd.pick([-1, 0, 1]); /* -1=GENDERLESS, 0=MALE, 1=FEMALE */
    const shiny = rnd.frac() < 0.2;
    const variant = shiny ? rnd.between(0, 2) : 0;
    const nature = rnd.between(0, 24);
    const abilityIndex = rnd.between(0, 2);

    const mon = globalScene.addPlayerPokemon(
      speciesObj,
      rnd.between(5, 50),
      abilityIndex,
      formIndex,
      gender,
      shiny,
      variant,
      [rnd.between(0, 31), rnd.between(0, 31), rnd.between(0, 31), rnd.between(0, 31), rnd.between(0, 31), rnd.between(0, 31)],
      nature,
    );

    const opType = rnd.between(0, 3);
    if (opType === 0) {
      /* OP_SEEN */
      const inc = rnd.frac() < 0.8;
      const trainer = rnd.frac() < 0.3;
      opLines.push(`OP_SEEN ${speciesId} ${formIndex} ${mon.gender} ${shiny ? 1 : 0} ${variant} ${inc ? 1 : 0} ${trainer ? 1 : 0}`);
      gameData.setPokemonSeen(mon, inc, trainer);
    } else if (opType === 1) {
      /* OP_IVS */
      const ivs = [rnd.between(0, 31), rnd.between(0, 31), rnd.between(0, 31), rnd.between(0, 31), rnd.between(0, 31), rnd.between(0, 31)];
      opLines.push(`OP_IVS ${speciesId} ${ivs.join(" ")}`);
      gameData.updateSpeciesDexIvs(speciesId, ivs);
    } else if (opType === 2) {
      /* OP_CAUGHT */
      /* First unlock root caughtAttr for some mons so non-incrementing setPokemonCaught isn't short-circuited */
      const unlockRoot = rnd.frac() < 0.5;
      if (unlockRoot) {
        const rootId = speciesObj.getRootSpeciesId();
        gameData.dexData[rootId].caughtAttr |= 157n;
        opLines.push(`OP_UNLOCK_ROOT ${rootId}`);
      }
      opLines.push(`OP_CAUGHT ${speciesId} ${formIndex} ${mon.gender} ${shiny ? 1 : 0} ${variant} ${abilityIndex} ${nature}`);
      await gameData.setPokemonCaught(mon, false, false, false);
    } else {
      /* OP_FRIENDSHIP */
      const amount = rnd.between(-10, 50);
      const capped = rnd.frac() < 0.5;
      opLines.push(`OP_FRIENDSHIP ${speciesId} ${amount} ${capped ? 1 : 0}`);
      mon.addFriendship(amount, capped);
    }
  }

  /* Pokemon.addFriendship on a Pokemon made with DNA Splicers: each of the two root species gets its candy, at half the rate. A
   * stream of its own and placed after the operations above, so those stay as they were. Every eighth one fuses a Pokemon with a
   * species of its own line, which feeds the one bar twice. */
  const fusedRnd = new Phaser.Math.RandomDataGenerator(["dex_fused_friendship_777"]);
  for (let i = 0; i < 400; i++) {
    const speciesObj = fusedRnd.pick(speciesList);
    let fusionObj = fusedRnd.pick(speciesList);
    if (i % 8 === 0) {
      fusionObj =
        speciesList.find(s => s !== speciesObj && s.getRootSpeciesId() === speciesObj.getRootSpeciesId()) ?? fusionObj;
    }
    const amount = fusedRnd.between(-10, 50);
    const capped = fusedRnd.frac() < 0.5;
    const mon = globalScene.addPlayerPokemon(speciesObj, fusedRnd.between(5, 50), 0, 0, 0, false, 0, [0, 0, 0, 0, 0, 0], 0);
    mon.fusionSpecies = fusionObj;
    mon.fusionFormIndex = 0;
    opLines.push(`OP_FRIENDSHIP_FUSED ${speciesObj.speciesId} ${fusionObj.speciesId} ${amount} ${capped ? 1 : 0}`);
    mon.addFriendship(amount, capped);
  }

  // Dump final GameData
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
});
