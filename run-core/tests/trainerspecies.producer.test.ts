/*
 * Upstream half of run-core's trainer party species test (tools/difftest.sh copies it into the upstream
 * checkout). Each case builds a real Trainer of a random type (half of them with species pools) and
 * variant, gives the battle an enemy party of 0 to 5 members (real species, random types), a random
 * wave and game mode, then asks upstream's own trainer.genNewPartyMemberSpecies(level, strength) on a
 * freshly sown Phaser.Math.RND and records the species and the RNG state. build/trainerspeciestest
 * replays the cases.
 */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getGameMode } from "#app/game-mode";
import { BattleType } from "#enums/battle-type";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { TrainerType } from "#enums/trainer-type";
import { TrainerVariant } from "#enums/trainer-variant";
import { Trainer } from "#field/trainer";
import { GameManager } from "#test/framework/game-manager";
import { trainerConfigs } from "#trainers/trainer-config";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

it("trainerspecies producer", async () => {
  const N = Number(process.env.DIFF_N ?? 20000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-trainerspecies-cases.txt";
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const battle = scene.currentBattle as any;

  let seed = Number(process.env.DIFF_SEED ?? 12345);
  const rnd = (n: number) => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return (seed >>> 8) % n;
  };
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const seedStr = () => Array.from({ length: 24 }, () => chars[rnd(chars.length)]).join("");
  const state = () => Phaser.Math.RND.state().split(",").slice(1).join(" ");
  const modes = [GameModes.CLASSIC, GameModes.ENDLESS, GameModes.SPLICED_ENDLESS, GameModes.DAILY, GameModes.CHALLENGE];
  const types = Object.values(TrainerType).filter(v => typeof v === "number" && (trainerConfigs as any)[v]) as number[];
  const pooled = types.filter(t => (trainerConfigs as any)[t].speciesPools);
  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  const realEnemyParty = battle.enemyParty;

  const lines: string[] = [];
  let thrown = 0;
  for (let i = 0; i < N; i++) {
    const gm = getGameMode(modes[rnd(modes.length)]);
    scene.gameMode = gm;
    battle.gameMode = gm;
    const wave = rnd(8) === 0 ? 20 : 1 + rnd(300);
    battle.waveIndex = wave;
    battle.battleType = BattleType.TRAINER;
    const type = rnd(2) === 0 ? pooled[rnd(pooled.length)] : types[rnd(types.length)];
    const variant = [TrainerVariant.DEFAULT, TrainerVariant.FEMALE, TrainerVariant.DOUBLE][rnd(3)];
    Phaser.Math.RND.sow([seedStr()]);
    let trainer: any;
    try {
      trainer = new Trainer(type, variant);
    } catch {
      thrown++;
      lines.push(`X ${type}`);
      continue;
    }
    battle.trainer = trainer;
    const party: any[] = [];
    for (let k = rnd(6); k > 0; k--) {
      const sp = speciesDataRegistry.getSpecies(speciesIds[rnd(speciesIds.length)]);
      const t = rnd(3) === 0 ? [rnd(18)] : [rnd(18), rnd(18)];
      party.push({ species: sp, getTypes: () => t });
    }
    battle.enemyParty = party;
    const template = trainer.getPartyTemplate();
    const evoKind = template.evoLevelThresholdKind ?? 1;
    const balanced = template.isBalanced(party.length) ? 1 : 0;
    const level = 1 + rnd(100);
    Phaser.Math.RND.sow([seedStr()]);
    const before = state();
    let species: number;
    try {
      species = trainer.genNewPartyMemberSpecies(level, template.getStrength(party.length)).speciesId;
    } catch {
      thrown++;
      lines.push(`X ${type}`);
      trainer.destroy();
      continue;
    }
    const partyTypes = party.flatMap(p => p.getTypes());
    lines.push(
      [
        "T", gm.modeId, type, level, wave, evoKind, balanced, wave === 20 && gm.isClassic ? 1 : 0, party.length,
        ...party.map(p => p.species.getRootSpeciesId()), partyTypes.length, ...partyTypes,
        trainer.config.speciesPools ? 1 : 0, before, species, state(),
      ].join(" "),
    );
    trainer.destroy();
  }
  battle.enemyParty = realEnemyParty;
  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 900_000);
