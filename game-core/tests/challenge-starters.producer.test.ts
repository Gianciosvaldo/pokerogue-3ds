/* Challenge Mode's starter rules, against upstream at 8555c08: Fresh Start, through the real
 * functions the starter select and SelectStarterPhase call, in a real Challenge Mode game mode:
 *   getRunValueLimit()                      STARTER_POINTS     (no ported challenge changes it: 10)
 *   gameData.getSpeciesStarterValue(id)     STARTER_COST       (Fresh Start: the base cost)
 *   checkStarterValidForChallenge(id, props, soft)   STARTER_CHOICE (Fresh Start 1), and its evolution walk
 *   getStarterData(id)                      STARTER_SELECT_MODIFY (Fresh Start's view of the profile)
 *   SelectStarterPhase.initBattle(starters) STARTER_MODIFY     (Fresh Start's changes to the starter Pokemon)
 * Each case sets a random profile for the species it asks about (dex and starter entries), sets the challenges (each 0 = off), and records
 * what upstream answers. The harness stubs what initBattle does after the party (newBattle, the arena, assets, the music).
 *
 * Trace lines (whitespace separated):
 *   V 1 SEED N
 *   C i fresh budget inverse flip passives                                  one case's Fresh Start value and getRunValueLimit()
 *   D species caught_hex nature ability passive reduction egg iv0..iv5     the profile entry the next lines read
 *   Q species form shiny variant female cost hard soft_valid   cost = getSpeciesStarterValue, hard = checkStarterValidForChallenge(.., soft = false),
 *                                                      soft_valid = the same with soft = true
 *   W species caught_hex nature ability passive reduction egg iv0..iv5     getStarterData(species) under the challenges
 *   B n before(c s0 s1 s2) | after                     a party of n, and the Phaser RNG around SelectStarterPhase.initBattle
 *   S species form shiny variant female ability nature passive tera iv0..iv5 m0 m1 m2 m3     the starter handed over (moves 0-padded)
 *   T inverse m(-1,-1) ... m(18,18)                           getTypeDamageMultiplier(attack, defend) for -1..18 each (361 values), under Inverse Battle's value
 *   E k e0..e17                                       party member k's getAttackTypeEffectiveness against the 18 types (no source)
 *   P k passive                                       party member k's hasPassive()
 *   F species form stat0..stat5                       member 0 fused with member 1 (that species and form): its calculateStats() stats
 *   Y trainer boss passive wave hasPassive            an enemy Pokemon's hasPassive() (trainer slot set, bossSegments, its passive flag, the wave)
 *   M id species form level nature abilityIndex ability passive gender luck tera hp stat0..stat5 iv0..iv5 m0..m3   the Pokemon after the first
 *                                                      updatePartyForModifiers (the stats upstream recalculates when the first battle starts)
 *   R eviolite miniBlackHole / N nature minted hp stats / U stat boosted hp stats   the rewards (see the block)
 *   L limitedCatch limitedSupport hardcore             the case's run rule values (after C)
 *   G wave baseCost shopStatus n item1 cost1 ..        gameMode.getShopStatus() and getPlayerShopModifierTypeOptionsForWave(wave, baseCost) (SHOP, SHOP_ITEM)
 *   H heal preventRevive                               applyChallenges(PARTY_HEAL, true) and applyChallenges(PREVENT_REVIVE, false)
 *   K move valid usable allowed                        applyChallenges(POKEMON_MOVE, move, true), new PokemonMove(move).isUsable(member 0, true) and Copycat's
 *                                                      CallMoveAttrWithBanlist.isMoveAllowed(move)
 *   A metWave me option valid                          applyChallenges(POKEMON_ADD_TO_PARTY, enemy, true) for an enemy met on metWave, in a battle with no
 *                                                      encounter (me -1), Teleporting Hijinks (0) or Fight or Flight (1), its option 0..2 selected
 *   I singleGeneration singleType                      the case's Single Generation and Single Type values (after L; they change Q and W too)
 *   J species caughtAttr viewCaughtAttr                getStarterData(species).dexEntry.caughtAttr of Ralts, Snorunt and Burmy with both genders caught
 *                                                      (cases without Fresh Start)
 *   X species form fusionSpecies fusionForm allowed    isAllowedInChallenge() of member 0 made that species and form (fusion -1: none), each
 *                                                      member as it is first (its own species and form)
 * GC_CHALLENGE_N=400 GC_CHALLENGE_SEED=777 GC_CHALLENGE_OUT=/abs/trace.txt, Node 22. */
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { defaultStarterSpecies } from "#app/constants";
import { audioManager } from "#app/global-audio-manager";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { activeOverrides } from "#app/overrides";
import { speciesEggMoves } from "#balance/egg-moves";
import { Challenges } from "#enums/challenges";
import { ModifierTier } from "#enums/modifier-tier";
import { Unlockables } from "#enums/unlockables";
import { modifierPool } from "#modifiers/modifier-pools";
import { BaseStatBoosterModifierType, getPlayerShopModifierTypeOptionsForWave, PokemonNatureChangeModifierType } from "#modifiers/modifier-type";
import { allMoves } from "#data/data-lists";
import { ChallengeType } from "#enums/challenge-type";
import { MoveId } from "#enums/move-id";
import { MysteryEncounterType } from "#enums/mystery-encounter-type";
import { PokemonMove } from "#moves/pokemon-move";
import { BooleanHolder } from "#utils/common";
import { getTypeDamageMultiplier } from "#data/type";
import { EnemyPokemon } from "#field/pokemon";
import { TrainerSlot } from "#enums/trainer-slot";
import { GameModes } from "#enums/game-modes";
import { SpeciesId } from "#enums/species-id";
import { PokemonType } from "#enums/pokemon-type";
import { Pokemon } from "#field/pokemon";
import { SelectStarterPhase } from "#phases/select-starter-phase";
import { GameManager } from "#test/framework/game-manager";
import { getRunValueLimit, getStarterData } from "#ui/utils/starter-select-ui-utils";
import { applyChallenges, checkStarterValidForChallenge } from "#utils/challenge-utils";
import * as fs from "node:fs";
import Phaser from "phaser";
import { expect, it, vi } from "vitest";

const N = Number(process.env.GC_CHALLENGE_N ?? 400);
const SEED = Number(process.env.GC_CHALLENGE_SEED ?? 777);
const OUT = process.env.GC_CHALLENGE_OUT ?? "/tmp/challenge-starters.txt";
let seed = SEED;
const rnd = (n: number) => {
  seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
  return (seed >>> 8) % n;
};
const state = (r: any) => r.state().replace(/^!rnd,/, "").split(",").join(" ");

it("native Challenge Mode starter rules", async () => {
  expect(Number(process.versions.node.split(".")[0])).toBe(22);
  const native = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5);
  await game.classicMode.startBattle(SpeciesId.MEW);
  BattleScene.prototype.randBattleSeedInt = native;
  const scene: any = game.scene;
  const ids: number[] = speciesDataRegistry.getAllStarters().sort((a: number, b: number) => a - b);
  for (const key of ["SHINY_OVERRIDE", "VARIANT_OVERRIDE", "NATURE_OVERRIDE", "IVS_OVERRIDE", "GENDER_OVERRIDE", "STARTER_SPECIES_OVERRIDE", "STARTER_FUSION_OVERRIDE", "STARTER_FUSION_SPECIES_OVERRIDE"])
    vi.spyOn(activeOverrides as any, key as any, "get").mockReturnValue(key === "STARTER_FUSION_OVERRIDE" ? false : (null as any));
  vi.spyOn(scene, "newBattle").mockImplementation((() => {}) as any);
  vi.spyOn(scene.arena, "init").mockImplementation((() => {}) as any);
  vi.spyOn(audioManager, "playBgm").mockImplementation((() => {}) as any);
  vi.spyOn(Pokemon.prototype, "loadAssets").mockResolvedValue(undefined as any);
  vi.spyOn(Pokemon.prototype, "updateInfo").mockResolvedValue(undefined as any);

  const lines: string[] = [`V 1 ${SEED} ${N}`];
  let seenChart = 0;
  const cover = { genderDropped: 0, monoAllowed: 0, monoBarred: 0, monoFused: 0, monoSoftForm: 0, monoGender: 0, inverse: 0, flip: 0, flipFusion: 0, passives: 0, enemyPassive: 0, freshPools: 0, openPools: 0, flipRewards: 0, hardShops: 0, closedShops: 0, noHeals: 0, refusedMoves: 0, refusedAdds: 0, teleportAdds: 0, freshCases: 0, validHard: 0, invalidHard: 0, softOnly: 0, moved: 0, shinyFresh: 0, formReset: 0, eggRemoved: 0 };
  const FORM_SPECIES = [SpeciesId.PIKACHU, SpeciesId.EEVEE, SpeciesId.PICHU, SpeciesId.ROTOM, SpeciesId.MELOETTA];
  // species whose forms change by an item (mega stones, plates, memories, masks...) or whose types depend on the form, and the ones whose
  // gender Single Generation / Single Type keep
  const FORM_CHANGE_SPECIES = [SpeciesId.CHARIZARD, SpeciesId.AMPHAROS, SpeciesId.GYARADOS, SpeciesId.PINSIR, SpeciesId.LOPUNNY, SpeciesId.ALTARIA,
    SpeciesId.SCEPTILE, SpeciesId.AUDINO, SpeciesId.ARCEUS, SpeciesId.SILVALLY, SpeciesId.OGERPON, SpeciesId.ZACIAN, SpeciesId.GROUDON,
    SpeciesId.NECROZMA, SpeciesId.CASTFORM, SpeciesId.MELOETTA, SpeciesId.ROTOM, SpeciesId.ORICORIO, SpeciesId.RALTS, SpeciesId.SNORUNT,
    SpeciesId.BURMY, SpeciesId.WORMADAM, SpeciesId.SHAYMIN, SpeciesId.GIRATINA, SpeciesId.KYUREM, SpeciesId.CALYREX];
  const hex = (b: bigint) => b.toString(16);

  for (let i = 0; i < N; i++) {
    // the challenge: Fresh Start 1 or 2 in most cases, none in some (a Challenge Mode game with every challenge at 0 is Classic's starters).
    // Lower Max Starter Cost and Lower Starter Points are not in upstream's allChallenges: setChallengeValue on them changes nothing.
    const fresh = i % 7 === 0 ? 0 : 1 + ((i + rnd(2)) % 2);
    scene.gameMode = getGameMode(GameModes.CHALLENGE);
    scene.gameMode.setChallengeValue(Challenges.FRESH_START, fresh);
    // the battle rules: Inverse Battle and Flip Stat (0 or 1), Passives (0, 1 or 2)
    const inverse = rnd(3) === 0 ? 1 : 0;
    const flip = rnd(3) === 0 ? 1 : 0;
    const passives = rnd(3);
    scene.gameMode.setChallengeValue(Challenges.INVERSE_BATTLE, inverse);
    scene.gameMode.setChallengeValue(Challenges.FLIP_STAT, flip);
    scene.gameMode.setChallengeValue(Challenges.PASSIVES, passives);
    if (fresh) cover.freshCases++;
    if (inverse) cover.inverse++;
    if (flip) cover.flip++;
    if (passives) cover.passives++;
    lines.push(`C ${i} ${fresh} ${getRunValueLimit()} ${inverse} ${flip} ${passives}`);
    // the run rules: Limited Catch (0, 1), Limited Support (0..3), Hardcore (0, 1)
    const limitedCatch = rnd(2);
    const support = rnd(4);
    const hardcore = rnd(2);
    scene.gameMode.setChallengeValue(Challenges.LIMITED_CATCH, limitedCatch);
    scene.gameMode.setChallengeValue(Challenges.LIMITED_SUPPORT, support);
    scene.gameMode.setChallengeValue(Challenges.HARDCORE, hardcore);
    lines.push(`L ${limitedCatch} ${support} ${hardcore}`);
    // Single Generation (a quarter of the cases) and Single Type (most): the starter rules (Q, W) and isAllowedInChallenge (X) read them
    let singleGen = rnd(4) === 0 ? 1 + rnd(9) : 0;
    let singleType = rnd(4) === 0 ? 0 : 1 + rnd(18);
    // the cases without Fresh Start (whose STARTER_SELECT_MODIFY would answer first) cycle through the values that drop a gender: Single
    // Generation 4 (Ralts, Snorunt), then Single Type Fighting (Ralts), Ghost (Snorunt), Flying and Grass (Burmy), then a random one
    if (!fresh) {
      const j = Math.floor(i / 7) % 6;
      if (j === 0) { singleGen = 4; singleType = 0; }
      else if (j <= 4) { singleGen = 0; singleType = 1 + [PokemonType.FIGHTING, PokemonType.GHOST, PokemonType.FLYING, PokemonType.GRASS][j - 1]; }
    }
    scene.gameMode.setChallengeValue(Challenges.SINGLE_GENERATION, singleGen);
    scene.gameMode.setChallengeValue(Challenges.SINGLE_TYPE, singleType);
    lines.push(`I ${singleGen} ${singleType}`);
    // getTypeDamageMultiplier over the whole chart (UNKNOWN, the 18 types and Stellar), once for each Inverse Battle value
    if (!(seenChart >> inverse & 1)) {
      seenChart |= 1 << inverse;
      const row: number[] = [];
      for (let a = -1; a <= 18; a++) for (let d = -1; d <= 18; d++) row.push(getTypeDamageMultiplier(a, d));
      lines.push(`T ${inverse} ${row.join(" ")}`);
    }

    // profile queries: a random profile for a handful of starters
    for (let q = 0; q < 16; q++) {
      // a third from the species Fresh Start 1 allows; some with item form changes (Single Type's soft check reads them) or the genders
      // Single Generation and Single Type keep
      const pick = rnd(6);
      const id = pick === 0 ? speciesDataRegistry.getSpecies(FORM_CHANGE_SPECIES[rnd(FORM_CHANGE_SPECIES.length)]).getRootSpeciesId() : pick <= 2 ? defaultStarterSpecies[rnd(defaultStarterSpecies.length)] : ids[rnd(ids.length)];
      const species = speciesDataRegistry.getSpecies(id);
      const dex = scene.gameData.dexData[id];
      const sd = scene.gameData.starterData[id];
      const forms = Math.max(1, species.forms.length);
      const form = rnd(forms);
      // caughtAttr: shiny / non-shiny, the variants, genders, and the form bits (bit 7 + form)
      let caught = 0n;
      if (rnd(4) !== 0) caught |= 1n;
      if (rnd(3) === 0) caught |= 2n;
      if (rnd(2)) caught |= 4n;
      if (rnd(2)) caught |= 8n;
      caught |= 16n;
      if (rnd(3) === 0) caught |= 32n;
      if (rnd(4) === 0) caught |= 64n;
      for (let f = 0; f < forms; f++) if (f === 0 || rnd(2)) caught |= 128n << BigInt(f);
      dex.caughtAttr = caught;
      let nat = 0;
      for (let n = 0; n < 25; n++) if (rnd(3) === 0) nat |= 1 << (n + 1);
      dex.natureAttr = nat;
      dex.ivs = [0, 0, 0, 0, 0, 0].map(() => rnd(32));
      sd.abilityAttr = rnd(8);
      sd.passiveAttr = rnd(4);
      sd.valueReduction = rnd(3);
      sd.eggMoves = rnd(16);
      lines.push(`D ${id} ${hex(caught)} ${nat} ${sd.abilityAttr} ${sd.passiveAttr} ${sd.valueReduction} ${sd.eggMoves} ${dex.ivs.join(" ")}`);
      const props = { formIndex: form, shiny: rnd(2) === 0, variant: rnd(3), female: rnd(2) === 0 };
      const cost = scene.gameData.getSpeciesStarterValue(id);
      const hard = checkStarterValidForChallenge(id, props, false);
      const soft = checkStarterValidForChallenge(id, props, true);
      if (hard) cover.validHard++;
      else cover.invalidHard++;
      if (!hard && soft) cover.softOnly++;
      if (singleType && !hard && soft && pick === 0) cover.monoSoftForm++;
      lines.push(`Q ${id} ${form} ${+props.shiny} ${props.variant} ${+props.female} ${cost} ${+hard} ${+soft}`);
      const view = getStarterData(id);
      lines.push(
        `W ${id} ${hex(view.dexEntry.caughtAttr)} ${view.dexEntry.natureAttr} ${view.starterDataEntry.abilityAttr} ${view.starterDataEntry.passiveAttr} ${view.starterDataEntry.valueReduction} ${view.starterDataEntry.eggMoves} ${view.dexEntry.ivs.join(" ")}`,
      );
      if (fresh && FORM_SPECIES.includes(id) && (view.dexEntry.caughtAttr & ~255n) === 0n && (caught & ~255n) !== 0n) cover.formReset++;
      if (fresh && sd.eggMoves && view.starterDataEntry.eggMoves === 0) cover.eggRemoved++;
      if ((singleGen || singleType) && (view.dexEntry.caughtAttr & 12n) !== (caught & 12n)) cover.monoGender++;
    }

    // the species whose gender Single Generation and Single Type keep, both genders caught, in every case without Fresh Start
    if (!fresh) {
      for (const id of [SpeciesId.RALTS, SpeciesId.SNORUNT, SpeciesId.BURMY]) {
        const dex = scene.gameData.dexData[id];
        dex.caughtAttr = 1n | 4n | 8n | 16n | 128n | (id === SpeciesId.BURMY ? 256n | 512n : 0n);
        const view = getStarterData(id);
        if ((view.dexEntry.caughtAttr & 12n) !== 12n) cover.genderDropped++;
        lines.push(`J ${id} ${hex(dex.caughtAttr)} ${hex(view.dexEntry.caughtAttr)}`);
      }
    }

    // a party through SelectStarterPhase.initBattle
    const n = 1 + rnd(6);
    const starters: any[] = [];
    const used = new Set<number>();
    const eggByRoot = new Map<number, number>();   // the species of one line share their root's egg moves, set once for the party
    while (starters.length < n) {
      // lean toward the species Fresh Start has special rules for
      // and, in one case in ten, Calyrex first: its level moves are not in level order (Fresh Start's moveset sorts them)
      const id = starters.length === 0 && i % 10 === 3 ? SpeciesId.CALYREX : rnd(4) === 0 ? FORM_SPECIES[rnd(FORM_SPECIES.length)] : ids[rnd(ids.length)];
      if (used.has(id)) continue;
      used.add(id);
      const species = speciesDataRegistry.getSpecies(id);
      const shiny = rnd(3) === 0;
      const variant = rnd(3);
      const formIndex = species.forms.length ? (rnd(2) ? rnd(species.forms.length) : 0) : 0;
      const root = species.getRootSpeciesId();
      if (!eggByRoot.has(root)) eggByRoot.set(root, Object.hasOwn(speciesEggMoves, root) ? rnd(16) : 0);
      const eggBits = eggByRoot.get(root)!;
      // a legal moveset (validateStarterMoveset reads the form's own learnset): level 1-5 moves of the form and the egg moves the profile has unlocked
      const movePool: number[] = species.getLevelMoves(formIndex).filter(([l]: [number, number]) => l >= 1 && l <= 5).map(([, m]: [number, number]) => m);
      if (eggBits) for (let e = 0; e < 4; e++) if (eggBits & (1 << e)) movePool.push((speciesEggMoves as any)[root][e]);
      const moveset: number[] = [];
      const want = 1 + rnd(4);
      for (let t = 0; t < 20 && moveset.length < want && movePool.length; t++) {
        const m = movePool[rnd(movePool.length)];
        if (!moveset.includes(m)) moveset.push(m);
      }
      scene.gameData.starterData[root].eggMoves = eggBits;
      const attr = 1n | (shiny ? 2n | (variant >= 1 ? 32n : 0n) | (variant >= 2 ? 64n : 0n) | 16n : 16n);
      scene.gameData.dexData[id].caughtAttr = attr | (128n << BigInt(formIndex));
      const s = {
        speciesId: id,
        shiny,
        variant,
        formIndex,
        female: species.malePercent !== null && rnd(2) === 1,
        abilityIndex: species.abilityHidden && rnd(2) === 0 ? 2 : species.ability2 === species.ability1 ? 0 : rnd(2),
        passive: rnd(2) === 0,
        nature: rnd(25),
        pokerus: false,
        ivs: [0, 0, 0, 0, 0, 0].map(() => rnd(32)),
        moveset,
        teraType: rnd(3) === 0 ? null : rnd(2) === 0 ? species.type1 : (species.type2 ?? species.type1),
      };
      starters.push(s);
    }
    scene.modifiers = [];
    scene.party = [];
    scene.currentBattle = null;
    Phaser.Math.RND.sow([`challenge-starters-${SEED}-${i}`]);
    const before = state(Phaser.Math.RND);
    // what is handed over, taken before initBattle (the Pokemon keep the starter's ivs array and the challenge caps it in place)
    const given = starters.map(s => ({ ivs: [...s.ivs], abilityIndex: s.abilityIndex, passive: s.passive }));
    new SelectStarterPhase().initBattle(starters);
    await new Promise(r => setTimeout(r, 0));
    const after = state(Phaser.Math.RND);
    await scene.updatePartyForModifiers(scene.getPlayerParty(), true);
    const party = scene.getPlayerParty();
    expect(party).toHaveLength(n);
    lines.push(`B ${n} ${before} | ${after}`);
    for (let k = 0; k < n; k++) {
      const s = starters[k];
      const p = party[k];
      const mv = [0, 1, 2, 3].map(j => s.moveset[j] ?? 0);
      lines.push(
        `S ${s.speciesId} ${s.formIndex} ${+s.shiny} ${s.variant} ${+s.female} ${s.abilityIndex} ${s.nature} ${+s.passive} ${s.teraType ?? -1} ${given[k].ivs.join(" ")} ${mv.join(" ")}`,
      );
      const pm = [0, 1, 2, 3].map(j => p.getMoveset()[j]?.moveId ?? 0);
      lines.push(
        `M ${p.id} ${p.species.speciesId} ${p.formIndex} ${p.level} ${p.nature} ${p.abilityIndex} ${p.getAbility(true).id} ${+!!p.passive} ${p.gender} ${p.luck} ${p.teraType} ${p.hp} ${p.stats.join(" ")} ${p.ivs.join(" ")} ${pm.join(" ")}`,
      );
      // getAttackTypeEffectiveness of the member against each of the 18 types, and hasPassive() (the battle rules read the challenges)
      lines.push(`E ${k} ${[...Array(18).keys()].map(t => p.getAttackTypeEffectiveness(t, { simulated: true })).join(" ")}`);
      lines.push(`P ${k} ${+p.hasPassive()}`);
      lines.push(`X ${p.species.speciesId} ${p.formIndex} -1 0 ${+p.isAllowedInChallenge()}`);
      if (fresh) {
        if (s.shiny) cover.shinyFresh++;
        if (p.ivs.join() !== given[k].ivs.join() || p.abilityIndex !== given[k].abilityIndex || p.passive !== given[k].passive) cover.moved++;
      }
    }
    // Flip Stat on a fusion: the first member fused with the second (calculateBaseStats flips both species' stats before averaging), then put back
    if (n >= 2) {
      const a: any = party[0];
      const b: any = party[1];
      const keep = { fs: a.fusionSpecies, ff: a.fusionFormIndex, stats: [...a.stats], hp: a.hp };
      a.fusionSpecies = b.species;
      a.fusionFormIndex = b.formIndex;
      a.calculateStats();
      lines.push(`F ${b.species.speciesId} ${b.formIndex} ${a.stats.join(" ")}`);
      a.fusionSpecies = keep.fs;
      a.fusionFormIndex = keep.ff;
      a.stats = keep.stats;
      a.hp = keep.hp;
      if (flip) cover.flipFusion++;
    }
    // hasPassive() of an enemy Pokemon: wild or a trainer's, boss or not, with and without its passive, on a few waves (a Challenge Mode game's
    // final boss is wave 200). The Pokemon is built once and its fields set; the battle is a stand-in with the wave and the Classic final check.
    {
      const enemy: any = new EnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR), 20, TrainerSlot.NONE, false);
      for (const trainer of [0, 1]) for (const boss of [0, 1]) for (const flag of [0, 1]) for (const wave of [5, 25, 200, 250]) {
        enemy.trainerSlot = trainer ? TrainerSlot.TRAINER : TrainerSlot.NONE;
        enemy.bossSegments = boss ? 2 : 0;
        enemy.passive = !!flag;
        scene.currentBattle = {
          waveIndex: wave,
          get isClassicFinalBoss() {
            return scene.gameMode.isClassic && scene.gameMode.isWaveFinal(wave);
          },
        };
        const has = enemy.hasPassive();
        if (passives && has) cover.enemyPassive++;
        lines.push(`Y ${trainer} ${boss} ${flag} ${wave} ${+has}`);
      }
      enemy.destroy?.();
      scene.currentBattle = null;
    }
    // The rewards read the challenges too. The Eviolite's and the Mini Black Hole's pool weights with both unlocked (Fresh Start turns them
    // off), then a Mint and a vitamin on the first member (calculateStats with Flip Stat): the stats and hp each leaves
    {
      scene.gameData.unlocks[Unlockables.EVIOLITE] = true;
      scene.gameData.unlocks[Unlockables.MINI_BLACK_HOLE] = true;
      const weight = (tier: ModifierTier, id: string) => {
        const w: any = modifierPool[tier].find((e: any) => e.modifierType.id === id);
        expect(w).toBeDefined();
        return typeof w.weight === "function" ? w.weight(party, 0) : w.weight;
      };
      const ev = weight(ModifierTier.ULTRA, "EVIOLITE"), mbh = weight(ModifierTier.MASTER, "MINI_BLACK_HOLE");
      if (fresh && !ev && !mbh) cover.freshPools++;
      if (!fresh && ev && mbh) cover.openPools++;
      lines.push(`R ${ev} ${mbh}`);
      const p: any = party[0];
      const nature = (p.getNature() + 1 + (i % 24)) % 25;
      const minted = scene.addModifier(new PokemonNatureChangeModifierType(nature).newModifier(p), false, false);
      lines.push(`N ${nature} ${+minted} ${p.hp} ${p.stats.join(" ")}`);
      const stat = i % 6;
      const vitamin: any = new BaseStatBoosterModifierType(stat);
      vitamin.id = "BASE_STAT_BOOSTER";
      // addModifier answers false for a persistent modifier it added (only a form change item sets success): whether it is held is the answer
      const held = vitamin.newModifier(p);
      const boosted = vitamin.selectFilter(p) == null && (scene.addModifier(held, false, false), scene.modifiers.includes(held));
      lines.push(`U ${stat} ${+boosted} ${p.hp} ${p.stats.join(" ")}`);
      if (flip && minted && boosted) cover.flipRewards++;
      scene.modifiers = [];
    }
    // isAllowedInChallenge (POKEMON_IN_BATTLE: Single Generation and Single Type) of member 0 made other species and forms, fused or not
    {
      const a: any = party[0];
      const keep = { sp: a.species, f: a.formIndex, fs: a.fusionSpecies, ff: a.fusionFormIndex };
      for (let t = 0; t < 10; t++) {
        const sid = rnd(2) === 0 ? FORM_CHANGE_SPECIES[rnd(FORM_CHANGE_SPECIES.length)] : ids[rnd(ids.length)];
        const sp = speciesDataRegistry.getSpecies(sid);
        a.species = sp;
        a.formIndex = sp.forms.length ? rnd(sp.forms.length) : 0;
        let fid = -1, fform = 0;
        if (t >= 6) {
          fid = rnd(2) === 0 ? FORM_CHANGE_SPECIES[rnd(FORM_CHANGE_SPECIES.length)] : ids[rnd(ids.length)];
          const fsp = speciesDataRegistry.getSpecies(fid);
          fform = fsp.forms.length ? rnd(fsp.forms.length) : 0;
          a.fusionSpecies = fsp;
          a.fusionFormIndex = fform;
        } else {
          a.fusionSpecies = null;
          a.fusionFormIndex = 0;
        }
        const allowed = a.isAllowedInChallenge();
        if (singleGen || singleType) {
          if (allowed) cover.monoAllowed++;
          else cover.monoBarred++;
          if (fid >= 0) cover.monoFused++;
        }
        lines.push(`X ${sid} ${a.formIndex} ${fid} ${fform} ${+allowed}`);
      }
      a.species = keep.sp;
      a.formIndex = keep.f;
      a.fusionSpecies = keep.fs;
      a.fusionFormIndex = keep.ff;
    }
    // The run rules. SHOP and SHOP_ITEM on two waves (one late enough for every row), PARTY_HEAL and PREVENT_REVIVE, POKEMON_MOVE through its three
    // readers, POKEMON_ADD_TO_PARTY for enemies met on a biome's first wave or not, in and out of an encounter
    {
      const itemName = (o: any) => o.type.localeKey.split(".").pop();
      for (const wave of [1 + rnd(200), 150 + rnd(50)]) {
        const baseCost = 100 * (1 + rnd(60));
        const status = scene.gameMode.getShopStatus();
        const opts = getPlayerShopModifierTypeOptionsForWave(wave, baseCost);
        if (hardcore && opts.length) cover.hardShops++;
        if (!status) cover.closedShops++;
        lines.push(["G", wave, baseCost, +status, opts.length, ...opts.flatMap((o: any) => [itemName(o), o.cost])].join(" "));
      }
      const heal = new BooleanHolder(true);
      applyChallenges(ChallengeType.PARTY_HEAL, heal);
      const prevent = new BooleanHolder(false);
      applyChallenges(ChallengeType.PREVENT_REVIVE, prevent);
      if (!heal.value) cover.noHeals++;
      lines.push(`H ${+heal.value} ${+prevent.value}`);
      const copycat: any = allMoves[MoveId.COPYCAT].getAttrs("CopyMoveAttr")[0];
      expect(copycat).toBeDefined();
      for (const move of [MoveId.REVIVAL_BLESSING, MoveId.TACKLE, 1 + rnd(allMoves.length - 1)]) {
        const valid = new BooleanHolder(true);
        applyChallenges(ChallengeType.POKEMON_MOVE, move, valid);
        const usable = new PokemonMove(move).isUsable(party[0], true)[0];
        const allowed = copycat.isMoveAllowed(move);
        if (!valid.value && !usable && !allowed) cover.refusedMoves++;
        lines.push(`K ${move} ${+valid.value} ${+usable} ${+allowed}`);
      }
      const enemy: any = new EnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.CATERPIE), 5, TrainerSlot.NONE, false);
      for (let k = 0; k < 3; k++) {
        const metWave = k === 0 ? 1 + 10 * rnd(20) : 2 + rnd(198);
        const me = rnd(3) - 1, option = rnd(3);
        enemy.metWave = metWave;
        const options = [{}, {}, {}];
        scene.currentBattle =
          me < 0
            ? { waveIndex: metWave }
            : {
                waveIndex: metWave,
                mysteryEncounter: {
                  encounterType: me === 0 ? MysteryEncounterType.TELEPORTING_HIJINKS : MysteryEncounterType.FIGHT_OR_FLIGHT,
                  options,
                  selectedOption: options[option],
                },
              };
        const add = new BooleanHolder(true);
        applyChallenges(ChallengeType.POKEMON_ADD_TO_PARTY, enemy, add);
        if (limitedCatch && !add.value) cover.refusedAdds++;
        if (limitedCatch && add.value && metWave % 10 !== 1) cover.teleportAdds++;
        lines.push(`A ${metWave} ${me} ${option} ${+add.value}`);
      }
      enemy.destroy?.();
      scene.currentBattle = null;
    }
    scene.gameMode.setChallengeValue(Challenges.SINGLE_GENERATION, 0);
    scene.gameMode.setChallengeValue(Challenges.SINGLE_TYPE, 0);
    for (const p of party) p.destroy?.();
    scene.party = [];
  }
  lines.push("Z");
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
  console.log(
    `CHALLENGE-STARTERS seed=${SEED} cases=${N} fresh=${cover.freshCases} hardValid=${cover.validHard} hardInvalid=${cover.invalidHard} inverse=${cover.inverse} flip=${cover.flip} flipFusion=${cover.flipFusion} passives=${cover.passives} enemyPassive=${cover.enemyPassive} freshPools=${cover.freshPools} openPools=${cover.openPools} flipRewards=${cover.flipRewards} hardShops=${cover.hardShops} closedShops=${cover.closedShops} noHeals=${cover.noHeals} refusedMoves=${cover.refusedMoves} refusedAdds=${cover.refusedAdds} teleportAdds=${cover.teleportAdds} monoAllowed=${cover.monoAllowed} monoBarred=${cover.monoBarred} monoFused=${cover.monoFused} monoSoftForm=${cover.monoSoftForm} monoGender=${cover.monoGender} genderDropped=${cover.genderDropped} softOnly=${cover.softOnly} changedByFresh=${cover.moved} shinyFresh=${cover.shinyFresh} formsReset=${cover.formReset} eggRemoved=${cover.eggRemoved}`,
  );
  expect(cover.freshCases && cover.validHard && cover.invalidHard && cover.moved && cover.inverse && cover.flip && cover.flipFusion && cover.passives && cover.enemyPassive && cover.freshPools && cover.openPools && cover.flipRewards && cover.hardShops && cover.closedShops && cover.noHeals && cover.refusedMoves && cover.refusedAdds && cover.teleportAdds && cover.monoAllowed && cover.monoBarred && cover.monoFused && cover.monoSoftForm && cover.monoGender && cover.genderDropped).toBeGreaterThan(0);
}, 900000);
