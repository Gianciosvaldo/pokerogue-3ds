/* Actual getNameToRender / getPokemonNameWithAffix / pokemon.name of Pokemon that carry a nickname, as the messages name them.
 * Nicknames are stored the way the rename form stores them (btoa(unescape(encodeURIComponent(text)))) and decoded by the pinned
 * decodeNickname, so what is captured is what upstream shows. Same shape as fusion-message.producer.test.ts. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getPokemonNameWithAffix } from "#app/messages";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it, vi } from "vitest";
it("actual nickname message identity", async () => {
  const N = Number(process.env.NICKNAME_N ?? 8192),
    seed = Number(process.env.NICKNAME_SEED ?? 777);
  if (!Number.isSafeInteger(N) || N < 1024 || !process.versions.node.startsWith("22.")) throw Error("Node22,N>=1024");
  const g = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  g.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);
  await g.classicMode.startBattle(SpeciesId.BULBASAUR);
  g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
  const s: any = g.scene,
    pm: any = s.phaseManager,
    ps: any[] = [g.field.getPlayerPokemon(), g.field.getEnemyPokemon()];
  vi.spyOn(pm, "shiftPhase").mockImplementation(() => {});
  vi.spyOn(s, "updateFieldScale").mockImplementation(async () => {});
  for (const p of ps) {
    vi.spyOn(p, "loadAssets").mockImplementation(async () => {});
    vi.spyOn(p, "updateInfo").mockImplementation(async () => {});
    vi.spyOn(p, "playAnim").mockImplementation(() => {});
    vi.spyOn(p, "updateSpritePipelineData").mockImplementation(() => {});
  }
  const all = Object.keys(speciesDataRegistry.data)
    .map(Number)
    .flatMap(id =>
      Array.from({ length: Math.max(1, speciesDataRegistry.getSpecies(id).forms.length) }, (_, k) => [id, k]),
    );
  let r = seed;
  const rnd = (n: number) => {
      r = (Math.imul(r, 1103515245) + 12345) >>> 0;
      return (r >>> 8) % n;
    },
    hex = (v: string) => Buffer.from(v).toString("hex"),
    // what the form's submit stores
    enc = (t: string) => btoa(unescape(encodeURIComponent(t)));
  // what the 3DS keyboard can type and then some: plain, accents, apostrophes, inner and outer spaces, symbols, non-Latin text, emoji
  const fixed = [
    "Sparky",
    "Mr. Pikachu the Great!",
    "Ace",
    "  padded  ",
    "Ñandú",
    "Bébé",
    "Œuf d'été",
    "ÀÉÎÕÜ àéîõü",
    "12345678901234567890",
    "a",
    "あいうえお",
    "Pokémon ♀♂",
    "🔥Blaze🔥",
    "{{pokemonName}}",
    "100% $t(x)",
    "A B",
    "Wild Pikachu",
    "Foe",
  ];
  const alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 '-.,éèàùñüïç";
  const randomName = () => {
    const len = 1 + rnd(20);
    return Array.from({ length: len }, () => alphabet[rnd(alphabet.length)]).join("");
  };
  const rows = [`NICKNAME 1 ${N} ${seed}`];
  for (let i = 0; i < N; i++) {
    // mode 0: nothing special, 1: an enemy-side name, 2: a disguise (the nickname is hidden), 3: transformed (the nickname stays)
    const pidx = i % 2,
      p = ps[pidx],
      other = ps[1 - pidx],
      mode = Math.floor(i / 2) % 4;
    for (const q of ps) {
      q.resetSummonData();
      q.resetTurnData();
      q.resetBattleAndWaveData();
      const [id, form] = all[rnd(all.length)],
        [fid, ff] = all[rnd(all.length)];
      q.species = speciesDataRegistry.getSpecies(id);
      q.formIndex = form;
      q.fusionSpecies = rnd(2) ? speciesDataRegistry.getSpecies(fid) : null;
      q.fusionFormIndex = ff;
      q.fusionAbilityIndex = 0;
      q.abilityIndex = 0;
      q.customPokemonData.ability = AbilityId.NONE;
      q.fusionCustomPokemonData = null;
      q.nickname = null;
      q.passive = false;
      q.customPokemonData.passive = AbilityId.NONE;
      q.moveset = [];
      q.calculateStats();
      q.hp = q.getMaxHp();
      q.generateName();
    }
    // the nickname: none (undefined, null or ""), a fixed edge case, or random text
    const kind = rnd(8);
    let text: string | null = null;
    if (kind === 0) p.nickname = undefined;
    else if (kind === 1) p.nickname = "";
    else {
      text = kind <= 4 ? fixed[rnd(fixed.length)] : randomName();
      p.nickname = enc(text);
    }
    // the disguise's own nickname is not modelled (the app keeps it): the other Pokemon has none when it disguises
    other.nickname = mode === 2 || rnd(2) ? null : enc(randomName());
    s.currentBattle.waveIndex = Math.floor(i / 8) % 2 ? 200 : 1;
    ps[1].trainerSlot = Math.floor(i / 16) % 2 ? 1 : 0;
    if (mode === 2) p.setIllusion(other);
    if (mode === 3) {
      pm.clearAllPhases();
      pm.phaseQueue.deferredActive = false;
      const ph = pm.create("PokemonTransformPhase", p.getBattlerIndex(), other.getBattlerIndex(), false);
      pm.currentPhase = ph;
      ph.start();
      await Promise.resolve();
      await Promise.resolve();
      await Promise.resolve();
      if (!p.isTransformed()) throw Error("Transform refused");
    }
    const ill = p.summonData.illusion;
    rows.push(
      `C ${i} ${p.species.speciesId} ${p.formIndex} ${p.fusionSpecies?.speciesId ?? 0} ${p.fusionFormIndex} ${+p.isPlayer()} ${+p.hasTrainer()} ${s.currentBattle.waveIndex} ${+!!ill} ${ill?.species ?? 0} ${ill?.formIndex ?? 0} ${ill?.fusionSpecies?.speciesId ?? 0} ${ill?.fusionFormIndex ?? 0} ${+p.isTransformed()} ${text === null ? "-" : hex(text)} ${hex(p.getNameToRender())} ${hex(getPokemonNameWithAffix(p))} ${hex(p.name)}`,
    );
  }
  rows.push(`NICKNAME_COMPLETE ${N}`);
  fs.writeFileSync(process.env.NICKNAME_OUT ?? "/tmp/nickname-msg.txt", rows.join("\n") + "\n");
}, 600000);
