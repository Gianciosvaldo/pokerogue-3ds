/*
 * Upstream half of reward-core's item application test (tools/difftest.sh copies it into upstream).
 * Tests applying reward items (potions, revives, Sacred Ash, PP items, evolution items, form change items)
 * to random parties with random HP, statuses, PP, PP Ups, held items, and challenge settings.
 */
import { EvolutionItem } from "#balance/pokemon-evolutions";
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allMoves, modifierTypes } from "#data/data-lists";
import { SpeciesFormChangeItemTrigger } from "#data/form-change-triggers";
import { Status } from "#data/status-effect";
import { Challenges } from "#enums/challenges";
import { FormChangeItem } from "#enums/form-change-item";
import { GameModes } from "#enums/game-modes";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { PokemonHeldItemModifier } from "#modifiers/modifier";
import { PokemonMove } from "#moves/pokemon-move";
import { EvolutionItemModifierType, FormChangeItemModifierType, PokemonMoveModifierType } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";
import * as statusHelpers from "#data/status-effect";

it("reward apply producer", async () => {
  const N = Number(process.env.DIFF_N ?? 4000);
  const OUT = process.env.DIFF_OUT ?? "/tmp/pr-apply-cases.txt";
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
  const pick = <T>(v: readonly T[]): T => v[rnd(v.length)];
  const keyOf = (t: any) => (t.id ?? "UNKNOWN") as string;

  const itemKeys = [
    "POTION", "SUPER_POTION", "HYPER_POTION", "MAX_POTION", "FULL_RESTORE",
    "REVIVE", "MAX_REVIVE", "FULL_HEAL", "SACRED_ASH",
    "ETHER", "MAX_ETHER", "ELIXIR", "MAX_ELIXIR",
    "PP_UP", "PP_MAX",
    "EVOLUTION_ITEM", "RARE_EVOLUTION_ITEM",
    "FORM_CHANGE_ITEM", "RARE_FORM_CHANGE_ITEM",
    "MEGA_BRACELET", "DYNAMAX_BAND", "DNA_SPLICERS",
  ];

  const shown: string[] = [];
  if (scene.ui) {
    vi.spyOn(scene.ui, "showText").mockImplementation((...args: any[]) => {
      shown.push(String(args[0]));
      if (typeof args[2] === "function") args[2]();
    });
  }

  const evoItemIds = Object.values(EvolutionItem).filter((v): v is number => typeof v === "number" && v > 0);
  const formChangeItemIds = Object.values(FormChangeItem).filter((v): v is number => typeof v === "number" && v > 0);

  const speciesIds = Object.keys(speciesDataRegistry.data).map(Number);
  const moveIds = allMoves.map((m: any) => m.id).filter((id: number) => id > 0);
  const statuses = [
    StatusEffect.POISON, StatusEffect.TOXIC, StatusEffect.PARALYSIS, StatusEffect.SLEEP, StatusEffect.FREEZE, StatusEffect.BURN,
  ];

  const pool = Array.from({ length: 6 }, () => scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5));
  // Fusion destroys secondary display objects; reused fixtures still run real item logic.
  // Only sprite presentation is intercepted, never status/stats/selection/application.
  for (const pokemon of pool) vi.spyOn(pokemon, "playSprite").mockImplementation(() => {});
  const party = scene.getPlayerParty();

  const lines: string[] = [];
  let fusionDraws: number[] = [];
  const realStatus = statusHelpers.getRandomStatus;
  vi.spyOn(statusHelpers, "getRandomStatus").mockImplementation((a,b)=>{
    const realRandom = Math.random;
    try {
      Math.random = () => { const draw=realRandom(); fusionDraws.push(draw); return draw; };
      return realStatus(a,b);
    } finally { Math.random=realRandom; }
  });
  let thrown = 0;

  for (let i = 0; i < N; i++) {
    try {
      const isChallenge = rnd(2) === 0;
      const gm = getGameMode(isChallenge ? GameModes.CHALLENGE : GameModes.CLASSIC);
      let challenges = 0;
      if (isChallenge) {
        if (rnd(2) === 0) {
          gm.setChallengeValue(Challenges.HARDCORE, 1);
          challenges |= 1 << Challenges.HARDCORE;
        }
      }
      scene.gameMode = gm;
      battle.gameMode = gm;

      const size = 1 + rnd(6);
      party.length = 0;
      for (let k = 0; k < size; k++) {
        const p = pool[k];
        p.resetSummonData();
        p.resetBattleAndWaveData();
        const sid = pick(speciesIds);
        const sp = speciesDataRegistry.getSpecies(sid);
        p.species = sp;
        p.formIndex = sp.forms.length ? rnd(sp.forms.length) : 0;
        p.fusionSpecies = null;
        p.fusionFormIndex = 0;
        p.level = 1 + rnd(100);
        p.gender = rnd(3) - 1;
        p.nature = rnd(25);
        p.teraType = rnd(18);

        const nm = 1 + rnd(4);
        p.moveset = [];
        for (let m = 0; m < nm; m++) {
          const mv = new PokemonMove(pick(moveIds), rnd(4), rnd(4));
          const maxPp = mv.getMovePp();
          mv.ppUsed = rnd(3) === 0 ? 0 : rnd(maxPp + 1);
          p.moveset.push(mv);
        }
        p.calculateStats();
        const max = p.getMaxHp();
        const r = rnd(8);
        p.hp = r < 2 ? max : r === 2 ? 0 : 1 + rnd(max);
        const st = p.hp === 0 ? (rnd(2) === 0 ? StatusEffect.FAINT : 0) : rnd(3) === 0 ? pick(statuses) : 0;
        p.status = st ? new Status(st) : null;
        party.push(p);
      }

      // items (e.g. HEALING_CHARM, MEGA_BRACELET, DYNAMAX_BAND)
      scene.modifiers.length = 0;
      if (rnd(2) === 0) {
        const hcFunc = (modifierTypes as any)["HEALING_CHARM"];
        if (hcFunc) {
          const t = hcFunc().withIdFromFunc(hcFunc);
          const m = t.newModifier();
          m.stackCount = 1 + rnd(5);
          scene.modifiers.push(m);
        }
      }
      if (rnd(2) === 0) {
        const mbFunc = (modifierTypes as any)["MEGA_BRACELET"];
        if (mbFunc) {
          const t = mbFunc().withIdFromFunc(mbFunc);
          scene.modifiers.push(t.newModifier());
        }
      }
      if (rnd(2) === 0) {
        const dbFunc = (modifierTypes as any)["DYNAMAX_BAND"];
        if (dbFunc) {
          const t = dbFunc().withIdFromFunc(dbFunc);
          scene.modifiers.push(t.newModifier());
        }
      }

      const itemKey = itemKeys[i % itemKeys.length];
      const targetPartyIdx = rnd(party.length);
      const targetMon = party[targetPartyIdx];
      const targetMoveIdx = rnd(targetMon.moveset.length);

      let modifierType: any;
      let arg = 0;
      let filterMsg: string | null = null;

      if (itemKey === "EVOLUTION_ITEM" || itemKey === "RARE_EVOLUTION_ITEM") {
        const rare = itemKey === "RARE_EVOLUTION_ITEM";
        const evolutions = speciesDataRegistry.hasEvolutions(targetMon.species.speciesId)
          ? speciesDataRegistry.getEvolutions(targetMon.species.speciesId)
          : [];
        const validItems = evolutions.filter((e: any) => e.item && (e.item > 50) === rare).map((e: any) => e.item);
        if (validItems.length > 0 && rnd(2) === 0) {
          arg = pick(validItems);
        } else {
          arg = pick(evoItemIds);
        }
        modifierType = new EvolutionItemModifierType(arg);
      } else if (itemKey === "FORM_CHANGE_ITEM" || itemKey === "RARE_FORM_CHANGE_ITEM") {
        const rare = itemKey === "RARE_FORM_CHANGE_ITEM";
        const formChanges = speciesDataRegistry.hasFormChanges(targetMon.species.speciesId)
          ? speciesDataRegistry.getFormChanges(targetMon.species.speciesId)
          : [];
        const validItems = formChanges
          .map((fc: any) => fc.findTrigger(SpeciesFormChangeItemTrigger)?.item)
          .filter((i: any) => !!i && (i >= 100 && i < 150) === rare);
        if (validItems.length > 0 && rnd(2) === 0) {
          arg = pick(validItems);
        } else {
          arg = pick(formChangeItemIds);
        }
        modifierType = new FormChangeItemModifierType(arg);
      } else if (itemKey === "DNA_SPLICERS") {
        const typeFunc = (modifierTypes as any)["DNA_SPLICERS"];
        modifierType = typeFunc();
        if (party.length < 2) {
          filterMsg = "It won’t have any effect.";
        } else {
          arg = (targetPartyIdx + 1 + rnd(party.length - 1)) % party.length;
          const secPartyMon = party[arg];
          if (secPartyMon.isFusion()) {
            filterMsg = "It won’t have any effect.";
          }
        }
      } else if (itemKey === "MEGA_BRACELET" || itemKey === "DYNAMAX_BAND") {
        const typeFunc = (modifierTypes as any)[itemKey];
        modifierType = typeFunc();
        if (scene.modifiers.some((m: any) => keyOf(m.type) === itemKey)) {
          filterMsg = "It won’t have any effect.";
        }
      } else {
        const typeFunc = (modifierTypes as any)[itemKey];
        modifierType = typeFunc();
      }

      // Record input state for C
      lines.push(["A", itemKey, arg, targetPartyIdx, targetMoveIdx, challenges, party.length].join(" "));
      for (const p of party) {
        const ms = p.getMoveset();
        const baseMs = p.moveset;
        lines.push(
          [
            "P", p.id, p.species.speciesId, p.formIndex, p.fusionSpecies ? p.fusionSpecies.speciesId : -1, p.fusionFormIndex,
            p.gender, p.fusionGender ?? 0, p.level, p.hp, p.getMaxHp(), p.status?.effect ?? 0, p.getNature(), p.teraType,
            p.friendship, p.getAbility(true).id, p.getAbility().id, p.hasPassive() ? p.getPassiveAbility().id : 0,
            p.canApplyAbility() ? 1 : 0, p.canApplyAbility(true) ? 1 : 0, p.pauseEvolutions ? 1 : 0,
            p.isAllowedInChallenge() ? 1 : 0, p.canSetStatus(StatusEffect.TOXIC, true, true, null, true) ? 1 : 0,
            p.canSetStatus(StatusEffect.BURN, true, true, null, true) ? 1 : 0, p.getLearnableLevelMoves().length > 0 ? 1 : 0,
            p.getPersistentTreasureCount(),
            ms.length, ...ms.flatMap((m: any) => [m.moveId, m.ppUsed, m.ppUp, m.getMovePp()]),
            baseMs.length, ...baseMs.map((m: any) => m.moveId),
            ...p.ivs,...p.stats,p.abilityIndex,p.shiny?1:0,p.variant,p.luck,
          ].join(" "),
        );
      }
      const mods = scene.modifiers;
      lines.push(
        [
          "I", mods.length,
          ...mods.flatMap((m: any) => [
            keyOf(m.type), m.stackCount,
            m instanceof PokemonHeldItemModifier ? party.findIndex((p: any) => p.id === m.pokemonId) : -1,
            m.formChangeItem ?? 0,
          ]),
        ].join(" "),
      );

      // Execute item apply in upstream
      if (filterMsg === null && modifierType.selectFilter) {
        filterMsg = modifierType.selectFilter(targetMon);
      }
      if (filterMsg === null && modifierType instanceof PokemonMoveModifierType) {
        if (modifierType.moveSelectFilter) {
          filterMsg = modifierType.moveSelectFilter(targetMon.moveset[targetMoveIdx]);
        }
      }

      fusionDraws=[];
      shown.length = 0;
      let applied = false;
      if (filterMsg === null) {
        let mod: any;
        if (modifierType instanceof PokemonMoveModifierType) {
          mod = modifierType.newModifier(targetMon, targetMoveIdx);
        } else if (itemKey === "DNA_SPLICERS") {
          const secPartyMon = party[arg];
          mod = modifierType.newModifier(targetMon, secPartyMon);
        } else if (typeof modifierType.newModifier === "function") {
          mod = modifierType.newModifier(targetMon);
        }
        if (mod) {
          const preModCount = scene.modifiers.length;
          const ret = scene.addModifier(mod);
          applied = ret || scene.modifiers.length > preModCount;
        }
      }

      if(fusionDraws.length>1) throw new Error("unexpected fusion draw count");
      lines.push(["D",fusionDraws.length,...fusionDraws].join(" "));

      // Record result state after upstream apply (including text count)
      const outMon = party.flatMap((p: any) => [
        p.hp, p.status?.effect ?? 0, p.moveset.length,
        ...p.moveset.flatMap((m: any) => [m.ppUsed, m.ppUp, m.getMovePp()]),
      ]);
      lines.push(["R", applied ? 1 : 0, filterMsg ? filterMsg.replace(/\s+/g, "_") : "OK", shown.length, ...outMon].join(" "));
      lines.push("E");
    } catch (e) {
      thrown++;
      lines.push(`X ${String(e).slice(0, 80).replace(/\s+/g, "_")}`);
    }
  }

  party.length = 0;
  scene.modifiers.length = 0;
  if (thrown) throw new Error(`${thrown} upstream application cases threw`);
  lines.push(`# threw: ${thrown}`);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
}, 1_800_000);
