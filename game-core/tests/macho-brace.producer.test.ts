import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it } from "vitest";
it("Macho Brace native stat and EXP owners",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt,g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP);
 await g.classicMode.startBattle(SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=g.scene,p:any=s.getPlayerPokemon(),rows:any[]=[];s.modifierBar.updateModifiers=()=>{};p.updateInfo=async()=>{};
 for(const stack of [0,1,49,50])for(const participated of [false,true])for(const defeated of [false,true])for(const fainted of [false,true]){
  s.modifiers=[];p.level=1000;p.friendship=0;p.ivs=Array(6).fill(31);
  const f=modifierTypes.MYSTERY_ENCOUNTER_MACHO_BRACE,m:any=f().withIdFromFunc(f).newModifier(p);m.stackCount=stack;
  if(stack)s.modifiers.push(m);p.calculateStats();p.hp=fainted?0:p.stats[0];
  const input={stack:stack?m.stackCount:0,hp:p.hp,stats:p.stats.slice(),friendship:p.friendship},rg=Phaser.Math.RND.state(),rb=s.currentBattle.battleSeedState;
  s.applyPartyExp(0,defeated,false,new Set(participated?[p.id]:[]));
  expect(Phaser.Math.RND.state()).toBe(rg);expect(s.currentBattle.battleSeedState).toBe(rb);
  rows.push({participated,defeated,fainted,level:p.level,maxExpLevel:s.getMaxExpLevel(),input,output:{stack:stack?m.stackCount:0,hp:p.hp,stats:p.stats.slice(),friendship:p.friendship}});
 }
 expect(rows.length).toBe(32);
 const statsRows:any[]=[];
 for(const species of [SpeciesId.MEW,SpeciesId.SHEDINJA,SpeciesId.CHARIZARD])for(const fused of [false,true])for(const level of [1,5,100,1000,65536])for(let nature=0;nature<25;nature++)for(const stack of [1,2,3,24,25,26,47,48,49,50]){
  s.modifiers=[];p.resetSummonData();p.species=speciesDataRegistry.getSpecies(species);p.formIndex=0;
  p.fusionSpecies=fused?speciesDataRegistry.getSpecies(SpeciesId.MEW):null;p.fusionFormIndex=0;p.fusionAbilityIndex=0;p.abilityIndex=0;
  p.level=level;p.nature=nature;p.customPokemonData.nature=-1;p.passive=false;p.ivs=Array(6).fill(31);p.hp=1;
  const f=modifierTypes.MYSTERY_ENCOUNTER_MACHO_BRACE,m:any=f().withIdFromFunc(f).newModifier(p);m.stackCount=stack;
  m.virtualStackCount=stack<50&&statsRows.length%2?1:0;s.modifiers=[m];
  const before={hp:p.hp,stats:p.stats.slice()},rg=Phaser.Math.RND.state(),rb=s.currentBattle.battleSeedState;
  p.calculateStats();
  expect(Phaser.Math.RND.state()).toBe(rg);expect(s.currentBattle.battleSeedState).toBe(rb);
  statsRows.push({species,fusion:p.fusionSpecies?.speciesId??0,level,nature,stack,virtual:m.virtualStackCount,baseAbility:p.getAbility(true).id,ability:p.getAbility().id,ivs:p.ivs.slice(),before,after:{hp:p.hp,stats:p.stats.slice()}});
 }
 expect(statsRows.length).toBe(7500);
 const owners:any[]=[],party=s.getPlayerParty(),pool=[p,s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.SHEDINJA),5),s.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.CHARIZARD),5)];
 for(const q of pool)q.updateInfo=async()=>{};
 const snapshot=()=>party.map((q:any)=>({id:q.id,species:q.species.speciesId,fusion:q.fusionSpecies?.speciesId??0,level:q.level,nature:q.getNature(),ivs:q.ivs.slice(),baseAbility:q.getAbility(true).id,ability:q.getAbility().id,onField:q.isOnField(),hp:q.hp,stats:q.stats.slice(),friendship:q.friendship}));
 const inventory=()=>s.modifiers.map((m:any)=>({key:m.type.id,owner:party.findIndex((q:any)=>q.id===m.pokemonId),stack:m.stackCount,virtual:m.virtualStackCount}));
 const pm:any=s.phaseManager;
 for(const fused of [false,true])for(let scenario=0;scenario<6;scenario++)for(let mask=0;mask<8;mask++)for(const stack of [1,49,50])for(const defeated of [false,true])for(const level of [5,1000]){
  party.splice(0,party.length,...(scenario===2||scenario===4?pool.slice().reverse():pool));s.modifiers=[];
  pm.phaseQueue.levels=[[]];pm.phaseQueue.currentLevel=0;pm.phaseQueue.deferredActive=false;
  for(let j=0;j<3;j++){
   const q=pool[j];q.resetSummonData();q.species=speciesDataRegistry.getSpecies([SpeciesId.MEW,SpeciesId.SHEDINJA,SpeciesId.CHARIZARD][j]);q.formIndex=0;
   q.fusionSpecies=fused?speciesDataRegistry.getSpecies(SpeciesId.MEW):null;q.fusionAbilityIndex=0;q.fusionFormIndex=0;q.level=level;q.nature=0;q.customPokemonData.nature=-1;q.ivs=Array(6).fill(31);q.friendship=0;q.passive=false;q.abilityIndex=0;
  }
  const add=(j:number,value:number,virtual=0)=>{const f=modifierTypes.MYSTERY_ENCOUNTER_MACHO_BRACE,m:any=f().withIdFromFunc(f).newModifier(pool[j]);m.stackCount=value;m.virtualStackCount=virtual;s.modifiers.push(m);};
  add(0,stack,scenario===5&&stack<50?1:0);
  if(scenario===3)add(0,stack===50?49:50);
  if(scenario===4)add(1,stack);
  if(scenario===5){const f=modifierTypes.SOOTHE_BELL,m:any=f().withIdFromFunc(f).newModifier(pool[0]);m.stackCount=3;s.modifiers.push(m);}
  for(const q of pool){q.calculateStats();q.hp=scenario===1&&q===pool[0]?0:q.stats[0];}
  const input=snapshot(),stock=inventory(),ids=party.filter((_q:any,j:number)=>mask&(1<<j)).map((q:any)=>q.id);
  const rg=Phaser.Math.RND.state(),rb=s.currentBattle.battleSeedState,expValue=s.getEnemyPokemon().getExpValue();
  s.applyPartyExp(expValue,defeated,false,new Set(ids));
  const queue=pm.phaseQueue.levels.flatMap((lv:any[])=>lv.map((phase:any)=>({name:phase.phaseName,party:phase.partyMemberIndex,exp:phase.expValue})));
  for(const phase of queue)expect(["ExpPhase","ShowPartyExpBarPhase"]).toContain(phase.name);
  expect(Phaser.Math.RND.state()).toBe(rg);expect(s.currentBattle.battleSeedState).toBe(rb);
  owners.push({fused,scenario,mask,stack,defeated,level,expValue,maxExpLevel:s.getMaxExpLevel(),enemy:{species:s.getEnemyPokemon().species.speciesId,form:s.getEnemyPokemon().formIndex,level:s.getEnemyPokemon().level},input,stock,output:snapshot(),afterStock:inventory(),queue});
 }
 expect(owners.length).toBe(1152);
 fs.writeFileSync(process.env.MACHO_OUT!,JSON.stringify({version:1,pin:"8555c08c823b856cbec4eb99ca84ea52a955836d",rows,statsRows,owners})+"\n");
},240000);
