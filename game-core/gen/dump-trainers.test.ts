/*
 * Dumps what game-core reads from upstream's trainer configs, as C: for every TrainerType in
 * trainerConfigs, config.moneyMultiplier, isBoss, doubleOnly, hasCharSprite, how many modifierRewardFuncs it has,
 * the trainersCommon class its personal names come from, the names Trainer.getName gives it, and the
 * dialogue keys Trainer.getEncounterMessages / getVictoryMessages give each variant (initTrainerTypeDialogue
 * has run, as it does when upstream's scene starts), and how the trainer Terastallizes: the slots config.trainerAI lists
 * from the start (setInstantTera at config time), and what setRandomTeraModifiers' genAIFuncs do (read by running them on
 * fake parties: which wave they start at, the fixed slot per party size or random, the Tera type they give).
 *
 * The names are upstream's own getName, run on each variant for the slot / title pairs the game asks
 * for, with i18next.t swapped for one that returns $t(key) (trainerConfigs and the Trainer class are
 * imported afresh for that, as the configs translate their names and titles when they are made). The
 * personal names are placeholders, @N and @P, that game-core replaces with the key of the name run-core
 * rolled. Key existence is still read from upstream's English locale. Run with tools/redump.sh
 * <upstream dir>; writes gen/out/gc_trainers.c. The output is generated: don't edit it.
 */
import { TrainerSlot } from "#enums/trainer-slot";
import { TrainerVariant } from "#enums/trainer-variant";
import { GameManager } from "#test/framework/game-manager";
import { toCamelCase } from "#utils/strings";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it, vi } from "vitest";

it("dump trainers", async () => {
  new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  const out = process.env.DUMP_OUT ?? "/tmp/gc_trainers.c";
  const real = (await import("i18next")).default;
  vi.resetModules();
  vi.doMock("i18next", () => {
    const keyed = Object.create(real);
    keyed.t = (k: string, o?: object) => (o ? real.t(k, o) : `\u0001${k}\u0002`);
    return { default: keyed };
  });
  const { trainerConfigs } = await import("#trainers/trainer-config");
  const { TrainerType } = await import("#enums/trainer-type");
  const { Trainer } = await import("#field/trainer");
  const { initTrainerTypeDialogue } = await import("#data/dialogue");
  initTrainerTypeDialogue();

  const common = JSON.parse(fs.readFileSync("locales/en/trainers-common.json", "utf8"));
  /* the names in the order gc_trainer_config.names has them, for each variant */
  const asks: [TrainerSlot, boolean][] = [
    [TrainerSlot.NONE, true],
    [TrainerSlot.TRAINER, false],
    [TrainerSlot.TRAINER_PARTNER, false],
    [TrainerSlot.TRAINER, true],
  ];
  const variants = [TrainerVariant.DEFAULT, TrainerVariant.FEMALE, TrainerVariant.DOUBLE];
  const pool: string[] = [];
  const index = new Map<string, number>();
  const intern = (s: string) => {
    if (!index.has(s)) { index.set(s, pool.length); pool.push(s); }
    return index.get(s)!;
  };
  /* a C string literal: UTF-8, other than printable ASCII as octal escapes */
  const cstr = (s: string) =>
    `"${[...Buffer.from(s, "utf8")]
      .map(b => (b === 0x22 || b === 0x5c ? `\\${String.fromCharCode(b)}` : b >= 0x20 && b < 0x7f ? String.fromCharCode(b) : `\\${b.toString(8).padStart(3, "0")}`))
      .join("")}"`;

  /* the dialogue keys, one run per distinct list: a list is {first, n} into gc_trainer_dialogue_keys */
  const dialogueKeys: string[] = [];
  const dialogueRuns = new Map<string, string>();
  const dialogueList = (messages: string[] | undefined) => {
    const list = messages ?? [];
    const id = JSON.stringify(list);
    if (!dialogueRuns.has(id)) {
      dialogueRuns.set(id, `{${dialogueKeys.length}, ${list.length}}`);
      dialogueKeys.push(...list);
    }
    return dialogueRuns.get(id)!;
  };
  const gs = await import("#app/global-scene");
  const sceneAt = (waveIndex: number) => gs.initGlobalScene({ currentBattle: { waveIndex } } as any);
  const RND = Phaser.Math.RND;
  /* genAIFuncs, probed: party sizes 1..6 of a Pokemon that is not Shedinja, many seeds, the waves either side of GYM_LEADER_TERA_WAVE */
  const probeTera = (c: any) => {
    const funcs: ((party: any[]) => void)[] = c.genAIFuncs;
    if (funcs.length === 0) return { random: false, minWave: 1, slot: [-1, -1, -1, -1, -1, -1, -1], type: -1 };
    if (funcs.length !== 1) throw new Error(`a config with ${funcs.length} genAIFuncs`);
    const ai = c.trainerAI;
    const saved = { mode: ai.teraMode, logic: ai.teraLogic.slice() };
    const run = (wave: number, len: number, seed: string) => {
      sceneAt(wave);
      RND.sow([seed]);
      const party = Array.from({ length: len }, () => ({ species: { speciesId: 1 }, teraType: -1 }));
      ai.teraLogic.length = saved.logic.length;
      funcs[0](party);
      const picked = ai.teraLogic.slice(saved.logic.length);
      ai.teraMode = saved.mode;
      ai.teraLogic.length = saved.logic.length;
      return { picked, party };
    };
    const countAt = (wave: number) => {
      const n = new Set<number>();
      for (let len = 1; len <= 6; len++) n.add(run(wave, len, "x").picked.length);
      if (n.size !== 1 || [...n][0] > 1) throw new Error("tera count is not 0 or 1");
      return [...n][0];
    };
    const c1 = countAt(1), c99 = countAt(99), c100 = countAt(100), c5000 = countAt(5000);
    if (c99 !== c1 || c5000 !== c100) throw new Error("tera count changes at a wave other than 100");
    const minWave = c1 ? 1 : c100 ? 100 : -1;
    if (minWave < 0) throw new Error("a config that never Terastallizes");
    const slot = [-1];
    let type = -2;
    for (let len = 1; len <= 6; len++) {
      const seen = new Set<number>();
      for (let k = 0; k < 40; k++) {
        const r = run(100, len, `seed${k}`);
        seen.add(r.picked[0] as number);
        const types = r.party.filter(m => m.teraType !== -1).map(m => m.teraType);
        if (types.length > 1) throw new Error("more than one tera type set");
        const t = types.length ? types[0] : -1;
        if (type !== -2 && type !== t) throw new Error("the tera type changes");
        type = t;
      }
      slot.push(seen.size === 1 ? [...seen][0] : -1);
    }
    return { random: true, minWave, slot, type };
  };
  const types = Object.keys(trainerConfigs).map(Number).sort((a, b) => a - b);
  const rows: string[] = [];
  for (const t of types) {
    const c = trainerConfigs[t];
    const classKey = toCamelCase(TrainerType[t]);
    const hasNames = Object.hasOwn(common, classKey);
    const names: number[] = [];
    for (const variant of variants) {
      /* the Trainer constructor: a personal name when trainersCommon has the class, and a partner's for a
       * double (for a double-only class, both halves of one name); otherwise Phaser's default name, "" */
      const fake = {
        config: c,
        variant,
        name: hasNames ? "@N" : "",
        partnerName: hasNames && variant === TrainerVariant.DOUBLE ? "@P" : undefined,
      };
      for (const [slot, title] of asks) {
        const s = (Trainer.prototype.getName as (s: TrainerSlot, t: boolean) => string)
          .call(fake, slot, title)
          .replace(/\u0001([^\u0002]*)\u0002/g, "$$t($1)");
        names.push(intern(s));
      }
    }
    const dialogue = variants.map(variant => {
      const fake = { config: c, variant };
      const encounter = (Trainer.prototype.getEncounterMessages as () => string[]).call(fake);
      const victory = (Trainer.prototype.getVictoryMessages as () => string[]).call(fake);
      return `{${dialogueList(encounter)}, ${dialogueList(victory)}}`;
    });
    /* the AI's tera slots from the config's own setInstantTera calls (a condition only a Mystery Encounter sets) */
    let staticMask = 0;
    for (const e of (c.trainerAI as any).teraLogic as (number | [number, () => boolean])[]) {
      if (typeof e !== "number" || e < 0 || e > 7) throw new Error(`trainer ${t}: a conditional or out of range tera slot`);
      staticMask |= 1 << e;
    }
    const tera = probeTera(c);
    rows.push(
      `{${t}, ${hasNames ? cstr(classKey) : "NULL"}, ${c.moneyMultiplier}, ${c.isBoss}, ${c.doubleOnly}, `
        + `${c.modifierRewardFuncs.length}, {${[0, 4, 8].map(i => `{${names.slice(i, i + 4).join(", ")}}`).join(", ")}}, `
        + `${c.hasCharSprite}, {${dialogue.join(", ")}}, `
        + `${staticMask}, ${tera.random}, ${tera.minWave}, {${tera.slot.join(", ")}}, ${tera.type}}`,
    );
  }
  vi.doUnmock("i18next");
  const lines = [
    "/* Generated by gen/dump-trainers.test.ts from upstream's trainerConfigs and Trainer.getName. Do not edit. */",
    '#include "gc/data.h"',
    "",
    `const char *const gc_trainer_name_templates[${pool.length}] = {`,
    ...pool.map(s => `    ${cstr(s)},`),
    "};",
    `const char *const gc_trainer_dialogue_keys[${Math.max(dialogueKeys.length, 1)}] = {`,
    ...dialogueKeys.map(k => `    ${cstr(k)},`),
    "};",
    `const int gc_trainer_config_count = ${rows.length};`,
    `const gc_trainer_config gc_trainer_configs[${rows.length}] = {`,
    ...rows.map(r => `    ${r},`),
    "};",
    "",
  ];
  fs.writeFileSync(out, lines.join("\n"));
});
