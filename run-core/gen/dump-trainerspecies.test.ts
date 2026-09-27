/*
 * Dumps what Trainer.genNewPartyMemberSpecies reads, for run-core, as C:
 * - for every species (by SpeciesId, in getAllSpecies() order, which is ascending id): isCatchable(),
 *   the root of its prevolution chain (getRootSpeciesId()) and its type1/type2 (isOfType, the balanced
 *   party check);
 * - for every trainer type: config.speciesPools by TrainerPoolTier (an entry that is an array of species
 *   instead of a species is -1: upstream rerolls it), config.speciesFilter captured by calling it on every
 *   species (as a bitset over the species order, together with isCatchable: what randomSpecies keeps;
 *   identical bitsets are shared), and the roots of signatureSpecies[TrainerType[type]]
 *   (checkDuplicateSpecies);
 * - what genPartyMember reads: hasStaticParty, useSameSeedForAllMembers, getDerivedType(), trainerTypeDouble,
 *   doubleOnly, the indexes with a partyMemberFunc, and signatureSpecies as groups (the double trainers' pools).
 * The filters are plain functions of the species (type, legendary...), so calling them once captures them.
 * Run with tools/redump.sh <upstream dir>; writes gen/out/run_trainerspecies.c. Don't edit the output.
 */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { signatureSpecies } from "#balance/signature-species";
import { TrainerPoolTier } from "#enums/trainer-pool-tier";
import { TrainerType } from "#enums/trainer-type";
import { GameManager } from "#test/framework/game-manager";
import { trainerConfigs } from "#trainers/trainer-config";
import * as fs from "node:fs";
import Phaser from "phaser";
import { expect, it } from "vitest";

it("dump trainer species data", () => {
  new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  const out = process.env.DUMP_OUT ?? "/tmp/run_trainerspecies.c";
  expect([TrainerPoolTier.COMMON, TrainerPoolTier.ULTRA_RARE]).toEqual([0, 4]);
  const all = speciesDataRegistry.getAllSpecies();
  const ids = all.map(s => s.speciesId);
  // The C side indexes species by their place in pr_species_gens (ascending id): the same order
  expect(ids).toEqual([...ids].sort((a, b) => a - b));
  const root = (id: number) => {
    let r = id;
    while (speciesDataRegistry.hasPrevolution(r)) {
      r = speciesDataRegistry.getPrevolution(r)!;
    }
    return r;
  };
  const rows = all.map(
    s => `    {${s.isCatchable() ? 1 : 0}, ${root(s.speciesId)}, {${s.type1}, ${s.type2 ?? -2}}},`,
  );

  const types = Object.values(TrainerType).filter(v => typeof v === "number") as number[];
  const bytes = Math.ceil(all.length / 8);
  const filters: string[] = [];
  const filterIndex = new Map<string, number>();
  const pools: number[] = [];
  const sigs: number[] = [];
  const sigGroups: string[] = [];
  const sigSpecies: number[] = [];
  const trows: string[] = [];
  for (const t of types) {
    const cfg: any = (trainerConfigs as any)[t];
    if (!cfg) {
      continue;
    }
    const tiers: string[] = [];
    for (let tier = 0; tier <= 4; tier++) {
      if (cfg.speciesPools && Object.hasOwn(cfg.speciesPools, tier)) {
        const p = cfg.speciesPools[tier] as any[];
        tiers.push(`{${pools.length}, ${p.length}}`);
        for (const e of p) {
          pools.push(typeof e === "number" ? e : -1);
        }
      } else {
        tiers.push("{0, -1}");
      }
    }
    let filter = -1;
    if (cfg.speciesFilter) {
      const bits = new Array(bytes).fill(0);
      all.forEach((s, i) => {
        if (s.isCatchable() && cfg.speciesFilter(s)) {
          bits[i >> 3] |= 1 << (i & 7);
        }
      });
      const key = bits.join(",");
      if (!filterIndex.has(key)) {
        filterIndex.set(key, filters.length);
        filters.push(`    ${key},`);
      }
      filter = filterIndex.get(key)!;
    }
    // genPartyMember: which indexes have a partyMemberFunc (a closure: not ported), the seed offset's
    // inputs, the double partner, and signatureSpecies as groups (an entry that is a list is one group)
    let funcs = 0;
    for (const k of Object.keys(cfg.partyMemberFuncs)) {
      const i = Number(k);
      // keys run negative too: genPartyMember also asks for index - template.size (-1 is the last member)
      if (!(i >= -8 && i < 8)) {
        throw new Error(`partyMemberFuncs key ${k}: widen member_funcs`);
      }
      funcs |= 1 << (i + 8);
    }
    const groups = ((signatureSpecies as any)[TrainerType[t]] ?? []).map((g: any) => (Array.isArray(g) ? g : [g]));
    const groupStart = sigGroups.length;
    for (const g of groups) {
      sigGroups.push(`{${sigSpecies.length}, ${g.length}}`);
      sigSpecies.push(...g);
    }
    const sig = ((signatureSpecies as any)[TrainerType[t]] ?? []).flat(1).map(root);
    const sigStart = sigs.length;
    sigs.push(...sig);
    trows.push(
      `    {${t}, ${cfg.speciesPools ? 1 : 0}, {${tiers.join(", ")}}, ${filter}, ${sigStart}, ${sig.length}, `
        + `${cfg.hasStaticParty ? 1 : 0}, ${cfg.useSameSeedForAllMembers ? 1 : 0}, ${cfg.getDerivedType()}, `
        + `${cfg.trainerTypeDouble ?? 0}, ${cfg.doubleOnly ? 1 : 0}, ${funcs}, ${groupStart}, ${groups.length}},`,
    );
  }
  const c = [
    "/* Generated by gen/dump-trainerspecies.test.ts from upstream's species registry, trainer configs and",
    " * signature species. Do not edit. */",
    '#include "pr/run.h"',
    "",
    `const pr_species_trainer_info pr_species_trainer[${all.length}] = {`,
    ...rows,
    "};",
    `const int pr_trainer_species_count = ${trows.length};`,
    `const pr_trainer_species pr_trainer_species_data[${trows.length}] = {`,
    ...trows,
    "};",
    `const int16_t pr_trainer_pool_entries[${Math.max(pools.length, 1)}] = {${pools.join(", ") || "0"}};`,
    `const uint16_t pr_trainer_signature_roots[${Math.max(sigs.length, 1)}] = {${sigs.join(", ") || "0"}};`,
    `const pr_signature_group pr_trainer_signature_groups[${Math.max(sigGroups.length, 1)}] = {${sigGroups.join(", ") || "{0, 0}"}};`,
    `const uint16_t pr_trainer_signature_species[${Math.max(sigSpecies.length, 1)}] = {${sigSpecies.join(", ") || "0"}};`,
    `const int pr_trainer_filter_bytes = ${bytes};`,
    `const uint8_t pr_trainer_species_filters[${filters.length * bytes}] = {`,
    ...filters,
    "};",
    "",
  ];
  fs.writeFileSync(out, c.join("\n"));
  console.log("trainers", trows.length, "pool entries", pools.length, "filters", filters.length, "signature", sigs.length);
});
