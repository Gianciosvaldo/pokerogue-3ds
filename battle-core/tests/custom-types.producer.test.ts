/* Actual pinned Pokemon.getTypes() with customPokemonData.types / fusionCustomPokemonData.types set, over
 * species, form, fusion half, summonData.types, addedType and Tera. Node22.
 * CUSTOM_TYPES_N=4000 CUSTOM_TYPES_SEED=777 CUSTOM_TYPES_OUT=<trace>. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { CustomPokemonData } from "#data/pokemon-data";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";
it("actual getTypes with custom types", async () => {
 const N = Number(process.env.CUSTOM_TYPES_N ?? 4000), SEED = Number(process.env.CUSTOM_TYPES_SEED ?? 777);
 if (!Number.isSafeInteger(N) || N < 1000 || !Number.isSafeInteger(SEED) || SEED < 0 || !process.versions.node.startsWith("22.")) throw Error("Node22, N>=1000");
 const OUT = process.env.CUSTOM_TYPES_OUT; if (!OUT) throw Error("CUSTOM_TYPES_OUT required");
 const g = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
 g.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR);
 const scene = g.scene as any;
 const ids = Object.keys(speciesDataRegistry.data).map(Number);
 let seed = SEED; const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
 const p = scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR), 50) as any;
 const pick = () => { const id = ids[rnd(ids.length)]; const sp = speciesDataRegistry.getSpecies(id); return [id, sp, rnd(Math.max(1, sp.forms.length))] as const; };
 /* a custom slot: null (absent), or one of the 18 regular types */
 const slot = () => (rnd(3) === 0 ? null : rnd(18));
 const customTypes = () => { const k = rnd(8); return k === 0 ? [] : k === 1 ? [slot()] : [slot(), slot()]; };
 const rows = [`CUSTOMTYPES 1 ${N} ${SEED}`];
 const before = Phaser.Math.RND.state();
 for (let i = 0; i < N; i++) {
  const [sid, sp, form] = pick();
  p.species = sp; p.formIndex = form;
  let fid = 0, fform = 0;
  if (rnd(2)) { const [id2, sp2, f2] = pick(); fid = id2; fform = f2; p.fusionSpecies = sp2; p.fusionFormIndex = f2; }
  else { p.fusionSpecies = null; p.fusionFormIndex = 0; }
  const ct = rnd(3) === 0 ? [] : customTypes();
  p.customPokemonData = new CustomPokemonData({ types: ct });
  /* the fusion half's data: absent object, or an object with types */
  let fct: (number | null)[] = [];
  if (rnd(3) === 0) p.fusionCustomPokemonData = null;
  else { fct = customTypes(); p.fusionCustomPokemonData = new CustomPokemonData({ types: fct }); }
  const sum: number[] = rnd(4) === 0 ? Array.from({ length: 1 + rnd(2) }, () => rnd(18)) : [];
  p.summonData.types = sum;
  p.summonData.addedType = rnd(4) === 0 ? 1 + rnd(17) : null;
  const tera = rnd(5) === 0;
  p.isTerastallized = tera; p.teraType = tera ? (rnd(6) === 0 ? 18 : rnd(18)) : p.teraType;
  const enc = (a: (number | null)[]) => { const x = [a[0] ?? null, a[1] ?? null]; return x.map(v => (v === null ? 0 : v + 1)).join(" "); };
  rows.push(`C ${i} ${sid} ${form} ${fid} ${fform} ${enc(ct)} ${enc(fct)} ${sum.length} ${sum[0] ?? 0} ${sum[1] ?? 0} ${p.summonData.addedType ?? 0} ${tera ? 1 : 0} ${tera ? p.teraType : -1}`);
  for (let m = 0; m < 16; m++) {
   const t: number[] = p.getTypes({ includeTeraType: !(m & 1), returnOriginalTypesIfStellar: !!(m & 2), bypassSummonData: !!(m & 4), ignoreThirdType: !!(m & 8) });
   rows.push(`R ${m} ${t.length} ${t.join(" ")}`);
  }
 }
 if (before !== Phaser.Math.RND.state()) throw Error("getTypes changed RNG");
 rows.push(`CUSTOMTYPES_COMPLETE ${N}`);
 fs.writeFileSync(OUT, rows.join("\n") + "\n");
 console.log(`CUSTOMTYPES cases=${N}`);
}, 600000);
