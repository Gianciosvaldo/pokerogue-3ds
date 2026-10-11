/* Actual held-item selection and Scene update boundary, pinned8555c08/Node22.
 * Party UI choices, graphical refresh/audio and phase-end are headless inputs. */
import { audioManager } from "#app/global-audio-manager";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import { PokemonFriendshipBoosterModifier } from "#modifiers/modifier";
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { PokemonData } from "#system/pokemon-data";
import { GameManager } from "#test/framework/game-manager";
import { NumberHolder } from "#utils/common";
import Phaser from "phaser";
import * as fs from "node:fs";
import {expect,it,vi} from "vitest";
it("canonical Soothe Bell actual caller",async()=>{
 const N=Number(process.env.GC_SOOTHEACQ_N??2000),SEED=Number(process.env.GC_SOOTHEACQ_SEED??777);
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
 vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});
 const type=(name:string)=>{const f=(modifierTypes as any)[name];return f().withIdFromFunc(f);};
 const rng=()=>Phaser.Math.RND.state().replace(/^!rnd,/,"").split(",").map(Number);
 const orb=(name:string)=>name==="SOOTHE_BELL";
 const lines=[`SOOTHEACQ_CALLER 1 ${N}`];let messages=0,errors=0,ended=0,result=false,filter="";
 vi.spyOn(scene.phaseManager,"queueMessage").mockImplementation((message:any)=>{expect(String(message)).toMatch(/The stack for Soothe Bell is full\./);messages++;});
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
 const emit=(tag:string)=>{lines.push(`${tag} ${scene.modifiers.length}`);for(const m of scene.modifiers){const owner=m.pokemonId==null||m.pokemonId===-1?-1:party.findIndex((p:any)=>p.id===m.pokemonId);expect(owner>=-1).toBe(true);expect(m.virtualStackCount).toBe(0);lines.push(`${tag==="I"?"H":"K"} ${m.type.id} ${m.stackCount} ${owner} 0 0 0`);}};
 let changes=0,full=0,blocked=0,wild=0,duplicate=0,fainted=0;
 for(let ci=0;ci<N;ci++){
  const family="SOOTHE_BELL",other=family;
  const count=1+rnd(6),target=rnd(count),mode=Math.floor(ci/2)%5,path=Math.floor(ci/10)%2,cost=Math.floor(ci/2)%3===0?-1:ci%7===0?0:50+rnd(500),tier=rnd(5);
  party.length=0;scene.modifiers.length=0;messages=errors=ended=0;result=false;filter="";
  for(let j=0;j<count;j++){const p=pool[(j+ci%2)%6];p.resetSummonData();p.resetBattleAndWaveData();p.passive=false;p.pauseEvolutions=false;p.fusionSpecies=null;p.fusionFormIndex=0;p.fusionAbilityIndex=0;p.abilityIndex=0;p.level=1+rnd(100);p.exp=rnd(100000);p.ivs=Array.from({length:6},()=>rnd(32));p.nature=rnd(25);p.customPokemonData.nature=-1;p.moveset=[new PokemonMove(MoveId.TACKLE,rnd(4),rnd(4)),new PokemonMove(MoveId.GROWL,rnd(4),rnd(4))];party.push(p);}
  const append=(name:string,owner=-1,stack=1)=>{const t=type(name),m=owner<0&&orb(name)?t.newModifier(party[0]):owner<0?t.newModifier():t.newModifier(party[owner]);if(orb(name)&&owner<0)m.pokemonId=-1;m.stackCount=stack;scene.modifiers.push(m);};
  if(ci%100!==0){
  append("HEALING_CHARM");append("LEFTOVERS",(target+1)%count);
  if(ci%3===0){append(family,-1,1+rnd(3));wild++;}
  if(mode===1||mode===3)append(family,target,3);else if(mode===2||mode===4)append(family,target,1+ci%2);
  if(mode===3||mode===4){append(mode===3?family:other,target);duplicate++;}
  if(count>1)append(other,(target+1)%count,1+rnd(3));
  if(ci%4===0)append("MULTI_LENS",(target+1)%count,1+rnd(2));
  if(ci%50<4){const boundary=ci%50<2?95:96;while(scene.modifiers.length<boundary)append("HEALING_CHARM");}
  }
  party.forEach((p:any)=>{p.calculateStats();p.hp=rnd(2)?p.getMaxHp():0;});fainted+=+(party[target].hp===0);
  for(let j=0;j<5;j++)scene.pokeballCounts[j]=j?0:5;
  scene.money=10000.25+ci;Phaser.Math.RND.sow([`soothe-acquisition-${SEED}-${ci}`]);const r=rng(),battleRng=scene.currentBattle.rngState;
  const profile=JSON.stringify([scene.gameData.achvUnlocks,scene.gameData.gameStats]),partyBefore=JSON.stringify(party.map((p:any)=>new PokemonData(p)));
  lines.push(`C ${ci} ${count} ${target} ${path} ${cost} ${tier} ${scene.money} ${ci%2}`,`R ${r.join(" ")}`);
  party.forEach((p:any)=>{const v=mon(p);expect(v.length).toBe(54);v.forEach((n:number)=>expect(Number.isFinite(n)).toBe(true));lines.push(`P ${v.join(" ")}`);});emit("I");
  const before=JSON.stringify(scene.modifiers.map((m:any)=>[m.type.id,m.pokemonId,m.stackCount])),balls=Array.from({length:5},(_,j)=>scene.pokeballCounts[j]);
  const t=type(family);t.tier=tier;
  if(path===0)(new SelectModifierPhase() as any).applyModifier(t.newModifier(party[target]),cost,false);
  else{
   const ui=vi.spyOn(scene.ui,"setModeWithoutClear").mockImplementation((_mode:any,_kind:any,_cursor:any,callback:any,selectFilter:any)=>{filter=selectFilter(party[target])??"";if(!filter)callback(target,0);});
   (new SelectModifierPhase() as any).openModifierMenu(t,cost,()=>{});await Promise.resolve();ui.mockRestore();
  }
  const projected=scene.modifiers.filter((m:any)=>m instanceof PokemonFriendshipBoosterModifier&&m.pokemonId===party[target].id).length;
  lines.push(`A ${+result} ${errors} ${ended} ${messages} ${+!!filter} ${scene.money} ${Array.from({length:5},(_,j)=>scene.pokeballCounts[j]-balls[j]).join(" ")} ${projected}`);
  party.forEach((p:any)=>lines.push(`T ${mon(p).join(" ")}`));emit("J");lines.push(`S ${rng().join(" ")}`,`E ${ci}`);
  expect(profile).toBe(JSON.stringify([scene.gameData.achvUnlocks,scene.gameData.gameStats]));expect(partyBefore).toBe(JSON.stringify(party.map((p:any)=>new PokemonData(p))));expect(rng()).toEqual(r);expect(scene.currentBattle.rngState).toBe(battleRng);
  changes+=+(before!==JSON.stringify(scene.modifiers.map((m:any)=>[m.type.id,m.pokemonId,m.stackCount])));full+=messages;blocked+=+!!filter;
 }
 expect(changes&&full&&blocked&&wild&&duplicate&&fainted).toBeGreaterThan(0);
 lines.push(`Z ${N} ${changes} ${full} ${blocked} ${wild} ${duplicate} ${fainted}`);fs.writeFileSync(process.env.GC_SOOTHEACQ_OUT??"/tmp/game-soothe-acquisition.txt",lines.join("\n")+"\n");
 console.log(`Soothe Bell actual acquisition changes=${changes} fullfallback=${full} selectionblocked=${blocked} wildcard=${wild} duplicate=${duplicate} fainted=${fainted}`);
},240000);
