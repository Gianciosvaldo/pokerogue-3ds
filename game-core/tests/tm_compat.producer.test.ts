/* Native getCompatibleTms / TmModifierType filter, pinned source; Classic only.
 * Every registry species/form, plus a deterministic fusion and known-move profile. */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import { TmModifierType } from "#modifiers/modifier-type";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect, it } from "vitest";
it("native TM labels and teaching filter", async () => {
  const native = BattleScene.prototype.randBattleSeedInt;
  expect(process.versions.node.split('.')[0]).toBe('22');
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle('single').ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
    .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  BattleScene.prototype.randBattleSeedInt = native;
  const p: any = game.scene.getPlayerParty()[0];
  const ids = Object.keys(speciesDataRegistry.data).map(Number).sort((a,b)=>a-b);
  const rows: string[] = []; let filters = 0, forms = 0;
  for (let si = 0; si < ids.length; si++) {
    p.species = speciesDataRegistry.getSpecies(ids[si]);
    for (let f = 0; f < Math.max(1, p.species.forms.length); f++) {
      p.formIndex = f; forms++;
      for (let profile = 0; profile < 3; profile++) {
        p.fusionSpecies = profile === 2 ? speciesDataRegistry.getSpecies(ids[(si+317)%ids.length]) : null;
        p.fusionFormIndex = p.fusionSpecies ? (si+f)%Math.max(1,p.fusionSpecies.forms.length) : 0;
        const known = profile ? [MoveId.BIND,MoveId.FLAMETHROWER,MoveId.THUNDERBOLT,MoveId.SPLASH] : [];
        p.moveset = known.map(m => new PokemonMove(m));
        p.usedTMs = [MoveId.FLAMETHROWER]; p.hp = profile === 2 ? 0 : 1;
        const compatible = new Set(p.getCompatibleTms());
        let states = '';
        for (let move = 0; move < 921; move++) {
          const state = known.includes(move) ? 2 : compatible.has(move) ? 1 : 0;
          states += state;
          if (move > 0 && (move % 47 === 0 || known.includes(move))) {
            const filter = new TmModifierType(move).selectFilter;
            expect(filter(p) == null).toBe(state === 1); filters++;
          }
        }
        rows.push(`C ${rows.length} ${ids[si]} ${f} ${p.fusionSpecies?.speciesId??0} ${p.fusionFormIndex} ${profile} ${states}`);
      }
    }
  }
  expect(ids.length).toBe(1084); expect(forms).toBe(1500); expect(rows.length).toBe(4500); expect(filters).toBe(97500);
  fs.writeFileSync(process.env.TM_COMPAT_OUT!, `TM_COMPAT 1 ${rows.length}\n${rows.join('\n')}\nTM_COMPAT_COMPLETE ${rows.length}\n`);
  console.log(`TM_COMPAT species=${ids.length} forms=${forms} profiles=${rows.length} filter_checks=${filters}`);
}, 240000);
