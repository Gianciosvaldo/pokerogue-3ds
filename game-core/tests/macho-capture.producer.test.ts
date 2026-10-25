import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { PokeballType } from "#enums/pokeball";
import { SpeciesId } from "#enums/species-id";
import { UiMode } from "#enums/ui-mode";
import { AttemptCapturePhase } from "#phases/attempt-capture-phase";
import { PartyExpPhase } from "#phases/party-exp-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect, it } from "vitest";

it("native successful catch queues the real Macho Brace Victory owner", async () => {
  const native = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset(MoveId.SPLASH)
    .enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
  await game.classicMode.startBattle(SpeciesId.MEW);
  BattleScene.prototype.randBattleSeedInt = native;
  const scene: any = game.scene, p: any = scene.getPlayerPokemon(), enemy: any = scene.getEnemyPokemon();
  const party = scene.getPlayerParty(), pm: any = scene.phaseManager, rows: any[] = [];
  while (party.length < 6) party.push(scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.MEW), 5));
  for (const q of party) q.updateInfo = async () => {};
  scene.modifierBar.updateModifiers = () => {};
  // Presentation completions only. The actual catch, menu handler, leaveField,
  // native VictoryPhase and PartyExpPhase owners execute without replacement.
  scene.ui.showText = (_text: string, _delay: unknown, done?: () => void) => done?.();
  scene.ui.setMode = async (mode: UiMode, config: any) => {
    if (mode === UiMode.OPTION_SELECT) config.options.at(-1).handler();
  };
  enemy.hideInfo = async () => {};
  const snapshot = (m: any) => ({ stack: m.stackCount, hp: p.hp, stats: p.stats.slice(), friendship: p.friendship });
  for (const stack of [1, 49, 50]) for (const level of [5, 1000]) for (const participated of [false, true]) {
    pm.phaseQueue.levels = [[]]; pm.phaseQueue.currentLevel = 0; pm.phaseQueue.deferredActive = false;
    scene.modifiers = []; scene.enemyModifiers = [];
    p.level = level; p.nature = 0; p.customPokemonData.nature = -1;
    p.ivs = Array(6).fill(31); p.friendship = 0;
    const factory = modifierTypes.MYSTERY_ENCOUNTER_MACHO_BRACE;
    const m: any = factory().withIdFromFunc(factory).newModifier(p); m.stackCount = stack;
    scene.modifiers = [m]; p.calculateStats(); p.hp = p.stats[0];
    scene.currentBattle.playerParticipantIds = new Set(participated ? [p.id] : []);
    enemy.hp = enemy.stats[0]; enemy.status = null; enemy.switchOutStatus = false;
    if (!enemy.isOnField()) scene.field.add(enemy);
    const before = snapshot(m), globalRng = Phaser.Math.RND.state(), battleRng = scene.currentBattle.battleSeedState;
    const capture: any = new AttemptCapturePhase(0, PokeballType.MASTER_BALL);
    capture.removePb = () => {}; capture.end = () => {};
    capture.catch();
    let victory: any;
    for (let turn = 0; turn < 100 && !victory; turn++) {
      await Promise.resolve();
      victory = pm.phaseQueue.levels.flat().find((q: any) => q.phaseName === "VictoryPhase");
    }
    expect(victory?.phaseName).toBe("VictoryPhase");
    expect(party.length).toBe(6); expect(enemy.hp).toBe(0);
    expect(m.stackCount).toBe(stack);
    victory.end = () => {}; victory.start();
    const after = snapshot(m);
    // The non-victory EXP phase must leave the earned stock and friendship alone.
    const partyExp: any = new PartyExpPhase(17, false, scene.currentBattle.playerParticipantIds);
    partyExp.end = () => {}; partyExp.start();
    expect(snapshot(m)).toEqual(after);
    expect(Phaser.Math.RND.state()).toBe(globalRng);
    expect(scene.currentBattle.battleSeedState).toBe(battleRng);
    rows.push({ stack, level, participated, before, after, victoryQueued: true, partyExpUnchanged: true });
  }
  expect(rows.length).toBe(12);
  fs.writeFileSync(process.env.MACHO_CAPTURE_OUT!, JSON.stringify({ version: 1,
    pin: "8555c08c823b856cbec4eb99ca84ea52a955836d", rows }) + "\n");
}, 120000);
