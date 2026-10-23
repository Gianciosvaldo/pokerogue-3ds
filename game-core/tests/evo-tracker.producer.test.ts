/* Actual MoneyRewardModifier -> Scene.addModifier, evolution eligibility and cleanup. */
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { EvoTrackerModifier } from "#modifiers/modifier";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("native Gimmighoul reward and evolution owners",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await g.classicMode.startBattle(SpeciesId.GIMMIGHOUL);
 BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,party=s.getPlayerParty(),pool=[party[0],s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.GIMMIGHOUL),5),s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.GIMMIGHOUL),5)];
 s.updateMoneyText=()=>{};s.animateMoneyChanged=()=>{};s.validateAchvs=()=>{};
 // Presentation only; Scene.updateModifiers and updatePartyForModifiers stay real.
 s.modifierBar.updateModifiers=()=>{};
 const add=(key:string,owner=-1,stack=1)=>{const f=(modifierTypes as any)[key],t=f().withIdFromFunc(f),m=owner<0?t.newModifier():t.newModifier(party[owner]);m.stackCount=stack;s.modifiers.push(m);return m;};
 const stock=()=>s.modifiers.map((m:any)=>({key:m.type.id,owner:m.pokemonId==null?-1:party.findIndex((p:any)=>p.id===m.pokemonId),stack:m.stackCount,virtual:m.virtualStackCount,locked:!m.isTransferable}));
 const rows:any[]=[];const counts=[0,1,7,8,9,997,998,999];
 for(let family=0;family<3;family++)for(let arrangement=0;arrangement<6;arrangement++)for(const count of counts)for(let treasure=0;treasure<4;treasure++){
  party.splice(0,party.length,...pool);s.modifiers=[];
  for(let j=0;j<3;j++){
   const p=party[j];p.resetSummonData();p.resetBattleAndWaveData();p.formIndex=(treasure+j)%2;p.fusionFormIndex=0;p.level=5;
   const primary=arrangement===1&&j===0||arrangement===3&&j!==1||arrangement===4||arrangement===5&&j===1;
   const fused=arrangement===2&&j===0||arrangement===3&&j===1||arrangement===5;
   p.species=speciesDataRegistry.getSpecies(primary?SpeciesId.GIMMIGHOUL:SpeciesId.MEW);
   if(!primary)p.formIndex=0;
   p.fusionSpecies=fused?speciesDataRegistry.getSpecies(SpeciesId.GIMMIGHOUL):null;
   p.fusionFormIndex=fused?treasure%2:0;p.abilityIndex=p.fusionAbilityIndex=0;p.passive=false;p.calculateStats();p.hp=p.stats[0];
   if(count&& (primary||fused))add("EVOLUTION_TRACKER_GIMMIGHOUL",j,count);
  }
  if(treasure>=1)add("AMULET_COIN",-1,5);
  if(treasure>=2){add("GOLDEN_POKEBALL",-1,3);for(let j=0;j<3;j++)add("GOLDEN_PUNCH",j,5);}
  if(treasure===3){add("AMULET_COIN",-1,1);for(let j=0;j<3;j++)add("GOLDEN_PUNCH",j,1);}
  s.currentBattle.waveIndex=treasure===3?5850:11;s.money=treasure===2?Number.MAX_SAFE_INTEGER:1000.25;s.pokeballCounts[0]=0;
  const input=stock(),before=s.money,rb=s.currentBattle.battleSeedState,rg=Phaser.Math.RND.state();
  const mons=party.map((p:any)=>({species:p.species.speciesId,fusion:p.fusionSpecies?.speciesId??-1,form:p.formIndex,treasure:p.getPersistentTreasureCount(),beforeEvo:p.getEvolution()?.speciesId??0}));
  const f=(modifierTypes as any)[["NUGGET","BIG_NUGGET","RELIC_GOLD"][family]],m=f().withIdFromFunc(f).newModifier();
  expect(m.apply()).toBe(true);
  const after=stock(),evolutions=party.map((p:any)=>p.getEvolution()?.speciesId??0);
  expect(Phaser.Math.RND.state()).toBe(rg);expect(s.currentBattle.battleSeedState).toBe(rb);
  const tracker= s.modifiers.find((x:any)=>x instanceof EvoTrackerModifier);
  if(tracker){const clone=tracker.clone() as any;expect(clone.apply()).toBe(true);expect(clone.matchType(tracker)).toBe(true);expect(clone.isTransferable).toBe(false);expect(clone.getSpecies()).toBe(SpeciesId.GIMMIGHOUL);expect(clone.getMaxHeldItemCount(party[0])).toBe(999);}
  rows.push({family,arrangement,count,treasure,wave:s.currentBattle.waveIndex,before,mons,input,money:s.money,balls:s.pokeballCounts[0],after,evolutions});
 }
 const cleanup:any[]=[];
 for(let form=0;form<2;form++)for(let copies=1;copies<=3;copies++)for(let fused=0;fused<2;fused++){
  party.splice(0,party.length,...pool);s.modifiers=[];const p=party[0];
  p.species=speciesDataRegistry.getSpecies(SpeciesId.GIMMIGHOUL);p.formIndex=form;
  p.fusionSpecies=fused?speciesDataRegistry.getSpecies(SpeciesId.MEW):null;p.fusionFormIndex=0;p.level=5;
  for(let j=0;j<copies;j++)add("EVOLUTION_TRACKER_GIMMIGHOUL",0,10+j);
  add("EVOLUTION_TRACKER_GIMMIGHOUL",1,10);add("GOLDEN_PUNCH",0,5);
  const before=stock(),e=p.getEvolution();expect(e?.speciesId).toBe(SpeciesId.GHOLDENGO);
  // Asset loading and status UI are asynchronous presentation boundaries.
  p.loadAssets=async()=>{};p.updateInfo=async()=>{};
  await p.evolve(e,p.species);
  cleanup.push({form,copies,fused,before,after:stock(),species:p.species.speciesId,fusion:p.fusionSpecies?.speciesId??-1});
 }
 expect(cleanup.length).toBe(12);
 expect(rows.length).toBe(576);
 fs.writeFileSync(process.env.TRACKER_OUT!,JSON.stringify({version:1,rows,cleanup})+"\n");
},240000);
