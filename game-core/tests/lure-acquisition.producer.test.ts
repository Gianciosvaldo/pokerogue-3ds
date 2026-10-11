/* Actual Lure Scene/public selection and pool filtering; no lapse/encounter. */
import { audioManager } from "#app/global-audio-manager";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { ModifierPoolType } from "#enums/modifier-pool-type";
import { PokemonMove } from "#moves/pokemon-move";
import { DoubleBattleChanceBoosterModifier } from "#modifiers/modifier";
import { itemPoolChecks,regenerateModifierPoolThresholds } from "#modifiers/modifier-type";
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { ModifierData } from "#system/modifier-data";
import { PokemonData } from "#system/pokemon-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import {expect,it,vi} from "vitest";
it("Lure acquisition actual caller",async()=>{
 const N=Number(process.env.LUREACQ_N??2000),SEED=Number(process.env.LUREACQ_SEED??777);
 expect(process.versions.node.split(".")[0]).toBe("22");expect(Number.isSafeInteger(SEED)&&SEED>=0&&SEED<=0xffffffff).toBe(true);expect(Number.isSafeInteger(N)&&N>=120&&N<=100000).toBe(true);
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 const scene=game.scene as any,party=scene.getPlayerParty(),pool=Array.from({length:6},()=>scene.addPlayerPokemon(party[0].species,5));
 for(const p of pool)vi.spyOn(p,"updateInfo").mockResolvedValue(undefined);
 vi.spyOn(scene.modifierBar,"updateModifiers").mockImplementation(()=>{});
 vi.spyOn(scene.ui,"getHandler").mockImplementation(()=>({updateCostText(){}}) as any);
 vi.spyOn(scene.ui,"clearText").mockImplementation(()=>{});vi.spyOn(scene.ui,"setMode").mockResolvedValue(undefined as any);
 vi.spyOn(scene,"updateMoneyText").mockImplementation(()=>{});vi.spyOn(scene,"animateMoneyChanged").mockImplementation(()=>{});
 let restore=0;vi.spyOn(audioManager,"playSound").mockImplementation((key:any)=>{restore+=+(key==="se/restore");});
 const type=(name:string)=>{const f=(modifierTypes as any)[name];return f().withIdFromFunc(f);};
 const rng=()=>Phaser.Math.RND.state().replace(/^!rnd,/,"").split(",").map(Number);
 const lines=[`LURE_ACQUISITION 1 ${N}`],keys=["LURE","SUPER_LURE","MAX_LURE"],durations=[10,15,30];let textMessages:string[]=[],messages=0,errors=0,ended=0,result=false;
 const actualQueue=scene.phaseManager.queueMessage;vi.spyOn(scene.phaseManager,"queueMessage").mockImplementation((...args:any[])=>{messages++;textMessages.push(String(args[0]));return actualQueue.apply(scene.phaseManager,args);});
 vi.spyOn(scene.ui,"playError").mockImplementation(()=>{errors++;});vi.spyOn(scene.phaseManager,"shiftPhase").mockImplementation(()=>{ended++;});
 const actualAdd=scene.addModifier;scene.addModifier=function(...args:any[]){result=actualAdd.apply(this,args);return result;};
  const mon = (p: any) => [p.id, p.species.speciesId, p.formIndex, p.fusionSpecies?.speciesId ?? -1,
    p.fusionFormIndex ?? 0, p.level, p.exp, p.hp, p.status?.effect ?? 0, p.getNature(),
    p.nature, p.customPokemonData.nature, p.getAbility(true).id, p.getAbility().id,
    p.hasPassive() ? p.getPassiveAbility().id : 0, p.friendship, p.gender, p.abilityIndex,
    p.fusionAbilityIndex ?? -1, +p.shiny, +p.passive, p.variant, p.luck, +p.pauseEvolutions,
    p.teraType, p.getMoveset().length,
    ...Array.from({length:4}, (_,j) => {const m=p.getMoveset()[j];return m?[m.moveId,m.ppUsed,m.ppUp,m.getMovePp()]:[0,0,0,0];}).flat(),
    ...p.ivs, ...p.stats];

 const emit=(tag:string)=>{lines.push(`${tag} ${scene.modifiers.length}`);for(const m of scene.modifiers){const owner=m.pokemonId==null||m.pokemonId===-1?-1:party.findIndex((p:any)=>p.id===m.pokemonId);expect(owner>=-1).toBe(true);lines.push(`${tag==="I"?"H":"K"} ${m.type.id} ${m.stackCount} ${owner} 0 ${m.getBattleCount?.()??0}`);}};
 let changes=0,full=0,blocked=0,refreshed=0,paidCases=0;const families=[0,0,0],paths=[0,0];
 keys.forEach(k=>itemPoolChecks.set(k as any,false));
 for(let ci=0;ci<N;ci++){
  const family=ci%3,key=keys[family],max=durations[family],mode=Math.floor(ci/3)%8,path=Math.floor(ci/48)%2;
  const count=1+rnd(6),paid=Math.floor(ci/24)%3!==0,cost=paid?42+rnd(500):-1,tier=rnd(5);
  scene.currentBattle.waveIndex=ci%17===0?199:5;party.length=0;scene.modifiers.length=0;messages=errors=ended=restore=0;textMessages=[];result=false;
  for(let j=0;j<count;j++){const p=pool[(j+ci%6)%6];p.resetSummonData();p.resetBattleAndWaveData();p.passive=false;p.pauseEvolutions=false;p.fusionSpecies=null;p.fusionFormIndex=0;p.fusionAbilityIndex=0;p.abilityIndex=0;p.level=5+rnd(60);p.exp=rnd(1000);p.ivs=Array.from({length:6},()=>rnd(32));p.nature=rnd(25);p.customPokemonData.nature=-1;p.moveset=[new PokemonMove(MoveId.SPLASH)];party.push(p);}
  const append=(name:string,stack=1,duration=0,owner=-1)=>{const t=type(name),m=owner<0?t.newModifier():t.newModifier(party[owner]);m.stackCount=stack;if(duration)m.battleCount=duration;scene.modifiers.push(m);};
  if(mode!==7){append("HEALING_CHARM");append("LEFTOVERS",1,0,0);append("DIRE_HIT",1,3);}
  if([1,2,3,4,5].includes(mode))append(key,1+ci%2,mode===3||mode===5?max:mode===2?1:[Math.floor(max*.6)-1,Math.floor(max*.6),max-1][Math.floor(ci/24)%3]);
  if(mode===4||mode===5)append(key,mode===4?2:1,mode===4?max:1);
  if(mode===6)append(keys[(family+1)%3],2,2);
  if(ci%50<2){const target=mode===0||mode===6||mode===7?95:96;while(scene.modifiers.length<target)append("HEALING_CHARM");}
  party.forEach((p:any)=>{p.calculateStats();p.hp=p.getMaxHp();});
  scene.phaseManager.clearAllPhases();for(let j=0;j<5;j++)scene.pokeballCounts[j]=j?0:5;
  scene.money=10000.25+ci;Phaser.Math.RND.sow([`lure-acquisition-${SEED}-${ci}`]);const r=rng(),br=scene.currentBattle.rngState;
  const seedState=Phaser.Math.RND.state();regenerateModifierPoolThresholds(party,ModifierPoolType.PLAYER);const eligible=keys.map(k=>+!!itemPoolChecks.get(k as any));Phaser.Math.RND.state(seedState);
  const partyBefore=JSON.stringify(party.map((p:any)=>new PokemonData(p))),profile=JSON.stringify([scene.gameData.achvUnlocks,scene.gameData.gameStats]);
  lines.push(`C ${ci} ${count} ${family} ${path} ${cost} ${tier} ${scene.money} ${scene.currentBattle.waveIndex}`,`R ${r.join(" ")}`,`F ${eligible.join(" ")}`);
  party.forEach((p:any)=>{const v=mon(p);v.forEach((x:number)=>expect(Number.isFinite(x)).toBe(true));lines.push(`P ${v.join(" ")}`);});emit("I");
  const before=JSON.stringify(scene.modifiers.map((m:any)=>[m.type.id,m.stackCount,m.getBattleCount?.()??0])),balls=Array.from({length:5},(_,j)=>scene.pokeballCounts[j]);
  const t=type(key);t.tier=tier;const phase=new SelectModifierPhase() as any;
  if(path===0)phase.applyModifier(t.newModifier(),cost,false);
  else if(eligible[family])phase.applyChosenModifier(t,cost,()=>{});
  else blocked++;
  lines.push(`A ${+result} ${errors} ${ended} ${messages} ${restore} ${scene.money} ${Array.from({length:5},(_,j)=>scene.pokeballCounts[j]-balls[j]).join(" ")}`);
  lines.push(`M ${Buffer.from(textMessages.join("\n")).toString("hex")||"-"}`);party.forEach((p:any)=>lines.push(`T ${mon(p).join(" ")}`));emit("J");lines.push(`S ${rng().join(" ")}`,`E ${ci}`);
  for(const m of scene.modifiers.filter((m:any)=>m instanceof DoubleBattleChanceBoosterModifier)){const data=new ModifierData(m,true),loaded=new ModifierData(JSON.parse(JSON.stringify(data)),true).toModifier(DoubleBattleChanceBoosterModifier);expect(loaded.getBattleCount()).toBe(m.getBattleCount());expect(loaded.stackCount).toBe(m.stackCount);expect(loaded.getMaxBattles()).toBe(m.getMaxBattles());}
  const queued=scene.phaseManager.phaseQueue.levels.flat();expect(queued.map((p:any)=>p.phaseName)).toEqual(textMessages.map(()=>"MessagePhase"));
  expect(partyBefore).toBe(JSON.stringify(party.map((p:any)=>new PokemonData(p))));expect(profile).toBe(JSON.stringify([scene.gameData.achvUnlocks,scene.gameData.gameStats]));expect(rng()).toEqual(r);expect(br).toBe(scene.currentBattle.rngState);
  changes+=+(before!==JSON.stringify(scene.modifiers.map((m:any)=>[m.type.id,m.stackCount,m.getBattleCount?.()??0])));full+=messages;refreshed+=restore;paidCases+=+paid;families[family]++;paths[path]++;
 }
 expect(changes&&full&&blocked&&refreshed&&paidCases).toBeGreaterThan(0);expect(families.every(n=>n>0)&&paths.every(n=>n>0)).toBe(true);
 lines.push(`Z ${N}`);fs.writeFileSync(process.env.LUREACQ_OUT??"/tmp/lure-acquisition.txt",lines.join("\n")+"\n");
 console.log(`LURE_ACQUISITION cases=${N} changes=${changes} fullfallback=${full} publicfiltered=${blocked} refresh=${refreshed} paid=${paidCases} kinds=${families.join("/")} paths=${paths.join("/")}`);
},360000);
