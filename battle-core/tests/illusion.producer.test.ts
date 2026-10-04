/* Additive oracle at 8555c08. Uses unchanged, dormant Pokemon methods: never
 * registers the disabled Illusion ability or edits an existing producer. */
import i18next from "i18next";
import { getPokemonNameWithAffix } from "#app/messages";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it, expect } from "vitest";

it("dormant Illusion identity/type helper producer", async () => {
 const n = Number(process.env.ILLUSION_N ?? 2000);
 let seed = Number(process.env.ILLUSION_SEED ?? 777) >>> 0;
 const rnd = (k:number) => { seed=(Math.imul(seed,1103515245)+12345)>>>0; return (seed>>>8)%k; };
 const game = new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH)
  .startingWave(5).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 const player:any=game.field.getPlayerPokemon(), enemy:any=game.field.getEnemyPokemon();
 let p:any=player,source:any=enemy;
 // Only presentation boundaries are stubbed: set/break/reset/getters stay real.
 for(const mon of [p,source]) {
  mon.loadAssets=async()=>{}; mon.updateInfo=()=>{}; mon.playAnim=()=>{};
  mon.initShinySparkle=()=>{}; mon.isOnField=()=>false;
 }
 const ids=Object.keys(speciesDataRegistry.data).map(Number);
 const identity=(mon:any) => [mon.species.speciesId,mon.formIndex,mon.fusionSpecies?.speciesId??0,mon.fusionFormIndex??0,mon.gender];
 const prepare=(mon:any) => {
  mon.resetSummonData(); mon.species=speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
  mon.formIndex=mon.species.forms.length?rnd(mon.species.forms.length):0;
  mon.fusionSpecies=rnd(2)?speciesDataRegistry.getSpecies(ids[rnd(ids.length)]):null;
  mon.fusionFormIndex=mon.fusionSpecies?.forms.length?rnd(mon.fusionSpecies.forms.length):0;
  mon.gender=rnd(3)-1; mon.id=rnd(0x1000000); mon.isTerastallized=!!rnd(2);mon.teraType=rnd(19);
  mon.summonData.types=rnd(2)?[rnd(19),rnd(19)]:[];mon.summonData.addedType=rnd(19);
  mon.customPokemonData.types=[];mon.fusionCustomPokemonData={types:[]};mon.nickname=null;mon.generateName();
 };
 const lines=[`V 1 ${n}`]; let pokemon=0,species=0,fusion=0,changed=0;
 for(let i=0;i<n;i++) {
  p=i%4<2?player:enemy;source=p===player?enemy:player;prepare(p);prepare(source); const mode=i%2;
  const real=identity(p),stats=[...p.stats],ability=p.getAbility().id;
  const input=[...real,p.id,Number(p.isTerastallized),p.teraType,p.summonData.addedType,p.summonData.types.length,
   p.summonData.types[0]??-1,p.summonData.types[1]??-1,...identity(source),mode,Number(p.isPlayer()),Number(p.hasTrainer())];
  const before=Phaser.Math.RND.state();
  expect(p.setIllusion(mode?source.species:source)).toBe(true);
  mode?species++:pokemon++; if(p.summonData.illusion.fusionSpecies)fusion++;
  const snap=p.summonData.illusion;
  lines.push("C "+input.join(" "));
  const check=() => {
   const s=p.getSpeciesForm(false,true),f=p.getFusionSpeciesForm(false,true);
   lines.push("A "+[Number(!!p.summonData.illusion),s.speciesId,s.formIndex,f?.speciesId??0,f?.formIndex??0,p.getGender(false,true)].join(" "));
   for(let flags=0;flags<32;flags++) {
    const types=p.getTypes({includeTeraType:!(flags&1),returnOriginalTypesIfStellar:!!(flags&2),bypassSummonData:!!(flags&4),ignoreThirdType:!!(flags&8),useIllusion:!!(flags&16)});
    lines.push("T "+[flags,types.length,...types].join(" "));
   }
   const named = !p.getFusionSpeciesForm(false,true);
   lines.push("M "+Number(named));
   if(named) for(const [key,name] of [["battle:singleWildAppeared",p.getNameToRender()],["battle:playerComeBack",getPokemonNameWithAffix(p)]]) {
    const bytes=[...Buffer.from(i18next.t(key,{pokemonName:name}),"utf8")]; lines.push("S "+[bytes.length,...bytes].join(" "));
   }
   expect(identity(p)).toEqual(real); expect(p.stats).toEqual(stats);expect(p.getAbility().id).toBe(ability);
  };
  if(JSON.stringify(p.getTypes({includeTeraType:false}))!==JSON.stringify(p.getTypes({includeTeraType:false,useIllusion:true})))changed++;
  check();
  // Snapshot independence: later edits to the original source cannot change it.
  source.species=speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP);source.gender=0;
  expect(snap.species).not.toBeUndefined(); check();
  expect(p.breakIllusion()).toBe(true);expect(p.breakIllusion()).toBe(false);check();
  expect(p.setIllusion(source)).toBe(true);p.resetSummonData();check();
  expect(Phaser.Math.RND.state()).toBe(before);
 }
 expect(pokemon).toBeGreaterThan(0);expect(species).toBeGreaterThan(0);expect(fusion).toBeGreaterThan(0);expect(changed).toBeGreaterThan(0);
 lines.push(`Z ${n}`);fs.writeFileSync(process.env.ILLUSION_OUT??"/tmp/illusion.txt",lines.join("\n")+"\n");
 console.log(`Illusion helpers: ${n} cases, Pokemon ${pokemon}, species ${species}, fusion ${fusion}, changed typing ${changed}, breaks ${n}, reset cleanup ${n}; natural ability dispatch 0; thrown/skipped 0`);
},120000);
