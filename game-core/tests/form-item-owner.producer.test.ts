/* General form owners. Phase selection is recorded before animation executes;
 * this file alone does not validate execution of the resulting form phase. */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { SpeciesFormChangeItemTrigger } from "#data/form-change-triggers";
import { AbilityId } from "#enums/ability-id";
import { FormChangeItem } from "#enums/form-change-item";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PartyUiHandler } from "#ui/party-ui-handler";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect, it } from "vitest";

it("native form acquisition and complete party-item eligibility inputs", async () => {
  const native = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("double").moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MEW);
  await game.classicMode.startBattle(SpeciesId.MEW, SpeciesId.MEW, SpeciesId.MEW);
  BattleScene.prototype.randBattleSeedInt = native;
  const scene: any = game.scene, pm: any = scene.phaseManager, party: any[] = scene.getPlayerParty();
  expect(party.length).toBe(3);expect(party[2].isOnField()).toBe(false);
  const enums = Object.values(FormChangeItem).filter((v): v is number => typeof v === "number");
  expect(enums.length).toBe(159);
  const cache = new Map<string, any>();
  const make = (key: string, item: number, p: any, tier = 0) => {
    const id = `${key}:${item}`;
    if (!cache.has(id)) {
      const factory = (modifierTypes as any)[key], generator = factory().withIdFromFunc(factory);
      const type = generator.generateType([p], [item]); expect(type).not.toBeNull();type.id = key;cache.set(id,type);
    }
    const type = cache.get(id);type.tier = tier;return type.newModifier(p);
  };
  const rows: any[] = [], refs: any[] = [];
  for (const species of speciesDataRegistry.getAllSpecies()) {
    if (!speciesDataRegistry.hasFormChanges(species.speciesId)) continue;
    for (const [index, fc] of speciesDataRegistry.getFormChanges(species.speciesId).entries()) {
      const trigger: any = fc.findTrigger(SpeciesFormChangeItemTrigger);if (!trigger) continue;
      rows.push({ species: species.speciesId,index,item: trigger.item,from: species.forms.findIndex(f => f.formKey === fc.preFormKey),
        to: species.forms.findIndex(f => f.formKey === fc.formKey),toKey: fc.formKey });refs.push(fc);
    }
  }
  expect(rows.length).toBe(416);
  const ruleIndex = new Map(refs.map((fc,i) => [fc,i]));
  const stock = () => [scene.modifiers,scene.enemyModifiers].map(side => side.map((m: any) =>
    [m.type.id,m.formChangeItem,m.pokemonId,m.active,m.stackCount,m.virtualStackCount,!m.isTransferable]));
  const acquisition: any[] = [];
  for (let rule = 0; rule < rows.length; rule++) for (const owner of [0,2]) for (const rare of [false,true])
    for (let gate = 0; gate < 4; gate++) for (const duplicate of [false,true]) for (let tier = 0; tier < 5; tier++) {
      const row = rows[rule], p = party[owner];
      for (const q of [...party,...scene.getEnemyParty()]) {
        q.species=speciesDataRegistry.getSpecies(SpeciesId.MEW);q.formIndex=0;q.resetSummonData();q.fusionSpecies=null;q.passive=false;q.hp=1;
        q.summonData.ability=AbilityId.LEVITATE;
      }
      p.species=speciesDataRegistry.getSpecies(row.species);p.formIndex=row.from;p.abilityIndex=0;
      p.gender=row.toKey === "female" ? 1 : 0;p.nature=row.toKey === "lowkey" ? 1 : 0;p.customPokemonData.nature=-1;
      p.summonData.ability=gate&1 ? AbilityId.LEVITATE : AbilityId.NONE;
      // All six source caught predicates are observed separately by the retained
      // callback proof. This owning path exercises both none and all caught.
      for (const id of [643,644,791,792,896,897]) scene.gameData.dexData[id].caughtAttr=gate&2 ? 1n : 0n;
      scene.modifiers=[];scene.enemyModifiers=[];
      const key=rare?"RARE_FORM_CHANGE_ITEM":"FORM_CHANGE_ITEM";
      if (duplicate) {const old:any=make(rare?"FORM_CHANGE_ITEM":"RARE_FORM_CHANGE_ITEM",row.item,p,tier);old.active=false;scene.modifiers.push(old);}
      for(let ball=0;ball<5;ball++)scene.pokeballCounts[ball]=0;
      pm.phaseQueue.levels=[[]];pm.phaseQueue.currentLevel=0;pm.phaseQueue.deferredActive=false;
      const m:any=make(key,row.item,p,tier),globalRng=Phaser.Math.RND.state(),battleRng=scene.currentBattle.battleSeedState;
      const filter=m.type.selectFilter(p),result=scene.addModifier(m,true,false,false,true);
      const queue=pm.phaseQueue.levels.map((level:any[])=>level.map(q=> {
        expect(["FormChangePhase","MessagePhase"]).toContain(q.phaseName);
        return {kind:q.phaseName,rule:q.formChange?ruleIndex.get(q.formChange):-1,text:q.text??""};
      }));
      expect(Phaser.Math.RND.state()).toBe(globalRng);expect(scene.currentBattle.battleSeedState).toBe(battleRng);
      acquisition.push({rule,owner,rare,gate,duplicate,tier,onField:p.isOnField(),ability:p.getAbility().id,
        baseAbility:p.getAbility(true).id,level:p.level,nature:p.getNature(),gender:p.gender,hp:p.hp,
        filter,result,stock:stock(),queue,balls:Array.from({length:5},(_,i)=>scene.pokeballCounts[i])});
    }
  expect(acquisition.length).toBe(66560);
  const menu:any[]=[];
  for (const species of [SpeciesId.MEW,SpeciesId.NECROZMA]) for(const item of enums) for(let flags=0;flags<16;flags++)
    for(let owner=0;owner<3;owner++) for(const enemy of [false,true]) for(const foreign of [false,true]) for(const reverse of [false,true]) {
      const p=party[owner];p.species=speciesDataRegistry.getSpecies(species);p.formIndex=0;
      const target=foreign?party[(owner+1)%3]:p;
      const wanted=[item,FormChangeItem.ULTRANECROZIUM_Z,FormChangeItem.N_LUNARIZER,FormChangeItem.N_SOLARIZER];
      const list:any[]=[];
      for(let i=0;i<wanted.length;i++) {
        if(wanted.indexOf(wanted[i])!==i)continue; // one canonical class/item/holder entry
        const m:any=make(i%2?"RARE_FORM_CHANGE_ITEM":"FORM_CHANGE_ITEM",wanted[i],target);m.active=!!(flags&(1<<i));list.push(m);
      }
      if(reverse)list.reverse();scene.modifiers=enemy?[]:list;scene.enemyModifiers=enemy?list:[];
      const globalRng=Phaser.Math.RND.state(),battleRng=scene.currentBattle.battleSeedState;
      const selected=PartyUiHandler.prototype.getFormChangeItemsModifiers.call({},p);
      expect(selected.every((m:any)=>list.includes(m))).toBe(true);
      expect(Phaser.Math.RND.state()).toBe(globalRng);expect(scene.currentBattle.battleSeedState).toBe(battleRng);
      menu.push({species,item,flags,owner,enemy,foreign,reverse,stock:stock(),selected:selected.map((m:any)=>list.indexOf(m))});
    }
  expect(menu.length).toBe(122112);
  fs.writeFileSync(process.env.FORM_OWNER_OUT!,JSON.stringify({version:1,pin:"8555c08c823b856cbec4eb99ca84ea52a955836d",
    ids:party.map(p=>p.id),enums,rows,acquisition,menu})+"\n");
},240000);
