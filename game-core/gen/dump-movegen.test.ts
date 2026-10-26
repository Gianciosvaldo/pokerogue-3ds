/*
 * Dumps what game-core's port of src/ai/ai-moveset-gen.ts (a wild, non-boss Pokemon's generateMoveset) reads and
 * battle-core's tables do not carry, as C: the balance constants, FORCED_SIGNATURE_MOVES and STAB_BLACKLIST, a word of
 * move facts per move (the attribute and condition checks the generator makes), a record per ability (the weather,
 * contact and poison attributes the generator asks for) and, per ability that has an AiMovegenMoveStatsAbAttr or a
 * VariableMovePowerAbAttr, what Move.effectivePowerAbilityCheck gives for each damaging move: the neutral result
 * (no weather, genderless, not Flying, the move's own type) plus the rows where one of those four things changes it.
 * Run with tools/redump.sh <upstream dir>; writes gen/out/gc_movegen_data.c. The output is generated: don't edit it.
 */
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { globalScene } from "#app/global-scene";
import * as MG from "#balance/moveset-generation";
import { FORBIDDEN_SINGLES_MOVES, LEVEL_BASED_DENYLIST } from "#balance/forbidden-moves";
import { EXCLUDED_MOVES_FOR_WORSE_OFFENSIVE_STAT, getSpeciesDeniedOffensiveStat } from "#balance/off-stat-denylist";
import { FORCED_SIGNATURE_MOVES } from "#balance/signature-moves";
import { SUPERCEDED_MOVES } from "#balance/superceded-moves";
import { allAbilities, allMoves } from "#data/data-lists";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveCategory } from "#enums/move-category";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { StatusEffect } from "#enums/status-effect";
import { TrainerSlot } from "#enums/trainer-slot";
import { PokemonType } from "#enums/pokemon-type";
import { WeatherType } from "#enums/weather-type";
import { targetSleptOrComatoseCondition, userSleptOrComatoseCondition } from "#moves/move-condition";
import { isWeatherInstantCharge } from "#moves/move-utils";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

/* the bits of gc_mg_move.flags (src/gc_movegen.c names them) */
const F: Record<string, number> = {
  CHARGING: 1 << 0, RECHARGE: 1 << 1, DELAYED: 1 << 2, MULTIHIT_POWER_INC: 1 << 3, MULTIHIT: 1 << 4, DEF_ATK: 1 << 5,
  PHOTON: 1 << 6, SHELL_SIDE: 1 << 7, FIXED_DAMAGE: 1 << 8, TARGET_ATK_USER_ATK: 1 << 9, SACRIFICIAL: 1 << 10,
  SACRIFICIAL_ON_HIT: 1 << 11, WEATHER_CHANGE: 1 << 12, COND_TARGET_SLEEP: 1 << 13, COND_USER_SLEEP: 1 << 14,
  SELF_STATUS_SINGLE: 1 << 15, THUNDER_ACC: 1 << 16, STORM_ACC: 1 << 17, ANTI_SUN: 1 << 18, SLEEP_STATUS: 1 << 19,
  SLEEP_SELF: 1 << 20, POISON_ATTR: 1 << 21, VARIABLE_TYPE: 1 << 22, WEATHER_BALL_TYPE: 1 << 23, TERA_CATEGORY: 1 << 24,
  CALL_MOVE: 1 << 25, HP_SPLIT: 1 << 26, TERRAIN_CHANGE: 1 << 27,
};
const VT_KIND: Record<string, number> = {
  "": 0, FormChangeItemTypeAttr: 1, TechnoBlastTypeAttr: 2, AuraWheelTypeAttr: 3, RagingBullTypeAttr: 4, IvyCudgelTypeAttr: 5,
  WeatherBallTypeAttr: 6, HiddenPowerTypeAttr: 7, TeraBlastTypeAttr: 8, TeraStarstormTypeAttr: 9, MatchUserTypeAttr: 10,
  TerrainPulseTypeAttr: 11, CombinedPledgeTypeAttr: 12,
};

it("dump movegen", async () => {
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.startingWave(45);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const out = process.env.DUMP_OUT ?? "/tmp/gc_movegen.c";

  const moves = allMoves.length;
  const moveRows: string[] = [];
  const unknownVt = new Set<string>();
  for (let id = 0; id < moves; id++) {
    const m = allMoves[id];
    if (!m) {
      moveRows.push("{0, 0, 0, 0, 0}");
      continue;
    }
    let flags = 0;
    const has = (a: string) => m.hasAttr(a as any);
    const set = (name: string, cond: boolean) => { if (cond) flags |= F[name]; };
    set("CHARGING", m.isChargingMove());
    set("RECHARGE", has("RechargeAttr"));
    set("DELAYED", has("DelayedAttackAttr"));
    set("MULTIHIT_POWER_INC", has("MultiHitPowerIncrementAttr"));
    set("MULTIHIT", has("MultiHitAttr"));
    set("DEF_ATK", has("DefAtkAttr"));
    set("PHOTON", has("PhotonGeyserCategoryAttr"));
    set("SHELL_SIDE", has("ShellSideArmCategoryAttr"));
    set("FIXED_DAMAGE", has("FixedDamageAttr"));
    set("TARGET_ATK_USER_ATK", has("TargetAtkUserAtkAttr"));
    set("SACRIFICIAL", has("SacrificialAttr"));
    set("SACRIFICIAL_ON_HIT", has("SacrificialAttrOnHit"));
    set("WEATHER_CHANGE", has("WeatherChangeAttr"));
    set("COND_TARGET_SLEEP", m.hasCondition(targetSleptOrComatoseCondition));
    set("COND_USER_SLEEP", m.hasCondition(userSleptOrComatoseCondition));
    set("SELF_STATUS_SINGLE", m.is("SelfStatusMove") && m.attrs.length === 1);
    set("THUNDER_ACC", has("ThunderAccuracyAttr"));
    set("STORM_ACC", has("StormAccuracyAttr"));
    set("ANTI_SUN", has("AntiSunlightPowerDecreaseAttr"));
    const sleepAttr = m.attrs.some(
      (a: any) =>
        (a.is("StatusEffectAttr") && a.effect === StatusEffect.SLEEP)
        || (a.is("AddBattlerTagAttr") && a.tagType === BattlerTagType.DROWSY),
    );
    set("SLEEP_STATUS", m.is("StatusMove") && !m.is("SelfStatusMove") && sleepAttr);
    set("SLEEP_SELF", m.is("SelfStatusMove") && sleepAttr);
    set("POISON_ATTR", m.getAttrs("StatusEffectAttr").some((a: any) => [StatusEffect.POISON, StatusEffect.TOXIC].includes(a.effect)));
    const vt = m.getAttrs("VariableMoveTypeAttr").at(-1);
    set("VARIABLE_TYPE", vt != null);
    set("WEATHER_BALL_TYPE", has("WeatherBallTypeAttr"));
    set("TERA_CATEGORY", has("TeraMoveCategoryAttr"));
    set("CALL_MOVE", has("CallMoveAttr"));
    set("HP_SPLIT", has("HpSplitAttr"));
    set("TERRAIN_CHANGE", has("TerrainChangeAttr"));
    let vtKind = 0;
    if (vt != null) {
      const name = vt.constructor.name;
      if (name in VT_KIND) vtKind = VT_KIND[name];
      else unknownVt.add(name);
    }
    /* removeSelfStatBoost's category: 0 = none, else MoveCategory + 1 */
    let boostCat = 0;
    const sattr: any = m.getAttrs("StatStageChangeAttr")[0];
    if (sattr != null) {
      switch (id) {
        case MoveId.CURSE: case MoveId.BULK_UP: case MoveId.HONE_CLAWS: boostCat = MoveCategory.PHYSICAL + 1; break;
        case MoveId.CALM_MIND: case MoveId.TAKE_HEART: boostCat = MoveCategory.SPECIAL + 1; break;
        default:
          if (sattr.stats.length === 1) {
            if (sattr.stats[0] === Stat.ATK) boostCat = MoveCategory.PHYSICAL + 1;
            else if (sattr.stats[0] === Stat.SPATK) boostCat = MoveCategory.SPECIAL + 1;
          }
      }
    }
    let instant = 0;
    for (let w = 0; w < 10; w++) if (isWeatherInstantCharge(m, w as WeatherType)) instant |= 1 << w;
    /* the charge attributes' instant-charge in sun, for droughtAiMovegenEffect */
    const chargeSun = m.isChargingMove() && m.getChargeAttrs("WeatherInstantChargeAttr").some((a: any) => a.weatherTypes.includes(WeatherType.SUNNY));
    moveRows.push(`{${flags}u, ${vtKind}, ${boostCat}, ${instant}, ${chargeSun ? 1 : 0}}`);
  }
  if (unknownVt.size) throw new Error(`unknown VariableMoveTypeAttr: ${[...unknownVt].join(",")}`);

  /* abilities */
  const abRows: string[] = [];
  for (let id = 0; id < allAbilities.length; id++) {
    const ab = allAbilities[id];
    const wc = ab.getAttrs("PostSummonWeatherChangeAbAttr").at(-1) as any;
    const pre = ab.getAttrs("PreAttackWeatherOverrideAbAttr") as any[];
    const post = ab.getAttrs("PostAttackApplyStatusEffectAbAttr") as any[];
    const def = ab.getAttrs("PostDefendApplyStatusEffectAbAttr") as any[];
    const postPoison = post.some(a => a.effects.includes(StatusEffect.POISON));
    const postToxic = post.some(a => a.effects.includes(StatusEffect.TOXIC));
    const postToxicNoContact = post.some(a => a.effects.includes(StatusEffect.TOXIC) && !a.contactRequired);
    const defPoison = def.some(a => a.effects.includes(StatusEffect.POISON) || a.effects.includes(StatusEffect.TOXIC));
    /* hasSunInstantCharge: the weather types of PostSummonWeatherChange and PreAttackWeatherOverride */
    const sunWc = (ab.getAttrs("PostSummonWeatherChangeAbAttr") as any[]).some(a => [WeatherType.SUNNY, WeatherType.HARSH_SUN].includes(a.weatherType));
    const sunPre = pre.some(a => [WeatherType.SUNNY, WeatherType.HARSH_SUN].includes(a.weatherType));
    const bits =
      (ab.hasAttr("PostSummonWeatherChangeAbAttr") ? 1 : 0) | (ab.hasAttr("SuppressWeatherEffectAbAttr") ? 2 : 0)
      | (ab.hasAttr("PreAttackWeatherOverrideAbAttr") ? 4 : 0) | (ab.hasAttr("IgnoreContactAbAttr") ? 8 : 0)
      | (defPoison ? 16 : 0) | (postPoison ? 32 : 0) | (postToxic ? 64 : 0) | (postToxicNoContact ? 128 : 0)
      | (sunWc || sunPre ? 256 : 0) | (ab.hasAttr("PostSummonTerrainChangeAbAttr") ? 512 : 0) | (ab.hasAttr("GorillaTacticsAbAttr") ? 1024 : 0);
    /* the weather of the last PostSummonWeatherChange (Weather Ball's type for the generator), -1 = none */
    abRows.push(`{${bits}, ${wc ? wc.weatherType : -1}}`);
  }

  /* effectivePowerAbilityCheck */
  const dummy = scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP), 50, TrainerSlot.NONE, false, true);
  dummy.gender = -1;
  dummy.customPokemonData.types = [];
  const damaging: number[] = [];
  for (const m of allMoves) if (m && m.id > 0 && m.category !== MoveCategory.STATUS && m.power > 0) damaging.push(m.id);
  const withAttrs: number[] = [];
  for (const ab of allAbilities) if (ab.hasAttr("AiMovegenMoveStatsAbAttr") || ab.hasAttr("VariableMovePowerAbAttr")) withAttrs.push(ab.id);
  type Tup = [number, number, boolean, boolean];
  const tup = (abId: number, moveId: number): Tup => {
    dummy.customPokemonData.ability = abId;
    globalScene.movesetGenInProgress = true;
    const p = (allMoves[moveId] as any).effectivePowerAbilityCheck(dummy);
    globalScene.movesetGenInProgress = false;
    return [p.powerMult.value, p.accMult.value, p.instantCharge.value, p.maxMultiHit.value];
  };
  const same = (a: Tup, b: Tup) => Object.is(a[0], b[0]) && Object.is(a[1], b[1]) && a[2] === b[2] && a[3] === b[3];
  const isDefault = (a: Tup) => a[0] === 1 && a[1] === 1 && !a[2] && !a[3];
  const num = (x: number) => (x === Infinity ? "INFINITY" : x === -Infinity ? "-INFINITY" : Number.isInteger(x) ? `${x}.0` : x.toPrecision(17));
  /* dims: 0 neutral, 1 weather (value = WeatherType), 2 gender (0 male, 1 female), 3 Flying, 4 the move's type (value = PokemonType) */
  const powRows: { ab: number; move: number; dim: number; val: number; t: Tup }[] = [];
  const realGetMoveType = dummy.getMoveType.bind(dummy);
  const skipAbilities = new Set<number>([233]); /* Neuroforce: the effectiveness against the user's own types, ported by hand */
  for (const ab of withAttrs) {
    if (skipAbilities.has(ab)) continue;
    for (const mv of damaging) {
      const vtMove = allMoves[mv].hasAttr("VariableMoveTypeAttr");
      const mt = allMoves[mv].type;
      dummy.getMoveType = vtMove ? () => mt : realGetMoveType;
      const base = tup(ab, mv);
      if (!isDefault(base)) powRows.push({ ab, move: mv, dim: 0, val: 0, t: base });
      for (let w = 1; w < 10; w++) {
        scene.arena.weather = { weatherType: w, isImmutable: () => false, turnsLeft: 0, maxDuration: 0, isEffectSuppressed: () => false } as any;
        const t = tup(ab, mv);
        scene.arena.weather = null;
        if (!same(t, base)) powRows.push({ ab, move: mv, dim: 1, val: w, t });
      }
      for (const g of [0, 1]) {
        dummy.gender = g;
        const t = tup(ab, mv);
        if (!same(t, base)) powRows.push({ ab, move: mv, dim: 2, val: g, t });
      }
      dummy.gender = -1;
      dummy.customPokemonData.types = [PokemonType.FLYING];
      {
        const t = tup(ab, mv);
        if (!same(t, base)) powRows.push({ ab, move: mv, dim: 3, val: 1, t });
      }
      dummy.customPokemonData.types = [];
      if (vtMove) {
        for (let ty = 0; ty < 18; ty++) {
          if (ty === mt) continue;
          dummy.getMoveType = () => ty;
          const t = tup(ab, mv);
          if (!same(t, base)) powRows.push({ ab, move: mv, dim: 4, val: ty, t });
        }
      }
      dummy.getMoveType = realGetMoveType;
    }
  }
  /* the gender rows (Rivalry) are the same for every damaging move: one row with move 0 for all of them */
  {
    const byAb = new Map<number, typeof powRows>();
    for (const r of powRows) if (r.dim === 2) byAb.set(r.ab, [...(byAb.get(r.ab) ?? []), r]);
    for (const [ab, rows] of byAb) {
      for (const g of [0, 1]) {
        const mine = rows.filter(r => r.val === g);
        if (mine.length !== damaging.length || mine.some(r => !same(r.t, mine[0].t))) throw new Error(`gender rows of ability ${ab} are not uniform`);
      }
      for (let i = powRows.length - 1; i >= 0; i--) if (powRows[i].dim === 2 && powRows[i].ab === ab) powRows.splice(i, 1);
      for (const g of [0, 1]) powRows.push({ ab, move: 0, dim: 2, val: g, t: rows.find(r => r.val === g)!.t });
    }
  }
  /* a (ability, move) with rows in two different dimensions would need the product: refuse */
  const seen = new Map<string, number>();
  for (const r of powRows) {
    if (r.dim === 0) continue;
    const key = `${r.ab} ${r.move}`;
    const prev = seen.get(key);
    if (prev != null && prev !== r.dim) throw new Error(`ability ${r.ab} move ${r.move} varies in dims ${prev} and ${r.dim}`);
    seen.set(key, r.dim);
  }
  powRows.sort((a, b) => a.ab - b.ab || a.move - b.move || a.dim - b.dim || a.val - b.val);

  /* the balance lists */
  const sigRows: string[] = [];
  const sigMoves: number[] = [];
  for (const [sp, v] of Object.entries(FORCED_SIGNATURE_MOVES)) {
    const list = Array.isArray(v) ? v : [v as number];
    sigRows.push(`{${sp}, ${sigMoves.length}, ${list.length}, ${Array.isArray(v) ? 1 : 0}}`);
    sigMoves.push(...list);
  }
  const stab = [...MG.STAB_BLACKLIST].sort((a, b) => a - b);
  const sorted = (set: ReadonlySet<number>) => [...set].sort((a, b) => a - b);
  const singles = sorted(FORBIDDEN_SINGLES_MOVES);
  const levelDeny = sorted(LEVEL_BASED_DENYLIST);
  const offExempt = sorted(EXCLUDED_MOVES_FOR_WORSE_OFFENSIVE_STAT);
  /* SUPERCEDED_MOVES: a move and the moves that make it redundant */
  const supRows: string[] = [];
  const supMoves: number[] = [];
  for (const [mv, list] of Object.entries(SUPERCEDED_MOVES).sort((a, b) => Number(a[0]) - Number(b[0]))) {
    supRows.push(`{${mv}, ${supMoves.length}, ${list!.length}}`);
    supMoves.push(...list!);
  }
  /* getSpeciesDeniedOffensiveStat: per species (form -1 = every form) and form; 0 denies physical, 1 special */
  const offRows: string[] = [];
  for (const sp of speciesDataRegistry.getAllSpecies()) {
    if (sp == null) continue;
    const forms = Math.max(sp.forms.length, 1);
    const all = getSpeciesDeniedOffensiveStat(sp.speciesId, 0);
    let uniform = true;
    for (let f = 1; f < forms; f++) if (getSpeciesDeniedOffensiveStat(sp.speciesId, f) !== all) uniform = false;
    if (uniform) {
      if (all !== undefined) offRows.push(`{${sp.speciesId}, -1, ${all}}`);
      continue;
    }
    for (let f = 0; f < forms; f++) {
      const v = getSpeciesDeniedOffensiveStat(sp.speciesId, f);
      if (v !== undefined) offRows.push(`{${sp.speciesId}, ${f}, ${v}}`);
    }
  }

  const lines = [
    "/* Generated by gen/dump-movegen.test.ts from upstream's move and ability data. Do not edit. */",
    '#include "gc/movegen.h"',
    "#include <math.h>",
    "",
    `const gc_mg_constants gc_mg_const = {${MG.BASE_LEVEL_WEIGHT_OFFSET}, ${MG.EVO_MOVE_BP_THRESHOLD}, ${MG.EVOLUTION_MOVE_WEIGHT}, ${MG.RELEARN_MOVE_WEIGHT}, ${MG.BASE_WEIGHT_MULTIPLIER}, ${MG.MOVE_POWER_CEILING}, ${MG.FORCED_SIGNATURE_MOVE_CHANCE}, ${MG.LEVEL_BASED_DENYLIST_THRESHOLD}, ${MG.BOSS_EXTRA_WEIGHT_MULTIPLIER}};`,
    "",
    `const int gc_mg_move_count = ${moves};`,
    `const gc_mg_move gc_mg_moves[${moves}] = {`,
    ...moveRows.map(r => `    ${r},`),
    "};",
    "",
    `const int gc_mg_ability_count = ${allAbilities.length};`,
    `const gc_mg_ability gc_mg_abilities[${allAbilities.length}] = {`,
    ...abRows.map(r => `    ${r},`),
    "};",
    "",
    `const uint16_t gc_mg_sig_moves[${Math.max(sigMoves.length, 1)}] = {${sigMoves.join(", ") || "0"}};`,
    `const int gc_mg_sig_count = ${sigRows.length};`,
    `const gc_mg_signature gc_mg_signatures[${Math.max(sigRows.length, 1)}] = {`,
    ...sigRows.map(r => `    ${r},`),
    "};",
    "",
    `const int gc_mg_stab_blacklist_count = ${stab.length};`,
    `const uint16_t gc_mg_stab_blacklist[${stab.length}] = {${stab.join(", ")}};`,
    "",
    `const int gc_mg_forbidden_singles_count = ${singles.length};`,
    `const uint16_t gc_mg_forbidden_singles[${singles.length}] = {${singles.join(", ")}};`,
    `const int gc_mg_level_denylist_count = ${levelDeny.length};`,
    `const uint16_t gc_mg_level_denylist[${levelDeny.length}] = {${levelDeny.join(", ")}};`,
    `const int gc_mg_offstat_exempt_count = ${offExempt.length};`,
    `const uint16_t gc_mg_offstat_exempt[${offExempt.length}] = {${offExempt.join(", ")}};`,
    `const int gc_mg_offstat_count = ${offRows.length};`,
    `const gc_mg_offstat gc_mg_offstats[${Math.max(offRows.length, 1)}] = {`,
    ...offRows.map(r => `    ${r},`),
    "};",
    `const uint16_t gc_mg_superceded_moves[${Math.max(supMoves.length, 1)}] = {${supMoves.join(", ") || "0"}};`,
    `const int gc_mg_superceded_count = ${supRows.length};`,
    `const gc_mg_superceded gc_mg_supercededs[${Math.max(supRows.length, 1)}] = {`,
    ...supRows.map(r => `    ${r},`),
    "};",
    "",
    `const int gc_mg_pow_count = ${powRows.length};`,
    `const gc_mg_pow gc_mg_pows[${Math.max(powRows.length, 1)}] = {`,
    ...powRows.map(r => `    {${r.ab}, ${r.move}, ${r.dim}, ${r.val}, ${num(r.t[0])}, ${num(r.t[1])}, ${r.t[2] ? 1 : 0}, ${r.t[3] ? 1 : 0}},`),
    "};",
    "",
  ];
  fs.writeFileSync(out, lines.join("\n"));
  const dims = [0, 0, 0, 0, 0];
  for (const r of powRows) dims[r.dim]++;
  console.log(`movegen: ${moves} moves, ${allAbilities.length} abilities, ${withAttrs.length} with power attrs, ${powRows.length} rows (dims ${dims.join("/")}), ${sigRows.length} signatures, ${stab.length} stab blacklisted`);
}, 1200000);
