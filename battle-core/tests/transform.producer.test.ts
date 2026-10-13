/* Additive owning boundary proof. Executes pinned TransformAttr, Imposter's
 * real canApply/apply and PokemonTransformPhase; only presentation is stubbed. */
import { BattleScene } from "#app/battle-scene";
import { audioManager } from "#app/global-audio-manager";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { PokemonMove } from "#moves/pokemon-move";
import { applyAbAttrs } from "#abilities/apply-ab-attrs";
import { PokemonTransformPhase } from "#phases/pokemon-transform-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it, vi } from "vitest";

const N = Number(process.env.TRANSFORM_N ?? 2000);
let seed = Number(process.env.TRANSFORM_SEED ?? 777);
const initialSeed = seed;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const stateRng = (s: string) => s.replace(/^!rnd,/, "").replaceAll(",", " ");
const rows = [`TRANSFORM 1 ${N} ${initialSeed}`];
const counts = Array(24).fill(0);
// Fixed 104-number projection, including permanent/effective identity, moves and sprite scale.
function snapshot(p: any): number[] {
  const sd = p.summonData, form = p.getSpeciesForm(), transformed = p.isTransformed();
  const formIndex = Math.max(0, speciesDataRegistry.getSpecies(form.speciesId).forms.indexOf(form));
  const moves = (ms: any[]) => [ms.length, ...Array.from({ length: 4 }, (_, i) => ms[i]?.moveId ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUsed ?? 0), ...Array.from({ length: 4 }, (_, i) => ms[i]?.ppUp ?? 0),
    ...Array.from({ length: 4 }, (_, i) => ms[i]?.maxPpOverride ?? -1)];
  const types = p.getTypes();
  return [p.species.speciesId, p.formIndex, p.gender, p.id, p.level, p.nature, p.hp, p.status?.effect ?? 0,
    +p.shiny, p.getAbility(true).id, p.getAbility().id, p.passive ? p.getPassiveAbility().id : 0, +p.passive,
    p.customPokemonData.passive, p.fusionSpecies?.speciesId ?? 0, sd.illusion?.species ?? 0,
    +p.isOnField(), +p.switchOutStatus,
    ...p.stats, ...Array.from({ length: 6 }, (_, s) => sd.stats[s] ?? 0),
    ...[Stat.ATK, Stat.DEF, Stat.SPATK, Stat.SPDEF, Stat.SPD, Stat.ACC, Stat.EVA].map(s => p.getStatStage(s)),
    ...moves(p.getMoveset()), sd.types?.length ?? 0, ...Array.from({ length: 4 }, (_, i) => sd.types?.[i] ?? 0),
    sd.addedType ?? 0, +p.isTerastallized, p.teraType, +transformed,
    transformed ? sd.speciesForm.speciesId : 0, transformed ? formIndex : 0, transformed ? sd.gender ?? 0 : 0,
    ...(transformed ? moves(p.moveset) : [0, ...Array(12).fill(0), ...Array(4).fill(0)]),
    +!!p.getTag(BattlerTagType.POWER_TRICK), +!!p.getTag(BattlerTagType.SUBSTITUTE),
    form.speciesId, formIndex, p.getGender(), ...Array.from({ length: 6 }, (_, s) => p.getStat(s, false)),
    types.length, ...Array.from({ length: 5 }, (_, i) => types[i] ?? 0), p.variant, p.getSpriteScale(), +p.isMega(), +p.isMax()];
}
it("pinned Transform and Imposter boundary", async () => {
  if (!process.versions.node.startsWith("22.") || !Number.isSafeInteger(N) || N < 48) throw Error("Node22 and N>=48 required");
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
    .passiveAbility(AbilityId.NONE).enemyPassiveAbility(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP)
    .moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
  await game.classicMode.startBattle(SpeciesId.DITTO, SpeciesId.MEW);
  game.override.moveset([]).enemyMoveset([]);
  const scene: any = game.scene, pm: any = scene.phaseManager, battle: any = scene.currentBattle;
  BattleScene.prototype.randBattleSeedInt = function(range: number, min = 0) { return (this as any).currentBattle.randSeedInt(range, min); };
  const slots: any[] = scene.getField();
  const species = [SpeciesId.DITTO, SpeciesId.MEW, SpeciesId.CHARIZARD, SpeciesId.PIKACHU, SpeciesId.AEGISLASH, SpeciesId.GENGAR];
  const moveIds = [MoveId.TRANSFORM, MoveId.SPLASH, MoveId.TACKLE, MoveId.SKETCH, MoveId.RECOVER, MoveId.GROWL];
  let sounds: string[] = [];
  vi.spyOn(audioManager, "playSound").mockImplementation((key: any) => { sounds.push(String(key)); });
  for (const p of slots) {
    vi.spyOn(p, "loadAssets").mockImplementation(() => Promise.resolve());
    vi.spyOn(p, "updateInfo").mockImplementation(() => Promise.resolve());
    vi.spyOn(p, "playAnim").mockImplementation(() => {});
  }
  const attr = allMoves[MoveId.TRANSFORM].getAttrs("TransformAttr")[0];
  for (let i = 0; i < N; i++) {
    const state = i % 24, kind = Math.floor(i / 24) % 3; counts[state]++;
    slots.forEach((p, b) => {
      p.resetSummonData(); p.resetTurnData(); p.resetBattleAndWaveData();
      p.species = speciesDataRegistry.getSpecies(b === 0 ? SpeciesId.DITTO : species[rnd(species.length)]);
      p.formIndex = b === 0 ? 0 : rnd(p.species.forms.length || 1);
      p.fusionSpecies = null; p.fusionAbilityIndex = -1; p.level = 1 + rnd(100); p.nature = rnd(25);
      p.shiny = !!rnd(2); p.variant = rnd(3); p.gender = rnd(3) - 1;
      p.passive = false; p.customPokemonData.passive = AbilityId.NONE;
      p.summonData.ability = b === 0 && kind === 1 ? AbilityId.IMPOSTER : AbilityId.BALL_FETCH;
      p.switchOutStatus = false; p.isTerastallized = false; p.teraType = rnd(18); p.status = null;
      p.stats = Array.from({ length: 6 }, () => 20 + rnd(1000)); p.hp = 1 + rnd(p.stats[0]);
      for (const s of [Stat.ATK, Stat.DEF, Stat.SPATK, Stat.SPDEF, Stat.SPD, Stat.ACC, Stat.EVA]) p.setStatStage(s, rnd(13) - 6);
      p.moveset = Array.from({ length: 1 + rnd(4) }, () => new PokemonMove(moveIds[rnd(moveIds.length)], rnd(3), rnd(4), rnd(2) ? undefined : 7));
      p.generateName();
    });
    const user = slots[0], target = slots[2];
    if (state === 2) target.fusionSpecies = speciesDataRegistry.getSpecies(SpeciesId.MEW);
    if (state === 3) user.fusionSpecies = speciesDataRegistry.getSpecies(SpeciesId.MEW);
    if (state === 4) target.summonData.illusion = { species: user.species.speciesId, formIndex: 0, gender: -1 };
    if (state === 5) user.summonData.illusion = { species: target.species.speciesId, formIndex: 0, gender: 1 };
    if (state === 6 || state === 8) {
      const p = state === 6 ? user : target; p.summonData.speciesForm = speciesDataRegistry.getSpecies(SpeciesId.PIKACHU);
      p.summonData.gender = 1; p.summonData.moveset = [new PokemonMove(MoveId.GROWL, 1, 0, 5)];
    }
    if (state === 7 || state === 9) target.addTag(BattlerTagType.SUBSTITUTE, 0, MoveId.SUBSTITUTE, target.id);
    if (state === 9) slots[3].addTag(BattlerTagType.SUBSTITUTE, 0, MoveId.SUBSTITUTE, slots[3].id);
    if (state === 10) user.addTag(BattlerTagType.POWER_TRICK, 0, MoveId.POWER_TRICK, user.id);
    if (state === 11) for (let s = 1; s < 6; s++) target.setStat(s, 1.5 * target.stats[s], false);
    if (state === 12) { target.summonData.types = [3, 9]; target.summonData.addedType = 7; }
    if (state === 13) { user.summonData.addedType = 7; target.summonData.types = [3, 9]; target.summonData.addedType = 12; }
    if (state === 14) { user.isTerastallized = true; target.isTerastallized = true; }
    if (state === 15) { target.hp = 0; }
    if (state === 16) { slots[3].hp = 0; }
    if (state === 17) { user.passive = true; user.customPokemonData.passive = AbilityId.RUN_AWAY; }
    if (state === 18) { target.summonData.ability = AbilityId.INTIMIDATE; }
    if (state === 19) { target.summonData.ability = AbilityId.IMPOSTER; }
    if (state === 20) { user.summonData.abilitySuppressed = true; }
    if (state === 22) target.summonData.types = [-1]; // pinned UNKNOWN typing is copied, not "fixed"
    // Missing target is a direct-phase guard, separate from caller eligibility.
    const missing = state === 21 && kind === 2;
    const targetSlot = state === 23 && kind !== 1 ? 0 : 2;
    const moveTarget = slots[targetSlot];
    pm.clearAllPhases(); pm.phaseQueue.deferredActive = false; pm.dynamicQueueManager.clearLastTurnOrder();
    battle.battleSeed = `transform-${initialSeed}-${i}`; battle.battleSeedState = null;
    battle.turn = 1; scene.waveSeed = `transform-wave-${initialSeed}`;
    Phaser.Math.RND.sow([`transform-global-${initialSeed}-${i}`]); battle.randSeedInt(2);
    rows.push(`C ${i} ${kind} ${+missing} ${+user.canTransformInto(moveTarget)} ${+user.summonData.abilitySuppressed} ${targetSlot}`,
      `R ${stateRng(Phaser.Math.RND.state())}`, `B ${stateRng(battle.battleSeedState)}`,
      ...slots.map((p, b) => `I ${b} ${snapshot(p).join(" ")}`));
    sounds = [];
    if (kind === 0) {
      if (attr.getCondition()!(user, moveTarget, allMoves[MoveId.TRANSFORM])) attr.apply(user, moveTarget, allMoves[MoveId.TRANSFORM], []);
    } else if (kind === 1) applyAbAttrs("PostSummonTransformAbAttr", { pokemon: user });
    else pm.unshiftNew("PokemonTransformPhase", 0, targetSlot, false);
    const transforms: any[] = pm.dynamicQueueManager.dynamicPhaseMap.get("PokemonTransformPhase")?.queue.slice() ?? [];
    const chosen = transforms[0]?.targetIndex ?? -1;
    rows.push(`CHOSEN ${chosen}`, `RB ${stateRng(battle.battleSeedState)}`);
    for (const phase of transforms) {
      vi.spyOn(phase, "end").mockImplementation(() => {});
      // Clear earlier ability display phases, then execute the owning phase.
      pm.clearAllPhases();
      if (missing) battle.enemyParty[0] = undefined;
      phase.start(); await Promise.resolve(); await Promise.resolve(); await Promise.resolve();
      if (missing) battle.enemyParty[0] = target;
    }
    rows.push(...slots.map((p, b) => `O ${b} ${snapshot(p).join(" ")}`),
      `RO ${stateRng(Phaser.Math.RND.state())}`, `BO ${stateRng(battle.battleSeedState)}`,
      `SOUND ${sounds.filter(s => s === "battle_anims/PRSFX- Transform").length}`);
    for (const p of slots) p.resetSummonData();
    rows.push(...slots.map((p, b) => `Z ${b} ${snapshot(p).join(" ")}`), "E");
  }
  rows.push(`TRANSFORM_COMPLETE ${N}`);
  fs.writeFileSync(process.env.TRANSFORM_OUT ?? "/tmp/transform.txt", rows.join("\n") + "\n");
  console.log(`TRANSFORM cases=${N} states=${counts.join("/")}`);
});
