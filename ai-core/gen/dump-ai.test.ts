/*
 * Dumps what the enemy AI reads from upstream's move objects that battle-core's tables don't carry.
 * Copy into an upstream checkout as test/tests/zz-dump/dump-ai.test.ts and run
 *   DUMP_OUT=/abs/ai_data.c npx vitest run test/tests/zz-dump/dump-ai.test.ts
 * (tools/redump.sh does both).
 *
 * Per move: calculateEffectivePower() with no Pokemon (AttackMove.getTargetBenefitScore reads it),
 * its selection restrictions (Move.checkRestrictions: Gravity, Belch, Stuff Cheeks, consecutive use)
 * and which class's getUserBenefitScore each entry of move.conditions uses.
 * Per move attribute, in the move's own order (battle-core's pr_moves[id].attr0 + i): its class,
 * the class whose getUserBenefitScore / getTargetBenefitScore it runs (found up its prototype
 * chain, so a subclass that inherits one is dispatched to the right code), selfTarget, and the
 * fields those methods read. An owner class the C doesn't know is written as AI_OWN_UNKNOWN and
 * the C reports the move unported, so a new upstream override can't be missed silently.
 */
import { allMoves } from "#data/data-lists";
import { consecutiveUseRestriction, gravityUseRestriction } from "#moves/move-condition";
import { GameManager } from "#test/framework/game-manager";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";

// The classes whose benefit-score methods the C ports (src/ai_score.c), in AI_OWN_* order.
const OWNERS = [
  "MoveAttr", "HighCritAttr", "CritOnlyAttr", "TargetHalfHpDamageAttr", "SurviveDamageAttr", "RecoilAttr",
  "SacrificialAttr", "SacrificialAttrOnHit", "HalfSacrificialAttr", "AddSubstituteAttr", "HealAttr", "FlameBurstAttr",
  "SacrificialFullRestoreAttr", "HitHealAttr", "MultiHitAttr", "StatusEffectAttr", "MultiStatusEffectAttr",
  "PsychoShiftEffectAttr", "StealHeldItemChanceAttr", "RemoveHeldItemAttr", "HealStatusEffectAttr", "BypassSleepAttr",
  "TerrainChangeAttr", "StatStageChangeAttr", "PartingShotAttr", "AddBattlerTagAttr", "GulpMissileTagAttr",
  "HitsTagAttr", "RevivalBlessingAttr", "ForceSwitchOutAttr", "RepeatMoveAttr", "ReducePpMoveAttr", "ShiftStatAttr",
];
const COND_OWNERS = ["MoveCondition", "FirstMoveCondition"];
const LEVEL_OWNERS = ["StatStageChangeAttr", "GrowthStatStageChangeAttr"];

/** The class up the prototype chain that defines `method` itself */
function owner(o: any, method: string): string {
  for (let p = Object.getPrototypeOf(o); p && p !== Object.prototype; p = Object.getPrototypeOf(p)) {
    if (Object.hasOwn(p, method)) {
      return p.constructor.name;
    }
  }
  return "?";
}
const idx = (list: string[], name: string) => {
  const i = list.indexOf(name);
  return i < 0 ? "AI_OWN_UNKNOWN" : String(i);
};

it("dump ai data", async () => {
  new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  const moves: string[] = [];
  const attrs: string[] = [];
  const unknown = new Set<string>();
  for (let id = 0; id < allMoves.length; id++) {
    const m: any = allMoves[id];
    if (!m) {
      moves.push(`    {0, 0, 0, ${attrs.length}, 0}, /* ${id} */`);
      continue;
    }
    let restr = 0;
    for (const r of m.restrictions as any[]) {
      if (r === gravityUseRestriction) {
        restr |= 1;
      } else if (r === consecutiveUseRestriction) {
        restr |= 2;
      } else if (r.i18nkey === "battle:moveDisabledBelch") {
        restr |= 4;
      } else if (r.i18nkey === "battle:moveDisabledNoBerry") {
        restr |= 8;
      } else {
        restr |= 128; // unknown: the C reports the move unported
        unknown.add(`restriction ${r.i18nkey} on ${m.name}`);
      }
    }
    let conds = 0;
    for (const c of m.conditions as any[]) {
      const o = owner(c, "getUserBenefitScore");
      if (o === "FirstMoveCondition") {
        conds += 1;
      } else if (o !== "MoveCondition") {
        conds |= 128;
        unknown.add(`condition owner ${o} on ${m.name}`);
      }
    }
    const a0 = attrs.length;
    for (const a of m.attrs as any[]) {
      const ubs = owner(a, "getUserBenefitScore");
      const tbs = owner(a, "getTargetBenefitScore");
      for (const o of [ubs, tbs]) {
        if (!OWNERS.includes(o)) {
          unknown.add(`owner ${o} (${a.constructor.name} on ${m.name})`);
        }
      }
      const lev = a.getLevels ? owner(a, "getLevels") : "";
      if (lev && !LEVEL_OWNERS.includes(lev)) {
        unknown.add(`getLevels owner ${lev} (${a.constructor.name} on ${m.name})`);
      }
      // the fields the ported methods read (0 when the class has none)
      const stats: number[] = Array.isArray(a.stats) ? a.stats : [];
      // StatusEffectAttr.effect / MultiStatusEffectAttr.effects (the only owners that read them)
      const statusOwner = ["StatusEffectAttr", "MultiStatusEffectAttr"].includes(tbs);
      const effects: number[] = !statusOwner ? [] : Array.isArray(a.effects) ? a.effects : [a.effect];
      if (effects.length > 4 || stats.length > 8) {
        unknown.add(`too many effects or stats (${a.constructor.name} on ${m.name})`);
      }
      const sw = a.constructor.name === "PartingShotAttr" ? a.switchAttr : a;
      const tagScore = typeof a.getTagTargetBenefitScore === "function" ? a.getTagTargetBenefitScore() : 0;
      // BattlerTagType is a string enum: written as battle-core's constant, for the two owners that read it
      const tagType = ["HitsTagAttr", "AddBattlerTagAttr"].includes(tbs) ? `BATTLER_TAG_TYPE_${a.tagType}` : "-1";
      attrs.push(
        `    {"${a.constructor.name}", ${idx(OWNERS, ubs)}, ${idx(OWNERS, tbs)}, ${lev ? LEVEL_OWNERS.indexOf(lev) : 0},`
          + ` ${a.selfTarget ? 1 : 0}, ${a.stages ?? 0}, {${stats.join(", ") || "0"}}, ${stats.length},`
          + ` {${effects.join(", ") || "0"}}, ${effects.length}, ${tagScore}, ${tagType},`
          + ` ${a.doubleDamage ? 1 : 0}, ${sw.selfSwitch ? 1 : 0}, ${sw.switchType ?? 0},`
          + ` ${a.healRatio ?? 0}, ${a.healStat ?? -1}, ${a.statToSwitch ?? 0}, ${a.statToSwitchWith ?? 0}},`,
      );
    }
    const power = m.calculateEffectivePower();
    moves.push(`    {${power}, ${restr}, ${conds}, ${a0}, ${m.attrs.length}}, /* ${id} ${m.name} */`);
  }
  const out = [
    "/* Generated by gen/dump-ai.test.ts from upstream's live move objects. Do not edit. */",
    '#include "pr/ai_data.h"',
    '#include "pr_enums.h"',
    "",
    `const char *const ai_unknown_notes = ${JSON.stringify([...unknown].join("; "))};`,
    "",
    `const ai_attr_info ai_attrs[${Math.max(attrs.length, 1)}] = {`,
    ...attrs,
    "};",
    "",
    `const ai_move_info ai_moves[${allMoves.length}] = {`,
    ...moves,
    "};",
    `const int ai_move_count = ${allMoves.length};`,
    "",
  ];
  fs.writeFileSync(process.env.DUMP_OUT ?? "/tmp/ai_data.c", out.join("\n"));
});
