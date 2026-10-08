/* Additive MoneyRewardModifier.apply monetary-state oracle, pin8555c08 Node22.
 * Real wave/Amulet methods and Scene.addMoney. UI/MoneyAchv calls are counted,
 * not implemented; primary/fused Gimmighoul trackers are an explicit C guard. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { MoneyRewardModifier, MoneyMultiplierModifier } from "#modifiers/modifier";
import { MoneyAchv } from "#system/achv";
import { PokemonData } from "#system/pokemon-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

it("money-item monetary-state producer", async () => {
  const N = Number(process.env.MONEY_N ?? 2000), SEED = Number(process.env.MONEY_SEED ?? 777);
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 120).toBe(true);
  let seed = SEED;
  const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE)
    .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any, party = scene.getPlayerParty();
  const pool = Array.from({length: 6}, () => scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR), 5));
  const names = ["NUGGET", "BIG_NUGGET", "RELIC_GOLD"], counts = [0, 0, 0];
  const species = [SpeciesId.BULBASAUR, SpeciesId.PIKACHU, SpeciesId.GHOLDENGO, SpeciesId.CHARIZARD, SpeciesId.MAGIKARP];
  const waves = [1, 9, 10, 11, 19, 20, 21, 49, 50, 99, 100, 199, 200, 201, 999, 1000, 9999, 10000];
  const lines = [`M 1 ${N}`]; let capped = 0, duplicate = 0, fraction = 0, fused = 0, gholdengo = 0, ordered = 0;
  let ui = 0, animate = 0, achv = 0, amount = -1;
  const oldUI = scene.updateMoneyText, oldAnimate = scene.animateMoneyChanged, oldAchv = scene.validateAchvs, oldAdd = scene.addMoney;
  scene.updateMoneyText = () => { ui++; };
  scene.animateMoneyChanged = (positive: boolean) => { expect(positive).toBe(true); animate++; };
  scene.validateAchvs = (type: any) => { expect(type).toBe(MoneyAchv); achv++; };
  scene.addMoney = function (n: number) { amount = n; oldAdd.call(this, n); };
  const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").map(Number);
  const items = () => scene.modifiers.map((m: any) => `${m.type.id} ${m.stackCount} ${m.pokemonId == null ? -1 : party.findIndex((p: any) => p.id === m.pokemonId)} 0 ${m.getBattleCount?.() ?? 0} 0`);
  const emitItems = () => { const a = items(); lines.push(`I ${a.length}`, ...a.map((s: string) => `H ${s}`)); };
  const snapshotParty = () => JSON.stringify(party.map((p: any) => new PokemonData(p)));
  for (let i = 0; i < N; i++) {
    party.length = 0; scene.modifiers.length = 0;
    const which = i % 3, wave = i % 2 ? 1 + rnd(10000) : waves[Math.floor(i / 6) % waves.length];
    const nParty = 1 + rnd(6);
    for (let j = 0; j < nParty; j++) {
      const p = pool[j]; p.species = speciesDataRegistry.getSpecies(species[rnd(species.length)]); p.formIndex = 0;
      p.fusionSpecies = rnd(2) ? speciesDataRegistry.getSpecies(species[rnd(species.length)]) : null;
      p.fusionFormIndex = 0; party.push(p); fused += +(p.fusionSpecies != null);
      gholdengo += +(p.species.speciesId === SpeciesId.GHOLDENGO || p.fusionSpecies?.speciesId === SpeciesId.GHOLDENGO);
    }
    const create = (name: string) => { const f = (modifierTypes as any)[name]; return f().withIdFromFunc(f); };
    const coinCount = Math.floor(i / 3) % 6;
    const append = (name: string, stack = 1) => {
      const t = create(name), m = name === "LEFTOVERS" ? t.newModifier(party[rnd(nParty)]) : t.newModifier();
      m.stackCount = stack; scene.modifiers.push(m);
    };
    append("HEALING_CHARM");
    for (let j = 0; j < coinCount; j++) { append("AMULET_COIN", 1 + rnd(5)); if (j === 0) append("LEFTOVERS"); }
    if (coinCount >= 2) duplicate++;
    scene.currentBattle.waveIndex = wave;
    const multiplier = [1, 2.5, 10][which], base = scene.getWaveMoneyAmount(multiplier);
    const reversed = scene.modifiers.filter((m: any) => m instanceof MoneyMultiplierModifier).toReversed();
    // Supplemental source-class order coverage, not a replacement for real apply.
    const value = {value: base}; reversed.forEach((m: any) => m.apply(value));
    const modes = [0, rnd(1000000) + 0.25, Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER - 1, Number.MAX_SAFE_INTEGER - 100000, rnd(1000000)];
    const money = modes[Math.floor(i / 3) % modes.length]; fraction += +(money !== Math.floor(money));
    scene.money = money; Phaser.Math.RND.sow([`money-${SEED}-${i}`]);
    const beforeRng = rng(), battleRng = scene.currentBattle.rngState, beforeParty = snapshotParty();
    lines.push(`C ${i} ${names[which]} ${wave} ${money}`, `P ${nParty}`);
    party.forEach((p: any) => lines.push(`B ${p.id} ${p.species.speciesId} ${p.fusionSpecies?.speciesId ?? -1}`));
    emitItems(); lines.push(`R ${beforeRng.join(" ")}`);
    ui = animate = achv = 0; amount = -1;
    const modifier = create(names[which]).newModifier(); expect(modifier).toBeInstanceOf(MoneyRewardModifier);
    const result = modifier.apply(); counts[which]++;
    expect(result).toBe(true); expect(ui).toBe(1); expect(animate).toBe(1); expect(achv).toBe(1);
    capped += +(scene.money === Number.MAX_SAFE_INTEGER); ordered += +(value.value !== amount);
    lines.push(`A ${+result} ${amount} ${scene.money} ${scene.money - money} ${ui} ${animate} ${achv}`); emitItems();
    expect(snapshotParty()).toBe(beforeParty); expect(rng()).toEqual(beforeRng); expect(scene.currentBattle.rngState).toBe(battleRng);
    lines.push(`E ${i} ${rng().join(" ")}`);
  }
  scene.updateMoneyText = oldUI; scene.animateMoneyChanged = oldAnimate; scene.validateAchvs = oldAchv; scene.addMoney = oldAdd;
  counts.forEach(c => expect(c).toBeGreaterThan(0));
  for (const count of [capped, duplicate, fraction, fused, gholdengo, ordered]) expect(count).toBeGreaterThan(0);
  lines.push(`Z ${N} 0 0 ${counts.join(" ")} ${capped} ${duplicate} ${fraction} ${fused} ${gholdengo} ${ordered}`);
  fs.writeFileSync(process.env.MONEY_OUT ?? "/tmp/money-items.txt", lines.join("\n") + "\n");
}, 240000);
