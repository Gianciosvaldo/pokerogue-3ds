/* Actual Scene.applyPartyExp -> Pokemon.addFriendship; presentation only stubs. */
import { getGameMode } from "#app/game-mode";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getStarterValueFriendshipCap } from "#balance/starters";
import { timedEventManager } from "#app/global-event-manager";
import { modifierTypes } from "#data/data-lists";
import { GameModes } from "#enums/game-modes";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import { PokemonData } from "#system/pokemon-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import {expect,it,vi} from "vitest";
it("battle friendship real distribution",async()=>{
 const N=Number(process.env.SOOTHB_N??2000),SEED=Number(process.env.SOOTHB_SEED??777);
 expect(process.versions.node.split(".")[0]).toBe("22");expect(Number.isSafeInteger(N)&&N>=120&&N<=100000).toBe(true);expect(Number.isSafeInteger(SEED)&&SEED>=0&&SEED<=0xffffffff).toBe(true);
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 const scene=game.scene as any,party=scene.getPlayerParty(),battle=scene.currentBattle;
 const pool=Array.from({length:6},()=>scene.addPlayerPokemon(party[0].species,100));
 vi.spyOn(scene,"validateAchv").mockReturnValue(false);vi.spyOn(scene.candyBar,"showStarterSpeciesCandy").mockImplementation(()=>{});
 vi.spyOn(timedEventManager,"getClassicFriendshipMultiplier").mockReturnValue(3);
 const type=(name:string)=>{const f=(modifierTypes as any)[name];return f().withIdFromFunc(f);};
 const rng=()=>Phaser.Math.RND.state().replace(/^!rnd,/,"").split(",").map(Number);
 const lines=[`SOOTHE_BATTLE 1 ${N}`];let boosted=0,capped=0,refused=0,negative=0;
  const mon = (p: any) => [p.id, p.species.speciesId, p.formIndex, p.fusionSpecies?.speciesId ?? -1,
    p.fusionFormIndex ?? 0, p.level, p.exp, p.hp, p.status?.effect ?? 0, p.getNature(),
    p.nature, p.customPokemonData.nature, p.getAbility(true).id, p.getAbility().id,
    p.hasPassive() ? p.getPassiveAbility().id : 0, p.friendship, p.gender, p.abilityIndex,
    p.fusionAbilityIndex ?? -1, +p.shiny, +p.passive, p.variant, p.luck, +p.pauseEvolutions,
    p.teraType, p.getMoveset().length,
    ...Array.from({length:4}, (_,j) => {const m=p.getMoveset()[j];return m?[m.moveId,m.ppUsed,m.ppUp,m.getMovePp()]:[0,0,0,0];}).flat(),
    ...p.ivs, ...p.stats];

 for(let ci=0;ci<N;ci++){
  const count=1+rnd(6),mode=ci%2,shape=Math.floor(ci/2)%8;
  scene.gameMode=battle.gameMode=getGameMode(mode?GameModes.ENDLESS:GameModes.CLASSIC);
  scene.phaseManager.clearAllPhases();battle.playerParticipantIds.clear();scene.modifiers.length=0;party.length=0;
  for(let j=0;j<count;j++){
   const p=pool[(j+ci%6)%6];p.resetSummonData();p.resetBattleAndWaveData();p.fusionSpecies=null;p.passive=false;p.pauseEvolutions=false;p.abilityIndex=0;p.fusionAbilityIndex=0;p.level=100;p.exp=0;p.nature=rnd(25);p.ivs=Array.from({length:6},()=>rnd(32));p.moveset=[new PokemonMove(MoveId.SPLASH)];p.calculateStats();p.hp=(ci+j)%5===0?0:p.getMaxHp();p.friendship=[199,200,254,255][(ci+j)%4];party.push(p);
   if((ci+j)%4!==0&&shape!==7)battle.playerParticipantIds.add(p.id);
  }
  const append=(owner:number,stack:number)=>{const m=type("SOOTHE_BELL").newModifier(party[Math.max(0,owner)]);m.pokemonId=owner<0?-1:party[owner].id;m.stackCount=stack;scene.modifiers.push(m);};
  if(shape===1||shape===2||shape===3)append(0,shape);
  if(shape===4){append(-1,1);append(0,3);}
  if(shape===5){append(0,2);append(-1,3);append(0,1);}
  if(shape===6)append(count-1,3);
  const root=party[0].species.getRootSpeciesId(),sd=scene.gameData.starterData[root];
  const cap=getStarterValueFriendshipCap(speciesDataRegistry.getStarterCost(root));
  sd.friendship=[0,cap-1,cap-9,cap,2*cap-1][Math.floor(ci/16)%5];sd.candyCount=ci%7===0?9999:ci%7===1?9998:rnd(100);
  scene.money=1000.25+ci;Phaser.Math.RND.sow([`soothe-battle-${SEED}-${ci}`]);const r=rng(),br=battle.rngState;
  const inv=JSON.stringify(scene.modifiers),wallet=scene.money;
  lines.push(`C ${ci} ${count} ${mode} ${sd.friendship} ${sd.candyCount} ${wallet}`,`R ${r.join(" ")}`);
  party.forEach((p:any)=>{const v=mon(p);v.forEach((x:number)=>expect(Number.isFinite(x)).toBe(true));lines.push(`P ${v.join(" ")} ${+battle.playerParticipantIds.has(p.id)}`);});
  lines.push(`I ${scene.modifiers.length}`);for(const m of scene.modifiers)lines.push(`H ${m.pokemonId===-1?-1:party.findIndex((p:any)=>p.id===m.pokemonId)} ${m.stackCount}`);
  const old=party.map((p:any)=>p.friendship),oldCandy=sd.candyCount;
  scene.applyPartyExp(0,true);
  party.forEach((p:any,j:number)=>{boosted+=+(p.friendship-old[j]>3);capped+=+(old[j]>=254&&p.hp&&battle.playerParticipantIds.has(p.id));lines.push(`T ${mon(p).join(" ")}`);});
  refused+=+(oldCandy===9999&&sd.friendship===cap-1);
  expect(inv).toBe(JSON.stringify(scene.modifiers));expect(scene.money).toBe(wallet);expect(rng()).toEqual(r);expect(battle.rngState).toBe(br);
  expect(scene.phaseManager.phaseQueue.levels.flat().length).toBe(0);
  lines.push(`A ${sd.friendship} ${sd.candyCount}`,`S ${rng().join(" ")}`,`E ${ci}`);
  const control=party[0],before=control.friendship,progress=sd.friendship,candies=sd.candyCount,amount=[-1,0,6][ci%3],cappedControl=ci%3===2;scene.modifiers.length=0;control.addFriendship(amount,cappedControl);lines.push(`F ${before} ${progress} ${candies} ${amount} ${+cappedControl} ${control.friendship} ${sd.friendship} ${sd.candyCount}`);if(amount<=0){expect(control.friendship).toBe(Math.max(0,before+amount));expect(sd.friendship).toBe(progress);expect(sd.candyCount).toBe(candies);negative++;}
 }
 lines.push(`Z ${N}`);fs.writeFileSync(process.env.SOOTHB_OUT??"/tmp/soothe-battle.txt",lines.join("\n")+"\n");
 console.log(`SOOTHE_BATTLE cases=${N} boosted=${boosted} capped=${capped} refusedCandy=${refused} nonpositiveControls=${negative}`);
 expect(boosted&&capped&&refused).toBeGreaterThan(0);
},240000);
