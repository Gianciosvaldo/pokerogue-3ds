/* Actual BattleEnd, delegated lapse/removal and independent ModifierData resume. */
import { modifierTypes } from "#data/data-lists";
import { BattleType } from "#enums/battle-type";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import { DoubleBattleChanceBoosterModifier,LapsingPersistentModifier } from "#modifiers/modifier";
import { BattleEndPhase } from "#phases/battle-end-phase";
import { VictoryPhase } from "#phases/victory-phase";
import { ModifierData } from "#system/modifier-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import {expect,it,vi} from "vitest";
it("Lure actual BattleEnd boundary",async()=>{
 const N=Number(process.env.LURELAP_N??2000),SEED=Number(process.env.LURELAP_SEED??777);
 expect(process.versions.node.split(".")[0]).toBe("22");expect(Number.isSafeInteger(N)&&N>=120&&N<=100000).toBe(true);expect(Number.isSafeInteger(SEED)&&SEED>=0&&SEED<=0xffffffff).toBe(true);
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 const scene=game.scene as any,party=scene.getPlayerParty(),battle=scene.currentBattle,enemy=scene.getEnemyParty(),holder=party[0];
 const reserve=scene.addEnemyPokemon(enemy[0].species,5);enemy.push(reserve);
 for(const p of enemy)vi.spyOn(p,"destroy").mockImplementation(()=>{});
 vi.spyOn(holder,"updateInfo").mockResolvedValue(undefined);vi.spyOn(scene.modifierBar,"updateModifiers").mockImplementation(()=>{});vi.spyOn(scene.phaseManager,"shiftPhase").mockImplementation(()=>{});
 const type=(key:string)=>{const f=(modifierTypes as any)[key];return f().withIdFromFunc(f);};
 const rng=()=>Phaser.Math.RND.state().replace(/^!rnd,/,"").split(",").map(Number);
 const lines=[`LURE_LAPSE 1 ${N}`],keys=["LURE","SUPER_LURE","MAX_LURE"],maxes=[10,15,30];
 let observed:string[]=[],cleanup=0,lapseCalls=0,removals=0,updates=0;
 const clear=scene.clearEnemyHeldItemModifiers;scene.clearEnemyHeldItemModifiers=function(...a:any[]){cleanup++;observed.push("cleanup");return clear.apply(this,a);};
 const lapse=LapsingPersistentModifier.prototype.lapse;LapsingPersistentModifier.prototype.lapse=function(...a:any[]){expect(cleanup).toBe(1);lapseCalls++;observed.push(`lapse:${this.type.id}`);return lapse.apply(this,a);};
 const remove=scene.removeModifier;scene.removeModifier=function(m:any,...a:any[]){removals++;observed.push(`remove:${m.type.id}`);return remove.call(this,m,...a);};
 const update=scene.updateModifiers;scene.updateModifiers=function(...a:any[]){updates++;observed.push("update");return update.apply(this,a);};
 let expiredTotal=0,tempExpired=0,victories=0,flees=0,nonvictories=0,duplicates=0,trainerControls=0;const kinds=[0,0,0];
  const mon = (p: any) => [p.id, p.species.speciesId, p.formIndex, p.fusionSpecies?.speciesId ?? -1,
    p.fusionFormIndex ?? 0, p.level, p.exp, p.hp, p.status?.effect ?? 0, p.getNature(),
    p.nature, p.customPokemonData.nature, p.getAbility(true).id, p.getAbility().id,
    p.hasPassive() ? p.getPassiveAbility().id : 0, p.friendship, p.gender, p.abilityIndex,
    p.fusionAbilityIndex ?? -1, +p.shiny, +p.passive, p.variant, p.luck, +p.pauseEvolutions,
    p.teraType, p.getMoveset().length,
    ...Array.from({length:4}, (_,j) => {const m=p.getMoveset()[j];return m?[m.moveId,m.ppUsed,m.ppUp,m.getMovePp()]:[0,0,0,0];}).flat(),
    ...p.ivs, ...p.stats];


 const stock=()=>scene.modifiers.map((m:any)=>[m.type.id,m.stackCount,m.pokemonId==null||m.pokemonId===-1?-1:party.findIndex((p:any)=>p.id===m.pokemonId),0,m.getBattleCount?.()??0]);
 const emit=(tag:string)=>{const values=stock();lines.push(`${tag} ${values.length}`);values.forEach(v=>lines.push(`${tag==="I"?"H":"K"} ${v.join(" ")}`));};
 for(let ci=0;ci<N;ci++){
  const kind=ci%3,key=keys[kind],max=maxes[kind],event=Math.floor(ci/3)%3,dup=ci%3,initialVictory=event===0,duplicateVictory=dup>0&&ci%7===0,success=event===1;
  holder.resetSummonData();holder.resetTurnData();holder.resetBattleAndWaveData();holder.level=100;holder.exp=0;holder.nature=rnd(25);holder.ivs=Array.from({length:6},()=>rnd(32));holder.moveset=[new PokemonMove(MoveId.SPLASH)];holder.calculateStats();holder.hp=holder.getMaxHp();holder.passive=false;holder.pauseEvolutions=false;
  scene.modifiers.length=0;scene.enemyModifiers.length=0;scene.phaseManager.clearAllPhases();battle.playerParticipantIds.clear();battle.battleScore=0;battle.turn=2;battle.moneyScattered=0;battle.enemyFaints=0;scene.money=1000.25+ci;
  const append=(name:string,stack:number,timer=0,owner=-1)=>{const t=type(name),m=owner<0?t.newModifier():t.newModifier(holder);m.stackCount=stack;if(timer)m.battleCount=timer;scene.modifiers.push(m);};
  append("HEALING_CHARM",1);if(ci%30!==0)append(key,1+ci%2,[1,2,max][Math.floor(ci/9)%3]);append("DIRE_HIT",1,[1,2,5][Math.floor(ci/27)%3]);append("LEFTOVERS",1,0,0);
  if(ci%30!==0&&ci%4!==0)append(key,ci%2?1:2,[max,1,2][Math.floor(ci/9)%3]);
  if(ci%30!==0&&ci%5!==0)append(keys[(kind+1)%3],2,[1,2,maxes[(kind+1)%3]][Math.floor(ci/15)%3]);
  if(!enemy.includes(reserve))enemy.push(reserve);const originalStock=JSON.stringify(stock());enemy[0].hp=0;reserve.hp=reserve.getMaxHp();battle.battleType=BattleType.TRAINER;
  const member=new VictoryPhase(2);vi.spyOn(member,"end").mockImplementation(()=>{});member.start();expect(scene.phaseManager.hasPhaseOfType("BattleEndPhase")).toBe(false);expect(JSON.stringify(stock())).toBe(originalStock);trainerControls++;
  scene.phaseManager.clearAllPhases();battle.battleType=BattleType.WILD;enemy.length=1;enemy[0].hp=enemy[0].getMaxHp();battle.successfulRun=success;
  scene.enemyModifiers.push(type("LEFTOVERS").newModifier(enemy[0]));
  Phaser.Math.RND.sow([`lure-lapse-${SEED}-${ci}`]);const r=rng(),br=battle.rngState,monBefore=mon(holder),before=stock();
  const serialized=scene.modifiers.map((m:any)=>({json:JSON.parse(JSON.stringify(new ModifierData(m,true))),cls:m.constructor}));
  lines.push(`C ${ci} ${kind} ${+initialVictory} ${dup} ${+duplicateVictory} ${+success} ${scene.money}`,`R ${r.join(" ")}`,`P ${monBefore.join(" ")}`);emit("I");
  const execute=()=>{
   scene.phaseManager.clearAllPhases();for(let j=0;j<dup;j++)scene.phaseManager.pushNew("BattleEndPhase",duplicateVictory);
   cleanup=lapseCalls=removals=updates=0;observed=[];
   const phase=new BattleEndPhase(initialVictory);vi.spyOn(phase,"end").mockImplementation(()=>{});phase.start();
   expect(scene.phaseManager.hasPhaseOfType("BattleEndPhase")).toBe(false);expect(scene.enemyModifiers.length).toBe(0);expect(cleanup).toBe(1);expect(updates).toBe(2);expect(observed.slice(0,2)).toEqual(["cleanup","update"]);expect(observed.at(-1)).toBe("update");
   const expectedLapse=before.filter(v=>v[4]>0).map(v=>`lapse:${v[0]}`);expect(observed.filter(x=>x.startsWith("lapse:"))).toEqual(expectedLapse);expect(lapseCalls).toBe(expectedLapse.length);
   const expectedRemoved=before.filter(v=>v[4]===1).map(v=>`remove:${v[0]}`);expect(observed.filter(x=>x.startsWith("remove:"))).toEqual(expectedRemoved);
   expect(rng()).toEqual(r);expect(br).toBe(battle.rngState);expect(mon(holder)).toEqual(monBefore);
   return {stock:stock(),lapseCalls,removals,phaseVictory:phase.isVictory,queue:scene.phaseManager.phaseQueue.levels.flat().map((p:any)=>p.phaseName)};
  };
  const first=execute();lines.push(`A ${first.lapseCalls} ${first.removals} ${+first.phaseVictory} ${first.queue.length}`);expect(first.queue).toEqual(first.phaseVictory||success?[]:["SelectModifierPhase"]);
  lines.push(`T ${mon(holder).join(" ")}`);emit("J");lines.push(`S ${rng().join(" ")}`,`E ${ci}`);
  scene.modifiers=serialized.map(v=>new ModifierData(v.json,true).toModifier(v.cls));scene.enemyModifiers.push(type("LEFTOVERS").newModifier(enemy[0]));const loaded=execute();expect(loaded).toEqual(first);
  expiredTotal+=before.filter(v=>keys.includes(v[0] as string)&&v[4]===1).length;tempExpired+=before.filter(v=>v[0]==="DIRE_HIT"&&v[4]===1).length;victories+=+first.phaseVictory;flees+=+(!first.phaseVictory&&success);nonvictories+=+(!first.phaseVictory&&!success);duplicates+=dup>0?1:0;kinds[kind]++;
 }
 LapsingPersistentModifier.prototype.lapse=lapse;
 lines.push(`Z ${N}`);fs.writeFileSync(process.env.LURELAP_OUT??"/tmp/lure-lapse.txt",lines.join("\n")+"\n");
 console.log(`LURE_LAPSE cases=${N} expired=${expiredTotal} tempExpired=${tempExpired} victory=${victories} flee=${flees} nonvictory=${nonvictories} duplicateCases=${duplicates} trainerKONoTick=${trainerControls} kinds=${kinds.join("/")} sourceResume=${N}`);
 expect(expiredTotal&&tempExpired&&victories&&flees&&nonvictories).toBeGreaterThan(0);
},360000);
